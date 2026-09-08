"""File/New makes a document with no file, and saving one asks where to put it.

# requires: MOO_GTK3

A document with no file behind it is a state the rest of medit has to keep
working in, and the moment it matters is the first save: moo_edit_save() has
nowhere to write, so it goes through _moo_edit_save_as_dialog() and asks. If that
were skipped the first Ctrl+S on a new document would either do nothing or write
somewhere nobody chose.

Cancelling that dialog is the case worth pinning as much as taking it: the
document has to still be there, still hold what was typed, and still count as
unsaved -- a save that half-happened is how a new file goes missing.
"""

from lib.notebook import order, showing

OPENED = "notes.txt"

UNTITLED = "Untitled"

TYPED = "a new document"


def setup(s):
    s.open(s.write("workdir/" + OPENED, "notes\n"))


def run(t):
    t.check(order(t) == [OPENED], "one document is open to start with")

    t.menu("File", "New")

    t.wait(lambda: len(order(t)) == 2, "a second page to appear for the new document")
    t.check(UNTITLED in order(t),
            "the new document is on the strip as %r: %s" % (UNTITLED, order(t)))
    t.check(showing(t) == UNTITLED, "and it is the one showing")

    view = t.document()
    t.check(t.text(view) == "", "the new document is empty")

    t.focus()
    t.click(view)
    t.type_text(TYPED)
    t.wait(lambda: t.text(t.document()) == TYPED,
           "the typing to reach it; it holds %r" % t.text(t.document()))

    t.wait(lambda: "*" + UNTITLED in order(t),
           "the page to be marked as having unsaved changes: %s" % order(t))
    t.log("ok: an unsaved new document is marked on the strip")

    # Nowhere to write, so Save has to ask.
    t.menu("File", "Save")

    dialog = t.need(t.app, role="file chooser", depth=2,
                    what="the chooser Save opens for a document with no file")

    # Escape rather than the Cancel button: the chooser is taller than the screen
    # the tests run on, so its buttons are below the bottom edge and a click on
    # one lands outside the window.
    t.check(t.button(dialog, "Cancel") is not None, "the chooser offers Cancel")
    t.escape()
    t.wait(lambda: t.find(t.app, role="file chooser", depth=2) is None,
           "the chooser to close after Escape")

    t.check(t.text(t.document()) == TYPED,
            "cancelling the save left the document with what was typed in it")
    t.check("*" + UNTITLED in order(t),
            "and it is still on the strip, still unsaved: %s" % order(t))

    # Thrown away deliberately, so the quit at the end has nothing to ask about.
    t.key("ctrl+w")
    t.click(t.button(t.need(t.app, role="alert", depth=2,
                            what="the dialog asking about the unsaved document"),
                     "Discard"))
    t.wait(lambda: order(t) == [OPENED], "the new document to be discarded")
