"""The context menu entry goes by the click, and the keyboard one by the cursor.

# requires: MOO_BUILD_LSP, MOO_GTK3

GtkTextView leaves the cursor where it was on a right click, so an entry that
asked about the cursor would answer about wherever the cursor was last left --
and would look as though it needed the word selected first, selecting being
what moves the cursor into it. The plugin records where the press landed and
goes by that, and forgets it again on any key, so that a menu opened from the
keyboard is about the cursor after all.

Neither half can be seen on screen: both look like a menu with the same item in
it. What tells them apart is the position in the request, which is why this is
asserted from the server's log.

GTK+3 only: the click has to land on a particular word of the document, and the
document has no accessible to ask about that on GTK+2.
"""

CONTENT = "alpha beta\ngamma delta\n"

# One character of the second line -- the "l" of "delta" -- as an offset into
# the text of the whole document, and the same character counted from the start
# of its own line, which is what the request has to name. A single character and
# not a word: a word is wide enough to hide a click landing a few characters to
# the left of where it was aimed, and that is the whole of what this asserts.
#
# Clicked a quarter of the way into it rather than in the middle, because a
# text view resolves a click to the nearest place a caret could go and the
# middle of a glyph is the boundary between it and the next.
DELTA_L = 19
DELTA_L_IN_LINE = 8
INSIDE = 0.25


def setup(s):
    s.plugin("Lsp")

    # With the line numbers on, because a gutter is where this goes wrong: the
    # click arrives in the coordinates of the window it landed in, and reading
    # it as the widget's own moves it left by however wide the gutter is. Off,
    # the two differ by the text view's left margin alone, which is less than a
    # character and hides the fault.
    s.pref("Editor/show_line_numbers", True)
    s.open(s.write("workdir/hello.txt", CONTENT))
    s.open(s.write("workdir/notes.md", "no server for this one\n"))
    s.lsp_server(filter="globs:*.txt",
                 replies={"textDocument/definition": {
                     "uri": "file://" + s.path("workdir/hello.txt"),
                     "range": {"start": {"line": 0, "character": 0},
                               "end": {"line": 0, "character": 5}}}})


def run(t):
    t.wait_lsp("textDocument/didOpen")

    # The document with no server first: the entry is only offered where some
    # server handles the document, and hiding it is a thing update_doc_actions()
    # has to keep doing as the active document changes.
    menu = t.popup_at(document(t), 0, 1)

    t.check("Go to Definition" not in entries(t, menu),
            "no Go to Definition on a document no server handles: %s"
            % ", ".join(entries(t, menu)))
    t.escape()

    t.menu("Window", "hello.txt")

    view = document(t)
    t.focus()
    t.key("ctrl+Home")

    menu = t.popup_at(view, DELTA_L, DELTA_L + 1, at=INSIDE)
    t.click(t.item(menu, "Go to Definition"))

    asked = t.wait_lsp("textDocument/definition")["params"]["position"]

    t.check(asked["line"] == 1,
            "the server was asked about the line that was clicked, not the line "
            "the cursor was on: %s" % asked)

    t.check(asked["character"] == DELTA_L_IN_LINE,
            "and about the character that was clicked, not one further left: "
            "asked about %d, clicked on %d"
            % (asked["character"], DELTA_L_IN_LINE))

    t.log("ok: the entry goes by where the right click landed")

    t.wait(lambda: cursor(t) == "Line: 1 Col: 1",
           "the cursor to go where the server pointed; it went to %s" % cursor(t))

    # And now from the keyboard, with the cursor where the answer left it. Any
    # key forgets the click, so this must be about the cursor again.
    asked = len(t.lsp("textDocument/definition"))

    menu = t.popup()
    t.click(t.item(menu, "Go to Definition"))

    t.wait(lambda: len(t.lsp("textDocument/definition")) > asked,
           "the second question to reach the server")

    position = t.lsp("textDocument/definition")[-1]["params"]["position"]

    t.check(position == {"line": 0, "character": 0},
            "a menu opened from the keyboard asks about the cursor: %s" % position)


def document(t):
    """The text view of the document that is on screen."""
    views = [view for view in t.on_screen(t.find_all(t.frame, role="text", depth=30))
             if t.state(view, "editable")]

    return views[0]


def entries(t, menu):
    return [item.name for item in t.on_screen(t.find_all(menu, role="menu item", depth=3))]


def cursor(t):
    label = t.find(t.frame, role="label", name_prefix="Line:", depth=25)
    return label.name if label is not None else None
