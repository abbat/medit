"""File Selector: find a file among many, scroll to it, open it.

# requires: MOO_GTK3

The pane opens on an icon view -- MOO_FILE_VIEW_ICON is what MooFileView is
born with -- and that view lays its entries out in columns, filling downwards
and growing to the right, so a directory with more files than fit is read by
scrolling sideways. The scroll bar under it is therefore the whole of the
navigation, and a scroll bar with no range is a directory whose first files
cannot be reached at all.

That is what this checks, and it checks it the way a person would find out:
the range is there, dragging the thumb moves the view, and the file the view
puts under the pointer at each end is the file that opens.

MooIconView has no accessibility of its own -- it is one widget drawing its
own cells, and AT-SPI calls it "unknown" -- so nothing here reads a file name
out of the view. What it reads is which document medit opened, which is the
same evidence a person has and is stronger than a name in a tree: it says the
cell under those coordinates really was that file.
"""

from lib import input as ui

# Enough to need several columns in a pane this wide, and named so that the
# alphabetical order and the order they were created in are the same -- the
# test asserts which file is first, and a listing sorted by anything else
# would still be sorted the same way.
COUNT = 40
NAME = "a-file-with-a-fairly-long-name-%02d.txt"

FIRST = NAME % 0
LAST = NAME % (COUNT - 1)


def setup(s):
    s.plugin("FileSelector")

    for i in range(COUNT):
        s.write("workdir/" + NAME % i, "x")

    # The document decides where the pane starts: the selector comes up in the
    # home directory, and its "Jump to" button is what takes it to the
    # directory of whatever is open. Opening one file here is what makes that
    # button mean workdir.
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    view, bar = open_the_pane(t)

    value, low, high = t.value(bar)
    t.check(high > low,
            "the icon view can be scrolled: its scroll bar runs from %s to %s"
            % (low, high))

    at_the_left(t, view, bar)
    at_the_right(t, view, bar)


def open_the_pane(t):
    """Open the File Selector, point it at the documents, and find its parts."""
    t.menu("View", "Panes", "File Selector")

    # Pinned, because opening a document from it hands the focus to the
    # document and an unpinned pane closes the moment that happens -- and this
    # test opens two documents and keeps looking at the pane afterwards.
    t.pin_pane()

    t.click(t.need(t.frame, role="push button", name="Jump to",
                   what="the button that goes to the document's directory"))

    view = t.wait(lambda: the_icon_view(t), "the icon view of the file selector")
    t.log("the icon view is at %s" % (t.extents(view),))

    bar = t.wait(lambda: horizontal_bar(t, view),
                 "the scroll bar of the window the icon view is in")

    return view, bar


def the_icon_view(t):
    """MooIconView, which AT-SPI knows nothing about beyond where it is.

    There is exactly one node of an unknown role on screen in this window, and
    it is the icon view: everything else medit draws is a widget the toolkit
    describes. Anchoring on that rather than on the shape of the tree around it
    is deliberate -- GTK+3 wraps a child that is not GtkScrollable in a
    GtkViewport, so the tree above this node is itself something the fix
    changes.
    """
    found = [n for n in t.find_all(t.frame, role="unknown", depth=30)
             if ui.on_screen(n)]

    return found[0] if len(found) == 1 else None


def horizontal_bar(t, view):
    """The sideways scroll bar of the scrolled window the view is in.

    Found by walking up from the view rather than by where it is on screen: the
    window is a different size in CI than here, and a bar picked out by
    coordinates was picked out wrongly there. Walking up is also what survives
    the fix -- a child that is not GtkScrollable gets a GtkViewport put between
    it and the scrolled window, so the number of steps is one of the things
    that changes.
    """
    node = view

    for _ in range(4):
        node = node.parent

        if node is None:
            return None

        if node.getRoleName() == "scroll pane":
            break
    else:
        return None

    for bar in t.find_all(node, role="scroll bar", depth=1):
        width, height = t.extents(bar)[2:]

        if width > height and ui.on_screen(bar):
            return bar

    return None


def at_the_left(t, view, bar):
    """Scrolled fully left, the first cell is the first file by name."""
    to_end(t, bar, -1)

    t.check(t.value(bar)[0] == t.value(bar)[1],
            "the scroll bar is at its minimum after dragging it left")

    t.check(open_first_cell(t, view) == FIRST,
            "the first cell opened %s, the first file in the directory" % FIRST)


def at_the_right(t, view, bar):
    """Scrolled fully right, it is a later one -- the view really moved."""
    to_end(t, bar, +1)

    t.check(t.value(bar)[0] == t.value(bar)[2],
            "the scroll bar is at its maximum after dragging it right")

    opened = open_first_cell(t, view)

    t.check(opened != FIRST,
            "the first cell is no longer %s once the view is scrolled: it is %s"
            % (FIRST, opened))
    t.check(opened <= LAST or opened == "notes.txt",
            "and it is a file of this directory: %s" % opened)


def to_end(t, bar, direction):
    """Drag the thumb as far as it goes, and let the view catch up."""
    width = t.extents(bar)[2]
    t.drag(bar, direction * 2 * width, 0)


def open_first_cell(t, view):
    """Double-click the top-left cell and say which document that opened.

    The offset is inside the first cell and clear of its edges: the entries
    are a row high and start at the view's own origin, so a point a third of
    the way into the first row belongs to the first entry whatever the icon
    size is.
    """
    x, y, width, height = t.extents(view)

    before = t.frame.name
    ui.click_at(x + width // 4, y + 12, times=2)

    t.wait(lambda: t.frame.name != before,
           "a document to open from the cell at the top left of the view")

    return t.frame.name.rsplit("/", 1)[-1]
