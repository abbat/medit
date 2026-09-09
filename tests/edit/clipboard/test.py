"""Cut, Copy, Paste and Delete, and what tells the last two apart.

# requires: MOO_GTK3

The Edit menu's clipboard items are MooWindow's, dispatched through the
MooEditOps interface to whichever widget answers for editing at the time, and
their sensitivity is that widget's answer -- can-cut and can-copy are a selection,
can-select-all is any text at all. That is half of what is asserted; the other
half is the text after each step.

Delete is the interesting one. It looks like Cut and is not: it takes the
selection away without putting it on the clipboard, so a Paste after a Delete
brings back what was cut before it. That is what the last step here checks, and
it is the kind of thing a test of "the text got shorter" would miss.
"""

NAME = "notes.txt"

FIRST = "alpha\n"
SECOND = "beta\n"

CONTENT = FIRST + SECOND

SELECT_ALL = ("Edit", "Select All")
CUT = ("Edit", "Cut")
COPY = ("Edit", "Copy")
PASTE = ("Edit", "Paste")
DELETE = ("Edit", "Delete")


def setup(s):
    s.open(s.write("workdir/" + NAME, CONTENT))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)
    t.key("ctrl+Home")

    t.check(not sensitive(t, *CUT), "nothing to cut with no selection")
    t.check(not sensitive(t, *COPY), "and nothing to copy")
    t.check(sensitive(t, *SELECT_ALL), "but something to select, the document not being empty")

    t.menu(*SELECT_ALL)
    t.wait(lambda: t.selection(view) == (0, len(CONTENT)),
           "Select All to take the whole document")

    t.check(sensitive(t, *CUT), "a selection is something to cut")
    t.check(sensitive(t, *COPY), "and to copy")

    t.menu(*COPY)

    t.focus()
    t.key("ctrl+End")
    t.menu(*PASTE)
    t.wait(lambda: t.text(view) == CONTENT + CONTENT,
           "Paste to put the copy at the end")

    # Cut the first line, which is the copy's other half: what it took is what
    # the next Paste has to bring back.
    select_first_line(t, view)
    t.menu(*CUT)
    t.wait(lambda: t.text(view) == SECOND + CONTENT,
           "Cut to take the first line out of the document")

    # And delete the line that is now first, which must leave the clipboard
    # holding what Cut put there.
    select_first_line(t, view)
    t.menu(*DELETE)
    t.wait(lambda: t.text(view) == CONTENT, "Delete to take the first line out too")

    t.focus()
    t.key("ctrl+End")
    t.menu(*PASTE)
    t.wait(lambda: t.text(view) == CONTENT + FIRST,
           "Paste to bring back what Cut took, not what Delete took")

    # A modified document asks about itself when the runner quits medit.
    t.focus()
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")


def select_first_line(t, view):
    t.focus()
    t.key("ctrl+Home")
    t.key("shift+Down")
    t.wait_selection(view, (0, len(t.text(view).split("\n")[0]) + 1),
                     "the first line to be selected")


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
