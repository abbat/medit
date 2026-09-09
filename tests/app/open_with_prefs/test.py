"""An Open With entry described in the preferences, and used from the pane.

# requires: MOO_GTK3

The File Selector page of the preferences is a list of programs to open files
with -- src/plugins/moofileselector-prefs.cpp, which nothing had opened -- and
what it writes is read back by moofileview-tools.cpp, which turns each entry into
an action in the listing's Open With submenu. tests/app/file_selector_open uses
the entry medit always offers; this one describes an entry and then uses it.

The program is a script of the test's own that writes down what it was asked to
open, so what is asserted is the file name medit passed to it. The extensions
field is asserted with it: the entry claims "*.txt" and the folder holds a file
that does not match, whose menu must not offer the entry at all.
"""

from lib import input as ui

FIRST_ROW = 8
INTO_ROW = 20

NAME = "Shouty"

MATCHES = "a-open-me.txt"
DOES_NOT = "b-leave-me.log"


def setup(s):
    s.plugin("FileSelector")

    s.script("bin/shouty",
             "#!/bin/sh\nprintf '%%s\\n' \"$1\" >>%s\n" % s.path("opened.txt"))

    s.write("workdir/" + MATCHES, "the file the entry is for")
    s.write("workdir/" + DOES_NOT, "the file it is not for")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    view = open_the_pane(t)

    describe_the_program(t)

    # The file the entry claims: its Open With submenu offers the entry, and
    # choosing it runs the program on that file.
    menu = menu_on_the_row(t, view, 0)
    offered = open_with(t, menu)

    t.check(NAME in offered,
            "Open With offers %r for %s: %s" % (NAME, MATCHES, ", ".join(offered)))

    t.choose(menu, "Open With", NAME)

    t.wait(lambda: t.sandbox.path("workdir", MATCHES) in opened(t),
           "the program to be run on the file; it was run on %s" % opened(t))

    # And the file it does not: no entry, since the extensions did not match.
    menu = menu_on_the_row(t, view, 1)
    offered = open_with(t, menu)
    t.escape()

    t.check(NAME not in offered,
            "and it is not offered for %s: %s" % (DOES_NOT, ", ".join(offered)))


def describe_the_program(t):
    dialog = t.preferences("File Selector")

    t.click(page_buttons(t, dialog)[0])
    t.wait(lambda: rows(t, dialog) != [], "New to put a row in the list")

    fill(t, dialog, "Name:", NAME)
    fill(t, dialog, "Command:", "%s %%f" % t.sandbox.path("bin", "shouty"))
    fill(t, dialog, "Extensions:", "*.txt")

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")


def fill(t, dialog, label, text):
    """Type into the entry on the same row as that label."""
    entry = field(t, dialog, label)

    t.focus()
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(text)

    t.wait(lambda: t.text(entry) == text,
           "%s to hold %r; it holds %r" % (label, text, t.text(entry)))


def field(t, dialog, label):
    """The entry beside a label, which is the one on the same row."""
    for node in t.on_screen(t.find_all(dialog, role="label", depth=30)):
        if node.name != label:
            continue

        y = t.extents(node)[1]

        for entry in t.on_screen(t.find_all(dialog, role="text", depth=30)):
            if abs(t.extents(entry)[1] - y) <= 12:
                return entry

    return t.fail("no entry beside %r on the page" % label)


def page_buttons(t, dialog):
    """The four icon buttons beside the list, left to right: new, delete, down, up."""
    found = [b for b in t.on_screen(t.find_all(dialog, role="push button", depth=30))
             if not b.name]

    return sorted(found, key=lambda node: t.extents(node)[0])


def rows(t, dialog):
    return [cell.name for cell in t.on_screen(t.find_all(dialog, role="table cell", depth=30))
            if cell.name]


def opened(t):
    """What the program was asked to open, so far."""
    try:
        return t.sandbox.read("opened.txt").split()
    except OSError:
        return []


def open_with(t, menu):
    """What the Open With submenu of an open context menu holds.

    Opened rather than read where it stands: an item of a submenu that has not
    popped up is in the tree with no position, so it reads as nothing.
    """
    item = t.item(menu, "Open With")
    t.click(item)
    ui.key("Right")

    return [entry.name for entry in t.on_screen(t.find_all(item, depth=1))
            if entry.name]


def menu_on_the_row(t, view, row):
    x, y, width, height = t.extents(view)
    t.click_at(x + INTO_ROW, y + FIRST_ROW + row * row_height(t, view))

    return t.popup()


def row_height(t, view):
    """One row of the listing, which is a fifth of the way down four rows."""
    return 21


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
