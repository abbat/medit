"""Deleting a file from the file selector, and changing one's mind about it.

# requires: MOO_GTK3

The delete entry of the listing's menu asks before it does anything --
ask_delete_files() in src/moofileview/moofileview.c, a message dialog with Cancel
and Delete on it -- and then takes the file away through medit's own file system
layer. Nothing had ever deleted a file from the pane, so neither the question nor
what follows it had run.

The entry reads "Move to Trash..." rather than "Delete...", and that is not a
detail of the test: update_delete_action() decides between the two by whether
Shift is down as the menu is built, and the trash is what a menu opened without
it offers. Either way the file leaves the folder, which is what is asserted.

Both answers are given, because the question is the point: Cancel has to leave
the file where it is, and Delete has to take it away. The file on disk says which
happened, and the listing is asked as well, by reading what is in the first row
afterwards.
"""

import os

from lib import input as ui

FIRST_ROW = 8
INTO_ROW = 20

DOOMED = "aaa.txt"
KEPT = "bbb.txt"

DELETE = "Move to Trash..."


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/" + DOOMED, "the file this test deletes\n")
    s.write("workdir/" + KEPT, "the file it leaves alone\n")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    view = open_the_pane(t)

    doomed = t.sandbox.path("workdir", DOOMED)

    # Cancel: the question is asked and answered no.
    ask(t, view)
    dialog = t.wait(lambda: the_question(t), "the question about the file")

    asked = text_of(t, dialog)
    t.check(DOOMED in asked, "the question names the file: %r" % asked)

    t.click(t.button(dialog, "Cancel"))
    t.wait(lambda: the_question(t) is None, "the question to go")

    t.check(os.path.exists(doomed), "Cancel left the file where it was")

    # And again, answered yes.
    ask(t, view)
    dialog = t.wait(lambda: the_question(t), "the question again")
    t.click(t.button(dialog, "Delete"))
    t.wait(lambda: the_question(t) is None, "the question to go")

    t.wait(lambda: not os.path.exists(doomed),
           "the file to leave the folder once the question is answered")

    # The listing followed: the first row is the file that was second.
    t.check(selected(t, view) == KEPT,
            "and the listing has lost it: the first row is %s now"
            % selected(t, view))


def ask(t, view):
    """Select the first row and pick Delete from its menu."""
    x, y, width, height = t.extents(view)
    t.click_at(x + INTO_ROW, y + FIRST_ROW)

    t.choose(t.popup(), DELETE)


def the_question(t):
    """The question, which is a message dialog and so an alert rather than a dialog.

    It has no title either, which is why what it says is read out of its labels.
    """
    found = t.find_all(t.app, role="alert", depth=2)

    return found[0] if found else None


def text_of(t, dialog):
    return " ".join(label.name or "" for label in t.find_all(dialog, role="label"))


def selected(t, view):
    """Which file the first row holds, as its properties dialog is titled."""
    x, y, width, height = t.extents(view)
    t.click_at(x + INTO_ROW, y + FIRST_ROW)

    t.choose(t.popup(), "Properties")

    dialog = t.wait(lambda: the_properties_dialog(t), "the properties dialog")
    name = dialog.name.rsplit(" Properties", 1)[0]

    t.click(t.button(dialog, "Cancel"))
    t.wait(lambda: the_properties_dialog(t) is None, "the dialog to close")

    return name


def the_properties_dialog(t):
    for node in t.find_all(t.app, role="dialog", depth=2):
        if node.name.endswith(" Properties"):
            return node

    return None


def open_the_pane(t):
    t.menu("View", "Panes", "File Selector")
    t.pin_pane()
    t.click(t.need(t.frame, role="push button", name="Jump to",
                   what="the button that goes to the document's directory"))

    return t.wait(lambda: the_icon_view(t), "the icon view of the file selector")


def the_icon_view(t):
    found = [n for n in t.find_all(t.frame, role="unknown", depth=30)
             if ui.on_screen(n)]

    return found[0] if len(found) == 1 else None
