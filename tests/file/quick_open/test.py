"""Quick Open filters by name, jumps to a typed ":N" line, and Escape opens nothing.

# requires: MOO_GTK3

src/plugins/mooquickopen.cpp. The dialog searches the active document's own
directory (_moo_find_project_root() with no markers), and quick_open_split_line()
peels a trailing ":N" off the query the way the command line does, before the
rest of the query is matched against file names -- so "numbered.txt:12" both
finds numbered.txt and remembers line 12 for quick_open_open_selected().

The file index is stale-while-revalidate (_moo_file_index_get): the first query
for a fresh root gets nothing back synchronously, and the matching row only
appears once the background listing lands, so the test waits for it rather than
assuming it is there right away. Nothing is pre-selected after that either --
only Up/Down/PageUp/PageDown touch the tree selection -- so Enter needs a Down
first, or it is a no-op.
"""

from lib.notebook import order

LINES = 20

CONTENT = "".join("line %02d\n" % n for n in range(1, LINES + 1))

# Every line is the same width, so the start of line n is arithmetic.
WIDTH = len("line 01\n")

TARGET = 12


def setup(s):
    s.open(s.write("workdir/current.txt", "anchor\n"))
    s.write("workdir/numbered.txt", CONTENT)


def run(t):
    # Escape closes the dialog and opens nothing.
    t.menu("File", "Quick Open...")
    dialog = t.dialog("Quick Open")
    t.escape()
    t.no_toplevel("Quick Open")
    t.check(order(t) == ["current.txt"],
            "Escape opened nothing; the strip is %s" % order(t))

    # A query naming a line opens the file there.
    t.menu("File", "Quick Open...")
    dialog = t.dialog("Quick Open")

    entry = t.need(dialog, role="text", what="the search entry")
    t.click(entry)
    t.key("ctrl+a")
    t.type_text("numbered.txt:%d" % TARGET)

    t.wait(lambda: t.find(dialog, role="table cell", name_prefix="numbered.txt") is not None,
           "numbered.txt to appear once the background listing lands")

    t.key("Down")
    t.key("Return")

    t.no_toplevel("Quick Open")
    t.wait(lambda: sorted(order(t)) == sorted(["current.txt", "numbered.txt"]),
           "numbered.txt to open; the strip is %s" % order(t))

    view = t.document()
    t.wait_caret(view, (TARGET - 1) * WIDTH,
                 "the cursor to land at the start of line %d" % TARGET)
