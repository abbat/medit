"""Every other use of what the cursor is in, marked in the text itself.

# requires: MOO_BUILD_LSP, MOO_GTK3

Nothing about this is a widget: the server answers with ranges, the client
applies a GtkTextTag to them, and a tag has no name, no position and no
accessible of its own. What it does have is text attributes -- at-spi reports
the tags at a character as the attributes of that character -- and that is what
this reads. A colour off the screen would say the same thing and would also
depend on the theme, on the font and on where the line happens to be drawn.

What cannot be seen from here is which of the two marks a range wears -- a
place a symbol is written to is marked differently from a place it is read
from, and at-spi reports the colour of every tag on GTK+3 as 0,0,0, measured.
So this says a character is marked and lsp-tests.cpp says which way.

GTK+3 only, the document having no accessible on GTK+2.
"""

CONTENT = "alpha beta\ngamma alpha\n"

# Offsets into the whole document: the two "alpha"s, and "beta", which is
# neither of them.
FIRST = 0
BETA = 6
SECOND = 17

# What the server says the two uses are: read here, written there.
HIGHLIGHTS = [
    {"range": {"start": {"line": 0, "character": 0},
               "end": {"line": 0, "character": 5}}, "kind": 2},
    {"range": {"start": {"line": 1, "character": 6},
               "end": {"line": 1, "character": 11}}, "kind": 3},
]

SETTING = "Mark the other uses of what the cursor is in"


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))
    s.lsp_server(filter="globs:*.txt",
                 replies={"textDocument/documentHighlight": HIGHLIGHTS})


def run(t):
    t.wait_lsp("textDocument/didOpen")

    view = document(t)

    # The cursor moving is what asks: a document just opened has one sitting at
    # the top and nothing to say about it yet. End and back, so that the
    # position asked about is the one this test knows.
    t.focus()
    t.key("End")
    t.key("ctrl+Home")

    asked = t.wait_lsp("textDocument/documentHighlight")["params"]

    t.check(asked["position"] == {"line": 0, "character": 0},
            "the server was asked about the position of the cursor: %s"
            % asked["position"])

    t.wait(lambda: background(t, view, FIRST),
           "the word the cursor is in to be marked")

    t.check(background(t, view, SECOND), "and the use in the other line as well")
    t.check(background(t, view, BETA) is None,
            "and nothing else: %r is not marked" % t.text(view)[BETA:BETA + 4])
    t.log("ok: both uses are marked and the word between them is not")

    # And they go away again when the server has nothing to say about where the
    # cursor is now. Marks left behind are worse than none: they would point at
    # the uses of whatever the cursor was in a minute ago.
    t.sandbox.lsp_scenario("test", replies={"textDocument/documentHighlight": []})

    t.key("Down")

    t.wait(lambda: background(t, view, FIRST) is None,
           "the marks to come off when the cursor is somewhere else")
    t.check(background(t, view, SECOND) is None, "all of them")
    t.log("ok: the marks follow the cursor rather than pile up")

    # The setting that turns it off, which also has to take the marks off what
    # is on screen: it is applied where the preferences are, not at the next
    # request that never comes.
    t.sandbox.lsp_scenario("test", replies={"textDocument/documentHighlight": HIGHLIGHTS})

    t.focus()
    t.key("ctrl+Home")
    t.wait(lambda: background(t, view, FIRST), "the marks, once more")

    setting(t, False)

    count = len(t.lsp("textDocument/documentHighlight"))

    t.focus()
    t.key("End")
    t.settle(1)

    t.check(len(t.lsp("textDocument/documentHighlight")) == count,
            "nothing is asked once the setting is off")
    t.check(background(t, view, FIRST) is None,
            "and the marks that were on screen are gone")


def setting(t, on):
    """Tick or untick the setting on the client's page of the preferences."""
    dialog = t.preferences("Language Servers")
    box = t.need(dialog, role="check box", name=SETTING,
                 what="the %r check box" % SETTING)

    if t.state(box, "checked") != on:
        t.click(box)

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")
    t.log("the setting is now %s" % ("on" if on else "off"))


def background(t, view, offset):
    """Whether one character has a background of its own, from a tag.

    A tag is not a widget and a highlighted range is not a node in the tree;
    at-spi reports what the tags at a character say as that character's text
    attributes, and a background is what a mark puts there. The value is not
    the colour -- GTK+3 answers 0,0,0 for every tag there is -- so this is
    "marked" and "not marked", nothing finer.
    """
    return t.attributes(view, offset).get("bg-color")


def document(t):
    """The text view of the document, as opposed to a pane's."""
    views = [view for view in t.on_screen(t.find_all(t.frame, role="text", depth=30))
             if t.state(view, "editable")]

    return views[0]
