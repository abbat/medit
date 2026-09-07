"""What the server is told as the document changes: one didChange, then a save.

# requires: MOO_BUILD_LSP

The delay is the point. Every keystroke could be a notification, and for a
server that reparses on each one that is the difference between an editor and a
fan heater, so the client waits for the typing to stop -- Plugins/Lsp/sync_delay
milliseconds of quiet -- and sends the state of the buffer once.

A whole second of it here, so that xdotool's typing is comfortably inside one
window of quiet: what is being tested is that the burst becomes one message,
and a delay of the same order as the typing would be testing the timer.

Both toolkits: all of it is read from the server's log.
"""

CONTENT = "alpha\n"
TYPED = "beta"


def setup(s):
    s.plugin("Lsp")
    s.pref("Plugins/Lsp/sync_delay", 1000)
    s.open(s.write("workdir/hello.txt", CONTENT))
    s.lsp_server(filter="globs:*.txt")


def run(t):
    opened = t.wait_lsp("textDocument/didOpen")["params"]["textDocument"]

    t.check(opened["version"] == 1, "the document was announced as version 1")
    t.check(t.lsp("textDocument/didChange") == [],
            "and nothing has changed in it yet")

    t.focus()
    t.key("ctrl+End")
    t.type_text(TYPED)

    changed = t.wait_lsp("textDocument/didChange", timeout=30)["params"]

    t.check(len(t.lsp("textDocument/didChange")) == 1,
            "the whole burst of typing became one didChange")
    t.check(changed["textDocument"]["version"] == 2,
            "which is version 2 of the document")

    # The server claimed full synchronisation, so the change is the buffer.
    text = changed["contentChanges"][0]["text"]

    t.check(text == CONTENT + TYPED,
            "and it carries the text as the buffer now holds it: %r" % text)

    t.key("ctrl+s")

    saved = t.wait_lsp("textDocument/didSave")["params"]

    t.check(saved["textDocument"]["uri"].endswith("hello.txt"),
            "saving the document tells the server which one was saved")

    # And closing it takes it back off the server, which is what stops a
    # server answering about a document nobody is looking at any more.
    t.menu("File", "Close")

    closed = t.wait_lsp("textDocument/didClose")["params"]

    t.check(closed["textDocument"]["uri"].endswith("hello.txt"),
            "closing it says so too")
