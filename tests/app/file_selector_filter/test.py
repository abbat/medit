"""The file selector's filter box hides what does not match it.

# requires: MOO_GTK3

src/mooutils/moofiltermgr.c keeps the filters the pane offers, makes one out of
whatever is typed into the box at the foot of it, and remembers it in the
preferences; the Filter button beside it switches filtering on and off without
forgetting which filter it was. Nothing had typed into that box.

The listing cannot be read -- MooIconView draws its own rows and puts nothing in
the accessibility tree -- so what the filter did is read by opening the first row:
the folder holds one file the filter keeps and one it hides, and which of them
opens says which rows are there -- medit goes to the tab a file is already open
in, so what is asserted is which document ends up in front. The two names are
ordered so that the hidden one comes first: with no filter the first row is the
log, with "*.txt" it is the text file, and a filter that did nothing would bring
the log up both times.
"""

from lib import input as ui
from lib.notebook import order, showing

# A few pixels into the first row, and far enough in to be inside the icon of a
# short name. The same measurements as tests/app/file_selector_navigate.
FIRST_ROW = 12
INTO_ROW = 20

HIDDEN = "aaa.log"
KEPT = "bbb.txt"

FILTER = "*.txt"


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/" + HIDDEN, "a log file\n")
    s.write("workdir/" + KEPT, "a text file\n")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    view = open_the_pane(t)

    open_first_row(t, view)
    t.wait(lambda: showing(t) == HIDDEN,
           "the first row to be the log file with no filter set; "
           "the strip is %s" % order(t))

    set_filter(t, view, FILTER)

    open_first_row(t, view)
    t.wait(lambda: showing(t) == KEPT,
           "the first row to be the text file once the filter is set; "
           "the strip is %s" % order(t))

    # Switching the filter off brings the other file back to the top, which is
    # the button beside the box rather than the box itself.
    t.click(filter_button(t))

    open_first_row(t, view)
    t.wait(lambda: showing(t) == HIDDEN,
           "the log file to be the first row again with the filter switched off; "
           "the strip is %s" % order(t))


def open_first_row(t, view):
    x, y, width, height = t.extents(view)
    t.click_at(x + INTO_ROW, y + FIRST_ROW, times=2)


def set_filter(t, view, text):
    entry = filter_entry(t, view)

    t.focus()
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(text)
    t.key("Return")

    t.wait(lambda: t.text(entry) == text,
           "the filter to be typed in; the box holds %r" % t.text(entry))


def filter_button(t):
    return t.need(t.frame, role="toggle button", name="Filter",
                  what="the Filter button of the file selector")


def filter_entry(t, view):
    """The entry under the listing, which is the filter box."""
    vx, vy, vwidth, vheight = t.extents(view)

    for node in t.find_all(t.frame, role="text", depth=30):
        if not ui.on_screen(node):
            continue

        x, y, width, height = t.extents(node)

        if abs(x - vx) <= 80 and y > vy:
            return node

    t.fail("the pane has no filter box under its listing")


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
