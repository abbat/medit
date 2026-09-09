"""When the tabs do not all fit, the wheel scrolls the strip.

# requires: MOO_GTK3

labels_scroll() in moonotebook.c, reached from moo_notebook_scroll_event(). The
strip is medit's own, and so is this: GtkNotebook answers a wheel over its tabs
by switching pages, and MooNotebook moves the strip instead, leaving the current
page alone. It is also one of the two ways to reach a tab that is off the end of
the strip; the other is the arrow buttons, which tests/editor/tab_arrows drives.

Read by clicking one fixed point on the strip and asking which document came
forward. The strip is scrolled to its left end first, where labels_scroll()
clamps and the first tab has to be: from there, scrolling the other way has to
bring a later document under the same point. Neither reading depends on where a
tab is drawn or on what colour it is.

Starting from the left end is what makes it reliable, and finding that out cost a
CI run. The strip does not start there: opening a document scrolls it to show
that document's tab, and so does clicking one, so where the left of the strip is
depends on how many tabs fit -- which depends on the font. On the machine this was
written on there was room to scroll further right at the start; in CI the strip
was already at its right end, where the wheel does nothing and rightly so.

For the same reason the reading is a direction rather than a distance: three
notches move the tab under the fixed point by one name, not by three, because
clicking to read it scrolls the strip too.

labels_scroll() does nothing at all while the tabs fit, so a build where the
strip stopped overflowing would fail here rather than pass quietly: the same
click would answer with the same document.
"""

from lib.notebook import order, showing, strip, the_notebook

# Long enough names, and enough of them, that the strip cannot hold them all in
# any plausible window.
COUNT = 14

NAMES = ["a-long-document-name-%02d.txt" % i for i in range(COUNT)]

# How many wheel notches to send. One notch is about one tab, so going back has
# to be able to cross the whole list from wherever opening the documents left
# it -- COUNT and a few over, rather than a number that happens to work here.
FORWARD = 3
BACK = COUNT + 4


def setup(s):
    for name in NAMES:
        s.open(s.write("workdir/" + name, name + "\n"))


def run(t):
    t.check(sorted(order(t)) == sorted(NAMES),
            "all %d documents are open" % COUNT)

    x = t.extents(the_notebook(t))[0] + 6

    # To the left end first, so that what follows starts from a known place.
    # Where the strip sits to begin with is not knowable: opening a document
    # scrolls it to show that document's tab, and how many tabs fit before it
    # depends on the width of the font -- on one machine the left of the strip
    # was the eleventh tab and there was room to scroll further right, on
    # another it was the twelfth and the strip was already at its right end,
    # where the wheel correctly does nothing.
    scroll(t, x, BACK, down=False)

    first = document_at(t, x)
    t.check(first == NAMES[0],
            "scrolled as far left as it goes, the first tab is at the left end: %s"
            % first)

    scroll(t, x, FORWARD, down=True)

    later = document_at(t, x)
    t.check(later != first,
            "after scrolling, the same point on the strip is a different tab: %s "
            "rather than %s -- so the strip moved" % (later, first))

    t.check(order(t).index(later) > order(t).index(first),
            "and it moved towards the end of the strip: %s comes after %s"
            % (later, first))


def document_at(t, x):
    """Click one point on the strip and say which document came forward."""
    t.click_at(x, strip(t))
    return t.wait(lambda: showing(t), "a document to be showing after the click")


def scroll(t, x, notches, down):
    """Turn the wheel over the strip. Buttons 4 and 5 are up and down.

    In one call rather than a loop: every notch is a scroll event either way,
    and a loop of eighteen clicks spends eighteen seconds waiting for a widget
    that has already answered.
    """
    t.click_at(x, strip(t), button=5 if down else 4, times=notches)
