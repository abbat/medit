"""Configure Shortcuts: what it lists, what it finds, and what it hands over.

# requires: MOO_BUILD_LSP

Every command of every plugin that is switched on is in this dialog, which is
the whole of medit's answer to "can I change that key": there is no per-plugin
list of shortcuts anywhere, and a command that is not an action with a name is
not in here and cannot be rebound. So the first assertion is the list itself.

The second is the round trip. The dialog writes into the accelerator map and
into the preferences, and the key it wrote is the one that has to work
afterwards -- for an action of a plugin as much as for one of the editor.

Both toolkits: a tree, three radio buttons and a dialog that catches a
keystroke are all things gail describes as readily as GTK+3 does.
"""

CONTENT = "alpha\n"

# The client's commands, as the dialog names them (the display name of each
# action). If one of these is missing it is not configurable at all. "LSP" is
# the heading they are under -- a group of the action collection, which is also
# what puts "Lsp" in the middle of their accelerator paths.
LISTED = ("Complete Word", "Parameter Hints", "Go to Definition",
          "Find References", "Rename", "Format Document")

# Rebound from Shift+F12 to something no other action wants.
REBOUND = "Find References"
NEW_KEY = "ctrl+j"


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))
    s.lsp_server(filter="globs:*.txt", replies={"textDocument/references": []})


def run(t):
    t.wait_lsp("textDocument/didOpen")

    dialog = open_dialog(t)
    tree = t.need(dialog, role="tree table", what="the list of actions")
    listed = rows(t, tree)

    for name in LISTED:
        t.check(name in listed, "%r is in the list of shortcuts" % name)

    # And in a section of their own rather than among the editor's hundred:
    # the list is grouped by the action group, and each plugin makes one.
    # The editor's own heading is the window's display name, which is
    # translated -- the id beside it, which the accelerator paths are made of,
    # is not. In this locale the two read the same.
    t.check("Editor" in listed, "the editor's own commands are under a heading")
    t.check("LSP" in listed, "the client's commands are under a heading of their own")

    if t.gtk == 3:
        t.check("Terminal" in listed, "and the terminal's are under theirs")

    # The Search box, which for twenty years was a widget with nothing behind
    # it: the list has a search column, so typing into the list searched and
    # typing into the box that says "Search:" did nothing at all.
    search = t.wait(lambda: search_box(t, dialog), "the Search box")

    t.click(search)
    t.type_text(REBOUND)

    t.wait(lambda: selected(t, tree) == REBOUND,
           "the search box to pick out %r; it selected %r"
           % (REBOUND, selected(t, tree)))
    t.log("ok: the Search box finds a row")

    rebind(t, dialog, NEW_KEY)

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Configure Shortcuts")

    # And the key it wrote is the one the command answers to now.
    t.focus()
    t.key("ctrl+Home")
    t.key(NEW_KEY)

    t.wait_lsp("textDocument/references")
    t.log("ok: the command answers to the key the dialog gave it")


def open_dialog(t):
    t.menu("Edit", "Configure Shortcuts")

    return t.dialog("Configure Shortcuts")


def rebind(t, dialog, key):
    """Give the selected action a shortcut of the test's choosing.

    Custom first, which is what makes the button beside it live, and then the
    button, which opens a dialog that listens for the keys themselves --
    there is nothing to type a shortcut into, by design. It commits itself
    half a second after the last key, so the OK button is not pressed here.
    """
    # By its exact name: the three of them used to read "Shortcut|None",
    # "Shortcut|Default" and "Shortcut|Custom" in every language that had no
    # translation for them, the disambiguating prefix never having been
    # stripped.
    t.click(t.need(dialog, role="radio button", name="Custom",
                   what="the Custom radio button"))

    t.click(accel_button(t, dialog))

    catcher = t.dialog("Choose Accelerator")

    # The pointer is what the keys follow with no window manager, so it goes
    # onto the dialog that is listening for them.
    t.click(t.need(catcher, role="label", name_prefix="",
                   what="the label of the accelerator dialog"))
    t.key(key)

    t.no_toplevel("Choose Accelerator")
    t.log("ok: %s was caught by the accelerator dialog" % key)


def search_box(t, dialog):
    """The entry beside the list. Drawn, unlike the tree's own search popup."""
    boxes = t.on_screen(t.find_all(dialog, role="text"))

    return boxes[0] if len(boxes) == 1 else None


def accel_button(t, dialog):
    """The button the shortcut is set with: a button of the page, not of the dialog.

    It is a MooAccelButton, whose label is the shortcut it holds -- which is
    empty for an action that has none, so it cannot be looked up by name. The
    dialog's own three buttons are the ones it is not.
    """
    buttons = [b for b in t.on_screen(t.find_all(dialog, role="push button"))
               if (b.name or "") not in ("Help", "Cancel", "OK")]

    t.check(len(buttons) == 1,
            "the page has one button of its own: %s" % [b.name for b in buttons])

    return buttons[0]


def rows(t, tree):
    return [cell.name for cell in t.find_all(tree, role="table cell", depth=3)
            if cell.name]


def selected(t, tree):
    for cell in t.find_all(tree, role="table cell", depth=3):
        if cell.name and t.state(cell, "selected"):
            return cell.name

    return None
