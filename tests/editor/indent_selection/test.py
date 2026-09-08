"""Tab and Shift+Tab move whole lines, and leave the selection where it was.

# requires: MOO_GTK3

tab_indent() and tab_unindent() in mootextview-input.c, which is medit's own
code and not GtkTextView's: with a selection Tab shifts every line it touches
rather than replacing it, and with no selection it goes to the next tab stop
rather than inserting a fixed number of spaces.

Two things in it are easy to get wrong and are asserted here. The line the
selection ends on is not shifted when the selection stops at its very start --
otherwise selecting two lines by dragging down would indent three. And a
selection that began at column 0 has to still begin at column 0 afterwards,
which is what the starts_line branch is for: without it the selection would
start after the new indent and pressing Tab twice would indent less the second
time.

Spaces and a width of 4, so that what the buffer holds is what the test says
rather than a tab character whose width is a preference.
"""

CONTENT = "alpha\nbeta\ngamma\n"

# alpha 0-4, \n 5; beta 6-9, \n 10; gamma 11-15, \n 16
THIRD_LINE = 11

INDENTED = "    alpha\n    beta\ngamma\n"
THIRD_LINE_INDENTED = 19


def setup(s):
    s.pref("Editor/spaces_instead_of_tabs", True)
    s.pref("Editor/indent_width", 4)
    s.open(s.write("workdir/indent.txt", CONTENT))


def run(t):
    view = t.document()

    t.click(view)
    t.key("ctrl+Home")
    t.key("shift+Down")
    t.key("shift+Down")

    t.wait_selection(view, (0, THIRD_LINE),
                     "the first two lines are selected, up to the start of the third")

    t.key("Tab")

    t.wait(lambda: t.text(view) == INDENTED,
           "the two selected lines to be indented and the third left alone;\n"
           "the document holds %r" % t.text(view))
    t.log("ok: Tab shifts the selected lines and not the one the selection ends at")

    t.wait_selection(view, (0, THIRD_LINE_INDENTED),
                     "the selection still starts at column 0 and still ends at the "
                     "start of the third line")

    t.key("shift+Tab")

    t.wait(lambda: t.text(view) == CONTENT,
           "Shift+Tab to put the two lines back;\nthe document holds %r" % t.text(view))
    t.log("ok: Shift+Tab shifts them back")

    t.wait_selection(view, (0, THIRD_LINE), "and the selection is what it was")

    # No selection: to the next tab stop, which from column 2 is two spaces and
    # not four. A tab that always inserted indent_width would pass everything
    # above and fail here.
    t.key("ctrl+Home")
    t.key("Right")
    t.key("Right")
    t.key("Tab")

    t.wait(lambda: t.text(view) == "al  pha\nbeta\ngamma\n",
           "Tab in the middle of a word to reach column 4;\n"
           "the document holds %r" % t.text(view))
    t.log("ok: with no selection Tab goes to the next tab stop")

    t.wait_caret(view, 4, "the cursor is at the tab stop it made")

    # Saved, or the quit at the end of the test turns into a dialog about it.
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")
