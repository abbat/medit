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
end. So the arrows are both what is clicked and what is read.

Nothing is asserted about where the strip starts, only about where each arrow
takes it: opening a document scrolls the strip to show that document's tab, so
where it comes to rest depends on how many tabs fit -- on the font, and so on the
machine. That cost tests/editor/tab_overflow a CI run before this test repeated
the mistake. Each end is reached by clicking an arrow until it gives up.

The names are read where a tab is certainly whole: at the left end of the strip
when nothing is scrolled off it, and at the right end when everything is. A click
on a half-shown tab would scroll the strip to show it, which is a different
question than the one being asked.
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

    # Wherever the strip starts, one of the two directions has somewhere to go:
    # the bug this test was written over left both arrows dead on a full strip.
    t.check(t.state(left, "sensitive") or t.state(right, "sensitive"),
            "the strip is full, and one of its two arrows works: the left one is "
            "%s and the right one %s" % (working(t, left), working(t, right)))

    scroll_with(t, left, "the left arrow")

    t.check(not t.state(left, "sensitive") and t.state(right, "sensitive"),
            "at the near end of the strip only the right arrow works")

    t.check(name_at(t, labels(t)[0] + INSET) == NAMES[0],
            "and the first document's tab is at the left end of it")

    # The wheel over the strip, which is the other way to scroll: down is right.
    x0, x1 = labels(t)
    t.click_at((x0 + x1) // 2, strip(t), button=5)

    t.check(t.state(left, "sensitive"),
            "the wheel scrolled the strip off its near end")

    scroll_with(t, right, "the right arrow")

    t.check(not t.state(right, "sensitive") and t.state(left, "sensitive"),
            "at the far end of the strip only the left arrow works")

    t.check(name_at(t, labels(t)[1] - INSET) == NAMES[-1],
            "and the last document's tab is at the right end of it")


def working(t, arrow):
    return "working" if t.state(arrow, "sensitive") else "not working"


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
