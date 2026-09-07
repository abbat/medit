"""The symbol tree: filled when it is looked at, and it goes where a row says.

# requires: MOO_BUILD_LSP, MOO_GTK3

GTK+3 only, being a pane -- see diagnostics_pane for why.

The first assertion is about what is *not* sent. A documentSymbol request is
real work for a server, and the answer is used for nothing but this tree, so
nothing is asked while the pane is closed. Silence is the whole of
queue_symbols_update(), and there is no other way to see it.
"""

CONTENT = "alpha beta\ngamma delta\n"

SYMBOLS = [
    {"name": "alpha", "kind": 12,
     "range": {"start": {"line": 0, "character": 0},
               "end": {"line": 0, "character": 10}},
     "selectionRange": {"start": {"line": 0, "character": 0},
                        "end": {"line": 0, "character": 5}},
     "children": [
         # With a detail, which the row shows in place of the kind.
         {"name": "beta", "kind": 13, "detail": "int",
          "range": {"start": {"line": 0, "character": 6},
                    "end": {"line": 0, "character": 10}},
          "selectionRange": {"start": {"line": 0, "character": 6},
                             "end": {"line": 0, "character": 10}}},
     ]},
    {"name": "gamma", "kind": 5,
     "range": {"start": {"line": 1, "character": 0},
               "end": {"line": 1, "character": 11}},
     "selectionRange": {"start": {"line": 1, "character": 0},
                        "end": {"line": 1, "character": 5}}},
]

# The tree is expanded as it is filled, so the child is a row of its own.
EXPECTED = ["alpha function", "beta int", "gamma class"]


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))
    s.lsp_server(filter="globs:*.txt",
                 replies={"textDocument/documentSymbol": SYMBOLS})


def run(t):
    t.wait_lsp("textDocument/didOpen")
    t.settle(2)

    t.check(t.lsp("textDocument/documentSymbol") == [],
            "nothing is asked of the server while the pane is closed")

    t.menu("Tools", "Symbols")
    t.wait_lsp("textDocument/documentSymbol")

    tree = t.wait(lambda: symbol_tree(t), "the symbol tree")
    t.wait(lambda: rows(t, tree) == EXPECTED,
           "the symbols to arrive; the tree holds %s" % rows(t, tree))
    t.log("ok: the tree lists %s" % ", ".join(rows(t, tree)))

    # Selected with the pointer, activated with the keyboard: row-activated
    # wants a double click otherwise, and Enter on the selected row is the same
    # signal without the timing.
    beta = [row for row in t.find_all(tree, role="table cell", depth=3)
            if row.name == "beta int"][0]

    t.click(beta)
    t.key("Return")

    t.wait(lambda: cursor(t) == "Line: 1 Col: 7",
           "the cursor to go where the symbol is; it went to %s" % cursor(t))
    t.log("ok: activating a row goes to the symbol")

    # And the tree keeps up with the document: the buffer changing queues
    # another request, half a second after the typing stops. Pinned, or the
    # pane would close as soon as the document took the focus for the typing --
    # and then nothing would be asked at all, which is the assertion above.
    t.menu("Tools", "Symbols")
    t.pin_pane()

    asked = len(t.lsp("textDocument/documentSymbol"))

    t.click(document(t))
    t.type_text("x")

    t.wait(lambda: len(t.lsp("textDocument/documentSymbol")) > asked,
           "the tree to ask again after the document changed")
    t.log("ok: editing the document asks the server for the symbols again")

    # Saved, or the quit at the end of the test turns into a dialog about it.
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")


def document(t):
    """The text view of the document, as opposed to a pane's."""
    views = [view for view in t.on_screen(t.find_all(t.frame, role="text", depth=30))
             if t.state(view, "editable")]

    return views[0]


def symbol_tree(t):
    """The tree view of the pane, which is the only one in the window."""
    trees = t.on_screen(t.find_all(t.frame, role="tree table", depth=30))

    return trees[0] if len(trees) == 1 else None


def rows(t, tree):
    return [cell.name for cell in t.find_all(tree, role="table cell", depth=3)
            if cell.name]


def cursor(t):
    label = t.find(t.frame, role="label", name_prefix="Line:", depth=25)
    return label.name if label is not None else None
