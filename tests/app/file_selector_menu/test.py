"""File Selector: look at what a file is, rename it, and make another.

# requires: MOO_GTK3

One chain through the parts of the file view that a person actually uses on a
file they did not mean to call that: select it, open the menu it has, ask for
its properties, type a new name, open the file to see that the name took -- on
disk and in the view, which had to notice the rename and redraw -- and then
make a new file from the same menu.

Both dialogs on the way are built from a .ui file, and both had never been
opened by a test. The properties one is taken out of a placeholder window in
moofileprops.ui and put in a dialog of its own; the Create File one shares
moofileselector.ui with Save As, which is what broke it -- a GtkBuilder reads
the whole file at once, and the two dialogs had given their entries the same
id.
"""

from lib import input as ui

# A few pixels into the first row of the view. Entries are a row high and
# start at the view's own origin, so this is inside the first one whatever the
# icon size is -- and both names below sort first, so the first row is the
# file this test is about before and after the rename.
FIRST_ROW = 8

BEFORE = "aaa.txt"
AFTER = "aab.txt"
MADE = "aac.txt"

# What the menu is expected to offer on a file. Not all of it -- the point is
# that the menu is built and populated, not to pin down its contents.
ITEMS = ("Open", "Copy", "New File...", "New Folder", "Properties")


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/" + BEFORE, "the file this test renames")
    s.write("workdir/zzz.txt", "a second file, so the view is not of one thing")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    view = open_the_pane(t)

    menu = menu_on_the_first_file(t, view)
    dialog = properties_of(t, menu)

    rename_in(t, dialog)

    t.check(opened_from_the_first_row(t, view) == AFTER,
            "the first row of the view is %s now, and opening it opens that "
            "file" % AFTER)

    made_from_the_menu(t, view)


def open_the_pane(t):
    t.menu("View", "Panes", "File Selector")
    t.pin_pane()

    # The selector starts in the home directory; this is the button that takes
    # it to the directory of the document that is open.
    t.click(t.need(t.frame, role="push button", name="Jump to",
                   what="the button that goes to the document's directory"))

    return t.wait(lambda: the_icon_view(t), "the icon view of the file selector")


def made_from_the_menu(t, view):
    """Make a new file where the view is looking, and end up editing it.

    The dialog is offered a name and the test types over it, which is what a
    person does; what it proves is that medit created the file and opened it,
    since the window title is the document that is being edited.
    """
    menu = menu_on_the_first_file(t, view)
    t.click(t.item(menu, "New File..."))

    dialog = t.dialog("Create File")
    entry = t.need(dialog, role="text", what="the name of the file to create")

    t.focus()
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(MADE)

    t.check(t.text(entry) == MADE, "the new file is to be called %s" % MADE)

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Create File")

    t.wait(lambda: t.frame.name.endswith("/" + MADE),
           "medit to be editing the file the menu made")
    t.log("ok: %s was created and opened" % MADE)


def menu_on_the_first_file(t, view):
    """Select the first file and open the menu the view offers for it."""
    x, y, width, height = t.extents(view)

    t.click_at(x + width // 4, y + FIRST_ROW)

    menu = t.popup()
    labels = [item.name for item in t.find_all(menu, depth=2)]

    for wanted in ITEMS:
        t.check(wanted in labels,
                "the menu on a file offers %r" % wanted)

    return menu


def properties_of(t, menu):
    """Ask for the properties of the selected file and check it is its own."""
    t.click(t.item(menu, "Properties"))

    dialog = t.dialog("%s Properties" % BEFORE)

    entry = t.need(dialog, role="text", what="the name of the file")

    t.check(t.text(entry) == BEFORE,
            "the properties are of %s, and its name is in the entry" % BEFORE)

    location = [label.name for label in t.find_all(dialog, role="label", depth=8)]

    t.check(any(name.endswith("/workdir/") for name in location),
            "and they say it is in workdir: %s" % location)

    return dialog


def rename_in(t, dialog):
    """Type a new name over the old one and accept the dialog."""
    entry = t.need(dialog, role="text", what="the name of the file")

    # The keys have to be pointed at the dialog first. There is no window
    # manager, so nothing hands the input focus to a window that has just
    # appeared, and a click only moves the caret within a window that already
    # has it.
    t.focus()
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(AFTER)

    t.check(t.text(entry) == AFTER, "the entry holds the new name, %s" % AFTER)

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("%s Properties" % BEFORE)


def opened_from_the_first_row(t, view):
    """Open whatever is in the first row, and say which document that was."""
    x, y, width, height = t.extents(view)

    before = t.frame.name
    t.click_at(x + width // 4, y + FIRST_ROW, times=2)

    t.wait(lambda: t.frame.name != before,
           "a document to open from the first row of the view")

    return t.frame.name.rsplit("/", 1)[-1]


def the_icon_view(t):
    """MooIconView, which AT-SPI knows nothing about beyond where it is."""
    found = [n for n in t.find_all(t.frame, role="unknown", depth=30)
             if ui.on_screen(n)]

    return found[0] if len(found) == 1 else None
