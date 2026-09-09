"""Saving over a file that changed on disk asks first.

# requires: MOO_GTK3

medit watches the files it has open -- moofilewatch under mooedit-fileops.cpp --
and a document whose file has been written by something else is in a state of its
own from then on. Saving such a document would throw away what is on disk, so
_moo_edit_overwrite_modified_dialog() asks, and the answer decides which of the
two versions survives.

This is the third of medit's data-loss paths, beside the save dialogs of
tests/file/close_changes and the reload dialog of tests/file/reload, and the only
one nothing had driven. Both answers are given: Cancel leaves what another
process wrote, Overwrite replaces it with what is in the buffer.
"""

import os
import time

NAME = "notes.txt"

OPENED = "as it was opened\n"

FROM_OUTSIDE = "written by something else\n"

# No line break at the end of it: what is typed is what the buffer holds, and
# medit does not add one unless it is asked to.
TYPED = "typed in medit"


def setup(s):
    s.open(s.write("workdir/" + NAME, OPENED))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)
    t.key("ctrl+a")
    t.type_text(TYPED)

    t.wait(lambda: t.text(view).startswith("typed"),
           "the document to hold what was typed; it holds %r" % t.text(view))

    # And now the file changes underneath it.
    t.sandbox.write("workdir/" + NAME, FROM_OUTSIDE)

    # medit looks every half second and compares modification times, which are
    # whole seconds here: a file written within the same second as the last look
    # has the same time and the change is missed -- measured, in CI, where the
    # test then saved without being asked anything. So the time is pushed
    # forward, and the test waits for a look to have happened.
    path = t.sandbox.path("workdir", NAME)
    later = time.time() + 2
    os.utime(path, (later, later))
    t.settle(2)

    # Cancelled: what the other process wrote is still there.
    ask(t)
    dialog = t.wait(lambda: the_question(t), "the question about overwriting")

    asked = " ".join(label.name or "" for label in t.find_all(dialog, role="label"))
    t.check(NAME in asked, "the question names the file: %r" % asked)

    t.click(t.button(dialog, "Cancel"))
    t.wait(lambda: the_question(t) is None, "the question to go")

    t.check(t.sandbox.read("workdir", NAME) == FROM_OUTSIDE,
            "Cancel left the file as the other process wrote it: %r"
            % t.sandbox.read("workdir", NAME))

    # Accepted: the buffer wins.
    ask(t)
    dialog = t.wait(lambda: the_question(t), "the question again")
    t.click(t.button(dialog, "Overwrite"))

    t.wait(lambda: t.sandbox.read("workdir", NAME) == TYPED,
           "Overwrite to put the buffer on disk; the file holds %r"
           % t.sandbox.read("workdir", NAME))


def ask(t):
    t.focus()
    t.key("ctrl+s")


def the_question(t):
    """The question, which is a message dialog and so an alert."""
    found = t.find_all(t.app, role="alert", depth=2)

    return found[0] if found else None
