"""File Filters: settings that apply to the files a glob picks out.

# requires: MOO_GTK3

The File Filters page maps a filter to a set of document settings --
src/mooedit/mooeditfiltersettings.cpp parses both halves and applies them to
every document whose name the filter matches -- and it is the only place in medit
where a setting can be made to hold for some files and not others. Nothing had
written a row into it.

The setting is "strip", which takes the trailing blanks off a line as the
document is saved, and the filter picks the .txt file out of the two that are
open. What is asserted is the bytes of both afterwards: one is stripped and the
other is not, which no global setting could produce.
"""

from lib import input as ui

STRIPPED = "notes.txt"
LEFT_ALONE = "notes.log"

CONTENT = "alpha   \nbeta\n"

CLEANED = "alpha\nbeta\n"

FILTER = "globs:*.txt"
OPTIONS = "strip: true"


def setup(s):
    s.open(s.write("workdir/" + STRIPPED, CONTENT))
    s.open(s.write("workdir/" + LEFT_ALONE, CONTENT))


def run(t):
    dialog = t.preferences("File Filters")

    t.click(page_buttons(t, dialog)[0])

    # New adds an empty row and no more: a cell of it is edited by clicking it
    # once to take the row and once again to open the cell, and what ends the
    # edit is a click on the next cell rather than a key -- Return and Escape are
    # the dialog's own default and cancel responses.
    edit(t, cells(t, dialog)[0], FILTER)
    commit(t, dialog)

    edit(t, cells(t, dialog)[-1], OPTIONS)
    commit(t, dialog)

    t.wait(lambda: filled(t, dialog) == [FILTER, OPTIONS],
           "the row to hold the filter and its options; it holds %s"
           % filled(t, dialog))

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    for name in (STRIPPED, LEFT_ALONE):
        save(t, name)

    # Each was given a character at the end, which is what made it worth saving.
    t.check(t.sandbox.read("workdir", STRIPPED) == CLEANED + "!",
            "the file the filter matches was stripped as it was saved: %r"
            % t.sandbox.read("workdir", STRIPPED))

    t.check(t.sandbox.read("workdir", LEFT_ALONE) == CONTENT + "!",
            "and the one it does not match was left alone: %r"
            % t.sandbox.read("workdir", LEFT_ALONE))


def save(t, name):
    """Make a change in that document and save it."""
    t.menu("Window", name)

    view = t.document()
    t.focus()
    t.click(view)
    t.key("ctrl+End")
    t.type_text("!")
    t.key("ctrl+s")

    t.wait(lambda: "[modified]" not in (t.frame.name or ""),
           "%s to be saved" % name)


def edit(t, node, text):
    """Open a cell for editing and type into it.

    The focus first: without a window manager the keys belong to the editor
    window until something says otherwise, and a click in a dialog does not move
    them -- so the typing would go into the document instead.
    """
    t.focus()
    t.click(node)
    t.click(node)
    t.type_text(text)


def commit(t, dialog):
    """End a cell edit with Return.

    A cell being edited holds the keyboard and the pointer both, so a click
    somewhere else in the dialog lands in the entry rather than ending it -- and
    Return, which would be the dialog's default response with nothing being
    edited, is taken by the entry instead.
    """
    ui.key("Return")


def the_list(t, dialog):
    tables = sorted(t.on_screen(t.find_all(dialog, role="table", depth=30)),
                    key=lambda node: t.extents(node)[0])

    return tables[-1]


def cells(t, dialog):
    """The cells of the filter list, which is the table on the right."""
    found = [c for c in t.on_screen(t.find_all(dialog, role="table cell", depth=30))
             if t.extents(c)[0] > 260]

    return sorted(found, key=lambda node: t.extents(node)[0])


def filled(t, dialog):
    return [c.name for c in cells(t, dialog) if c.name]


def page_buttons(t, dialog):
    found = [b for b in t.on_screen(t.find_all(dialog, role="push button", depth=30))
             if not b.name]

    return sorted(found, key=lambda node: t.extents(node)[0])



