"""File Selector: go into a folder, and come back the four ways the menu offers.

# requires: MOO_GTK3

Up, Back, Forward and Home are the whole of the pane's navigation, and they are
not the same thing: Up is where the current directory sits, Back and Forward
are where the pane has been, and Home is fixed. Walking into a folder and out
of it again tells them apart -- after Up, Back is the folder and Forward is
where Up went, which no other pair of these would be.

What each one did is read from the entry above the listing, which says where
the pane is looking.
"""

from lib import input as ui

# A few pixels into the first row, and well below every row. Directories come
# before files in the listing, so the first row here is the folder this test
# walks into; the second point is empty space, where a click takes the focus
# without selecting anything -- none of these four entries needs a selection.
FIRST_ROW = 12
NO_ROW = -12

# How far into a row to click. From the left, because an entry is only as wide
# as its own name and a point a quarter of the way across the view misses a
# short one -- measured: a double click 66 px in did not open a folder called
# "inner", and 20 px in did. Twenty is inside the icon of any entry there is.
INTO_ROW = 20

INNER = "inner"


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/%s/deep.txt" % INNER, "at the bottom")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    view = open_the_pane(t)

    workdir = t.sandbox.path("workdir")
    inner = t.sandbox.path("workdir", INNER)

    t.check(where(t, view).rstrip("/") == workdir,
            "the pane starts in the document's directory: %s" % where(t, view))

    into_the_folder(t, view, inner)

    went(t, view, "Up", workdir, "up is the directory the folder is in")
    went(t, view, "Back", inner, "back is where the pane was before that")
    went(t, view, "Forward", workdir, "forward is where back came from")
    went(t, view, "Home", t.sandbox.path("home"), "and home is neither")


def into_the_folder(t, view, inner):
    """Double-click the first row, which is the only folder here."""
    x, y, width, height = t.extents(view)

    t.click_at(x + INTO_ROW, y + FIRST_ROW, times=2)

    t.wait(lambda: where(t, view).rstrip("/") == inner,
           "the pane to go into %s; it is in %s" % (INNER, where(t, view)))


def went(t, view, entry, expected, what):
    """Choose one entry of the menu and say where the pane ended up."""
    x, y, width, height = t.extents(view)

    t.click_at(x + INTO_ROW, y + height + NO_ROW)
    t.choose(t.popup(), entry)

    t.wait(lambda: where(t, view).rstrip("/") == expected.rstrip("/"),
           "%s to go to %s; the pane is in %s" % (entry, expected, where(t, view)))

    t.check(True, "%s: %s" % (what, where(t, view)))


def where(t, view):
    return t.text(path_entry(t, view))


def path_entry(t, view):
    """The entry above the listing, which says the directory."""
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
