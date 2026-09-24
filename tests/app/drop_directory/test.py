"""A directory dragged out of the file selector and onto the notebook.

# requires: MOO_GTK3

Dropping a directory onto the tab notebook used to be silently skipped --
notebook_drop_uri_list() ignored any dropped URI that resolved to a
directory. It now walks the directory recursively and opens every text file
found under it, the same as dropping the files themselves would.

No fake drag source is needed for this: the file selector's icon view is
already a real text/uri-list GTK drag source (icon_drag_begin() /
icon_drag_data_get() in moofileview.cpp), which drop_document and
file_selector_drag_drop already use as the *destination* of a drag. Dragging
one of its rows out onto the notebook instead exercises the real drop path,
with a real directory holding a real file.

The file is nested two levels deep, to prove the walk recurses into
subdirectories rather than only listing the top one; there is only one of it,
well under the confirmation threshold, so no question dialog should appear.
"""

from app.file_selector_common import FIRST_ROW, INTO_ROW, go, open_pane, select_row
from lib.notebook import drawn, order

NAME = "open.txt"
FOLDER = "folder"
INNER = "sub/inner.txt"
CONTENT = "a file inside the dropped folder\n"


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/%s/%s" % (FOLDER, INNER), CONTENT)
    s.open(s.write(NAME, "keep a tab open to drop onto\n"))


def run(t):
    view = open_pane(t)
    go(t, view, t.sandbox.path("workdir"))

    x, y, width, height = t.extents(view)
    select_row(t, view)
    t.drag_to(x + INTO_ROW, y + FIRST_ROW, *tab_target(t))

    t.wait(lambda: "inner.txt" in order(t), "the dropped file to open as a tab")
    t.check(t.find_all(t.app, role="alert", depth=2) == [],
            "no confirmation dialog for a handful of files")


def tab_target(t):
    """A point on the open tab: on screen, inside the notebook's drop target."""
    tab = drawn(t)[0]
    tx, ty, twidth, theight = t.extents(tab)
    return tx + twidth // 2, ty + theight // 2
