"""A double click takes a run of one kind of character, a triple click the line.

# requires: MOO_GTK3

_moo_text_view_extend_selection() in mootextview-input.c. GtkTextView selects a
word on a double click by pango's word rule; medit selects by character class
instead -- blank, word, or neither -- so a double click on punctuation takes the
punctuation and a double click in a run of blanks takes the blanks. Both are
what a user does to a line of code all day, and neither was watched.

The line ends bound both walks, which is why the runs below stop at the end of
the line rather than running into the next one.

A triple click takes the line and the break at the end of it, so that pasting it
somewhere puts a line there rather than joining two.
"""

CONTENT = "alpha beta  gamma();\nsecond line\n"

#  "alpha" 0-4, ' '5, "beta" 6-9, ' '10, ' '11, "gamma" 12-16,
#  '(' 17, ')' 18, ';' 19, \n 20, "second line" 21-31, \n 32

WORD = (6, 10, "beta")
BLANKS = (10, 12, "the two blanks between the words")
PUNCTUATION = (17, 20, "();, the whole run of it and no further than the line end")

FIRST_LINE = (0, 21, "the first line and the break at the end of it")


def setup(s):
    s.open(s.write("workdir/select.txt", CONTENT))


def run(t):
    view = t.document()

    for start, end, what in (WORD, BLANKS, PUNCTUATION):
        # In the middle of the first character of the run, so the click is not
        # near a boundary where a pixel would decide it.
        t.click_range(view, start, start + 1, times=2)
        t.wait_selection(view, (start, end),
                         "a double click selects %s" % what)

    t.click_range(view, 2, 3, times=3)
    t.wait_selection(view, FIRST_LINE[:2],
                     "a triple click selects %s" % FIRST_LINE[2])

    # Nothing was typed, so there is nothing to save and no dialog on the way
    # out; the selection is all this test leaves behind.
    t.check(t.text(view) == CONTENT, "and the document was not changed by any of it")
