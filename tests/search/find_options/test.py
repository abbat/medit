"""The Find dialog's options: whole words, regular expressions, from cursor.

# requires: MOO_GTK3

The boxes in the Options frame of the Find dialog are flags on the search, and
each one takes a different path through src/mooedit/mootextsearch.c -- the plain
scan, the word-boundary check around a hit, and the regular expression engine.
tests/search/find drives the dialog with the boxes at their defaults; this one
sets them, and asserts what each one changes about the answer.

The text is chosen so that a search that ignored the box would find something
else: "alpha" appears inside "alphabet", so whole words only is the difference
between the second match being on line 2 and being on line 3, and the regular
expression matches a word that appears nowhere literally.
"""

import time

CONTENT = "alpha\nalphabet\nbeta alpha\n"

#  alpha 0-4, \n 5 | alphabet 6-13, \n 14 | beta 15-18, alpha 20-24, \n 25
FIRST = (0, 5)
INSIDE_WORD = (6, 11)
LAST = (20, 25)
BETA = (15, 19)

OPTIONS = ("Whole words only", "Regular expression", "From cursor")


def setup(s):
    s.open(s.write("workdir/words.txt", CONTENT))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)

    # Plain, so that what the boxes change is visible against it: the second
    # match is the one inside "alphabet".
    search(t, "alpha")
    t.wait_selection(view, FIRST, "the first match is the word on line 1")

    t.menu("Search", "Find Next")
    t.wait_selection(view, INSIDE_WORD,
                     "and the next one is inside alphabet, nothing saying otherwise")

    # Whole words only: the match inside the longer word is skipped.
    home(t, view)
    search(t, "alpha", whole_words=True)
    t.wait_selection(view, FIRST, "the first whole word is still the one on line 1")

    t.menu("Search", "Find Next")
    t.wait_selection(view, LAST, "and the next whole word is the one on line 3")

    # A regular expression, matching a word that is nowhere in the text
    # literally.
    home(t, view)
    search(t, "b[a-z]+a", whole_words=False, regex=True)
    t.wait_selection(view, BETA, "the regular expression matched beta")


def home(t, view):
    t.focus()
    t.key("ctrl+Home")
    t.wait_caret(view, 0, "the cursor is back at the start")


def search(t, term, whole_words=False, regex=False):
    """Open Find, set the boxes this test steers, type the term and accept.

    Every box the test names is set each time rather than only when it changes:
    the dialog remembers what the last search used, so a box left alone is a box
    left where another step of the test put it.
    """
    t.menu("Search", "Find")

    dialog = t.dialog("Find")

    wanted = dict(zip(OPTIONS, (whole_words, regex, False)))

    for name, on in wanted.items():
        box = t.need(dialog, role="check box", name=name, what="the %r box" % name)

        if t.state(box, "checked") != on:
            t.click(box)
            t.wait(lambda b=box, o=on: t.state(b, "checked") == o,
                   "the %r box to be %s" % (name, "ticked" if on else "clear"))

    entry = t.need(dialog, role="text", what="the entry to type the search into")
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(term)

    # Clicked until the dialog goes, rather than once. The entry is a history
    # combo: typing a term that a previous search left in its history pops up the
    # completion list, which takes a pointer grab, and the click that dismisses it
    # is not delivered to anything else -- so with a popup up the first click is
    # spent on it and the second presses the button, and with no popup the first
    # one already did. Return has the same problem and no way to tell either.
    for _ in range(3):
        t.click(t.button(dialog, "Find"))

        if gone(t, "Find"):
            return

    t.fail("the Find dialog did not close")


def gone(t, name, timeout=3.0):
    """Whether the dialog is closed, waited for a little without failing."""
    deadline = time.time() + timeout

    while time.time() < deadline:
        if t.find(t.app, role="dialog", name=name, depth=2) is None:
            return True

        time.sleep(0.2)

    return False
