"""Go to Definition: inside the document, and into a file that is not open yet.

# requires: MOO_BUILD_LSP

Two things the client has to get right, and both are invisible from a
screenshot: the position it asks about is where the cursor is, and the place it
goes to is where the answer points -- in a file it has to open first, which is
the case with the trap in it. moo_editor_open_file() queues a move to the top
of the file at G_PRIORITY_HIGH_IDLE + 9 and do_move_cursor() cancels whatever
other move is pending when it runs, so asking for the column any earlier gets
it thrown away.

Both toolkits: the cursor is read off the status bar and the open document off
the window title, neither of which needs an accessible the GTK+2 build lacks.
"""

CONTENT = "alpha beta\ngamma delta\n"
OTHER = "one\ntwo\nthree\n"


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))

    # Written but not opened: going to a definition has to open it.
    s.write("workdir/other.txt", OTHER)

    s.lsp_server(filter="globs:*.txt",
                 replies={"textDocument/definition": here(s, "workdir/hello.txt", 1, 6)})


def here(s, name, line, character):
    """A Location, the way a server answers with one."""
    return {"uri": "file://" + s.path(name),
            "range": {"start": {"line": line, "character": character},
                      "end": {"line": line, "character": character + 1}}}


def run(t):
    t.wait_lsp("textDocument/didOpen")

    t.focus()
    t.key("ctrl+Home")

    t.menu("Document", "Go to Definition")

    asked = t.wait_lsp("textDocument/definition")["params"]

    t.check(asked["position"] == {"line": 0, "character": 0},
            "the server was asked about the position of the cursor: %s" % asked["position"])
    t.check(asked["textDocument"]["uri"].endswith("hello.txt"),
            "and about the document the cursor is in")

    t.wait(lambda: cursor(t) == "Line: 2 Col: 7",
           "the cursor to go where the server pointed; it went to %s" % cursor(t))
    t.log("ok: the cursor went to the definition inside the same document")

    # Now the same question with an answer in another file. The server reads
    # its scenario again before every message, so this needs no restart.
    t.sandbox.lsp_scenario(
        "test", replies={"textDocument/definition": here(t.sandbox, "workdir/other.txt",
                                                         2, 4)})

    t.menu("Document", "Go to Definition")

    t.wait(lambda: "other.txt" in (t.frame.name or ""),
           "the file the definition is in to be opened")
    t.wait(lambda: cursor(t) == "Line: 3 Col: 5",
           "and the cursor to be put on it; it is at %s" % cursor(t))
    t.log("ok: a definition in another file opens it at the right column")


def cursor(t):
    label = t.find(t.frame, role="label", name_prefix="Line:", depth=25)
    return label.name if label is not None else None
