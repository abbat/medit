"""A pane: make it wider, put it away, bring it back, and try to overdo it.

# requires: MOO_GTK3

Every pane of the window is a MooPaned holding the document and, along one
edge, the pane's own widget with a splitter between them. The splitter is not
a widget -- it is a GdkWindow the container draws on and takes button events
from -- so nothing in the accessibility tree points at it, and this test finds
it where the geometry says it must be: just outside the pane's content, on the
side the document is on.

What is asserted is the arithmetic of moo_paned_size_allocate() and the clamps
around it: the pane takes the width the splitter is dragged to, the document
gives up exactly that much, the width survives the pane being hidden and shown
again, and a drag that asks for the whole window is refused.

And that the splitter is drawn at all, which is the one thing here with no
evidence in the accessibility tree and so the one thing read as pixels. The
strip is flat when nothing draws on it and carries the two lines MooPaned puts
down either side of the grip when something does; the test asks for the
difference between the darkest and the lightest pixel across it rather than
for a colour, so it is the theme's line, whatever colour the theme draws it.
"""

from lib import input as ui

# Enough to be far outside the noise of one drag, and small enough that the
# document is still wider than its minimum afterwards.
STEP = 60

# The splitter sits between the pane and the document, and the pane's content
# starts this far inside it -- measured, and the same for every pane of the
# window. HANDLE is how much of the strip belongs to the splitter itself,
# between the border on the pane's side and the button box on the other.
SPLITTER = 3
HANDLE = 5

# How far apart the darkest and the lightest pixel across the splitter have to
# be for something to have been drawn on it. A flat strip answers 0; the two
# lines against the surface around them are worth about 200 on the theme the
# tests run under, so anything in between says "a line is there" without
# naming a colour.
CONTRAST = 40


def setup(s):
    s.plugin("FileSelector")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    open_the_pane(t)

    drawn(t)

    wide = wider_by(t, STEP)
    narrow_again(t, wide)
    remembered(t)
    refused(t)


def open_the_pane(t):
    t.menu("View", "Panes", "File Selector")

    # Pinned: an unpinned pane hides itself as soon as the document has the
    # focus back, and every step here reads the pane after touching something
    # else.
    t.pin_pane()

    t.log("the pane is %d wide, the document %d" % (pane_width(t), document_width(t)))


def drawn(t):
    """The splitter has lines on it, rather than being a strip of nothing.

    MooPaned draws the grip and the two lines beside it on a GdkWindow of its
    own, and on GTK+3 one ::draw arrives for the whole widget -- so which
    window a pass is for has to be asked, and asking wrongly leaves the strip
    blank with nothing to say so. There is no widget here to query and no
    colour to compare against: what says the drawing happened is that the strip
    is not all one colour.
    """
    x, y, width, height = pane_box(t)
    middle = y + height // 2

    strip = [t.pixel(px, middle)
             for px in range(x - SPLITTER - HANDLE + 1, x - SPLITTER + 1)]

    spread = max(map(grey, strip)) - min(map(grey, strip))

    t.check(spread >= CONTRAST,
            "the splitter is drawn: across it the pixels run from %s to %s, "
            "a spread of %d" % (min(strip, key=grey), max(strip, key=grey), spread))


def grey(colour):
    """How light a "#rrggbb" is, on the usual weighting."""
    red, green, blue = (int(colour[i:i + 2], 16) for i in (1, 3, 5))

    return (red * 299 + green * 587 + blue * 114) // 1000


def wider_by(t, step):
    """Drag the splitter towards the document; both sides move by the drag."""
    pane, document = pane_width(t), document_width(t)

    drag_splitter(t, -step)

    grown, shrunk = pane_width(t), document_width(t)

    t.check(grown > pane,
            "dragging the splitter %d px into the document widened the pane, "
            "%d -> %d" % (step, pane, grown))
    t.check(abs((grown - pane) + (shrunk - document)) <= 2,
            "the document gave up what the pane took: pane %+d, document %+d"
            % (grown - pane, shrunk - document))

    return grown


def narrow_again(t, wide):
    """And back the other way, to about where it started."""
    drag_splitter(t, +STEP)

    back = pane_width(t)

    t.check(back < wide,
            "dragging the splitter back narrowed the pane again, %d -> %d"
            % (wide, back))

    return back


def remembered(t):
    """Hidden and shown again, the pane keeps the width it was given."""
    width = pane_width(t)

    t.click(t.need(t.frame, role="push button", depth=30,
                   pred=lambda n: n.description == "Hide pane" and ui.on_screen(n),
                   what="the button that hides the pane"))

    t.wait(lambda: the_icon_view(t) is None, "the pane to be hidden")

    t.menu("View", "Panes", "File Selector")
    t.wait(lambda: the_icon_view(t) is not None, "the pane to come back")

    t.check(pane_width(t) == width,
            "the pane came back at the width it was dragged to, %d" % width)


def refused(t):
    """A drag that asks for the whole window leaves the document its own.

    MooPaned clamps the pane against what the document asked for rather than
    letting one side reach zero, and the failure this guards against is the
    clamp being skipped: a pane that can be dragged over the document is a
    window with no document in it and no way back.
    """
    frame_x, frame_y, frame, height = t.extents(t.frame)
    x, y, width, pane_height = pane_box(t)

    # To just inside the window's left edge rather than by the width of the
    # window: xdotool refuses a negative coordinate, and a drag that leaves the
    # screen is a drag the toolkit never sees the end of.
    t.drag_to(x - SPLITTER, y + pane_height // 2,
              frame_x + 1, y + pane_height // 2)

    t.check(document_width(t) > 0,
            "the document is still on screen after a drag across the whole "
            "window: %d px of it" % document_width(t))
    t.check(pane_width(t) < frame,
            "and the pane did not take the window: %d of %d px"
            % (pane_width(t), frame))


def drag_splitter(t, dx):
    """Drag the splitter of the pane sideways by dx.

    The pane is on the right of the window, so its splitter is just left of
    where its content begins and a negative dx makes the pane wider.
    """
    x, y, width, height = pane_box(t)
    t.drag_to(x - SPLITTER, y + height // 2, x - SPLITTER + dx, y + height // 2)


def pane_box(t):
    return t.extents(t.wait(lambda: the_icon_view(t), "the pane's content"))


def pane_width(t):
    return pane_box(t)[2]


def document_width(t):
    """How much of the window the document has, which is what the pane took.

    The biggest text widget on screen, and not the first one found: the pane
    has an entry for the path in it, which is a text widget too, is inside the
    pane, and therefore grows when the document shrinks -- reading that one
    turns this whole test into a tautology that passes.
    """
    views = [n for n in t.find_all(t.frame, role="text", depth=25)
             if ui.on_screen(n)]

    if not views:
        t.fail("no document is on screen")

    tallest = max(views, key=lambda n: t.extents(n)[3])

    return t.extents(tallest)[2]


def the_icon_view(t):
    """The file selector's icon view, the one thing in the pane AT-SPI cannot name."""
    found = [n for n in t.find_all(t.frame, role="unknown", depth=30)
             if ui.on_screen(n)]

    return found[0] if len(found) == 1 else None
