"""Exercise hidden files and the explicit parent-folder entry.

# requires: MOO_GTK3
"""

from app.file_selector_common import first_row_menu, open_pane, properties


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/.hidden", "hidden")
    s.write("workdir/visible", "visible")
    s.open(s.write("workdir/notes.txt", "notes"))


def run(t):
    view = open_pane(t)
    t.check(properties(t, view) == "notes.txt",
            "hidden files are not shown by default")

    t.choose(first_row_menu(t, view), "View", "Show Hidden Files")
    t.settle(1)
    t.check(properties(t, view) == ".hidden",
            "Show Hidden Files reveals the dot file")

    t.choose(first_row_menu(t, view), "View", "Show Parent Folder")
    t.settle(1)
    t.check(properties(t, view) == "..",
            "Show Parent Folder adds the parent entry")
