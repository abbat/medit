"""Go to Declaration: the fourth of the four questions about a position.

# requires: MOO_BUILD_LSP

definition, typeDefinition and implementation each had an entry; declaration
was announced in client_capabilities() and never asked, which is the one thing
this checks -- that the entry sends textDocument/declaration and not the
definition the other three route through.

The answer is a LocationLink and not a Location, because linkSupport is what
the client announces for this method: the place to go is then targetSelectionRange,
the name itself, rather than targetRange, which is the whole of what is declared.

Both toolkits: the cursor is read off the status bar, which needs no accessible
the GTK+2 build lacks.
"""

CONTENT = "alpha beta\ngamma delta\n"


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))

    s.lsp_server(filter="globs:*.txt",
                 replies={"textDocument/declaration": [{
                     "targetUri": "file://" + s.path("workdir/hello.txt"),
                     # The whole line, and the name inside it: the cursor has to
                     # land on the second of the two.
                     "targetRange": {"start": {"line": 1, "character": 0},
                                     "end": {"line": 1, "character": 11}},
                     "targetSelectionRange": {"start": {"line": 1, "character": 6},
                                              "end": {"line": 1, "character": 11}}}]})


def run(t):
    t.wait_lsp("textDocument/didOpen")

    t.focus()
    t.key("ctrl+Home")

    t.menu("Document", "Go to Declaration")

    asked = t.wait_lsp("textDocument/declaration")["params"]

    t.check(asked["position"] == {"line": 0, "character": 0},
            "the server was asked about the position of the cursor: %s" % asked["position"])
    t.check(asked["textDocument"]["uri"].endswith("hello.txt"),
            "and about the document the cursor is in")
    t.check(not t.lsp("textDocument/definition"),
            "the entry asks for a declaration and not for a definition")

    t.wait(lambda: cursor(t) == "Line: 2 Col: 7",
           "the cursor to go to the name the link points at; it went to %s" % cursor(t))


def cursor(t):
    label = t.find(t.frame, role="label", name_prefix="Line:", depth=25)
    return label.name if label is not None else None
