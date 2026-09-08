"""File Selector: copy a file somewhere, then move one.

# requires: MOO_GTK3

Cut, Copy and Paste in the pane are about files rather than about text, and
what they did is on disk: a copy leaves the original where it was, a move does
not. That is what this reads, since the view itself has no accessibility and
would only say that something is drawn.

The pane is pointed at each directory by typing the path into its entry, which
is what the entry is for and keeps every click on the first row of the
listing -- the one row whose position is known without knowing how tall a row
is on this machine's fonts.
"""

from lib import input as ui

# A few pixels into the first row, and well below every row: the first is the
# file each step works on, the second is empty space, where a click takes the
# focus and drops the selection.
FIRST_ROW = 8
NO_ROW = -12

# How far into a row to click. From the left, because an entry is only as wide
# as its own name and a point a quarter of the way across the view misses a
# short one -- measured: a double click 66 px in did not open a folder called
# "inner", and 20 px in did. Twenty is inside the icon of any entry there is.
INTO_ROW = 20

CLIP = "a-clip.txt"
BODY = "the file that is copied and then moved"


def setup(s):
    s.plugin("FileSelector")

    # workdir holds no directory of its own, so the first row of its listing is
    # the file this test is about rather than a folder.
    s.write("workdir/" + CLIP, BODY)
    s.write("copy-here/keep.txt", "so that the directory exists")
    s.write("move-here/keep.txt", "so that this one does too")

    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    view = open_the_pane(t)

    copied(t, view)
    moved(t, view)


def copied(t, view):
    """Copy leaves the file where it was and puts another one down."""
    on_the_first_row(t, view, "Copy")

    go(t, view, t.sandbox.path("copy-here"))
    paste(t, view)

    t.wait(lambda: t.sandbox.read("copy-here", CLIP) == BODY,
           "the file to be copied into copy-here")

    t.check(t.sandbox.read("workdir", CLIP) == BODY,
            "and Copy left the original in workdir")


def moved(t, view):
    """Cut takes it with it."""
    go(t, view, t.sandbox.path("workdir"))
    on_the_first_row(t, view, "Cut")

    go(t, view, t.sandbox.path("move-here"))
    paste(t, view)

    t.wait(lambda: t.sandbox.read("move-here", CLIP) == BODY,
           "the file to be moved into move-here")

    t.wait(lambda: t.sandbox.read("workdir", CLIP) == "",
           "and Cut to have taken it out of workdir, where it still reads %r"
           % t.sandbox.read("workdir", CLIP))


def on_the_first_row(t, view, entry):
    """Select whatever is in the first row and choose an entry of its menu."""
    x, y, width, height = t.extents(view)

    t.click_at(x + INTO_ROW, y + FIRST_ROW)
    t.choose(t.popup(), entry)


def paste(t, view):
    """Paste into the directory the view is looking at, nothing selected."""
    x, y, width, height = t.extents(view)

    t.click_at(x + INTO_ROW, y + height + NO_ROW)
    t.choose(t.popup(), "Paste")


def go(t, view, path):
    """Point the pane at a directory by typing where it is."""
    entry = path_entry(t, view)

    t.focus()
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(path + "/")
    t.key("Return")

    t.wait(lambda: where(t, view).rstrip("/") == path.rstrip("/"),
           "the pane to go to %s; it is in %s" % (path, where(t, view)))


def where(t, view):
    return t.text(path_entry(t, view))


def path_entry(t, view):
    """The entry above the listing, which says and takes the directory."""
    vx, vy, vwidth, vheight = t.extents(view)

    for node in t.find_all(t.frame, role="text", depth=30):
        if not ui.on_screen(node):
            continue

        x, y, width, height = t.extents(node)

        if abs(x - vx) <= 4 and y < vy:
            return node

    t.fail("the pane has no entry for the directory it is looking at")


def open_the_pane(t):
    t.menu("View", "Panes", "File Selector")
    t.pin_pane()
    t.click(t.need(t.frame, role="push button", name="Jump to",
                   what="the button that goes to the document's directory"))

    return t.wait(lambda: the_icon_view(t), "the icon view of the file selector")


def the_icon_view(t):
    found = [n for n in t.find_all(t.frame, role="unknown", depth=30)
             if ui.on_screen(n)]

    return found[0] if len(found) == 1 else None
