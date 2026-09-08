"""Clicking and dragging in the line-number margin selects whole lines.

# requires: MOO_GTK3

left_window_click() and select_lines() in mootextview-input.c. GtkTextView has
no notion of it: the left border window is medit's, the click in it is medit's,
and the whole-line selection it makes -- from the start of the first line to the
start of the line after the last, so that the break at the end is in it -- is
medit's too.

Three presses, which are the three ways in: a click takes one line, a drag down
takes every line it passes, and Shift+click extends what is already selected to
the line under the pointer. Each goes through a branch of its own.
"""

CONTENT = "one\ntwo\nthree\nfour\n"

#  "one" 0-2 \n3 | "two" 4-6 \n7 | "three" 8-12 \n13 | "four" 14-17 \n18

LINE_STARTS = [0, 4, 8, 14, 19]


def setup(s):
    # Without this the left window has no numbers in it, and a click there is
    # not a click on a line at all.
    s.pref("Editor/show_line_numbers", True)
    s.open(s.write("workdir/lines.txt", CONTENT))


def run(t):
    view = t.document()

    left, _, _, _ = t.extents(view)
    text_left, _, _, _ = t.range_extents(view, 0, 1)

    # A margin wide enough to hold a number, not just the view's own border:
    # measured at 21 pixels here against 2 with the numbers switched off, so a
    # test that only asked for "wider than nothing" would pass either way.
    t.check(text_left - left >= 10,
            "the numbers take a margin to the left of the text: the view starts at "
            "%d and the text at %d" % (left, text_left))

    margin = (left + text_left) // 2

    # One line, from one click.
    t.click_at(margin, middle_of(t, view, 1))
    t.wait_selection(view, (LINE_STARTS[1], LINE_STARTS[2]),
                     "a click in the margin selects that whole line, break and all")

    # Down the margin: every line it passes over.
    t.drag_to(margin, middle_of(t, view, 1), margin, middle_of(t, view, 2))
    t.wait_selection(view, (LINE_STARTS[1], LINE_STARTS[3]),
                     "a drag down the margin selects the lines it passed")

    # And Shift+click, which extends rather than starting again.
    t.click_at(margin, middle_of(t, view, 3), modifiers=("shift",))
    t.wait_selection(view, (LINE_STARTS[1], LINE_STARTS[4]),
                     "Shift+click in the margin extends the selection to that line")

    t.check(t.text(view) == CONTENT, "and none of it changed the document")


def middle_of(t, view, line):
    """The y in the middle of a line, taken from where its first character is."""
    _, y, _, height = t.range_extents(view, LINE_STARTS[line], LINE_STARTS[line] + 1)
    return y + height // 2
