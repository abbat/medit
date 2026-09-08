"""Home and End go where medit says, not where GtkTextView would.

# requires: MOO_GTK3

Both keys are medit's own: GtkTextView moves to the ends of the display line,
and moo_text_view_home_end() puts the cursor at the text instead and toggles to
the true end when it is already there. It is on by default (smart_home_end) and
is one of the few things a user presses hundreds of times a day, so the toggle
is spelled out here rather than sampled.

Offsets are counted rather than clicked: a click lands on a character whose
width depends on the font, and the whole test is about which character the
cursor is on.
"""

#                0123456789...
#   line 1:      "    alpha beta   "   4 spaces, 10 of text, 3 spaces, \n at 17
#   line 2:      "gamma"               starts at 18, no blanks at either end
CONTENT = "    alpha beta   \ngamma\n"

FIRST_NON_BLANK = 4
LINE_START = 0
LINE_END = 17
LAST_NON_BLANK = 14

SECOND_LINE_START = 18


def setup(s):
    # Trailing blanks are the point of half of this, and stripping them on save
    # is a setting that would take them away.
    s.pref("Editor/strip", False)
    s.open(s.write("workdir/home-end.txt", CONTENT))


def run(t):
    view = t.document()

    t.check(t.text(view) == CONTENT, "the document holds the line to walk along")

    t.click(view)
    t.key("ctrl+Home")
    for _ in range(7):
        t.key("Right")

    at(t, view, 7, "the cursor starts inside the word")

    # Home from the text: to the first character that is not a blank.
    t.key("Home")
    at(t, view, FIRST_NON_BLANK, "Home goes to the first non-blank")

    # Again, from there: to the true start of the line.
    t.key("Home")
    at(t, view, LINE_START, "Home again goes to column 0")

    # And back, which is what makes it a toggle rather than a walk to the left.
    t.key("Home")
    at(t, view, FIRST_NON_BLANK, "Home from column 0 goes back to the text")

    # End from the middle of the line: to the true end, blanks included.
    t.key("End")
    at(t, view, LINE_END, "End goes to the end of the line")

    # Again: back to the last character that is not a blank.
    t.key("End")
    at(t, view, LAST_NON_BLANK, "End again goes to the last non-blank")

    t.key("End")
    at(t, view, LINE_END, "End from there goes to the end of the line")

    # A line with nothing to skip: the toggle has nowhere to go and must stay
    # put rather than move by one.
    t.key("Down")
    t.key("End")
    at(t, view, SECOND_LINE_START + 5, "End on a line without blanks is its end")

    t.key("Home")
    at(t, view, SECOND_LINE_START, "Home on it is column 0")

    t.key("Home")
    at(t, view, SECOND_LINE_START, "and Home again leaves the cursor there")


def at(t, view, offset, what):
    t.wait_caret(view, offset, what)
