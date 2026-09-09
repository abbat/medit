"""A tool made in the preferences ends up in the Tools menu and runs.

# requires: MOO_GTK3

The other half of moousertools-prefs.cpp. tests/tools/prefs_page reads a tool
that a file described and switches it off; this one makes a tool that no file
describes, out of nothing but the page: New puts a row in the list, the combos
say what it is fed and what is done with what it prints, the code goes into the
text view below, and OK writes the lot to menu.xml and rebuilds the Tools menu.

What proves it is the document. The tool is described here as taking the line the
cursor is on and putting back what it prints, so a document that reads ALPHA
afterwards is the description having been saved, read back, and run.

Two bugs came out of writing it, and it is what would have caught both. The
settings of the type -- and with them the entry the code goes in -- never
appeared for a command that had just been made: the notebook holding them is
hidden while a command has no type, and choosing one switched it to the right
page without showing it again. And the two tabs of the page read
"user-tools-prefs|Tools menu", because the .ui file asked GtkBuilder to translate
a string with a context prefix and gave it "yes" as the context, so nothing
stripped the prefix where there was no translation.
"""

NEW_TOOL = "New Command"

CONTENT = "alpha\n"

SHOUTED = "ALPHA\n"

CODE = "tr a-z A-Z"

TABS = ["Tools menu", "Context menu"]

SHELL = "Shell command"
LINES = "Selected lines"
INSERT = "Insert into the document"


def setup(s):
    s.open(s.write("workdir/notes.txt", CONTENT))


def run(t):
    view = t.document()

    dialog = t.preferences("Tools")

    tabs = [tab.name for tab in t.find_all(dialog, role="page tab") if tab.name]
    t.check(tabs == TABS, "the page's tabs are named, not msgctxt'd: %s" % ", ".join(tabs))

    t.click(page_buttons(t, dialog)[0])
    t.wait(lambda: NEW_TOOL in rows(t, dialog),
           "New to put a row in the list; the list holds %s" % rows(t, dialog))

    # The row is left in an editable cell; clicking it is what ends that, since
    # the keys that would are the dialog's own default and cancel responses.
    t.click(t.need(dialog, role="table cell", name=NEW_TOOL, what="the new row"))

    # The row is left in an editable cell, and the rest of the page fills in only
    # once that edit is over: clicking the row is what ends it, since the keys
    # that would are the dialog's own default and cancel responses.
    t.click(t.need(dialog, role="table cell", name=NEW_TOOL, what="the new row"))

    # The type is what decides which page of settings the tool has, and a new
    # tool has none until it is picked.
    choose(t, combos(t, dialog)[-1], SHELL)

    t.wait(lambda: len(combos(t, dialog)) == 6,
           "the shell command's own settings to appear on the page")

    input_combo, output_combo = combos(t, dialog)[3], combos(t, dialog)[4]
    choose(t, input_combo, LINES)
    choose(t, output_combo, INSERT)

    # The code view is the one text widget of the page that is not an entry.
    # By what it is rather than by how big it is: the dialog is not the same size
    # everywhere the tests run, and a height was the first thing this got wrong.
    code = t.need(dialog, role="text", depth=30,
                  pred=lambda node: t.state(node, "multi_line"),
                  what="the text view the command's code goes in")
    t.focus()
    t.click(code)
    t.type_text(CODE)

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    # And now use it, which is the whole assertion: the menu offers it because
    # menu.xml was written and read again.
    t.focus()
    t.click(view)
    t.key("ctrl+Home")

    t.menu("Tools", NEW_TOOL)

    t.wait(lambda: t.text(view) == SHOUTED,
           "the tool to shout the line the cursor was on; the document holds %r"
           % t.text(view))

    # A modified document asks about itself when the runner quits medit.
    t.focus()
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")


def page_buttons(t, dialog):
    """The four icon buttons under the list, left to right: new, delete, down, up."""
    found = [b for b in t.on_screen(t.find_all(dialog, role="push button")) if not b.name]

    return sorted(found, key=lambda node: t.extents(node)[0])


def rows(t, dialog):
    return [cell.name for cell in t.on_screen(t.find_all(dialog, role="table cell"))
            if cell.name]


def combos(t, dialog):
    """The page's combo boxes, top to bottom.

    depth=30 because the ones that matter are the deepest: Requires, Save and
    Type are on the page itself, and Input, Output and Filter are inside the
    notebook page the Type combo chooses.
    """
    return sorted(t.on_screen(t.find_all(dialog, role="combo box", depth=30)),
                  key=lambda node: t.extents(node)[1])


def choose(t, combo, entry):
    """Drop a combo box and pick the entry by name.

    A combo's list is a menu in a toplevel of its own, so it is found the way a
    context menu is rather than inside the dialog.
    """
    t.click(combo)
    menu = t.wait(lambda: dropped_menu(t), "the list of the combo box")

    t.click(t.need(menu, role="menu item", name=entry, what="the %r entry" % entry))
    t.wait(lambda: combo.name == entry,
           "the combo box to say %r; it says %r" % (entry, combo.name))


def dropped_menu(t):
    menus = [menu for menu in t.find_all(t.app, role="menu", depth=2)]

    return menus[0] if menus else None
