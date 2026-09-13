"""Exercise the file selector's folder-first and case-sensitive sorting.

# requires: MOO_GTK3
"""

from app.file_selector_common import first_row_menu, open_pane, properties


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/a.txt", "a")
    s.write("workdir/B.txt", "b")
    s.write("workdir/dir/item.txt", "dir")
    s.open(s.write("workdir/notes.txt", "notes"))


def run(t):
    view = open_pane(t)

    # It is enabled by default. Turning it off exercises the other comparator
    # and makes the case-sensitive switch observable on the two files.
    menu = first_row_menu(t, view)
    t.choose(menu, "View", "Show Folders First")
    t.settle(1)
    t.check(properties(t, view) in ("a.txt", "B.txt"),
            "turning folders-first off puts files before the directory")

    menu = first_row_menu(t, view)
    t.choose(menu, "View", "Case Sensitive Sort")
    t.settle(1)
    first = properties(t, view)
    t.check(first == "B.txt",
            "case-sensitive sorting puts the upper-case name first: %s" % first)
