"""Selecting several files in the file selector: Ctrl, Shift and a rubber band.

# requires: MOO_GTK3

src/moofileview/mooiconview.c draws its own rows and does its own selecting:
Ctrl adds a row to what is selected, Shift takes everything between the cursor
and the row, and a drag begun on empty space is a rubber band over whatever it
covers -- moo_icon_view_maybe_drag_select() and drag_select_finish(). Only single
clicks and the arrow keys had ever reached any of it.

What is selected cannot be read: the view tells the accessibility tree nothing.
So each way of selecting is answered by deleting what it selected, which is a
question about "selected files" rather than about one, and what is left on disk
afterwards is the assertion -- the exact set, so a selection one row too wide or
too narrow fails it.

The document the window has open is in another folder, so the listing holds
nothing but the files this test made.
"""

import os

from lib import input as ui

# A few pixels into a row, and how tall a row is. The listing is one column here,
# so a row is the whole width of it and the rows go straight down.
FIRST_ROW = 8
ROW = 21

# How far into a row to click: inside the icon of any name there is.
INTO_ROW = 20

FILES = ("aaa.txt", "bbb.txt", "ccc.txt", "ddd.txt", "eee.txt")

DELETE = "Move to Trash..."


def setup(s):
    s.plugin("FileSelector")

    for name in FILES:
        s.write("workdir/" + name, "one of the files to select\n")

    s.open(s.write("elsewhere/notes.txt", "not in the folder under test\n"))


def run(t):
    view = open_the_pane(t)

    # Ctrl: the first row and the third, and nothing between them.
    click_row(t, view, 0)
    click_row(t, view, 2, modifiers=("ctrl",))
    delete(t, view)

    left(t, ["bbb.txt", "ddd.txt", "eee.txt"],
         "Ctrl+click deleted the two rows it added together")

    # Shift: from the row clicked to the row shift-clicked, inclusive.
    click_row(t, view, 0)
    click_row(t, view, 1, modifiers=("shift",))
    delete(t, view)

    left(t, ["eee.txt"], "Shift+click deleted the range between the two rows")

    # And a rubber band, begun below the last row and drawn up over it.
    x, y, width, height = t.extents(view)
    t.drag_to(x + width - 20, y + FIRST_ROW + 3 * ROW, x + INTO_ROW, y + 2)
    delete(t, view)

    left(t, [], "the rubber band deleted what it was drawn over")


def left(t, names, what):
    """What the folder holds now, once the deleting is over."""
    t.wait(lambda: holds(t) == sorted(names),
           "%s; the folder holds %s" % (what, holds(t)))

    t.check(holds(t) == sorted(names), "%s: %s" % (what, holds(t)))


def holds(t):
    return sorted(os.listdir(t.sandbox.path("workdir")))


def click_row(t, view, row, modifiers=()):
    x, y, width, height = t.extents(view)
    t.click_at(x + INTO_ROW, y + FIRST_ROW + row * ROW, modifiers=modifiers)


def delete(t, view):
    """Delete what is selected, from the menu, and answer the question."""
    t.choose(t.popup(), DELETE)

    dialog = t.wait(lambda: the_question(t), "the question about the files")
    t.click(t.button(dialog, "Delete"))
    t.wait(lambda: the_question(t) is None, "the question to go")


def the_question(t):
    found = t.find_all(t.app, role="alert", depth=2)

    return found[0] if found else None


def open_the_pane(t):
    t.menu("View", "Panes", "File Selector")
    t.pin_pane()

    view = t.wait(lambda: the_icon_view(t), "the icon view of the file selector")

    # By its path rather than by the Jump to button: the document is somewhere
    # else on purpose, so that the listing holds only the files under test.
    entry = path_entry(t, view)
    t.focus()
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(t.sandbox.path("workdir"))
    t.key("Return")

    t.wait(lambda: t.text(entry).rstrip("/") == t.sandbox.path("workdir"),
           "the pane to go to the folder; it is in %r" % t.text(entry))

    return view


def path_entry(t, view):
    vx, vy, vwidth, vheight = t.extents(view)

    for node in t.find_all(t.frame, role="text", depth=30):
        if not ui.on_screen(node):
            continue

        x, y, width, height = t.extents(node)

        if abs(x - vx) <= 4 and y < vy:
            return node

    return t.fail("the pane has no entry for the folder it is looking at")


def the_icon_view(t):
    found = [n for n in t.find_all(t.frame, role="unknown", depth=30)
             if ui.on_screen(n)]

    return found[0] if len(found) == 1 else None
