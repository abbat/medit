"""The recent files dialog says what it is, and offers to forget what it lists.

# requires: MOO_GTK3

The dialog behind File / Open Recent / "More..." came up with an empty title --
a window called "medit" by the window manager and nothing by anything else --
and the list it shows was the one thing it could not do anything about but open.
It now carries a title, and a button beside Cancel and Open empties the list:
src/mooutils/moohistorymgr.cpp builds both.

The button is not one of the dialog's responses, so the two things worth asking
after a click are that the list is gone -- the rows with it, and the saved copy
under the cache, so that the files do not come back on the next run -- and that
the dialog is still up, with the button insensitive for want of anything left
to forget.
"""

FIRST = "alpha.txt"
SECOND = "beta.txt"

RECENT = ("File", "Open Recent")
MORE = "More..."
TITLE = "Recent Files"
CLEAR = "Clear History"

# Where MooHistoryMgr keeps the list between runs, under the sandbox's
# XDG_CACHE_HOME: "Editor" is the name the editor's manager is created with.
SAVED = ("cache", "medit", "recent-files-editor.xml")


def setup(s):
    s.open(s.write("workdir/" + FIRST, "the first one\n"))
    s.open(s.write("workdir/" + SECOND, "the second one\n"))


def run(t):
    t.menu(*RECENT, MORE)

    # By its title, which is the whole of the first assertion: an untitled
    # dialog is not found by name at all.
    dialog = t.dialog(TITLE)

    rows = t.wait(lambda: listed(t, dialog) or None,
                  "the dialog to list the files that were opened")

    t.check(sorted(rows) == sorted([FIRST, SECOND]),
            "both files are listed: %s" % ", ".join(rows))

    clear = t.button(dialog, CLEAR)

    t.check(t.state(clear, "sensitive"),
            "and the button that forgets them is there to be pressed")

    t.wait(lambda: t.sandbox.exists(*SAVED),
           "the list to be written out, so that the clearing has something to "
           "take away")

    t.click(clear)

    t.wait(lambda: not listed(t, dialog),
           "the rows to go with the list they were showing")
    t.wait(lambda: not t.sandbox.exists(*SAVED),
           "and the saved copy to go with it, so that the files do not come "
           "back on the next run")

    t.check(t.find(t.app, role="dialog", name=TITLE, depth=2) is not None,
            "the dialog stays up: clearing the list is not an answer to it")
    t.check(not t.state(clear, "sensitive"),
            "and the button goes insensitive with nothing left to forget")

    t.log("ok: the dialog forgot the files it was listing")

    t.click(t.button(dialog, "Cancel"))
    t.wait(lambda: t.find(t.app, role="dialog", name=TITLE, depth=2) is None,
           "the dialog to close")


def listed(t, dialog):
    """The file names the dialog is showing.

    A row of the tree view is several cells -- the icon has one of its own, and
    it has no name -- so it is the named ones that are the files, and the name
    of one is its basename with the path under it.
    """
    return [cell.name.split("\n")[0]
            for cell in t.find_all(dialog, role="table cell") if cell.name]
