"""Pasting a file over one of the same name asks before it does anything.

# requires: MOO_GTK3

Copying, moving and dropping all end up in run_command_on_files() in
src/moofileview/moofileview.cpp, which used to hand the files to cp, mv or ln
and walk away: a name already taken in the destination was overwritten with no
question, and a command that failed said nothing. The question is now asked
once for every file whose name is taken, and it offers to replace what is
there, to skip the file, or to put it there under the name in its entry.

Both answers that do something are given here, because they are the point: a
rename leaves the file in the way alone and writes the copy beside it, and a
replace puts the copied contents into the file that was there. What is on disk
says which happened.

Pasting is the shortest way to that dialog -- drag and drop reaches the same
function, and the file selector's own drag and drop is tested in
file_selector_drag_drop.
"""

from app.file_selector_common import first_row_menu, go, open_pane

NAME = "a.txt"
COPY = "a (copy).txt"

NEW = "the file that is copied\n"
OLD = "the file already in the destination\n"


def setup(s):
    s.plugin("FileSelector")
    s.write("source/" + NAME, NEW)
    s.write("destination/" + NAME, OLD)
    s.open(s.write("source/notes.txt", "hello"))


def run(t):
    view = open_pane(t)

    go(t, view, t.sandbox.path("source"))
    t.choose(first_row_menu(t, view), "Copy")
    go(t, view, t.sandbox.path("destination"))

    # Rename: the suggested name is the one the entry comes up with.
    dialog = paste(t, view)
    asked = text_of(t, dialog)
    t.check(NAME in asked, "the question names the file: %r" % asked)

    t.click(t.button(dialog, "Rename"))
    t.wait(lambda: question(t) is None, "the question to go")

    t.wait(lambda: t.sandbox.read("destination", COPY) == NEW,
           "the copy to be written as %s" % COPY)
    t.check(t.sandbox.read("destination", NAME) == OLD,
            "and Rename to have left the file that was in the way alone")

    # Replace: the same paste again, answered the other way.
    dialog = paste(t, view)
    t.click(t.button(dialog, "Replace"))
    t.wait(lambda: question(t) is None, "the question to go again")

    t.wait(lambda: t.sandbox.read("destination", NAME) == NEW,
           "Replace to put the copied contents into %s" % NAME)


def paste(t, view):
    """Paste into the directory the view is looking at, and wait for the question."""
    x, y, width, height = t.extents(view)
    t.click_at(x + width - 10, y + height - 10)
    t.choose(t.popup(), "Paste")

    return t.wait(lambda: question(t), "the question about the name already taken")


def question(t):
    """The question, a message dialog and so an alert rather than a dialog."""
    found = t.find_all(t.app, role="alert", depth=2)

    return found[0] if found else None


def text_of(t, dialog):
    return " ".join(label.name or "" for label in t.find_all(dialog, role="label"))
