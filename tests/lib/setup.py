"""What a test puts in place before medit starts.

Some things cannot be arranged once the program is running. The terminal reads
its shell out of the preferences when the pane is first shown, and a test about
a shell that fails to start has to have that shell configured before then --
there is no moment in between where a test could step in.

So a test may define a setup function beside its run function::

    def setup(s):
        s.pref("Plugins/Terminal/shell", s.script("shell", "#!/bin/sh\\nexit 1\\n"))

    def run(t):
        ...

Everything it creates lives in the sandbox root, which is removed with the rest
of the sandbox when the test ends, and the same object is on the test as
t.sandbox, so a script written here and a file it wrote are named the same way
from both halves.
"""

import json
import os
import shlex
import sys

from xml.sax.saxutils import escape


HERE = os.path.dirname(os.path.abspath(__file__))

# Written by moo_prefs_save() under XDG_DATA_HOME at exit, and read at startup.
PREFS_FILE = os.path.join("medit", "prefs.xml")

# The user's copy of the language server list. lsp_manager_init() reads it once,
# when the plugin is switched on, so it has to be here before medit starts --
# the same reason the terminal's shell is a setup-time setting.
LSP_FILE = os.path.join("medit", "lsp.xml")

LSP_XML = """<?xml version="1.0" encoding="UTF-8"?>
<medit-lsp version="1.0">
%s
</medit-lsp>
"""

PREFS_XML = """<?xml version="1.0" encoding="UTF-8"?>
<moo-prefs version="1.0">
  <Prefs>
%s
  </Prefs>
</moo-prefs>
"""

PREFS_ITEM = '    <item name="%s" type="%s">%s</item>'


def _typed(value):
    """The type name and the text prefs.xml would carry for a value.

    Not everything is a string. moo_prefs_new_key_bool() and the plugin
    framework's enabled key register their type, and a file that calls a
    boolean a string makes item_set_type() convert it and complain -- one
    Moo-CRITICAL per run, in every test, drowning the criticals a test is
    there to notice. The words are the ones medit writes itself.
    """
    if isinstance(value, bool):
        return "bool", "TRUE" if value else "FALSE"

    if isinstance(value, int):
        return "int", str(value)

    return "string", escape(str(value))


