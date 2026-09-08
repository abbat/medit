"""The client's two hand-matched keys, moved and taken away.

# requires: MOO_BUILD_LSP

Ctrl+Space and Ctrl+Shift+Space cannot be ordinary accelerators: MooWindow
hands a key to the focused widget before it tries them, so the text view
swallows both and the actions never fire. The plugin matches them itself, and
a matcher is exactly the thing that goes on obeying the key it was written with
after somebody has changed it -- which is what this is here to catch.

Both are set in the file Configure Shortcuts writes, before medit starts: one
moved to another key, one cleared. So the assertions are "the new key works",
"the old one does not" and "a shortcut taken away stays taken away" -- the last
being the one that was wrong, the client falling back on the default it was
compiled with.

Both toolkits: the popup this opens is a window of the plugin's own, and the
request behind it is in the server's log either way.
"""

CONTENT = "alpha\n"

# What Configure Shortcuts writes: the window, the group the action is in,
# and the action.
MOVED = "Shortcuts/Editor/Lsp/LspComplete"
CLEARED = "Shortcuts/Editor/Lsp/LspSignature"

ITEMS = [{"label": "alphabet", "insertText": "alphabet"}]


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))

    s.pref(MOVED, "<Ctrl>j")
    s.pref(CLEARED, "")

    s.lsp_server(filter="globs:*.txt",
                 replies={"textDocument/completion": {"isIncomplete": False,
                                                      "items": ITEMS},
                          "textDocument/signatureHelp": {"signatures": []}})


def run(t):
    t.wait_lsp("textDocument/didOpen")

    t.focus()
    t.key("ctrl+Home")
    t.key("End")

    t.key("ctrl+space")
    t.key("ctrl+shift+space")
    t.settle(1)

    t.check(t.lsp("textDocument/completion") == [],
            "the shipped key asks nothing, the action having been moved off it")
    t.check(t.lsp("textDocument/signatureHelp") == [],
            "and the cleared one asks nothing, which is what clearing it means")

    t.key("ctrl+j")

    t.wait_lsp("textDocument/completion")
    t.log("ok: completion answers to the key the preferences name")

    t.key("Escape")
