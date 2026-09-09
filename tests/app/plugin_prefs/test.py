"""Switching a plugin off in the preferences, and on again.

# requires: MOO_GTK3

Every pane and half the menus of medit are plugins, and the Plugins page of the
preferences is where they are switched on and off -- mooplugin.c's
moo_plugin_set_enabled(), which unloads the plugin's windows, takes its actions
back out of the menus and writes the answer to the preferences. Nothing had
switched one off.

What is asserted is the Panes submenu of the View menu: the File Selector puts
its pane there when it is enabled, and taking the plugin away has to take the
entry with it.
"""

PLUGIN = "File Selector"

PANES = ("View", "Panes")


def setup(s):
    s.plugin("FileSelector")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    t.check(PLUGIN in panes(t), "the plugin's pane is offered to start with: %s" % panes(t))

    switch(t, PLUGIN)

    t.wait(lambda: PLUGIN not in panes(t),
           "the pane to go with the plugin; the menu offers %s" % panes(t))
    t.log("ok: switching the plugin off took its pane out of the menu")

    switch(t, PLUGIN)

    t.wait(lambda: PLUGIN in panes(t),
           "and to come back with it; the menu offers %s" % panes(t))
    t.log("ok: and switching it on again put it back")


def switch(t, name):
    """Tick or untick a plugin's box on the Plugins page, and accept."""
    dialog = t.preferences("Plugins")

    table = plugin_list(t, dialog)
    row = t.need(table, role="table cell", name=name,
                 what="the %r row of the plugin list" % name)

    # The box is a cell renderer of the row rather than a widget of its own, so
    # it is clicked by where it is drawn: the first column of the list, left of
    # the name.
    x, y, width, height = t.extents(table)
    t.click_at(x + 12, t.extents(row)[1] + t.extents(row)[3] // 2)

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")


def plugin_list(t, dialog):
    """The list of plugins, which is not the list of pages down the side.

    The dialog has two: the page list is the leftmost, and a page called "File
    Selector" is a row of it -- so a plugin of the same name has to be looked for
    inside the right table rather than in the dialog.
    """
    tables = sorted(t.on_screen(t.find_all(dialog, role="table", depth=30)),
                    key=lambda node: t.extents(node)[0])

    return tables[-1]


def panes(t):
    """What the View/Panes submenu offers, read in one opening."""
    menu = t.menu(*PANES)

    names = [item.name for item in t.on_screen(t.find_all(menu, depth=1)) if item.name]

    t.escape()
    t.escape()

    return names
