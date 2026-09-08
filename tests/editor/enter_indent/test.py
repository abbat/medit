"""Enter carries the indent down, and Shift+Enter does not.

# requires: MOO_GTK3

handle_enter() in mootextview-input.c and moo_indenter_character() behind it.
The new line is given the indent of the line it came from, which is the whole
reason typing in an indented block does not walk back to column 0 -- and the
Shift branch is the way out of it, for the line that is meant to start at the
margin.

The middle case matters as much as the end one: splitting a line moves the tail
down, and the tail is indented too.
"""

CONTENT = "    alpha\nplain\n"

#   "    alpha" 0-8, \n 9, "plain" 10-14, \n 15
AFTER_ENTER = "    alpha\n    \nplain\n"
AFTER_SHIFT_ENTER = "    alpha\n    \n\nplain\n"
AFTER_SPLIT = "    alp\n    ha\n    \n\nplain\n"


def setup(s):
    s.pref("Editor/spaces_instead_of_tabs", True)
    s.pref("Editor/indent_width", 4)
    s.open(s.write("workdir/enter.txt", CONTENT))


def run(t):
    view = t.document()

    t.click(view)
    t.key("ctrl+Home")
    t.key("End")
    t.wait_caret(view, 9, "the cursor is at the end of the indented line")

    t.key("Return")
    holds(t, view, AFTER_ENTER, "Enter to open a line indented like the one above")
    t.wait_caret(view, 14, "the cursor is past the indent it was given")

    # Out of the block: the same key with Shift adds nothing.
    t.key("shift+Return")
    holds(t, view, AFTER_SHIFT_ENTER, "Shift+Enter to open a line at the margin")
    t.wait_caret(view, 15, "the cursor is at column 0")

    # Splitting a line rather than ending one: the tail goes down indented.
    t.key("ctrl+Home")
    for _ in range(7):
        t.key("Right")

    t.key("Return")
    holds(t, view, AFTER_SPLIT, "Enter in the middle of a word to indent the tail")
    t.wait_caret(view, 12, "the cursor is in front of the tail it moved")

    # Saved, or the quit at the end of the test turns into a dialog about it.
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")


def holds(t, view, expected, what):
    t.wait(lambda: t.text(view) == expected,
           "%s;\nthe document holds %r" % (what, t.text(view)))
    t.log("ok: %s" % what)
