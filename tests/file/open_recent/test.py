"""Open Recent lists what was opened, opens it again, and has a dialog behind it.

# requires: MOO_GTK3

MooHistoryMgr, src/mooutils/moohistorymgr.c, which keeps the list and builds both
of the widgets that show it -- the submenu under File and the "More..." dialog at
the foot of it -- and which no test had ever looked at. The editor adds a file to
it as the file is opened, so the list is right in the same session and this test
does not have to restart medit to see it.

Three things are asserted, one per way in: the submenu lists what was opened,
an entry in it opens that file again after the documents are closed, and the
dialog behind "More..." opens a file the same way from its own list.
"""

from lib.notebook import order

FIRST = "alpha.txt"
SECOND = "beta.txt"

RECENT = ("File", "Open Recent")
MORE = "More..."


def setup(s):
    s.write("workdir/" + SECOND, "the second one\n")
    s.open(s.write("workdir/" + FIRST, "the first one\n"))


def run(t):
    open_by_path(t, t.sandbox.path("workdir", SECOND))
    t.wait(lambda: sorted(order(t)) == sorted([FIRST, SECOND]),
           "both files to be open; the strip is %s" % order(t))

    listed = recent(t)
    t.check(sorted(name for name in listed if name != MORE) == sorted([FIRST, SECOND]),
            "both files are in the recent list: %s" % ", ".join(listed))
    t.check(MORE in listed, "and the dialog is offered at the foot of it")

    # Close All leaves a window with one empty untitled document rather than an
    # empty window, and opening a file into that reuses it.
    t.menu("File", "Close All")
    t.wait(lambda: order(t) == ["Untitled"],
           "the documents to be closed; the strip is %s" % order(t))

    # An entry of the submenu opens its file again.
    t.menu(*RECENT, FIRST)
    t.wait(lambda: order(t) == [FIRST],
           "the recent entry to open its file; the strip is %s" % order(t))

    # And the dialog behind "More...", which is the other widget the same list
    # is drawn into: it has no title, so it is found as the only dialog there is.
    t.menu(*RECENT, MORE)
    dialog = t.wait(lambda: t.find(t.app, role="dialog", depth=2),
                    "the recent files dialog to open")

    row = t.need(dialog, role="table cell", name_prefix=SECOND,
                 what="the %s row of the recent files dialog" % SECOND)
    t.click(row)
    t.click(t.button(dialog, "Open"))

    t.wait(lambda: sorted(order(t)) == sorted([FIRST, SECOND]),
           "the dialog to open the file it was pointed at; the strip is %s" % order(t))


def recent(t):
    """The names in the Open Recent submenu, the "More..." item included."""
    menu = t.menu(*RECENT)

    names = [item.name for item in t.on_screen(t.find_all(menu, depth=1)) if item.name]

    # One Escape per level opened: the submenu takes the first and the menu bar
    # the next, and an Escape with nothing open is nothing.
    t.escape()
    t.escape()

    return names


def open_by_path(t, path):
    """Open the File/Open chooser and give it a path through its location entry.

    Ctrl+L is what opens that entry, and Enter is the chooser's default response:
    the dialog is taller than the screen the tests run on, so its buttons are
    below the bottom edge and cannot be clicked.
    """
    t.menu("File", "Open...")
    t.need(t.app, role="file chooser", depth=2, what="the Open dialog")

    t.focus()
    t.key("ctrl+l")
    t.type_text(path)
    t.key("Return")

    t.wait(lambda: t.find(t.app, role="file chooser", depth=2) is None,
           "the Open dialog to close")
