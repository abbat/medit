"""Restarting the servers reads the configuration file again.

# requires: MOO_BUILD_LSP

lsp_manager_init() reads lsp.xml once, when the plugin is switched on, so a
user who has just edited the file -- which is what Tools/LSP Servers... is for
-- needs a way to make medit look again that is not a restart of the editor.
Two things offer it, the menu item and the button on the preferences page, and
both go through lsp_manager_reload().

The assertion is that the *new* file is what took effect: the entry is replaced
by one naming a different server, and it is that server which ends up with the
document. Both toolkits.
"""

CONFIG = """<?xml version="1.0" encoding="UTF-8"?>
<medit-lsp version="1.0">
  <server id="%s">
    <filter>globs:*.txt</filter>
    <command>%s</command>
  </server>
</medit-lsp>
"""


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", "alpha\n"))
    s.lsp_server(filter="globs:*.txt")


def run(t):
    t.wait_lsp("textDocument/didOpen")

    name_the_server(t, "second")
    t.menu("Tools", "Restart Language Servers")

    t.wait_lsp("textDocument/didOpen", server="second")
    t.check(t.lsp_starts("second") == 1,
            "the menu item read the file again and started what it now names")
    t.wait_lsp("exit", server="test")
    t.log("ok: and the server the file used to name was asked to exit")

    # The same again, from the button on the client's page of the preferences.
    name_the_server(t, "third")

    dialog = t.preferences("Language Servers")
    t.click(t.button(dialog, "Restart Servers"))

    t.wait_lsp("textDocument/didOpen", server="third")
    t.log("ok: the button on the preferences page does the same")

    t.click(t.button(dialog, "Cancel"))
    t.no_toplevel("Preferences")


def name_the_server(t, id):
    """Rewrite lsp.xml so that it names a different server, as a user would."""
    t.sandbox.lsp_scenario(id)
    t.sandbox.lsp_config(CONFIG % (id, t.sandbox.lsp_command(id)))
    t.log("the configuration now names %s" % id)
