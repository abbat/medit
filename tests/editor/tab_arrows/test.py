"""A tab strip with more tabs than it can show: the arrows at its ends.

# requires: MOO_GTK3

A scrollable notebook draws an arrow at each end of a strip that overflows, and
a click on one steps to the next document and scrolls the strip to keep its tab
drawn. The arrows are drawn by the notebook rather than built out of widgets, so
nothing in the accessibility tree points at one; what the tree does hold is the
tabs, and which of them are drawn says where the strip has got to.

So each arrow is clicked where it has to be. They are small -- around
INSET * 2 wide -- and sit at the two ends of the row of tabs: the backward one
against the left edge of the notebook, the forward one just past the last tab
that is drawn. The window's close button is further right still, past the end
of the strip, and closes a document if it is hit by accident, so the documents
are counted again at the end.

An arrow held down repeats, and a click is long enough that one sometimes steps
twice. That costs nothing here: both ends stop rather than wrap, so clicking
until the document at the end is the current one arrives there either way.

tests/editor/tab_overflow turns the wheel over the same strip. The wheel is not
used here at all, so that a build where only one of the two ways of reaching an
off-strip tab broke fails in one place rather than in neither.
"""

from lib.notebook import drawn, showing, strip, tabs, the_notebook

# Enough documents, with names long enough, that the strip cannot show them all
# at any sane font size.
NAMES = ["document-number-%02d.txt" % n for n in range(1, 13)]

# How far into an arrow to click, from the end of the strip it sits at.
INSET = 16


def setup(s):
    for name in NAMES:
        s.open(s.write("workdir/" + name, "%s\n" % name))


def run(t):
    t.check(len(drawn(t)) < len(NAMES),
            "the strip cannot show all %d tabs: it draws %d"
            % (len(NAMES), len(drawn(t))))

    click_until(t, left_arrow, NAMES[0], "the left arrow")

    t.check(names(drawn(t))[0] == NAMES[0],
            "the strip is at its near end: it draws %s"
            % ", ".join(names(drawn(t))))

    click_until(t, right_arrow, NAMES[-1], "the right arrow")

    t.check(names(drawn(t))[-1] == NAMES[-1],
            "and at its far end: it draws %s" % ", ".join(names(drawn(t))))

    t.check(len(tabs(t)) == len(NAMES),
            "and all %d documents are still open: no click hit the close button"
            % len(NAMES))


def names(nodes):
    return [node.name for node in nodes]


def click_until(t, arrow, wanted, what):
    """Click an arrow until the document it steps towards is the current one."""
    for _ in range(len(NAMES) + 2):
        if showing(t) == wanted:
            t.log("%s reached %s" % (what, wanted))
            return

        x, y = arrow(t)
        t.click_at(x, y)

    t.fail("%s never reached %s; %s is showing" % (what, wanted, showing(t)))


def left_arrow(t):
    """Against the left edge of the notebook, before the first tab."""
    return t.extents(the_notebook(t))[0] + INSET, strip(t)


def right_arrow(t):
    """Just past the right edge of the last tab that is drawn."""
    x, _, width, _ = t.extents(drawn(t)[-1])

    return x + width + INSET, strip(t)
