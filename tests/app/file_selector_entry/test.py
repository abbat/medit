"""The file selector's path entry completes what is typed into it.

# requires: MOO_GTK3

src/moofileview/moofileentry.c is a completion of medit's own: it reads the
directory as the entry is typed into, offers what matches in a popup, and Tab
takes the first of them. Only the entry's text had ever been read by a test --
tests/app/file_selector_navigate uses it to say where the pane is looking -- and
nothing had typed into it.

So: half a folder name and Tab, which has to become the whole of it, and Return,
which has to take the pane there. The directory has two folders whose names begin
with the same letter, so a completion that took the only folder it found rather
than the one that matches would go to the wrong place.

Where the pane ended up is read twice over: from the entry, and by opening the
first thing in the listing, which is the file that folder holds and nothing else
does. The listing itself cannot be read -- MooIconView draws its own rows and
puts nothing in the accessibility tree -- which is why the file selector's tests
click into it by coordinates.
"""

from lib import input as ui
from lib.notebook import order

# A few pixels into the first row, and far enough in to be inside the icon of a
# short name. The same measurements as tests/app/file_selector_navigate.
FIRST_ROW = 12
INTO_ROW = 20

INNER = "inner"
OTHER = "island"


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/%s/deep.txt" % INNER, "at the bottom")
    s.write("workdir/%s/apart.txt" % OTHER, "somewhere else")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    view = open_the_pane(t)
    entry = path_entry(t, view)

    workdir = t.sandbox.path("workdir")

    t.check(t.text(entry).rstrip("/") == workdir,
            "the pane starts in the document's directory: %s" % t.text(entry))

    # "inn" is enough to tell inner from island; the completion has to find it
    # and Tab has to put it in the entry.
    typed = "%s/inn" % workdir

    t.focus()
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(typed)
    t.key("Tab")

    t.wait(lambda: t.text(entry).rstrip("/") == "%s/%s" % (workdir, INNER),
           "Tab to complete the folder's name; the entry holds %r" % t.text(entry))

    t.key("Return")

    t.wait(lambda: t.text(entry).rstrip("/") == "%s/%s" % (workdir, INNER),
           "Return to take the pane into it; the entry holds %r" % t.text(entry))

    # And the listing is that folder's: the only thing in it is the file below.
    x, y, width, height = t.extents(view)
    t.click_at(x + INTO_ROW, y + FIRST_ROW, times=2)

    t.wait(lambda: "deep.txt" in order(t),
           "opening the first row to open the file that folder holds; "
           "the strip is %s" % order(t))


def path_entry(t, view):
    """The entry above the listing, which says the directory."""
    vx, vy, vwidth, vheight = t.extents(view)

    for node in t.find_all(t.frame, role="text", depth=30):
        if not ui.on_screen(node):
            continue

        x, y, width, height = t.extents(node)

        if abs(x - vx) <= 4 and y < vy:
            return node

    return t.fail("the pane has no entry for the directory it is looking at")


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
