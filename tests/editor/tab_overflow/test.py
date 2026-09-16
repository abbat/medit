"""When the tabs do not all fit, the wheel over the strip moves through them.

# requires: MOO_GTK3

The notebook is scrollable, which is what makes a strip that is too narrow
scroll its tabs rather than shrink them, and a wheel over such a strip steps to
the next document and scrolls the strip to keep its tab drawn. It is one of the
two ways to reach a tab that is off the end of the strip; the other is the
arrows at its ends, which tests/editor/tab_arrows clicks.

Read from the tabs: each page is in the accessibility tree as a tab named after
its document, so which document is current and which tabs are drawn are both
readable, and neither reading depends on where a tab is drawn or on what colour
it is.

Nothing is assumed about where the strip starts. Opening a document scrolls it
to show that document's tab, so where it comes to rest depends on how many tabs
fit -- on the font, and so on the machine; that cost this test a CI run when it
did assume. Each end is reached by turning the wheel until it gets there.

The strip has to overflow for any of this to mean anything, so that is asserted
first: a build where it stopped overflowing would fail here rather than pass
quietly.
"""

from lib.notebook import drawn, order, showing, strip, tabs

# Long enough names, and enough of them, that the strip cannot hold them all in
# any plausible window.
COUNT = 14

NAMES = ["a-long-document-name-%02d.txt" % i for i in range(COUNT)]

# How many notches to turn. One notch is one document, so crossing the whole
# list from wherever opening them left the strip takes COUNT and a few over.
FORWARD = 3
BACK = COUNT + 4


def setup(s):
    for name in NAMES:
        s.open(s.write("workdir/" + name, name + "\n"))


def run(t):
    t.check(sorted(order(t)) == sorted(NAMES),
            "all %d documents are open" % COUNT)

    t.check(len(drawn(t)) < COUNT,
            "and the strip cannot hold them all: it draws %d of %d"
            % (len(drawn(t)), COUNT))

    # To the near end first, so that what follows starts from a known place.
    scroll(t, BACK, down=False)

    t.check(showing(t) == NAMES[0],
            "turned as far back as it goes, the first document is current: %s"
            % showing(t))
    t.check(names(drawn(t))[0] == NAMES[0],
            "and the strip scrolled to its near end to draw that tab: %s"
            % ", ".join(names(drawn(t))))

    off_the_end = names(tabs(t))[-1]
    t.check(off_the_end not in names(drawn(t)),
            "the last document's tab is off the far end of the strip")

    scroll(t, FORWARD, down=True)

    t.check(showing(t) == NAMES[FORWARD],
            "the wheel stepped forward %d documents, to %s"
            % (FORWARD, showing(t)))
    t.check(order(t).index(showing(t)) > order(t).index(NAMES[0]),
            "which is further along the strip than where it started")


def names(nodes):
    return [node.name for node in nodes]


def scroll(t, notches, down):
    """Turn the wheel over the strip. Buttons 4 and 5 are up and down.

    In one call rather than a loop: every notch is a scroll event either way,
    and a loop of eighteen clicks spends eighteen seconds waiting for a widget
    that has already answered.
    """
    t.click_at(middle(t), strip(t), button=5 if down else 4, times=notches)


def middle(t):
    """A point along the strip that is a tab whichever way it has scrolled."""
    first, last = drawn(t)[0], drawn(t)[-1]
    x0 = t.extents(first)[0]
    x1 = t.extents(last)[0] + t.extents(last)[2]

    return (x0 + x1) // 2
