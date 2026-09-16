"""Drop a selected file onto a directory in the file selector.

# requires: MOO_GTK3
"""

import os

from app.file_selector_common import open_pane, path_entry, where
from lib.notebook import tab_icon

FIRST_ROW = 12
INTO_ROW = 20


def setup(s):
    s.plugin("FileSelector")
    s.write("source.txt", "dragged")
    s.write("workdir/target/keep.txt", "keep")
    s.open(s.write("source.txt", "dragged"))


def run(t):
    view = open_pane(t)
    go(t, view, t.sandbox.path("workdir"))
    x, y, width, height = t.extents(view)

    # The only row is target. Select it and drop the document tab on it.
    t.click_at(x + INTO_ROW, y + FIRST_ROW)
    t.drag_to(*tab_icon(t, "source.txt"), x1=x + INTO_ROW, y1=y + FIRST_ROW)

    menu = t.wait(lambda: drop_menu(t), "the file drop menu")
    t.choose(menu, "Move Here")
    t.wait(lambda: t.sandbox.read("workdir/target/source.txt") == "dragged",
           "the dropped document in target")
    t.check(not os.path.exists(t.sandbox.path("source.txt")),
            "Move Here removes the source")


def go(t, view, path):
    entry = path_entry(t, view)
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(path)
    t.key("Return")
    t.wait(lambda: where(t, view) == path, "the file selector to enter target")


def drop_menu(t):
    for top in t.find_all(t.app, depth=1):
        for menu in t.find_all(top, role="menu", depth=1):
            if menu and any(n.name == "Move Here"
                            for n in t.on_screen(t.find_all(menu, depth=1))):
                return menu
    return None
