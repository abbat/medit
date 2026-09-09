"""Wrap Text and Show Line Numbers change the view, and belong to the document.

# requires: MOO_GTK3

wrap_text_toggled() and line_numbers_toggled() in mooeditwindow.cpp. Both hand
the change to the active view rather than to the window, so each document keeps
its own answer and the two menu items follow whichever document is in front. That
is the part that breaks quietly: a setting that leaked to the window would look
right until a second document was opened.

Both are read off the screen rather than from the menu's own tick, because the
tick would be right in a build where nothing happened. Line numbers are a margin
that appears to the left of the text, measured the way margin_select_lines
measures it; wrapping is the first line becoming several rows tall.
"""

WRAP = "Wrap Text"
NUMBERS = "Show Line Numbers"

LONG = "a long line " * 30

FIRST = "wrapped.txt"
SECOND = "plain.txt"


def setup(s):
    # The defaults this test starts from, said out loud: neither is on.
    s.pref("Editor/wrap_enable", False)
    s.pref("Editor/show_line_numbers", False)

    s.open(s.write("workdir/" + SECOND, "short\n"))
    s.open(s.write("workdir/" + FIRST, LONG + "\n"))


def run(t):
    view = t.document()

    t.check(t.text(view) == LONG + "\n", "the document with the long line is showing")

    # -- wrapping ----------------------------------------------------------
    flat = line_height(t, view)
    t.log("unwrapped, the line is %d high" % flat)

    t.menu("View", WRAP)

    t.wait(lambda: line_height(t, view) > flat,
           "the long line to be drawn over more than one row; it is %d high against "
           "%d" % (line_height(t, view), flat))
    t.log("ok: Wrap Text wraps the line")

    t.menu("View", WRAP)

    t.wait(lambda: line_height(t, view) == flat,
           "the line to go back to one row; it is %d high" % line_height(t, view))
    t.log("ok: and switching it off unwraps it again")

    # -- line numbers ------------------------------------------------------
    t.check(margin(t, view) < 10,
            "there is no room for numbers to the left of the text: %d pixels"
            % margin(t, view))

    t.menu("View", NUMBERS)

    t.wait(lambda: margin(t, view) >= 10,
           "a margin wide enough for the numbers to appear; it is %d pixels"
           % margin(t, view))
    t.log("ok: Show Line Numbers gives the view a margin")

    # -- and it belongs to the document ------------------------------------
    t.menu("Window", SECOND)

    other = t.wait(lambda: t.document() if t.text(t.document()) == "short\n" else None,
                   "the other document to come forward")

    t.check(margin(t, other) < 10,
            "the other document has no numbers: the setting went to the document "
            "and not to the window (%d pixels)" % margin(t, other))

    t.menu("Window", FIRST)

    t.wait(lambda: margin(t, t.document()) >= 10,
           "the numbers to still be there on the document they were switched on for")
    t.log("ok: each document keeps its own answer")


def line_height(t, view):
    """How tall the first line is drawn, in pixels."""
    return t.range_extents(view, 0, len(LONG))[3]


def margin(t, view):
    """How much room there is to the left of the first character."""
    left = t.extents(view)[0]
    text_left = t.range_extents(view, 0, 1)[0]

    return text_left - left
