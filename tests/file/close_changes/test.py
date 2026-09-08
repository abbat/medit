"""Closing a changed document asks, and each of the three answers is obeyed.

# requires: MOO_GTK3

moo_save_changes_dialog() in moodialogs.c, reached through
_moo_edit_save_changes_dialog() when File/Close is used on a document with
unsaved changes. It offers Discard, Cancel and Save, and this is the one place in
medit where getting a button wrong loses work the user typed: Discard must not
write, Save must write, and Cancel must leave both the document and the file
exactly as they were.

All three are here, on real files, and each is checked against what is on disk
rather than against what the window says -- the title going back to unmodified
proves nothing about the bytes.
"""

FIRST = "one.txt"
SECOND = "two.txt"

BODIES = {FIRST: "one\n", SECOND: "two\n"}

TYPED = "x"


def setup(s):
    for name in (FIRST, SECOND):
        s.open(s.write("workdir/" + name, BODIES[name]))


def run(t):
    # The one opened last is showing; change it.
    change(t, SECOND)

    # Cancel: nothing happens to anything.
    t.menu("File", "Close")
    cancel = t.button(ask(t, SECOND), "Cancel")
    t.click(cancel)

    t.wait(lambda: t.document() is not None, "the document to still be there")
    t.check(t.text(t.document()) == TYPED + BODIES[SECOND],
            "Cancel left the document open with the change still in it")
    t.check("[modified]" in (t.frame.name or ""),
            "and still counting as modified")
    t.check(t.sandbox.read("workdir/" + SECOND) == BODIES[SECOND],
            "and wrote nothing to the file")

    # Discard: the document goes, the file does not change.
    t.menu("File", "Close")
    t.click(t.button(ask(t, SECOND), "Discard"))

    t.wait(lambda: t.text(t.document()) == BODIES[FIRST],
           "the other document to be the one showing after the close")
    t.check(t.sandbox.read("workdir/" + SECOND) == BODIES[SECOND],
            "Discard closed it and left the file as it was on disk")

    # Save: the document goes and the change is on disk.
    change(t, FIRST)

    t.menu("File", "Close")
    t.click(t.button(ask(t, FIRST), "Save"))

    t.wait(lambda: t.sandbox.read("workdir/" + FIRST) == TYPED + BODIES[FIRST],
           "Save to write the change before closing; the file holds %r"
           % t.sandbox.read("workdir/" + FIRST))
    t.log("ok: Save wrote the change and then closed the document")


def change(t, name):
    """Type into the document that is showing, and make sure it is that one."""
    view = t.document()

    t.check(t.text(view) == BODIES[name], "%s is the document on screen" % name)

    t.focus()
    t.click(view)
    t.key("ctrl+Home")
    t.type_text(TYPED)

    t.wait(lambda: t.text(t.document()) == TYPED + BODIES[name],
           "the typing to reach %s" % name)


def ask(t, name):
    """The dialog that asks about the changes, and it has to name the document.

    A message dialog has no title, so it is found by its role -- ATK calls one
    an alert -- rather than the way every other dialog in these tests is found.
    """
    dialog = t.need(t.app, role="alert", depth=2,
                    what="the dialog asking about the unsaved changes")

    said = " ".join(label.name or "" for label in t.find_all(dialog, role="label"))

    t.check(name in said,
            "the dialog says which document it is about: %r" % said)

    return dialog
