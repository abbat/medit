"""Typing a name into the file selector's entry, rather than a path.

# requires: MOO_GTK3

The entry above the listing does two different things depending on what is typed
into it: a path is completed (tests/app/file_selector_entry), and a bare name is
a typeahead search through the folder -- ENTRY_STATE_TYPEAHEAD in
src/moofileview/moofileview.c, several hundred lines of matching, cycling and
temporary names that no test had reached.

Two files share the first letters, so the search has something to cycle through:
what is typed picks the first of them, Tab moves to the next, and Return opens
whichever the search is on. Which file opened is the whole assertion -- the
listing itself says nothing to the accessibility tree.
"""

from lib import input as ui
from lib.notebook import showing

FIRST = "aaa.txt"
SECOND = "aab.txt"
OTHER = "zzz.txt"


def setup(s):
    s.plugin("FileSelector")

    for name in (FIRST, SECOND, OTHER):
        s.write("workdir/" + name, "one of the files to search through\n")

    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    view = open_the_pane(t)
    entry = path_entry(t, view)

    # A bare name, which is a search rather than a path.
    t.focus()
    t.click(entry)
    t.key("ctrl+a")
    t.type_text("aa")

    t.key("Return")

    t.wait(lambda: showing(t) == FIRST,
           "the first file the search matched to open; %s is showing" % showing(t))

    # And again, with Tab, which walks the matches.
    t.focus()
    t.click(entry)
    t.key("ctrl+a")
    t.type_text("aa")

    # The first Tab fills the name of the match in, and the next moves on to
    # the one after it.
    t.key("Tab")
    t.wait(lambda: t.text(entry) == FIRST,
           "the first Tab to fill in the match; the entry holds %r" % t.text(entry))

    t.key("Tab")
    t.wait(lambda: t.text(entry) == SECOND,
           "and the next to move on; the entry holds %r" % t.text(entry))

    t.key("Return")

    t.wait(lambda: showing(t) == SECOND,
           "Tab to move the search on to the other match; %s is showing" % showing(t))


def path_entry(t, view):
    """The entry above the listing, which says the directory."""
    vx, vy, vwidth, vheight = t.extents(view)

    for node in t.find_all(t.frame, role="text", depth=30):
        if not ui.on_screen(node):
            continue

        x, y, width, height = t.extents(node)

        if abs(x - vx) <= 4 and y < vy:
            return node

    return t.fail("the pane has no entry for the directory it is looking at")


def open_the_pane(t):
    t.menu("View", "Panes", "File Selector")
    t.pin_pane()
    t.click(t.need(t.frame, role="push button", name="Jump to",
                   what="the button that goes to the document's directory"))

    return t.wait(lambda: the_icon_view(t), "the icon view of the file selector")


def the_icon_view(t):
    found = [n for n in t.find_all(t.frame, role="unknown", depth=30)
             if ui.on_screen(n)]

    return found[0] if len(found) == 1 else None
