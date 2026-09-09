"""Go to Line takes the cursor to the line that was typed, and Cancel does not.

# requires: MOO_GTK3

moo_text_view_run_goto_line() in mootextfind.c. The dialog is medit's own, built
from mootextgotoline.ui, and three things about it are decisions: it opens on the
line the cursor is already on, its spin button is bounded by the length of the
document, and Enter is the accept -- the entry is set to activate the default
response, which is what makes typing a number and pressing Enter the whole
interaction.

The top of its range is the buffer's line count, which is one more than the file
has lines of text -- the file ends with a newline, so there is an empty last line
and GTK counts it. The test says so rather than hiding it behind a number.

Cancel is asserted as well as OK. A dialog that moved the cursor while it was
being driven, or that moved it on the way out regardless of the answer, would
pass a test that only ever pressed OK.
"""

LINES = 20

CONTENT = "".join("line %02d\n" % n for n in range(1, LINES + 1))

# Every line is the same width, so the start of line n is arithmetic.
WIDTH = len("line 01\n")

START = 5
TARGET = 12


def setup(s):
    s.open(s.write("workdir/numbered.txt", CONTENT))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)
    t.key("ctrl+Home")

    for _ in range(START - 1):
        t.key("Down")

    t.wait_caret(view, (START - 1) * WIDTH, "the cursor is at the start of line %d" % START)

    # Cancelled: nothing moves.
    spin = open_dialog(t)

    t.check(t.value(spin)[0] == START,
            "the dialog opens on the line the cursor is on: it says %s"
            % t.value(spin)[0])
    # One more than the file has lines of text: it ends with a newline, so the
    # buffer has an empty last line and GTK counts it.
    t.check(t.value(spin)[2] == LINES + 1,
            "and will not go past the end of the document: its top is %s"
            % t.value(spin)[2])

    t.escape()
    t.no_toplevel("Go to Line")

    t.check(t.caret(view) == (START - 1) * WIDTH,
            "cancelling left the cursor where it was")

    # And taken: the cursor goes there.
    spin = open_dialog(t)

    t.click(spin)
    t.key("ctrl+a")
    t.type_text(str(TARGET))
    t.key("Return")

    t.no_toplevel("Go to Line")

    t.wait_caret(view, (TARGET - 1) * WIDTH,
                 "the cursor to be at the start of line %d" % TARGET)


def open_dialog(t):
    """Open Go to Line and return its spin button."""
    t.menu("Search", "Go to Line...")

    dialog = t.dialog("Go to Line")

    return t.need(dialog, role="spin button", what="the line number to type into")
