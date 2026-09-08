"""A pane carried from one edge of the window to another.

# requires: MOO_GTK3

MooBigPaned holds four MooPaneds, one along each edge, and a pane is moved
between them by dragging the grip in its own toolbar. While the drag is in
the air the window it would land in is outlined -- a shaped GdkWindow with
nothing but a frame in it, put over the drop area.

The frame is the point of this test as much as the move is. It is drawn on a
window of its own and shaped by a region, so neither the accessibility tree
nor any widget's geometry says whether it is there or what shape it is: the
test reads one line across the screen before and during the drag and asks what
changed. A frame changes a line where it crosses it and nowhere in between; a
block changes all of it; nothing at all changes none of it, which is what the
port did.
"""

from lib import input as ui

# Where in the pane's toolbar the drag grip is. It is an event box packed to
# fill what is left of the toolbar after the label, so anywhere in the gap
# between the two does; a third of the way across the pane is inside it.
GRIP = 3

# How much of a scanned line a frame may account for. Two frames cross a line
# at eight points at most, out of several hundred -- a filled shape of the
# same size would be most of it.
CROSSINGS = 0.15


def setup(s):
    s.plugin("FileSelector")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    open_the_pane(t)

    was = t.extents(pane_content(t))
    t.check(was[0] > t.extents(t.frame)[2] // 2,
            "the pane starts against the right edge of the window, at x=%d" % was[0])

    outline = carry_it_to_the_bottom(t)

    a_frame_and_not_a_block(t, outline)

    now = t.extents(pane_content(t))

    t.check(now[1] > was[1] + was[3] // 2,
            "the pane is along the bottom of the window now: it was at y=%d, "
            "it is at y=%d" % (was[1], now[1]))
    t.check(now[2] > was[2],
            "and it is as wide as the window rather than as narrow as the "
            "edge it came from: %d against %d" % (now[2], was[2]))


def open_the_pane(t):
    t.menu("View", "Panes", "File Selector")
    t.pin_pane()
    t.wait(lambda: pane_content(t), "the pane's content")


def carry_it_to_the_bottom(t):
    """Drag the grip to the bottom of the window, reading the screen on the way.

    Several lines across the lower half of the window rather than one: how far
    up the drop area for the bottom edge reaches is MooBigPaned's own
    arithmetic, and a test that picked one line would be asserting that
    arithmetic by accident. What is asserted is that the indicator crosses the
    lines it crosses as edges.
    """
    x, y, width, height = t.extents(t.frame)
    lines = [y + height * n // 8 for n in (4, 5, 6, 7)]

    before = {line: t.pixel_row(x, line, width) for line in lines}
    during = {}

    t.drag_to(grip(t), toolbar(t), x + width // 2, y + height - 60,
              during=lambda: during.update(
                  {line: t.pixel_row(x, line, width) for line in lines}))

    t.check(during, "the screen was read while the pane was in the air")

    return {line: [i for i, (was, now)
                   in enumerate(zip(before[line], during[line])) if was != now]
            for line in lines}


def a_frame_and_not_a_block(t, changed):
    """What the drop indicator put on those lines is edges, not a filling."""
    crossed = {line: points for line, points in changed.items() if points}

    for line in sorted(changed):
        t.log("y=%d: %d pixels changed" % (line, len(changed[line])))

    t.check(crossed,
            "the drop indicator is drawn: it changed the screen on %d of the "
            "%d lines scanned across the window" % (len(crossed), len(changed)))

    for line, points in sorted(crossed.items()):
        span = points[-1] - points[0] + 1

        t.check(len(points) <= span * CROSSINGS,
                "and at y=%d it is a frame rather than a block: %d pixels "
                "changed across a span of %d" % (line, len(points), span))


def grip(t):
    """Where the pane's toolbar can be taken hold of."""
    x, y, width, height = t.extents(pane_content(t))

    return x + width // GRIP


def toolbar(t):
    """The height of the pane's toolbar, which is above its content."""
    hide = t.need(t.frame, role="push button", depth=30,
                  pred=lambda n: n.description == "Hide pane" and ui.on_screen(n),
                  what="the button that hides the pane")
    x, y, width, height = t.extents(hide)

    return y + height // 2


def pane_content(t):
    """The file selector's icon view, which is what the pane holds."""
    found = [n for n in t.find_all(t.frame, role="unknown", depth=30)
             if ui.on_screen(n)]

    return found[0] if len(found) == 1 else None
