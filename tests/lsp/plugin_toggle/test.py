"""The client is off until it is asked for, and stops again when it is unasked.

# requires: MOO_BUILD_LSP

The plugin registers itself disabled -- it runs other people's programs, and
doing that on a first run because a language server happens to be installed is
not medit's decision -- so Preferences/Plugins is the switch, and the framework
attaches and detaches the window and document halves on the change, documents
that are already open included.

The switch is a toggle in the plugin list. Its cell exposes no checked state
over AT-SPI, so what it did is read from what happened afterwards: a server
process, the items in the Tools menu, and the shutdown reaching the server.
"""

CONTENT = "alpha beta\n"


def setup(s):
    # Deliberately not s.plugin("Lsp"): switching it on is what is being tested.
    s.open(s.write("workdir/hello.txt", CONTENT))
    s.lsp_server(filter="globs:*.txt")


def run(t):
    t.check(tools(t) == [], "the Tools menu offers nothing of the client's while it is off")
    t.check(t.lsp_starts() == 0, "and no server has been started")

    switch(t, "on")

    t.wait(lambda: t.lsp_starts() == 1, "a server to be started once the plugin is on")
    t.log("ok: switching the plugin on started a server")

    opened = t.wait_lsp("textDocument/didOpen")["params"]["textDocument"]
    t.check(opened["uri"].endswith("hello.txt"),
            "the document that was already open was announced to it")

    t.check(tools(t) == ["Diagnostics", "Symbols", "LSP Servers...",
                         "Restart Language Servers"],
            "and the Tools menu carries the client's items: %s" % ", ".join(tools(t)))

    switch(t, "off")

    t.wait_lsp("exit")
    t.check(len(t.lsp("shutdown")) == 1, "switching it off asked the server to shut down")
    t.check(tools(t) == [], "and took the items back out of the Tools menu")


def tools(t):
    """The client's items in the Tools menu, in the order they are offered."""
    ours = ("Diagnostics", "Symbols", "LSP Servers...", "Restart Language Servers")
    menu = t.need(t.frame, role="menu", name="Tools", what="the Tools menu")

    return [item.name for item in t.find_all(menu, role="menu item", depth=2)
            if item.name in ours]


def switch(t, how):
    """Turn the plugin on or off in Preferences/Plugins, and apply it.

    The toggle is the cell before the plugin's name in the same row -- the
    columns are Enabled and Plugin -- and the change reaches the plugin when
    the page is applied, not when the cell is clicked.
    """
    dialog = t.preferences("Plugins")

    tables = [table for table in t.find_all(dialog, role="table", depth=30)
              if any(c.name == "LSP" for c in t.find_all(table, role="table cell", depth=3))]

    t.check(len(tables) == 1, "the plugin list is on the Plugins page")

    cells = t.find_all(tables[0], role="table cell", depth=3)
    names = [cell.name for cell in cells]

    t.click(cells[names.index("LSP") - 1])
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")
    t.log("switched the plugin %s" % how)
