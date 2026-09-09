"""Bookmarks are set on a line and walked to, from the Document menu.

# requires: MOO_GTK3

action_toggle_bookmark(), action_next_bookmark(), action_prev_bookmark() and
populate_bookmark_menu(). Where the cursor ends up is the whole of what a
bookmark does, so that is what is asserted -- the mark drawn in the margin has no
accessible of its own, and a test of the margin would be a test of the theme.

Walked through the menu rather than by the accelerators, because the menu is what
was broken: the list of bookmarks used to be rebuilt from the Document item's
::select signal, which runs after GTK has sized and placed the submenu, so the
first opening after a bookmark had come or gone drew every item one item's height
away from where the accessibility tree said it was, and a click meant for "Next
Bookmark" landed on its neighbour. So the order here matters -- each walk is the
first opening of the menu after the list changed, which is exactly that case, and
inserting a look at the menu in front of one of them takes the teeth out of it.

Two decisions are pinned besides the obvious one. The walk is strictly one way:
next looks at the lines after the cursor and previous at the lines before it, and
neither wraps round when there is nothing there -- so the cursor stays where it is
rather than jumping to the other end. And toggling is a toggle: setting one on a
line that has one takes it away, and the walk then skips that line.
"""

LINES = 10

CONTENT = "".join("line %02d\n" % n for n in range(1, LINES + 1))

WIDTH = len("line 01\n")

MARKED = (3, 7)

MENU = ("Document",)

TOGGLE = "ctrl+b"

TOGGLE_ITEM = "Toggle Bookmark"
NEXT = "Next Bookmark"
PREVIOUS = "Previous Bookmark"


def setup(s):
    s.open(s.write("workdir/numbered.txt", CONTENT))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)

    # Set from the keyboard, so that the list has changed with the menu shut and
    # the walk below is the first opening that has to know about it.
    for line in MARKED:
        go_to(t, view, line)
        t.key(TOGGLE)

    go_to(t, view, 1)

    walk(t, view, NEXT, MARKED[0], "the first bookmark after the cursor")
    walk(t, view, NEXT, MARKED[1], "the second one")
    walk(t, view, NEXT, MARKED[1], "and nowhere from the last: the walk does not wrap")

    walk(t, view, PREVIOUS, MARKED[0], "back to the first")
    walk(t, view, PREVIOUS, MARKED[0], "and nowhere from the first, for the same reason")

    # Read in one opening, and read once: t.menu() clicks its way in, and a click
    # on a menu that is already open closes it again.
    listed = bookmarks(t)
    t.check(listed == ['3 - "line 03"', '7 - "line 07"'],
            "the menu lists both bookmarks, by line and by text: %s" % listed)
    t.escape()

    # Toggled off, from the menu this time. The cursor is on the line, so the item
    # is all it takes.
    t.menu(*MENU, TOGGLE_ITEM)

    go_to(t, view, 1)

    walk(t, view, NEXT, MARKED[1],
         "the second bookmark, the first having been toggled away")

    listed = bookmarks(t)
    t.check(listed == ['7 - "line 07"'],
            "and the toggled-off one is gone from the menu: %s" % listed)
    t.escape()


def bookmarks(t):
    """The bookmarks the Document menu lists, in the order it lists them."""
    menu = t.menu(*MENU)

    return [item.name for item in t.on_screen(t.find_all(menu, depth=1))
            if item.name and ' - "' in item.name]


def go_to(t, view, line):
    """Put the cursor at the start of that line, by keys."""
    t.focus()
    t.key("ctrl+Home")

    for _ in range(line - 1):
        t.key("Down")

    t.wait_caret(view, (line - 1) * WIDTH, "the cursor is on line %d" % line)


def walk(t, view, entry, line, what):
    t.menu(*MENU, entry)
    t.wait_caret(view, (line - 1) * WIDTH, "%s: line %d" % (what, line))
