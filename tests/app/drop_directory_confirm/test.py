"""Dropping a directory with too many files onto the notebook asks first.

# requires: MOO_GTK3

notebook_drop_uri_list() opens every text file under a dropped directory, but
a folder can hold far more than anyone means to open at once, so past
MOO_NOTEBOOK_DROP_CONFIRM_THRESHOLD files it asks before doing that --
moo_question_dialog(), the same OK/Cancel confirmation idiom
file_selector_delete already exercises for its own question. Both answers are
given for the same reason that test gives both to its own dialog: Cancel has
to open nothing, and OK has to open everything.

The scan that decides whether to ask stops as soon as it has passed the
threshold, so the dialog cannot name the exact count -- only the threshold
itself, which is what the text is checked for here.
"""

from app.file_selector_common import FIRST_ROW, INTO_ROW, go, open_pane, select_row
from lib.notebook import drawn, order

FOLDER = "folder"
COUNT = 25
NAME = "file-%02d.txt"
THRESHOLD = 20


def setup(s):
    s.plugin("FileSelector")

    for i in range(COUNT):
        s.write("workdir/%s/%s" % (FOLDER, NAME % i), "x")

    s.open(s.write("open.txt", "keep a tab open to drop onto\n"))


def run(t):
    view = open_pane(t)
    go(t, view, t.sandbox.path("workdir"))

    before = order(t)

    # Cancel: the question is asked, and answered no.
    drop_folder(t, view)
    dialog = t.wait(lambda: the_question(t), "the confirmation dialog")

    asked = text_of(t, dialog)
    t.check(str(THRESHOLD) in asked, "the dialog names the threshold: %r" % asked)

    t.click(t.button(dialog, "Cancel"))
    t.wait(lambda: the_question(t) is None, "the dialog to go")

    t.check(order(t) == before, "Cancel opened nothing from the folder")

    # And again, answered yes: every file in the folder opens.
    drop_folder(t, view)
    dialog = t.wait(lambda: the_question(t), "the confirmation dialog again")
    t.click(t.button(dialog, "OK"))
    t.wait(lambda: the_question(t) is None, "the dialog to go")

    t.wait(lambda: len(order(t)) == len(before) + COUNT,
           "all %d files to open" % COUNT)


def drop_folder(t, view):
    x, y, width, height = t.extents(view)
    select_row(t, view)
    t.drag_to(x + INTO_ROW, y + FIRST_ROW, *tab_target(t))


def tab_target(t):
    """A point on the open tab: on screen, inside the notebook's drop target."""
    tab = drawn(t)[0]
    tx, ty, twidth, theight = t.extents(tab)
    return tx + twidth // 2, ty + theight // 2


def the_question(t):
    found = t.find_all(t.app, role="alert", depth=2)
    return found[0] if found else None


def text_of(t, dialog):
    return " ".join(label.name or "" for label in t.find_all(dialog, role="label"))
