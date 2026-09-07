"""Resting the pointer on a word asks the server what it is.

# requires: MOO_BUILD_LSP, MOO_GTK3

What is asserted here is the question, not the tooltip. GTK+3's tooltip window
is not on the accessibility bus -- it is not a toplevel of the application, and
nothing with the "tool tip" role appears in the tree while one is up -- so the
text medit puts in it cannot be read the way everything else here is read.
Measured rather than assumed: the pointer over a word produces the requests and
no window at all.

That leaves the two things that can be seen, and both are the plugin's own
work: the position the pointer is over is turned into a position in the
document, and the preference is checked before anything is asked at all.

GTK+3 only, since the pointer has to be put on a particular word and only the
GTK+3 build has an accessible for the document to ask where it is drawn.
"""

CONTENT = "alpha beta\ngamma delta\n"

# "gamma" on the second line, as offsets into the document's text.
GAMMA = (11, 16)
DELTA = (17, 22)

HOVER = {"contents": {"kind": "plaintext", "value": "gamma is the third letter"}}


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))
    s.lsp_server(filter="globs:*.txt", replies={"textDocument/hover": HOVER})


def run(t):
    t.wait_lsp("textDocument/didOpen")

    view = document(t)
    t.hover(view, *GAMMA)

    asked = t.wait_lsp("textDocument/hover")["params"]["position"]

    t.check(asked["line"] == 1 and 0 <= asked["character"] <= 5,
            "the server was asked about the word under the pointer: %s" % asked)

    # Switched off, the pointer is not a question any more.
    switch_off(t)

    view = document(t)
    before = len(t.lsp("textDocument/hover"))

    t.hover(view, *DELTA)
    t.settle(2)

    t.check(len(t.lsp("textDocument/hover")) == before,
            "with the setting off, resting the pointer asks nothing")


def switch_off(t):
    dialog = t.preferences("Language Servers")

    box = t.need(dialog, role="check box", name="Describe what is under the pointer",
                 what="the hover check box")

    t.check(t.state(box, "checked"), "hover is on until it is turned off")

    t.click(box)
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")


def document(t):
    views = [view for view in t.on_screen(t.find_all(t.frame, role="text", depth=30))
             if t.state(view, "editable")]

    return views[0]
