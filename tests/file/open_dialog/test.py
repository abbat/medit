"""File/Open opens the file, and opening one that is already open goes to its tab.

# requires: MOO_GTK3

_moo_edit_open_dialog() puts up the chooser; moo_editor_open_files() decides what
to do with what comes back, and moo_editor_get_doc_for_file() is the part worth a
test: a file that is already open must not be opened a second time. Two documents
over the same file would each have their own buffer, and whichever was saved last
would silently undo the other -- so this is a data-loss path as much as the save
dialogs are.

The path is typed into the chooser's location entry rather than picked out of its
list: what is being tested is what medit does with a file name, and clicking
through a file list is a test of GTK.
"""

from lib.notebook import order, showing

OPEN_ALREADY = "notes.txt"
NOT_OPEN = "other.txt"


def setup(s):
    s.write("workdir/" + NOT_OPEN, "the other one\n")
    s.open(s.write("workdir/" + OPEN_ALREADY, "notes\n"))


def run(t):
    t.check(order(t) == [OPEN_ALREADY], "one document is open to start with")

    open_by_path(t, t.sandbox.path("workdir", NOT_OPEN))

    t.wait(lambda: sorted(order(t)) == sorted([OPEN_ALREADY, NOT_OPEN]),
           "the chosen file to be opened; the strip is %s" % order(t))
    t.check(showing(t) == NOT_OPEN, "and the document that was opened is showing")
    t.log("ok: the chooser opened the file that was named in it")

    # The one that is already open: no second copy of it, and medit goes to it.
    open_by_path(t, t.sandbox.path("workdir", OPEN_ALREADY))

    t.wait(lambda: showing(t) == OPEN_ALREADY,
           "medit to go to the document that file is already open in")

    t.check(order(t) == [OPEN_ALREADY, NOT_OPEN],
            "and it opened no second copy of it: %s" % order(t))


def open_by_path(t, path):
    """Open the File/Open chooser and give it a path through its location entry.

    Ctrl+L is what opens that entry, and Enter is the chooser's default response
    -- clicked buttons are no use here, since the dialog is taller than the
    screen the tests run on and its buttons are below the bottom edge.
    """
    t.menu("File", "Open...")

    dialog = t.need(t.app, role="file chooser", depth=2, what="the Open dialog")

    t.focus()
    t.key("ctrl+l")
    t.type_text(path)
    t.key("Return")

    t.wait(lambda: t.find(t.app, role="file chooser", depth=2) is None,
           "the Open dialog to close")

    return dialog
