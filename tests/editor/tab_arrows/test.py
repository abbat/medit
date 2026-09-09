"""A tab strip with more tabs than it can show: the arrows and the wheel.

# requires: MOO_GTK3

MooNotebook lays the tabs out itself, and when they do not fit it puts two arrow
buttons at the right end of the strip and scrolls the labels under them. The
buttons are an internal child, which is why nothing reached them until the
notebook began reporting them among its accessible children -- and how far each
of them scrolls, and when each stops working, is moo_notebook_check_arrows() and
labels_scroll(). tests/editor/tab_overflow turns the wheel over the same strip;
here the wheel is only asked to leave the arrows in the state it should.

How far the strip has scrolled is not in the accessibility tree, but the arrows
say it exactly: check_arrows() makes the left one sensitive only once something
has been scrolled off the left, and the right one insensitive only at the far
end. So the arrows are both what is clicked and what is read. The strip starts at
the far end, since the document that was opened last is the current one and its
tab is brought on screen.

The names are read where a tab is certainly whole: at the right end of the strip
while everything is scrolled off the left, and at the left end once nothing is.
A click on a half-shown tab would scroll the strip to show it, which is a
different question than the one being asked.
"""

from lib.notebook import strip, the_notebook

# Enough documents, with names long enough, that the strip cannot show them all
# at any sane font size.
NAMES = ["document-number-%02d.txt" % n for n in range(1, 13)]

# A few pixels in from the end of the labels, inside the first and the last tab.
INSET = 8


def setup(s):
    for name in NAMES:
        s.open(s.write("workdir/" + name, "%s\n" % name))


def run(t):
    left, right = arrows(t)

    t.check(t.state(left, "sensitive") and not t.state(right, "sensitive"),
            "the strip is full, and scrolled to the end: it has two arrows, "
            "and only the left one works")

    t.check(name_at(t, labels(t)[1] - INSET) == NAMES[-1],
            "the last document's tab is at the right end of the strip")

    scroll_with(t, left, "the left arrow")

    t.check(t.state(right, "sensitive"),
            "the right arrow works once the strip is back at the near end")

    t.check(name_at(t, labels(t)[0] + INSET) == NAMES[0],
            "and the first document's tab is at the left end of it")

    # The wheel over the strip, which is the other way to scroll: down is right.
    x0, x1 = labels(t)
    t.click_at((x0 + x1) // 2, strip(t), button=5)

    t.check(t.state(left, "sensitive"),
            "the wheel scrolled the strip off its near end")

    scroll_with(t, right, "the right arrow")

    t.check(not t.state(right, "sensitive"),
            "the right arrow stops working at the far end of the strip")

    t.check(name_at(t, labels(t)[1] - INSET) == NAMES[-1],
            "and the last document's tab is back at the right end")


def scroll_with(t, arrow, what):
    """Click an arrow until it gives up: the strip is as far over as it goes."""
    for _ in range(len(NAMES) + 2):
        if not t.state(arrow, "sensitive"):
            t.log("%s stopped working" % what)
            return

        t.click(arrow)

    t.fail("%s never stopped working" % what)


def name_at(t, x):
    """Which document the tab drawn at an x belongs to.

    The page that is drawn is the current one, and a click on a tab makes its
    page current -- the same reading the shared helper does, one x at a time.
    """
    t.click_at(x, strip(t))

    for page in t.find_all(the_notebook(t), depth=1):
        if page.name and t.on_screen([page]):
            return page.name

    return None


def arrows(t):
    """The two scroll buttons of the strip, left one first.

    They are only there while the tabs do not fit -- MooNotebook shows them and
    reports them among its accessible children then -- so finding two of them is
    part of the assertion.
    """
    found = t.on_screen(t.find_all(the_notebook(t), role="push button", depth=3))

    if len(found) != 2:
        t.dump(t.frame)
        t.fail("the strip has %d arrow buttons rather than two" % len(found))

    return sorted(found, key=lambda node: t.extents(node)[0])


def labels(t):
    """Where the labels are drawn: from the left of the strip to the arrows."""
    x, _, _, _ = t.extents(the_notebook(t))

    return x + 2, t.extents(arrows(t)[0])[0] - 2
