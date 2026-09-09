"""A document dragged from its tab into the file selector.

# requires: MOO_GTK3

Dropping a document on the pane is a way of saving it somewhere, and the whole of
it -- moo_file_selector_drop_doc(), the menu it puts up and the three things that
menu offers -- had never run: the plugin's own drop handling is the largest piece
of src/plugins/moofileselector.cpp and nothing had dragged anything into it.

With no modifier held the drop asks, which is the branch driven here: Move Here,
Save Here and Save Copy, and Escape. Move Here is the one taken, because it is
the one with two answers to check -- the file leaves the folder it was in and
appears in the folder it was dropped into, and the document goes with it rather
than being copied.
"""

import os

from lib import input as ui
from lib.notebook import order, spans, strip

FIRST_ROW = 8
INTO_ROW = 20

# How far into a tab its icon is drawn.
ICON = 12

NAME = "notes.txt"

CONTENT = "a document to drop\n"

INNER = "inner"

MOVE = "Move Here"
OFFERED = ("Move Here", "Save Here", "Save Copy")


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/%s/deep.txt" % INNER, "so the folder is not empty\n")
    s.open(s.write("workdir/" + NAME, CONTENT))


def run(t):
    view = open_the_pane(t)

    # Into the folder the document is going to: the drop saves into whatever the
    # pane is looking at.
    t.click_at(*into_the_first_row(t, view), times=2)
    t.wait(lambda: INNER in where(t, view),
           "the pane to go into %s; it is in %s" % (INNER, where(t, view)))

    tabs = spans(t, 1)
    left, right = tabs[NAME]
    x, y, width, height = t.extents(view)

    # From the icon at the left end of the tab rather than from the middle of it:
    # the drag source is the event box the icon sits in -- tab_icon_start_drag()
    # in mooeditwindow.cpp -- and the rest of the tab drags the tab along the
    # strip instead, which is what tests/editor/tab_drag does.
    t.drag_to(left + ICON, strip(t), x + width // 2, y + height // 2)

    menu = t.wait(lambda: dropped_menu(t), "the menu the drop puts up")
    offered = [item.name for item in t.on_screen(t.find_all(menu, depth=1))
               if item.name]

    for entry in OFFERED:
        t.check(entry in offered, "the drop offers %r: %s" % (entry, ", ".join(offered)))

    t.choose(menu, MOVE)

    t.wait(lambda: t.sandbox.read("workdir/%s" % INNER, NAME) == CONTENT,
           "the file to be written into the folder it was dropped in")

    t.check(gone(t, "workdir", NAME),
            "and taken out of the one it was in")

    t.check(order(t) == [NAME],
            "the document is still open, and is the same one: %s" % order(t))


def gone(t, *path):
    """Whether the file is no longer where it was."""
    return not os.path.exists(t.sandbox.path(*path))


def dropped_menu(t):
    """A menu in a toplevel of its own, which is where a popped-up menu lives."""
    for top in t.find_all(t.app, depth=1):
        for menu in t.find_all(top, role="menu", depth=1):
            if ui.on_screen(menu):
                return menu

    return None


def into_the_first_row(t, view):
    x, y, width, height = t.extents(view)

    return x + INTO_ROW, y + FIRST_ROW


def where(t, view):
    """The folder the pane is looking at, as its own entry says."""
    vx, vy, vwidth, vheight = t.extents(view)

    for node in t.find_all(t.frame, role="text", depth=30):
        if not ui.on_screen(node):
            continue

        x, y, width, height = t.extents(node)

        if abs(x - vx) <= 4 and y < vy:
            return t.text(node)

    return t.fail("the pane has no entry for the folder it is looking at")


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
