"""A machine with no configuration gets the real defaults, not an empty stub.

# requires: MOO_BUILD_LSP

This is the one test that puts nothing in place for the client, on purpose.
Every other test here writes an lsp.xml of its own, and that is exactly how the
defaults came to be broken once: they were read from the install only, so a run
from a build directory had none at all, and Tools/LSP Servers... -- the item
whose whole job is to hand the user a copy of them -- wrote an empty file. Every
green run had quietly supplied by hand the thing that was missing.

So: no lsp.xml, and the assertions are about what the untouched machine does.
Both toolkits -- the client is not a GTK+3-only plugin, and nothing here needs
to look inside a pane, so the file is read from disk and the window title says
which document is open.
"""

# Entries of the shipped file. Not a byte comparison against the source tree:
# an installed medit is allowed to ship its own defaults, and the packaging
# question is whether what arrived is the real list rather than which list it
# is. A stub, which is what the bug produced, fails every one of these.
EXPECTED = ("<medit-lsp", "<server id=\"clangd\">", "clangd --background-index",
            "<server id=\"pylsp\">", "<server id=\"example\" enabled=\"false\">")

# No newlines and no indentation in it: the editor indents a new line
# by itself, so a marker with either would not come back verbatim.
MINE = "<!-- mine -->"


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", "hello\n"))


def run(t):
    config = t.sandbox.lsp_file()

    t.check(t.sandbox.read_path(config) == "",
            "a machine nobody has configured has no lsp.xml of its own")

    t.menu("Tools", "LSP Servers...")
    t.wait(lambda: config in (t.frame.name or ""),
           "the configuration file to be opened as a document")

    written = t.sandbox.read_path(config)

    t.check(len(written) > 1000,
            "the file it created has something in it: %d bytes" % len(written))

    for expected in EXPECTED:
        t.check(expected in written, "and it carries %s" % expected)

    # What the user is meant to do with it -- edit it -- and then ask for it
    # again. The item creates the file only when there is none; a second
    # invocation must hand back what the user wrote, not the defaults again.
    # At the end, and typed rather than clicked into: the document behind the
    # keys is the one that was just opened, and on GTK+2 there is no accessible
    # for the view to click at. The focus first, since the menu that opened it
    # took it into a window that no longer exists.
    t.focus()
    t.key("ctrl+End")
    t.type_text(MINE)
    t.key("ctrl+s")
    t.wait(lambda: MINE in t.sandbox.read_path(config),
           "the edit to reach the file on disk")

    t.menu("Tools", "LSP Servers...")
    t.settle(1)

    t.check(MINE in t.sandbox.read_path(config),
            "asking for the file again leaves the user's own copy alone")
