"""File Selector: show the hidden files, and re-read the folder.

# requires: MOO_GTK3

The two entries of the View submenu change what the listing holds rather than
what any widget says, and MooIconView tells the accessibility tree nothing
about its contents -- so what the listing holds is read by selecting its first
row and asking the properties dialog whose file that is.

Which makes the order of a listing the thing to arrange: a dot file sorts ahead
of every ordinary name, so switching the hidden files on moves a different file
into the first row.

Reload is asserted for less, and deliberately. medit watches the folder it is
showing -- measured: a file written from outside is in the listing about a
second later, with nothing asked of the pane -- so there is no state that
Reload alone can be seen to fix. What is left to check is that it re-reads
rather than loses: the pane is looking at the same folder afterwards and the
listing still holds what it held, which is what a Reload that emptied the view
or threw the folder away would fail.
"""

from lib import input as ui

FIRST_ROW = 8

# How far into a row to click. From the left, because an entry is only as wide
# as its own name and a point a quarter of the way across the view misses a
# short one.
INTO_ROW = 20

VISIBLE = "a-visible.txt"
HIDDEN = ".hidden.txt"
BEHIND_ITS_BACK = ".a-appeared.txt"


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/" + VISIBLE, "an ordinary file")
    s.write("workdir/" + HIDDEN, "a file whose name begins with a dot")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    view = open_the_pane(t)

    t.check(first_row(t, view) == VISIBLE,
            "the listing starts with %s, the hidden file being hidden" % VISIBLE)

    show_hidden(t, view)

    t.check(first_row(t, view) == HIDDEN,
            "and with %s once the hidden files are shown" % HIDDEN)

    reloaded(t, view)


def show_hidden(t, view):
    """Switch the hidden files on, from the View submenu."""
    menu = menu_on_the_first_row(t, view)

    t.choose(menu, "View", "Show Hidden Files")
    t.settle(1)


def reloaded(t, view):
    """Reload re-reads the folder and keeps it."""
    t.sandbox.write("workdir/" + BEHIND_ITS_BACK, "written from outside")

    t.wait(lambda: first_row(t, view) == BEHIND_ITS_BACK,
           "medit to notice by itself a file written from outside")
    t.log("ok: the folder is watched, so Reload has no backlog to clear")

    where_it_was = where(t, view)

    menu = menu_on_the_first_row(t, view)
    t.choose(menu, "View", "Reload")
    t.settle(1)

    t.check(where(t, view) == where_it_was,
            "Reload left the pane in the folder it was in: %s" % where(t, view))

    t.check(first_row(t, view) == BEHIND_ITS_BACK,
            "and the listing still starts with %s" % BEHIND_ITS_BACK)


def where(t, view):
    """The folder the pane is looking at, as its own entry says."""
    vx, vy, vwidth, vheight = t.extents(view)

    for node in t.find_all(t.frame, role="text", depth=30):
        if not ui.on_screen(node):
            continue

        x, y, width, height = t.extents(node)

        if abs(x - vx) <= 4 and y < vy:
            return t.text(node)

    t.fail("the pane has no entry for the folder it is looking at")


def first_row(t, view):
    """Which file is in the first row, as its properties give the name.

    Nothing in the accessibility tree names an entry of this view, and its
    properties dialog is titled after the file -- so this is the reading, and
    it costs a dialog each time.
    """
    menu = menu_on_the_first_row(t, view)
    t.choose(menu, "Properties")

    dialog = t.wait(lambda: the_properties_dialog(t), "the properties dialog")
    name = dialog.name.rsplit(" Properties", 1)[0]

    t.click(t.button(dialog, "Cancel"))
    t.wait(lambda: the_properties_dialog(t) is None, "the dialog to close")

    return name


def the_properties_dialog(t):
    for node in t.find_all(t.app, role="dialog", depth=2):
        if node.name.endswith(" Properties"):
            return node

    return None


def menu_on_the_first_row(t, view):
    x, y, width, height = t.extents(view)

    t.click_at(x + INTO_ROW, y + FIRST_ROW)

    return t.popup()


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
