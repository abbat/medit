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

# One character of it -- the first "m" -- as an offset into the document and as
# the position in its line that the request has to name. A single character,
# because a word starting at the beginning of its line hides a pointer position
# that has drifted to the left, which is exactly what the context menu's did.
# A quarter of the way in, because a text view resolves a point to the nearest
# place a caret could go and the middle of a glyph is a boundary.
GAMMA_M = 13
GAMMA_M_IN_LINE = 2
INSIDE = 0.25

HOVER = {"contents": {"kind": "plaintext", "value": "gamma is the third letter"}}


def setup(s):
    s.plugin("Lsp")

    # With the line numbers on: a gutter is what turns a coordinate read in the
    # wrong window into a position several characters to the left.
    s.pref("Editor/show_line_numbers", True)

    s.open(s.write("workdir/hello.txt", CONTENT))
    s.lsp_server(filter="globs:*.txt", replies={"textDocument/hover": HOVER})


def run(t):
    t.wait_lsp("textDocument/didOpen")

    view = document(t)
    t.hover(view, GAMMA_M, GAMMA_M + 1, at=INSIDE)

    asked = t.wait_lsp("textDocument/hover")["params"]["position"]

    t.check(asked == {"line": 1, "character": GAMMA_M_IN_LINE},
            "the server was asked about the character under the pointer, and "
            "not one further left: %s" % asked)

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
