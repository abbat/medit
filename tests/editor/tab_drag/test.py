"""Three documents, and the middle tab dragged to the front.

# requires: MOO_GTK3

The tab strip is MooNotebook's own, drawn on a GdkWindow rather than built out
of widgets, so nothing in the accessibility tree points at a tab. What is in
the tree is the pages, named after their tabs by MooNotebookAccessible, in the
order the notebook holds them -- which is what reordering changes and is
therefore the evidence for the drop. Where the tabs are is found by clicking
along the strip and asking which page came forward.

While the tab is in the air it is a picture of itself: MooNotebook takes a
copy of the tab when the drag begins and paints it under the pointer at
LABEL_ALPHA. Nothing in the tree describes that, and it is the one thing here
read as pixels -- a tab is mostly its own light background, and anything that
is not a picture of the tab is not.
"""

from lib.notebook import order, showing, spans, strip

# What a tab looks like: mostly the light surface it is drawn on, with the
# text a small dark part of it. The tab in the air is that same picture
# blended over the strip, so it cannot be much darker than the tab at rest --
# where an uninitialised buffer painted at the same place is a block of
# whatever the allocator last left there.
TOLERANCE = 40


def setup(s):
    s.open(s.write("workdir/alpha.txt", "a"))
    s.open(s.write("workdir/bravo.txt", "b"))
    s.open(s.write("workdir/charlie.txt", "c"))


def run(t):
    t.check(order(t) == ["alpha.txt", "bravo.txt", "charlie.txt"],
            "the three documents are open in the order they were given")

    tabs = spans(t, 3)
    t.log("the tabs are at %s" % tabs)

    for name in ("alpha.txt", "bravo.txt", "charlie.txt"):
        t.check(name in tabs, "%s has a tab on the strip" % name)

    picture_of_itself(t, tabs)

    t.check(order(t) == ["bravo.txt", "alpha.txt", "charlie.txt"],
            "dropping the middle tab on the first one reordered them")
    t.check(showing(t) == "bravo.txt",
            "and the document that was dragged is still the one on screen")


def picture_of_itself(t, spans):
    """Drag bravo onto alpha, looking at the tab in the air on the way.

    The tab keeps the grip it was picked up by, so it is drawn to the left of
    the pointer by however far into it the press landed, not centred on the
    pointer. The drop is aimed so that the tab in the air comes to rest over
    the first tab and is wholly on screen -- half of it beyond the edge of the
    window would be measured as whatever is outside the window.
    """
    left, right = spans["bravo.txt"]
    start = (left + right) // 2
    grip = start - left
    width = right - left
    target = spans["alpha.txt"][0] + grip

    rest = brightness(t, left, right)
    t.log("the tab at rest is %d bright" % rest)

    seen = []

    t.drag_to(start, strip(t), target, strip(t),
              during=lambda: seen.append(
                  brightness(t, target - grip, target - grip + width)))

    t.check(seen and seen[0] >= rest - TOLERANCE,
            "the tab being dragged is a picture of the tab: %d bright against "
            "%d for the tab at rest" % (seen[0] if seen else -1, rest))


def brightness(t, left, right):
    """How light the strip is between two x, on average.

    Sampled on a grid rather than along one line: the text of a tab is a few
    dark rows in a light field, and one line through it says more about which
    row it went through than about the tab.
    """
    top = strip(t) - 8
    seen = [grey(t.pixel(x, y))
            for x in range(left + 6, right - 6, 6)
            for y in range(top, top + 22, 6)]

    return sum(seen) // len(seen)


def grey(colour):
    """How light a "#rrggbb" is, on the usual weighting."""
    red, green, blue = (int(colour[i:i + 2], 16) for i in (1, 3, 5))

    return (red * 299 + green * 587 + blue * 114) // 1000
