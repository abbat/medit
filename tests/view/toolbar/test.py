"""Show Toolbar hides and shows the toolbar, and Toolbar Style changes its shape.

# requires: MOO_GTK3

Both live in MooWindow rather than in the editor: "ShowToolbar" is a toggle bound
to the window's toolbar-visible condition, and "ToolbarStyle" is a
moo_menu_action with four radio items built by create_toolbar_style_action().
Neither was watched, and both are the kind of thing that is either wired up or
silently is not.

The style is read as the height of the toolbar rather than as the tick on the
radio item, because the tick would move in a build where the toolbar never
changed. Labels below icons need two rows where icons alone need one, so the
comparison is between two styles asked for by name and never against a number.
"""

SHOW = "Show Toolbar"
STYLE = "Toolbar Style"

ICONS = "Icons Only"
BELOW = "Labels Below Icons"


def setup(s):
    s.open(s.write("workdir/notes.txt", "nothing to see\n"))


def run(t):
    t.check(toolbar(t) is not None, "the toolbar is there to start with")

    t.menu("View", SHOW)

    t.wait(lambda: toolbar(t) is None, "the toolbar to go away when it is switched off")
    t.log("ok: Show Toolbar hides the toolbar")

    t.menu("View", SHOW)

    t.wait(lambda: toolbar(t) is not None, "the toolbar to come back")
    t.log("ok: and brings it back")

    # Two styles asked for by name, and the height between them.
    t.menu("View", STYLE, ICONS)
    t.settle(1)
    short = t.extents(toolbar(t))[3]
    t.log("with icons only the toolbar is %d high" % short)

    t.menu("View", STYLE, BELOW)

    t.wait(lambda: t.extents(toolbar(t))[3] > short,
           "the toolbar to grow when the labels go under the icons; it is %d high "
           "against %d" % (t.extents(toolbar(t))[3], short))
    t.log("ok: Toolbar Style changes the toolbar and not only the menu")

    t.check(chosen(t) == [BELOW],
            "and the style that was picked is the one ticked: %s" % chosen(t))
    t.escape()


def toolbar(t):
    """The window's toolbar, while it is drawn."""
    found = t.on_screen(t.find_all(t.frame, role="tool bar", depth=25))

    return found[0] if found else None


def chosen(t):
    """The ticked entries of the Toolbar Style submenu."""
    style = t.menu("View", STYLE)

    return [item.name for item in t.on_screen(t.find_all(style, depth=1))
            if item.name and t.state(item, "checked")]
