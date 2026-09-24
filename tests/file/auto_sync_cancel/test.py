"""With auto_sync on, declining the dialog still marks the tab.

# requires: MOO_GTK3

file_modified_on_disk() and file_deleted() in mooedit-fileops.cpp reuse the very
same moo_edit_reload()/moo_edit_close() the File menu uses, so with Editor/auto_sync
on and the document modified, the same "discard changes?"/"save changes?" dialog
comes up on its own -- there is no menu click to look for it after, since the file
watch is what triggered it.

Before this fix, declining that dialog left the tab with no mark at all: the flags
that drive the "!" prefix and the title token were only ever set on the non-auto_sync
branch, so a cancelled auto-sync reload or close was invisible even though the file
and the buffer had diverged, or the file was gone and the watch had died with it.

Two documents, two triggers: one is changed on disk while modified, the other is
deleted while modified. Cancel on each dialog has to leave the buffer alone -- exactly
as without auto_sync, see file/reload and file/close_changes -- and now also has to
mark the tab, since nothing else will.
"""

import os
import time

from lib.notebook import order

CHANGED = "changed.txt"
DELETED = "deleted.txt"

BODIES = {CHANGED: "changed\n", DELETED: "deleted\n"}

ON_DISK = "as it is on disk now\n"

TYPED = "typed "


def setup(s):
    for name in (CHANGED, DELETED):
        s.open(s.write("workdir/" + name, BODIES[name]))


def run(t):
    # DELETED was opened last and is showing; type into it, then switch and do
    # the same to CHANGED, so both are modified before either file moves.
    type_into(t, DELETED)
    t.menu("Window", "Previous Tab")
    type_into(t, CHANGED)

    # auto_sync is off until here, so neither of the two edits above armed its
    # 500ms save timer -- that timer otherwise beats the file watch's own half-
    # second poll to the punch, saves the buffer, and clears "modified" before
    # the dialog below ever gets to see it as changed on disk.
    dialog = t.preferences("File")
    tick(t, dialog, "Automatically sync files with disk", True)
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    # Changed on disk while modified: the reload dialog comes up on its own.
    t.sandbox.write("workdir/" + CHANGED, ON_DISK)

    # medit looks every half second and compares modification times, which are
    # whole seconds here: a file written within the same second as the last look
    # has the same time and the change is missed -- see file/overwrite_modified.
    # So the time is pushed forward, and the test waits for a look to have happened.
    path = t.sandbox.path("workdir", CHANGED)
    later = time.time() + 2
    os.utime(path, (later, later))
    t.settle(2)

    t.click(t.button(ask(t, CHANGED), "Cancel"))

    # Both flags are set at once here -- modified on disk, and still modified in
    # the buffer from the typing above -- so the tab shows both marks: "!*name".
    t.wait(lambda: "!*" + CHANGED in order(t),
           "Cancel to mark the tab; the strip holds %s" % order(t))
    t.check(t.text(t.document()) == TYPED + BODIES[CHANGED],
            "and the buffer still holds what was typed, not what is on disk")
    t.check("[modified on disk] [modified]" in (t.frame.name or ""),
            "and the title says both: %r" % t.frame.name)

    # Deleted while modified: the save-changes dialog comes up on its own.
    t.menu("Window", "Previous Tab")
    os.remove(t.sandbox.path("workdir", DELETED))
    t.click(t.button(ask(t, DELETED), "Cancel"))

    t.wait(lambda: "!*" + DELETED in order(t),
           "Cancel to mark the tab; the strip holds %s" % order(t))
    t.check(t.text(t.document()) == TYPED + BODIES[DELETED],
            "and the buffer still holds what was typed -- it is the only copy now")
    t.check("[deleted] [modified]" in (t.frame.name or ""),
            "and the title says both: %r" % t.frame.name)
    t.log("ok: cancelling an auto_sync reload or close still marks the tab")

    # Leave nothing modified behind: the harness quits through the menu after
    # run() returns and does not handle a save-changes dialog of its own.
    t.menu("File", "Close")
    t.click(t.button(ask(t, DELETED), "Discard"))
    t.wait(lambda: t.document() is not None, "the other document to still be there")

    t.menu("File", "Close")
    t.click(t.button(ask(t, CHANGED), "Discard"))


def type_into(t, name):
    view = t.document()

    t.check(t.text(view) == BODIES[name], "%s is the document on screen" % name)

    t.focus()
    t.click(view)
    t.key("ctrl+Home")
    t.type_text(TYPED)

    t.wait(lambda: t.text(t.document()) == TYPED + BODIES[name],
           "the typing to reach %s" % name)


def tick(t, dialog, name, on):
    box = t.need(dialog, role="check box", name=name, what="the %r check box" % name)

    if t.state(box, "checked") != on:
        t.click(box)
        t.wait(lambda: t.state(box, "checked") == on,
               "the %r box to be %s" % (name, "ticked" if on else "clear"))


def ask(t, name):
    """The dialog that asks about the changes, and it has to name the document."""
    dialog = t.need(t.app, role="alert", depth=2,
                    what="the dialog auto_sync brings up on its own")

    said = " ".join(label.name or "" for label in t.find_all(dialog, role="label"))

    t.check(name in said, "the dialog says which document it is about: %r" % said)

    return dialog
