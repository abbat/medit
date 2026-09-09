"""The File List pane: a group made, renamed and removed through its menu.

# requires: MOO_GTK3

src/plugins/moofilelist.cpp is a plugin that is on by default, has a pane of its
own, and had never been opened by a test. Its list is a tree of groups and files,
and everything a person can do to it that does not need a drag is in the context
menu: Add Group, Rename, Remove.

So that is what is driven, and what is asserted is the tree afterwards -- the row
appears, the row is called what it was renamed to, and the row is gone. The
rename is an in-place edit of the cell rather than a dialog, which is why the
name is typed straight after the menu item and finished with Return.

The list is not empty to start with, which is the first thing asserted: the
plugin puts the documents that are open into it by itself.
"""

from lib import input as ui

PANE = ("View", "Panes", "File List")

ADD = "Add Group"
RENAME = "Rename"
REMOVE = "Remove"

DOCUMENT = "notes.txt"

# What Add Group calls a new group before it is named.
FIRST_NAME = "Group"

RENAMED = "Papers"


def setup(s):
    s.open(s.write("workdir/" + DOCUMENT, "alpha\n"))


def run(t):
    tree = open_pane(t)

    t.check(rows(t, tree) == [DOCUMENT],
            "the document that is open is in the list already: %s" % rows(t, tree))

    menu(t, tree, DOCUMENT, ADD)
    t.wait(lambda: rows(t, tree) == sorted([DOCUMENT, FIRST_NAME]),
           "Add Group to put a group in the list; the list holds %s" % rows(t, tree))

    # Rename puts the row into an editable cell rather than opening a dialog, so
    # what follows the menu item is typing -- and no t.focus() before it, which
    # every other test does after a menu: pointing the X focus at the window
    # again makes GtkTreeView stop editing, and the typing then goes to the
    # document.
    menu(t, tree, FIRST_NAME, RENAME)
    t.type_text(RENAMED)
    t.key("Return")
    t.wait(lambda: rows(t, tree) == sorted([DOCUMENT, RENAMED]),
           "the typed name to stick; the list holds %s" % rows(t, tree))

    menu(t, tree, RENAMED, REMOVE)
    t.wait(lambda: rows(t, tree) == [DOCUMENT],
           "Remove to take the group away; the list holds %s" % rows(t, tree))


def open_pane(t):
    t.menu(*PANE)
    t.pin_pane()

    return t.wait(lambda: the_tree(t), "the File List pane's tree")


def the_tree(t):
    """The pane's tree view: the only table on screen once the pane is open."""
    tables = t.on_screen(t.find_all(t.frame, role="tree table", depth=30))

    return tables[0] if len(tables) == 1 else None


def rows(t, tree):
    """The names in the tree, one per name.

    A row of this tree describes itself twice -- the icon and the text are cells
    of their own and carry the same name -- so what is compared is the set of
    names rather than a list with a length.
    """
    return sorted({cell.name
                   for cell in t.on_screen(t.find_all(tree, role="table cell"))
                   if cell.name})


def menu(t, tree, row, entry):
    """Right-click that row of the tree and pick that item of its menu."""
    cell = t.need(tree, role="table cell", name=row, what="the %r row" % row)

    x, y, width, height = t.extents(cell)
    ui.click_at(x + width // 4, y + height // 2, button=3)

    popup = t.popup()
    item = t.need(popup, role="menu item", name=entry, what="the %r item" % entry)
    t.click(item)