class Setup(object):
    def __init__(self, root, data_home, log_dir):
        self.root = root
        self.data_home = data_home
        self.log_dir = log_dir
        self.files = []
        self._prefs = {}
        self._servers = []
        self._lsp_config = None

    def path(self, *parts):
        """A path inside the sandbox: what a helper script writes to."""
        return os.path.join(self.root, *parts)

    def read(self, *parts):
        """The contents of such a file, or "" while it does not exist yet."""
        return self.read_path(self.path(*parts))

    def exists(self, *parts):
        """Whether such a file or directory is there at all."""
        return os.path.exists(self.path(*parts))

    def isdir(self, *parts):
        """Whether it is there and is a directory."""
        return os.path.isdir(self.path(*parts))

    def read_bytes(self, *parts):
        """The bytes of such a file, or b"" while it does not exist yet.

        read() decodes, replacing whatever it could not, which is the wrong
        question for a test about what was written.
        """
        try:
            with open(self.path(*parts), "rb") as f:
                return f.read()
        except FileNotFoundError:
            return b""

    def read_path(self, path):
        """The same, for a path a helper here has already built."""
        try:
            with open(path, errors="replace") as f:
                return f.read()
        except FileNotFoundError:
            return ""

    def write(self, name, body):
        path = self.path(name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(body)
        return path

    def write_bytes(self, name, data):
        """A file with exactly those bytes in it, by path.

        write() encodes as the locale says, which is UTF-8 in these tests. A
        test about encodings needs the bytes it asked for and no others.
        """
        path = self.path(name)
        os.makedirs(os.path.dirname(path), exist_ok=True)

        with open(path, "wb") as f:
            f.write(data)

        return path

    def script(self, name, body):
        """An executable file in the sandbox, by path.

        Readable and runnable by this user and nobody else: the sandbox belongs
        to whoever runs the tests, and the only thing that executes what is in
        it is the medit started from here.
        """
        path = self.write(name, body)
        os.chmod(path, 0o700)
        return path

    def open(self, path):
        """A file for medit to open at startup, by path.

        On the command line rather than through the Open dialog: a test that
        needs a document in the window is not a test of the file chooser, and
        driving one to say so would make it a test of two things.
        """
        self.files.append(path)
        return path

    def pref(self, key, value):
        """One setting, as it would be in prefs.xml. Written by commit()."""
        self._prefs[key] = value

    def plugin(self, name, enabled=True):
        """Switch a plugin on before medit starts.

        The key belongs to the plugin framework rather than to the plugin --
        moo_plugin_register() creates it and reads the enabled state out of it
        -- so this is the same switch Preferences/Plugins operates. Written for
        every LSP test rather than hidden in a helper, because the client being
        off until it is asked for is a property worth seeing in each scenario.
        """
        self.pref("Plugins/%s/enabled" % name, bool(enabled))

    # -- language servers --------------------------------------------------

    def lsp_server(self, id="test", filter="globs:*.txt", root=None, env=None,
                   enabled=True, init_options=None, **scenario):
        """One entry in lsp.xml, played by lib/fake_lsp.py.

        Everything the server will do is in the scenario, which is written
        beside it and re-read at every start of the process -- so a test can
        change what the server answers and make medit start it again.
        """
        self._servers = [s for s in self._servers if s["id"] != id]
        self._servers.append({
            "id": id,
            "filter": filter,
            "root": root,
            "env": env or [],
            "enabled": enabled,
            "init_options": init_options,
        })

        self.lsp_scenario(id, **scenario)
        self._write_lsp_config()

        return self.lsp_scenario_path(id)

    def lsp_scenario(self, id="test", **scenario):
        """What that server answers, replacing whatever it was told before.

        Also callable from the test itself, through t.sandbox: the process
        reads this file when it starts, so rewriting it and making medit
        restart the server is how a test changes the server's mind.
        """
        scenario.setdefault("log", self.lsp_log_path(id))
        scenario.setdefault("starts", self.lsp_starts_path(id))

        path = self.lsp_scenario_path(id)
        os.makedirs(os.path.dirname(path), exist_ok=True)

        with open(path, "w") as f:
            json.dump(scenario, f, indent=2)

        return path

    def lsp_config(self, xml):
        """The whole of lsp.xml, verbatim, for a test about the file itself."""
        self._lsp_config = xml
        self._write_lsp_config()

    def lsp_scenario_path(self, id="test"):
        return self.path("lsp", "%s.json" % id)

    def lsp_log_path(self, id="test"):
        return self.path("lsp", "%s.jsonl" % id)

    def lsp_starts_path(self, id="test"):
        return self.path("lsp", "%s.starts" % id)

    def lsp_command(self, id="test"):
        """The command line lsp.xml carries for that server.

        The interpreter that runs the harness runs the server too: it is the
        one python3 the machine is known to have, since the tests themselves
        need it.
        """
        return " ".join(shlex.quote(part) for part in (
            sys.executable,
            os.path.join(HERE, "fake_lsp.py"),
            self.lsp_scenario_path(id)))

    def lsp_file(self):
        return os.path.join(self.data_home, LSP_FILE)

    def _write_lsp_config(self):
        if self._lsp_config is None and not self._servers:
            return None

        path = self.lsp_file()
        os.makedirs(os.path.dirname(path), exist_ok=True)

        with open(path, "w") as f:
            f.write(self._lsp_config if self._lsp_config is not None
                    else LSP_XML % "\n".join(self._server_xml(s) for s in self._servers))

        return path

    def _server_xml(self, server):
        lines = ['  <server id="%s" enabled="%s">'
                 % (escape(server["id"]), "true" if server["enabled"] else "false"),
                 "    <filter>%s</filter>" % escape(server["filter"]),
                 "    <command>%s</command>" % escape(self.lsp_command(server["id"]))]

        if server["root"]:
            lines.append("    <root>%s</root>" % escape(server["root"]))

        for entry in server["env"]:
            lines.append("    <env>%s</env>" % escape(entry))

        if server["init_options"]:
            lines.append("    <initialization-options>%s</initialization-options>"
                         % escape(json.dumps(server["init_options"])))

        lines.append("  </server>")

        return "\n".join(lines)

    # -- writing it out ----------------------------------------------------

    def commit(self):
        written = [path for path in (self._write_lsp_config(), self._write_prefs())
                   if path]

        return ", ".join(written) if written else None

    def _write_prefs(self):
        if not self._prefs:
            return None

        path = os.path.join(self.data_home, PREFS_FILE)
        os.makedirs(os.path.dirname(path), exist_ok=True)

        items = "\n".join(PREFS_ITEM % ((escape(key),) + _typed(value))
                          for key, value in sorted(self._prefs.items()))

        with open(path, "w") as f:
            f.write(PREFS_XML % items)

        return path
