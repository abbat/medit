"""Renaming a file to a name already taken asks before it overwrites anything.

# requires: MOO_GTK3

The name in the Properties dialog is applied by rename_file() in
src/moofileview/moofileview-dialogs.cpp, which renames through rename(2): a
destination that exists is replaced without a word, so the file whose name was
typed used to be gone with nothing to undo it, and a rename that failed reached
a g_warning in a terminal nobody is reading.

Both answers are given here, because the file on disk is what says which
happened: Cancel leaves the two files as they were, and Replace puts the
renamed file's contents into the name that was taken.
"""

from app.file_selector_common import first_row_menu, open_pane, properties_dialog

OLD = "a.txt"
NEW = "b.txt"

RENAMED = "the file being renamed\n"
IN_THE_WAY = "the file already there\n"


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/" + OLD, RENAMED)
    s.write("workdir/" + NEW, IN_THE_WAY)
    s.open(s.write("workdir/notes.txt", "notes"))


def run(t):
    view = open_pane(t)

    question = rename_first_row(t, view, NEW)
    asked = text_of(t, question)
    t.check(NEW in asked, "the question names the file: %r" % asked)

    t.click(t.button(question, "Cancel"))
    t.wait(lambda: alert(t) is None, "the question to go")

    t.check(t.sandbox.read("workdir", OLD) == RENAMED,
            "Cancel to leave the file it was asked about alone")
    t.check(t.sandbox.read("workdir", NEW) == IN_THE_WAY,
            "and to leave the file that was in the way alone")

    question = rename_first_row(t, view, NEW)
    t.click(t.button(question, "Replace"))
    t.wait(lambda: alert(t) is None, "the question to go again")

    t.wait(lambda: t.sandbox.read("workdir", NEW) == RENAMED,
           "Replace to put the renamed file's contents into %s" % NEW)
    t.check(not t.sandbox.exists("workdir", OLD),
            "and %s to be gone, because it was renamed and not copied" % OLD)


def rename_first_row(t, view, name):
    """Type @name into the first row's Properties dialog, and wait for the question."""
    t.choose(first_row_menu(t, view), "Properties")
    dialog = t.wait(lambda: properties_dialog(t), "the properties dialog")

    entry = t.need(dialog, role="text", what="the name entry")

    # The dialog is not modal and there is no window manager, so nothing has
    # handed it the keys after the menu it came from went away.
    t.focus()
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(name)
    t.wait_text(entry, name, what="the new name to be typed into the entry")
    t.click(t.button(dialog, "OK"))

    return t.wait(lambda: alert(t), "the question about the name already taken")


def alert(t):
    """The question, a message dialog and so an alert rather than a dialog."""
    found = t.find_all(t.app, role="alert", depth=2)

    return found[0] if found else None


def text_of(t, dialog):
    return " ".join(label.name or "" for label in t.find_all(dialog, role="label"))
