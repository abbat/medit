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

import os
import stat

from xml.sax.saxutils import escape


# Written by moo_prefs_save() under XDG_DATA_HOME at exit, and read at startup.
PREFS_FILE = os.path.join("medit", "prefs.xml")

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
    Moo-CRITICAL per run, in every test that writes one, drowning the
    criticals a test is there to notice. The words are the ones medit writes
    itself.
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

    def path(self, *parts):
        """A path inside the sandbox: what a helper script writes to."""
        return os.path.join(self.root, *parts)

    def read(self, *parts):
        """The contents of such a file, or "" while it does not exist yet."""
        try:
            with open(self.path(*parts), errors="replace") as f:
                return f.read()
        except FileNotFoundError:
            return ""

    def write(self, name, body):
        path = self.path(name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(body)
        return path

    def script(self, name, body):
        """An executable file in the sandbox, by path."""
        path = self.write(name, body)
        os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
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

    def commit(self):
        if not self._prefs:
            return None

        path = os.path.join(self.data_home, PREFS_FILE)
        os.makedirs(os.path.dirname(path), exist_ok=True)

        items = "\n".join(PREFS_ITEM % ((escape(key),) + _typed(value))
                          for key, value in sorted(self._prefs.items()))

        with open(path, "w") as f:
            f.write(PREFS_XML % items)

        return path
