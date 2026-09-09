"""Find Current Word searches for what the cursor is in, and wraps at the end.

# requires: MOO_GTK3

moo_text_view_run_find_current_word() in mootextfind.c: the word at the cursor
becomes the search term with no dialog in the way, the search starts after that
word rather than at it -- so the first press goes to the next occurrence and not
to the one the cursor is already in -- and when there is nothing further it wraps
round to the start.

Every position below is a decision of that function, and the backwards item is
the same code with the direction reversed, wrap included.
"""

CONTENT = "alpha beta\ngamma alpha\ndelta alpha\n"

#  alpha 0-4, beta 6-9, \n10 | gamma 11-15, alpha 17-21, \n22 | delta 23-27,
#  alpha 29-33, \n34

FIRST = (0, 5)
SECOND = (17, 22)
THIRD = (29, 34)

FORWARD = "Find Current Word"
BACKWARD = "Find Current Word Backwards"


def setup(s):
    s.open(s.write("workdir/words.txt", CONTENT))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)
    t.key("ctrl+Home")
    t.key("Right")
    t.key("Right")
    t.wait_caret(view, 2, "the cursor is inside the first alpha")

    # Forwards: past the word the cursor is in, to the next one.
    find(t, view, FORWARD, SECOND, "the second alpha, not the one the cursor was in")
    find(t, view, FORWARD, THIRD, "the third alpha")
    find(t, view, FORWARD, FIRST, "round to the first, there being nothing after the third")

    # And backwards, which wraps at the other end.
    find(t, view, BACKWARD, THIRD, "backwards from the first wraps to the last")
    find(t, view, BACKWARD, SECOND, "and then to the one before it")


def find(t, view, entry, expected, what):
    t.menu("Search", entry)
    t.wait_selection(view, expected, "%s is selected: %s" % (what, expected))
