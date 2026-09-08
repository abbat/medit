"""Backspace in the indent goes to the indent of a line above, not back one space.

# requires: MOO_GTK3

handle_backspace() in mootextview-input.c, which is medit's own and is on by
default (backspace_indents). When everything before the cursor on the line is
blank, Backspace replaces the whole indent rather than deleting a character,
and the width it replaces it with comes from moo_text_iter_get_prev_stop() --
which is not a fixed tab stop at all: it walks up the buffer and takes the
indent of the nearest line above that is narrower than where the cursor is.

So on a file that steps in four at a time, Backspace walks the levels back out
one at a time, and it would do the same on a file that steps in three. The test
is written on even steps because that is where a fixed-tab-stop implementation
would agree with this one for the first press and disagree later.

The last press is the other half of the function: with a non-blank before the
cursor, none of this applies and Backspace deletes one character.
"""

LINES = ["alpha", "    beta", "        gamma", "            delta"]

CONTENT = "\n".join(LINES) + "\n"

# The widths the walk should stop at, coming out of the deepest line: the
# indent of gamma, then of beta, then of alpha.
STEPS = [8, 4, 0]


def at_width(spaces):
    """The document with the last line indented by that many spaces."""
    return "\n".join(LINES[:3] + [" " * spaces + "delta"]) + "\n"


def setup(s):
    s.pref("Editor/spaces_instead_of_tabs", True)
    s.pref("Editor/indent_width", 4)
    s.open(s.write("workdir/backspace.txt", CONTENT))


def run(t):
    view = t.document()

    t.click(view)
    t.key("ctrl+Home")
    for _ in range(3):
        t.key("Down")
    t.key("Home")

    t.wait_caret(view, CONTENT.index("delta"), "the cursor is at the deepest line's text")

    for width in STEPS:
        expected = at_width(width)

        t.key("BackSpace")

        t.wait(lambda: t.text(view) == expected,
               "Backspace to leave the last line indented by %d;\n"
               "the document holds %r" % (width, t.text(view)))
        t.log("ok: Backspace takes the indent back to %d, the next level up" % width)

        t.wait_caret(view, expected.index("delta"),
                     "the cursor stays at the text it was in front of")

    # Nothing blank in front of it now, so the other branch: one character.
    t.key("End")
    t.key("BackSpace")

    t.wait(lambda: t.text(view) == "\n".join(LINES[:3] + ["delt"]) + "\n",
           "Backspace after a word to delete one character;\n"
           "the document holds %r" % t.text(view))
    t.log("ok: with a non-blank in front of it Backspace deletes a character")

    # Saved, or the quit at the end of the test turns into a dialog about it.
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")
