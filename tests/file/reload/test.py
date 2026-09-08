"""Reload asks before throwing away changes, and then re-reads the file.

# requires: MOO_GTK3

_moo_edit_reload_modified_dialog(), and moo_edit_question_dialog() under it.
File/Reload on a document with unsaved changes is the other way medit can lose
what was typed, so it asks first -- Cancel and Reload, with Cancel the default --
and Cancel has to mean the document is left alone.

The file is changed on disk behind medit's back while the document is changed in
the buffer, which is the situation the dialog is really about: after Reload the
document has to hold what the file now holds, not what it held when it was
opened, and not what was typed into it.
"""

NAME = "notes.txt"

OPENED = "as it was opened\n"
ON_DISK = "as it is on disk now\n"

TYPED = "typed "


def setup(s):
    s.open(s.write("workdir/" + NAME, OPENED))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)
    t.key("ctrl+Home")
    t.type_text(TYPED)
    t.wait(lambda: t.text(t.document()) == TYPED + OPENED,
           "the typing to reach the document")

    # Behind medit's back, which is what makes the reload worth anything.
    t.sandbox.write("workdir/" + NAME, ON_DISK)

    # Cancel: the document keeps what was typed.
    t.menu("File", "Reload")
    t.click(t.button(ask(t, NAME), "Cancel"))

    t.check(t.text(t.document()) == TYPED + OPENED,
            "Cancel left the document with the change in it")
    t.check("[modified]" in (t.frame.name or ""), "and still counting as modified")

    # Reload: the document holds what is on disk now.
    t.menu("File", "Reload")
    t.click(t.button(ask(t, NAME), "Reload"))

    t.wait(lambda: t.text(t.document()) == ON_DISK,
           "the document to hold what the file holds now; it holds %r"
           % t.text(t.document()))
    t.log("ok: Reload read the file again and discarded what was typed")

    t.check("[modified]" not in (t.frame.name or ""),
            "and the document does not count as modified any more")


def ask(t, name):
    """The dialog that asks before discarding, which has to name the file."""
    dialog = t.need(t.app, role="alert", depth=2,
                    what="the dialog asking before the changes are discarded")

    said = " ".join(label.name or "" for label in t.find_all(dialog, role="label"))

    t.check(name in said, "the dialog says which file it is about: %r" % said)

    return dialog
