"""The file selector's bookmarks: added from its menu, edited in its dialog.

# requires: MOO_GTK3

src/moofileview/moobookmarkmgr.c keeps the bookmarks of the file selector and
builds both things that show them -- the menu the toolbar's bookmark button drops
and the Edit Bookmarks dialog -- and no test had opened either. What is driven
here is the way in and the way round: bookmark the directory the file selector is
looking at, and find it in the editor's list.

What the editor's own buttons do is not driven. New, Add Separator and Delete
each rebuild the list under an inline cell edit, and clicking one of them from
here closes the dialog for reasons that are about the harness rather than about
medit -- so this test stops where it stops rather than pinning behaviour it
cannot see properly.

The menu itself cannot be read. It is not a toplevel of its own the way a context
menu is, it is in no accessibility tree this harness can reach, and its items
report an impossible position whether it is up or not. It can be driven, though:
a GTK menu grabs the keyboard when it pops up, so the arrows and Return reach it
wherever the input focus is. So the menu is walked blind, by counting the items
the arrows stop on, and every assertion is made about the dialog, which is an
ordinary dialog and is in the tree.

The sandbox starts with no bookmarks, which is what makes the counting knowable:
the menu is "Add Bookmark, Edit Bookmarks" until something is bookmarked, and
each bookmark adds one stop ahead of them. A step that walked to the wrong item
would open no dialog and the next assertion would fail.
"""

from lib import input as ui

DIALOG = "Edit Bookmarks"

# Where Add Bookmark is when the menu holds nothing else, counted in items the
# arrow keys stop on -- a separator is not one of them.
ADD = 1

WORKDIR = "workdir"


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/aaa.txt", "a file, so the directory is not empty")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    open_the_pane(t)

    dialog = open_editor(t, bookmarks=0)
    t.check(rows(t, dialog) == [],
            "the editor opens with no bookmarks in it: %s" % rows(t, dialog))

    press(t, t.button(dialog, "Cancel"))
    t.no_toplevel(DIALOG)

    # Bookmark the directory the selector is looking at, and look again.
    choose(t, ADD)

    dialog = open_editor(t, bookmarks=1)
    bookmark = t.sandbox.path(WORKDIR)

    t.check(rows(t, dialog) == [bookmark],
            "the bookmark is in the editor, labelled with its path: %s"
            % rows(t, dialog))

    # Accepted rather than cancelled, so that the list the editor hands back is
    # the one the menu keeps: the editor works on a copy of it.
    press(t, t.button(dialog, "OK"))
    t.no_toplevel(DIALOG)

    dialog = open_editor(t, bookmarks=1)

    t.check(rows(t, dialog) == [bookmark],
            "and the accepted list is what comes back: %s" % rows(t, dialog))

    press(t, t.button(dialog, "Cancel"))
    t.no_toplevel(DIALOG)


def open_the_pane(t):
    t.menu("View", "Panes", "File Selector")
    t.pin_pane()

    t.click(t.need(t.frame, role="push button", name="Jump to",
                   what="the button that goes to the document's directory"))


def open_editor(t, bookmarks):
    """Pick Edit Bookmarks, which sits after the bookmarks and Add Bookmark."""
    choose(t, bookmarks + 2)

    return t.dialog(DIALOG)


def bookmarks_button(t):
    """The toolbar button that drops the bookmarks, found by its tooltip."""
    return t.need(t.frame, depth=30,
                  pred=lambda node: node.description == "Bookmarks" and ui.on_screen(node),
                  what="the Bookmarks button of the file selector")


def choose(t, position):
    """Drop the menu and walk to the item at that position with the arrows."""
    t.click(bookmarks_button(t))

    for _ in range(position):
        ui.key("Down")

    ui.key("Return")


def rows(t, dialog):
    """The labels the editor lists, one per bookmark.

    A bookmark made from the menu is labelled with its own path, and the path is
    a column of its own, so such a row describes itself twice with the same text
    -- hence a set, with the trailing slash trimmed so that label and path are
    one string.
    """
    return sorted({cell.name.rstrip("/")
                   for cell in t.on_screen(t.find_all(dialog, role="table cell"))
                   if cell.name})


def press(t, node):
    """Click a widget of the editor at coordinates read once and checked.

    The editor rebuilds its widgets when the list changes, and a widget caught in
    the middle of that reports no position at all -- t.click() reads the position
    and clicks in one go, so it would hand that to xdotool and the test would die
    of a rejected coordinate rather than of anything about bookmarks.
    """
    x, y, width, height = t.wait(lambda: placed(t, node),
                                 "%r to have a position" % (node.name or ""))
    t.click_at(x + width // 2, y + height // 2)


def placed(t, node):
    box = t.extents(node)

    return box if box[2] > 0 and -32768 < box[0] < 32768 else None
