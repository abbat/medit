"""Undo and Redo walk the document back and forward, and say when they cannot.

# requires: MOO_GTK3

The Edit menu's Undo and Redo are MooWindow's, not the document's: they hand the
work to whichever widget currently answers for editing operations -- the
MooEditOps interface, moowindow.c -- and their sensitivity is that widget's
answer to can-undo and can-redo. So the two items are a reading of the undo stack
of the document in front, and that is what is asserted here: what the text is
after each step, and whether the item that did it is still available.

Typing is one entry in that stack per run of characters rather than per
character, which is what makes a single Undo take a whole word away; the test
pins that too, since it is the difference between medit's undo and a text
widget's.
"""

NAME = "notes.txt"

CONTENT = "alpha\nbeta\n"

WORD = "gamma"

TYPED = "gamma" + CONTENT

UNDO = ("Edit", "Undo")
REDO = ("Edit", "Redo")


def setup(s):
    s.open(s.write("workdir/" + NAME, CONTENT))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)
    t.key("ctrl+Home")

    t.check(not sensitive(t, *UNDO), "nothing to undo in a document just opened")
    t.check(not sensitive(t, *REDO), "and nothing to redo either")

    t.focus()
    t.type_text(WORD)
    t.wait(lambda: t.text(view) == TYPED, "the word to be typed")

    t.check(sensitive(t, *UNDO), "typing gives Undo something to do")
    t.check(not sensitive(t, *REDO), "and Redo still nothing")

    t.menu(*UNDO)
    t.wait(lambda: t.text(view) == CONTENT,
           "one Undo to take the whole word away, not one character of it")

    t.check(not sensitive(t, *UNDO), "and to empty the stack again")
    t.check(sensitive(t, *REDO), "leaving the word with Redo")

    t.menu(*REDO)
    t.wait(lambda: t.text(view) == TYPED, "Redo to put the word back")

    t.check(not sensitive(t, *REDO), "with nothing left to redo")

    # A modified document asks about itself when the runner quits medit.
    t.focus()
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")


def sensitive(t, *path):
    """Whether the item at that menu path can be activated.

    One opening of the menu per call, and the answer read before it is closed: a
    click on a menu that is already open closes it, so a caller that asks twice
    in one statement reads an empty menu the second time.
    """
    menu = t.menu(*path[:-1])
    item = t.need(menu, role="menu item", name=path[-1],
                  what="the %r item" % path[-1])
    answer = t.state(item, "sensitive")
    t.escape()

    return answer
