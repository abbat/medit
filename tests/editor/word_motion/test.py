"""Ctrl with an arrow, a Backspace or a Delete moves by medit's idea of a word.

# requires: MOO_GTK3

text_iter_forward_word_start() and its backward twin in mootextview-input.c,
reached through _moo_text_view_move_cursor() and
_moo_text_view_delete_from_cursor(). GtkTextView has its own word rule and this
replaces it, so every position below is a decision medit makes:

  - an underscore is part of a word, so foo_bar is one word and not two;
  - going forward, the blanks after a word go with it, and the cursor lands on
    the next word rather than after the one it left;
  - punctuation is a run of its own -- (, ) and the like are stepped over
    together, not one at a time;
  - a line end stops the walk. The cursor sits at the end of the line, and only
    the next press crosses to the next line. Neither direction jumps over a line
    break and into the middle of a word beyond it.

Ctrl+Delete and Ctrl+Backspace delete exactly what those walks cross, which is
why they are here rather than in a test of their own.
"""

CONTENT = "foo_bar baz(qux)\nnext\n"

#  f0 o1 o2 _3 b4 a5 r6  7 b8 a9 z10 (11 q12 u13 x14 )15 \n16 n17 e18 x19 t20 \n21

FORWARD = [
    (8, "over foo_bar and the blank after it, onto baz"),
    (11, "over baz, onto the bracket, since no blank follows"),
    (12, "over the bracket, onto qux"),
    (15, "over qux, onto the closing bracket"),
    (16, "over the closing bracket, to the end of the line"),
    (17, "across the line break, to the start of next"),
]

BACKWARD = [
    (16, "from the start of a line, back to the end of the one above"),
    (12, "back over the brackets and qux, to where qux starts"),
    (8, "back to where baz starts"),
    (0, "back to where the first word starts"),
]

WORD_AND_BLANK_GONE = "baz(qux)\nnext\n"
WORD_INSIDE_BRACKETS_GONE = "baz()\nnext\n"


def setup(s):
    s.open(s.write("workdir/words.txt", CONTENT))


def run(t):
    view = t.document()

    t.click(view)
    t.key("ctrl+Home")

    for offset, what in FORWARD:
        t.key("ctrl+Right")
        t.wait_caret(view, offset, "Ctrl+Right goes %s" % what)

    for offset, what in BACKWARD:
        t.key("ctrl+Left")
        t.wait_caret(view, offset, "Ctrl+Left goes %s" % what)

    # What the forward walk crossed is what Ctrl+Delete takes: the word and the
    # blank after it, in one press.
    t.key("ctrl+Delete")
    holds(t, view, WORD_AND_BLANK_GONE,
          "Ctrl+Delete to take the first word and the blank after it")

    # And backwards, from the closing bracket: the word inside, not the bracket.
    t.key("ctrl+Home")
    for _ in range(7):
        t.key("Right")

    t.wait_caret(view, 7, "the cursor is on the closing bracket")
    t.key("ctrl+BackSpace")
    holds(t, view, WORD_INSIDE_BRACKETS_GONE,
          "Ctrl+Backspace to take the word in front of it and leave the brackets")

    # Saved, or the quit at the end of the test turns into a dialog about it.
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")


def holds(t, view, expected, what):
    t.wait(lambda: t.text(view) == expected,
           "%s;\nthe document holds %r" % (what, t.text(view)))
    t.log("ok: %s" % what)
