"""The other pages of the preferences: View, File, Languages, File Filters.

# requires: MOO_GTK3

tests/edit/preferences drives the dialog itself -- the list of pages, OK against
Cancel -- on the General page. This one is about the rest of
src/mooedit/mooeditprefspage.cpp: four more pages, each built from its own .ui
file when it is first shown and each with an apply of its own.

Two of them are asserted by what they change rather than by what they hold. Show
line numbers on the View page is a setting of the window's own View menu, so the
menu says whether it took. Remove trailing spaces on the File page is a change to
what a save writes, so the bytes on disk say it -- and that is the one worth a
test on its own: it edits the document as it saves it.

The other two are asserted for less. Languages has nothing to configure in a
sandbox that has no language definitions, and File Filters is a list that starts
empty, so what is checked is that the page is built at all -- which is not
nothing: a page whose .ui file went missing would come up blank here.
"""

NAME = "notes.txt"

CONTENT = "alpha   \nbeta\n"

STRIPPED = "alpha\nbeta\n"

LINE_NUMBERS = "Show line numbers"
STRIP_SPACES = "Remove trailing spaces"

VIEW_MENU = ("View", "Show Line Numbers")


def setup(s):
    s.open(s.write("workdir/" + NAME, CONTENT))


def run(t):
    view = t.document()

    t.check(not ticked(t, *VIEW_MENU),
            "the document starts without line numbers")

    # The View page: a setting the window's own menu reports.
    dialog = t.preferences("View")
    tick(t, dialog, LINE_NUMBERS, True)
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    t.wait(lambda: ticked(t, *VIEW_MENU),
           "the preference to reach the document that is open, which the View "
           "menu is what says")

    # The File page: a setting that changes what a save writes.
    dialog = t.preferences("File")
    tick(t, dialog, STRIP_SPACES, True)
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    t.focus()
    t.click(view)
    t.key("ctrl+End")
    t.type_text("!")
    t.key("ctrl+s")

    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")

    body = t.sandbox.read("workdir", NAME)
    t.check(body == STRIPPED + "!",
            "the save took the trailing spaces off the first line: %r" % body)

    # And the two that are only asked to come up.
    dialog = t.preferences("Languages")
    t.need(dialog, role="label", name_prefix="Language", what="the Languages page")
    t.click(t.button(dialog, "Cancel"))
    t.no_toplevel("Preferences")

    dialog = t.preferences("File Filters")
    t.need(dialog, role="table", what="the list on the File Filters page")
    t.click(t.button(dialog, "Cancel"))
    t.no_toplevel("Preferences")


def tick(t, dialog, name, on):
    box = t.need(dialog, role="check box", name=name, what="the %r box" % name)

    if t.state(box, "checked") != on:
        t.click(box)
        t.wait(lambda: t.state(box, "checked") == on,
               "the %r box to be %s" % (name, "ticked" if on else "clear"))


def ticked(t, *path):
    """Whether that menu item has its tick, read in one opening of the menu."""
    menu = t.menu(*path[:-1])
    item = t.need(menu, role="check menu item", name=path[-1],
                  what="the %r item" % path[-1])
    state = t.state(item, "checked")
    t.escape()

    return state
