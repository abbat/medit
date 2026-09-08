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

from lib import input as ui

# The strip runs from the top of the notebook to the top of the page. Sampled
# at a fixed offset into it rather than at the middle of a tab: a tab that is
# not the current one is drawn a couple of pixels lower, and this is inside
# both.
STRIP = 17

# How finely to look along the strip for the edges of a tab. The step bounds
# how well an edge is known, and everything below stays well inside what it
# finds. How far to look is not fixed: the scan runs to the width of the
# notebook and stops as soon as the third tab answers, so a machine whose font
# makes the tabs wider costs a few more clicks rather than a failure.
STEP = 12

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

    spans = tab_spans(t)
    t.log("the tabs are at %s" % spans)

    for name in ("alpha.txt", "bravo.txt", "charlie.txt"):
        t.check(name in spans, "%s has a tab on the strip" % name)

    picture_of_itself(t, spans)

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


def tab_spans(t):
    """Click along the strip and note which page each x brings forward.

    Read from the page that is showing rather than from the window title: the
    title follows the document that has the focus, which after a run of clicks
    on the strip is not reliably the one whose tab was last clicked.
    """
    x0, y0, width, height = t.extents(the_notebook(t))
    spans = {}

    for x in range(x0 + 2, x0 + width, STEP):
        t.click_at(x, strip(t))
        name = showing(t)

        if name is None:
            continue

        low, high = spans.get(name, (x, x))
        spans[name] = (min(low, x), max(high, x))

        # The third tab has answered, so the second one's span is complete and
        # there is nothing further along worth the clicks.
        if len(spans) == 3:
            break

    return spans


def strip(t):
    """Where along the height of the window the tabs are drawn."""
    return t.extents(the_notebook(t))[1] + STRIP


def the_notebook(t):
    """The notebook the documents are in -- the one that is on screen."""
    return [n for n in t.find_all(t.frame, role="page tab list", depth=25)
            if ui.on_screen(n)][0]


def order(t):
    """The documents as the notebook holds them, named after their tabs."""
    return [n.name for n in t.find_all(the_notebook(t), depth=1)]


def showing(t):
    """The one page of the notebook that is drawn: the current document."""
    for page in t.find_all(the_notebook(t), depth=1):
        if ui.on_screen(page):
            return page.name

    return None
