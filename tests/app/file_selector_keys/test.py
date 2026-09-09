"""Walking the file selector's listing with the arrows, Home and End.

# requires: MOO_GTK3

src/mooutils/mooiconview.c is a view of medit's own -- GTK's icon view could not
do what the file selector wanted of it -- and the tests had only ever clicked in
it. Its keyboard is a good half of it: moo_icon_view_move_cursor() decides what
Down, Up, Home and End mean in a listing laid out in columns, and none of that
had ever run.

Which row the cursor is on cannot be read from the accessibility tree, since the
view describes nothing in it -- so it is read the way tests/app/file_selector_view
reads it, by asking the row for its properties and taking the name out of the
dialog's title. That costs a dialog per step and is the only reading there is.

The listing is four files with no folders, so the order is plain alphabetical and
each key has an answer that no other key would give.
"""

from lib import input as ui

FIRST_ROW = 8
INTO_ROW = 20

FILES = ("aaa.txt", "bbb.txt", "ccc.txt")

OPEN = "notes.txt"


def setup(s):
    s.plugin("FileSelector")

    for name in FILES:
        s.write("workdir/" + name, "one of the files to walk past\n")

    s.open(s.write("workdir/" + OPEN, "hello"))


def run(t):
    view = open_the_pane(t)

    # The click is the starting point rather than the test: it puts the cursor
    # on the first row, and everything after it is keys.
    x, y, width, height = t.extents(view)
    t.click_at(x + INTO_ROW, y + FIRST_ROW)

    t.check(selected(t) == FILES[0], "the click put the cursor on the first row")

    press(t, "Down")
    t.check(selected(t) == FILES[1], "Down goes to the next row")

    press(t, "Down")
    t.check(selected(t) == FILES[2], "and again")

    press(t, "Up")
    t.check(selected(t) == FILES[1], "Up comes back")

    press(t, "End")
    t.check(selected(t) == OPEN, "End goes to the last row of the listing")

    press(t, "Home")
    t.check(selected(t) == FILES[0], "and Home to the first")


def press(t, key):
    """A key for the listing, which has to be given the focus back each time.

    The properties dialog that reads the cursor takes the focus with it when it
    goes, and there is no window manager to hand it back.
    """
    t.focus()
    ui.key(key)


def selected(t):
    """Which file the cursor is on, as its properties dialog is titled."""
    menu = t.popup()
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
