"""Open several selected files from the file selector.

# requires: MOO_GTK3
"""

from app.file_selector_common import open_pane, path_entry, select_row
from lib.notebook import order


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/a.txt", "a")
    s.write("workdir/b.txt", "b")
    s.open(s.write("workdir/notes.txt", "notes"))


def run(t):
    view = open_pane(t)
    select_row(t, view, 0)
    select_row(t, view, 1, modifiers=("ctrl",))
    menu = t.popup()
    t.check("Open" in [n.name for n in t.on_screen(t.find_all(menu, depth=2)) if n.name],
            "Open is offered for multiple selected files")
    t.choose(menu, "Open")
    t.wait(lambda: "a.txt" in order(t) and "b.txt" in order(t),
           "both selected files to be opened")
