"""File Selector: the two entries that open what is selected.

# requires: MOO_GTK3

"Open" hands the selection to medit, and "Open With / Default Application"
hands it to whatever the desktop would use -- xdg-open, which in the sandbox is
a script of the test's own, so that what is asserted is the file medit passed
and not what the machine happens to have installed.

The entry is offered for one file, which is the usual case and the one a person
reaches for after clicking a name. It is not offered for a folder, because it
opens regular files and would silently do nothing.
"""

from lib import input as ui

# A few pixels into the first row of the view: entries are a row high and start
# at the view's own origin, so this is inside the first one whatever the icon
# size is. The directories of a listing come before its files, so the first row
# is a folder where there is one and the file this test is about where there
# is not.
FIRST_ROW = 8

# How far into a row to click. From the left, because an entry is only as wide
# as its own name and a point a quarter of the way across the view misses a
# short one -- measured: a double click 66 px in did not open a folder called
# "inner", and 20 px in did. Twenty is inside the icon of any entry there is.
INTO_ROW = 20

# Named to come first in the listing, ahead of the document that is already
# open: the test clicks the first row, and "notes.txt" would otherwise be it.
OPEN_ME = "a-open-me.txt"


def setup(s):
    s.plugin("FileSelector")

    # xdg-open, first on the path, writing down what it was asked to open.
    s.script("bin/xdg-open",
             "#!/bin/sh\nprintf '%%s\\n' \"$1\" >>%s\n" % s.path("opened.txt"))

    s.write("workdir/" + OPEN_ME, "the file the menu opens")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    view = open_the_pane(t)

    opened_by_medit(t, view)
    opened_by_the_desktop(t, view)
    not_offered_for_a_folder(t, view)


def opened_by_medit(t, view):
    """Select the file, choose Open, and medit is editing it."""
    menu = menu_on_the_first_row(t, view)

    t.check("Open" in entries(t, menu),
            "the menu offers Open for the one file that is selected: %s"
            % ", ".join(entries(t, menu)))

    t.choose(menu, "Open")

    # The documents medit holds, not the window title: the title follows
    # whichever document has the focus, and after using the pane that is still
    # the one that was open before.
    t.wait(lambda: OPEN_ME in documents(t),
           "medit to have opened %s; it holds %s" % (OPEN_ME, documents(t)))
    t.log("ok: Open opened %s" % OPEN_ME)


def opened_by_the_desktop(t, view):
    """And Open With / Default Application hands the same file to xdg-open."""
    menu = menu_on_the_first_row(t, view)

    t.choose(menu, "Open With", "Default Application")

    t.wait(lambda: t.sandbox.read("opened.txt").strip().endswith("/" + OPEN_ME),
           "xdg-open to be asked for %s; it was asked for %r"
           % (OPEN_ME, t.sandbox.read("opened.txt")))
    t.log("ok: the desktop was asked to open %s" % OPEN_ME)


def not_offered_for_a_folder(t, view):
    """One level up the first row is a folder, and Open would do nothing there."""
    menu = menu_on_the_first_row(t, view)
    t.choose(menu, "Up")

    t.wait(lambda: not where(t).rstrip("/").endswith("workdir"),
           "the view to go up out of workdir; it is in %s" % where(t))

    menu = menu_on_the_first_row(t, view)

    t.check("Open" not in entries(t, menu),
            "no Open on a folder, which it would not open: %s"
            % ", ".join(entries(t, menu)))

    t.escape()


def documents(t):
    """What medit has open, named after their tabs by MooNotebookAccessible."""
    books = [n for n in t.find_all(t.frame, role="page tab list", depth=25)
             if ui.on_screen(n)]

    return [page.name for book in books for page in t.find_all(book, depth=1)]


def open_the_pane(t):
    t.menu("View", "Panes", "File Selector")
    t.pin_pane()
    t.click(t.need(t.frame, role="push button", name="Jump to",
                   what="the button that goes to the document's directory"))

    return t.wait(lambda: the_icon_view(t), "the icon view of the file selector")


def menu_on_the_first_row(t, view):
    """Select whatever is in the first row and open the menu for it."""
    x, y, width, height = t.extents(view)

    t.click_at(x + INTO_ROW, y + FIRST_ROW)

    return t.popup()


def entries(t, menu):
    """The entries the menu is showing.

    On screen and not merely in the tree: an entry this menu is not offering is
    built all the same and sits there with no position at all.
    """
    return [item.name for item in t.find_all(menu, depth=2)
            if item.name and ui.on_screen(item)]


def where(t):
    """The directory the pane is looking at, as its own entry says."""
    vx, vy, vwidth, vheight = t.extents(the_icon_view(t))

    for node in t.find_all(t.frame, role="text", depth=30):
        if not ui.on_screen(node):
            continue

        x, y, width, height = t.extents(node)

        if abs(x - vx) <= 4 and y < vy:
            return t.text(node)

    return ""


def the_icon_view(t):
    """MooIconView, which AT-SPI knows nothing about beyond where it is."""
    found = [n for n in t.find_all(t.frame, role="unknown", depth=30)
             if ui.on_screen(n)]

    return found[0] if len(found) == 1 else None
