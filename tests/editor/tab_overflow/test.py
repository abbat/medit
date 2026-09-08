"""When the tabs do not all fit, the wheel scrolls the strip.

# requires: MOO_GTK3

labels_scroll() in moonotebook.c, reached from moo_notebook_scroll_event(). The
strip is medit's own, and so is this: GtkNotebook answers a wheel over its tabs
by switching pages, and MooNotebook moves the strip instead, leaving the current
page alone. It is also the only way to reach a tab that is off the end of the
strip other than the two arrow buttons, which are internal children of the
notebook and are in no accessibility tree, so this is the reachable half of the
same code.

Read by clicking one fixed point on the strip and asking which document came
forward: after scrolling one way it is a later document, and after scrolling far
enough the other way it is the very first tab, where labels_scroll() clamps.
None of it depends on where a tab is drawn or on what colour it is.

The strip does not start at the left end, which is worth knowing before reading
the numbers below: opening a document scrolls the strip to show its tab, so with
fourteen open the left of the strip is somewhere in the middle of the list. So
does clicking one, which is why three notches move the tab under the fixed point
by one name rather than by three -- the reading is a direction and a clamp, not a
distance.

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

    first = document_at(t, x)
    t.log("the click at the left of the strip answers with %s" % first)

    scroll(t, x, FORWARD, down=True)

    later = document_at(t, x)
    t.check(later != first,
            "after scrolling, the same point on the strip is a different tab: %s "
            "rather than %s -- so the strip moved" % (later, first))

    t.check(order(t).index(later) > order(t).index(first),
            "and it moved towards the end of the strip: %s comes after %s"
            % (later, first))

    scroll(t, x, BACK, down=False)

    back = document_at(t, x)
    t.check(back == NAMES[0],
            "scrolling the other way as far as it goes leaves the first tab at the "
            "left end: %s, where the strip stops" % back)


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
