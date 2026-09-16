"""Expand and Shrink Selection: the chain of ranges a server answers with.

# requires: MOO_BUILD_LSP

The two things worth checking are invisible from a screenshot. One is what the
selection becomes: growing takes the smallest range around it and shrinking the
largest range inside it, and both have to skip the range that is the selection
itself or the pair of them would do nothing. The other is the position the
question is asked about -- where the selection starts, not where the cursor is,
which after a growth sits one past the last selected character and so belongs
to whatever comes next.

GTK+3 only, like every test that reads a selection off a document.
"""

CONTENT = "alpha beta\ngamma delta\n"

# "alpha" inside "alpha beta" inside the whole of the text: offsets 0..5, 0..10
# and 0..22, which is what the client has to arrive at on its own.
CHAIN = [{"range": {"start": {"line": 0, "character": 0},
                    "end": {"line": 0, "character": 5}},
          "parent": {"range": {"start": {"line": 0, "character": 0},
                               "end": {"line": 0, "character": 10}},
                     "parent": {"range": {"start": {"line": 0, "character": 0},
                                          "end": {"line": 1, "character": 11}}}}}]


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))

    s.lsp_server(filter="globs:*.txt",
                 replies={"textDocument/selectionRange": CHAIN})


def run(t):
    t.wait_lsp("textDocument/didOpen")

    t.focus()
    t.key("ctrl+Home")

    view = t.document()

    t.menu("Document", "Expand Selection")

    asked = t.wait_lsp("textDocument/selectionRange")["params"]

    t.check(asked["positions"] == [{"line": 0, "character": 0}],
            "the server was asked about one position: %s" % asked["positions"])
    t.check(asked["textDocument"]["uri"].endswith("hello.txt"),
            "and about the document the cursor is in")

    t.wait_selection(view, (0, 5), "the innermost range around the cursor to be selected")

    t.menu("Document", "Expand Selection")

    asked = t.wait_lsp("textDocument/selectionRange")["params"]
    t.check(asked["positions"] == [{"line": 0, "character": 0}],
            "the second question is about where the selection starts, not where"
            " the cursor is: %s" % asked["positions"])

    t.wait_selection(view, (0, 10), "the range around that one to be selected next")

    t.menu("Document", "Shrink Selection")
    t.wait_lsp("textDocument/selectionRange")

    t.wait_selection(view, (0, 5), "shrinking to go back to the range inside it")
    t.log("ok: the selection grew by syntax and shrank back")
