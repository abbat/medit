"""Open Recent offers to forget the files it lists.

# requires: MOO_GTK3

The recent files list used to be a one-way street: every file ever opened stayed
under File / Open Recent for good, and the only way to take one out of it was to
open it and delete it from the disk. The submenu now ends with an item that
empties the list -- src/mooedit/mooeditor.cpp puts it there and
moo_history_mgr_clear() in src/mooutils/moohistorymgr.cpp does the forgetting.

Being out of the menu is not the same as being forgotten, so the list going away
is asked about three times over: the submenu is bound to the list being empty and
goes insensitive with it, the file the list is kept in is unlinked rather than
left behind for the next run to read, and a file opened afterwards is remembered
again, so that what was cleared is the list and not the remembering.
"""

FIRST = "alpha.txt"
SECOND = "beta.txt"

# Opened only after the clearing, so that what it does to the list is an entry
# arriving rather than one of the two above being opened a second time.
THIRD = "gamma.txt"

RECENT = ("File", "Open Recent")
MORE = "More..."
CLEAR = "Clear History"

# Where MooHistoryMgr keeps the list between runs, under the sandbox's
# XDG_CACHE_HOME: "Editor" is the name the editor's manager is created with.
SAVED = ("cache", "medit", "recent-files-editor.xml")


def setup(s):
    s.open(s.write("workdir/" + FIRST, "the first one\n"))
    s.open(s.write("workdir/" + SECOND, "the second one\n"))
    s.write("workdir/" + THIRD, "the third one\n")


def run(t):
    listed = recent(t)

    t.check(sorted(name for name in listed if name not in (MORE, CLEAR))
            == sorted([FIRST, SECOND]),
            "both files opened are in the recent list: %s" % ", ".join(listed))
    t.check(listed[-2:] == [MORE, CLEAR],
            "and the clearing is offered at the foot of it, under the dialog: "
            "%s" % ", ".join(listed))

    t.wait(lambda: t.sandbox.exists(*SAVED),
           "the list to be written out, so that the clearing has something to "
           "take away")

    t.menu(*RECENT, CLEAR)
    t.no_menu()

    t.wait(lambda: not sensitive(t, *RECENT),
           "the submenu to go insensitive with nothing left to open")
    t.wait(lambda: not t.sandbox.exists(*SAVED),
           "and the saved copy to go with it, so that the files do not come "
           "back on the next run")

    t.log("ok: the menu forgot the files it was listing")

    # And the list goes on filling from there: a file opened now is remembered,
    # and is the only thing the submenu has to offer.
    open_by_path(t, t.sandbox.path("workdir", THIRD))

    t.wait(lambda: sensitive(t, *RECENT),
           "the submenu to come back with the file that followed the clearing")

    listed = recent(t)

    t.check([name for name in listed if name not in (MORE, CLEAR)] == [THIRD],
            "and to offer that file alone: %s" % ", ".join(listed))


def recent(t):
    """The names in the Open Recent submenu, the items at its foot included."""
    menu = t.menu(*RECENT)

    names = [item.name for item in t.on_screen(t.find_all(menu, depth=1)) if item.name]

    # One Escape per level opened: the submenu takes the first and the menu bar
    # the next, and an Escape with nothing open is nothing.
    t.escape()
    t.escape()

    return names


def sensitive(t, *path):
    """Whether the submenu at that menu path can be opened at all."""
    item = t.menu(*path[:-1])
    item = t.need(item, role="menu", name=path[-1],
                  what="the %r item" % path[-1])
    answer = t.state(item, "sensitive")
    t.escape()

    return answer


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
