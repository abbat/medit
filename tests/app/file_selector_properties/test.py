"""Open Properties for both a regular file and a directory.

# requires: MOO_GTK3
"""

from app.file_selector_common import first_row_menu, open_pane, properties


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/a-file", "contents")
    s.write("workdir/a-folder/inside", "contents")
    s.open(s.write("workdir/notes.txt", "notes"))


def run(t):
    view = open_pane(t)
    t.check(properties(t, view) == "a-folder",
            "Properties identifies the directory")
    t.choose(first_row_menu(t, view), "Open")
    t.wait(lambda: properties(t, view) == "inside",
           "the folder contents to be shown")
