"""The Find dialog remembers what was searched for, and offers it again.

# requires: MOO_GTK3

The entry of the Find dialog is a MooHistoryCombo -- src/mooutils/moocombo.c and
src/mooutils/moohistorycombo.c -- which keeps what has been typed into it and
offers what matches as a popup list under the entry. Every test so far has
treated that popup as something to get out of the way; this one uses it, which is
the only way any of moo_combo_popup(), its key handling or its selection ever
runs.

Two searches to fill the history, then a prefix that matches one of them: Down
walks into the list and Return takes what is highlighted, so the entry holds the
whole word without it having been typed. What the search then finds is the proof
that the entry really held it -- the word is in the document once, and nowhere
near where the cursor was.
"""

import time

CONTENT = "one\nalpha\ntwo\nbeta\nthree\n"

#  one 0-2, \n 3 | alpha 4-8, \n 9 | two 10-12, \n 13 | beta 14-17, \n 18
ALPHA = (4, 9)
BETA = (14, 18)


def setup(s):
    s.open(s.write("workdir/words.txt", CONTENT))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)

    # Two searches, so that the history has something in it and something else.
    search(t, "beta")
    t.wait_selection(view, BETA, "the first search found beta")

    search(t, "alpha")
    t.wait_selection(view, ALPHA, "and the second found alpha")

    home(t, view)

    # And now the history: half a word, and the list is walked rather than typed.
    t.menu("Search", "Find")
    dialog = t.dialog("Find")

    entry = t.need(dialog, role="text", what="the entry to type the search into")
    t.click(entry)
    t.key("ctrl+a")
    t.type_text("be")

    t.key("Down")
    t.key("Return")

    t.wait(lambda: t.text(entry) == "beta",
           "the list to fill the entry in; it holds %r" % t.text(entry))

    accept(t, dialog)

    t.wait_selection(view, BETA,
                     "and the search that ran is the one the list offered")


def home(t, view):
    t.focus()
    t.key("ctrl+Home")
    t.wait_caret(view, 0, "the cursor is back at the start")


def search(t, term):
    """Open Find, type the term, and accept."""
    t.menu("Search", "Find")
    dialog = t.dialog("Find")

    entry = t.need(dialog, role="text", what="the entry to type the search into")
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(term)

    accept(t, dialog)


def accept(t, dialog):
    """Press Find until the dialog goes.

    The completion list takes a pointer grab while it is up, and the click that
    dismisses it reaches nothing else -- so with a popup up the first click is
    spent on it and the second presses the button.
    """
    for _ in range(3):
        t.click(t.button(dialog, "Find"))

        if gone(t):
            return

    t.fail("the Find dialog did not close")


def gone(t, timeout=3.0):
    deadline = time.time() + timeout

    while time.time() < deadline:
        if t.find(t.app, role="dialog", name="Find", depth=2) is None:
            return True

        time.sleep(0.2)

    return False
