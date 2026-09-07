"""The colour scheme picked in the context menu is the one the pane gets.

# requires: MOO_BUILD_TERMINAL

The schemes are the ones the python plugin carried, and the name that goes into
the settings is the untranslated one, so that a scheme survives a change of
locale -- the python plugin stored the translated name and lost the setting the
first time the user changed language.
"""

from lib import input as ui

SCHEME = "Green on Black"
BLACK = "#000000"


def setup(s):
    s.pref("Plugins/Terminal/shell", "/bin/sh")


def run(t):
    t.menu("Tools", "Terminal")
    terminal = t.need(t.frame, role="terminal", what="the terminal")
    t.wait(lambda: t.text(terminal).strip(), "the shell to draw a prompt")

    t.check(colour(t, terminal) != BLACK,
            "the terminal does not start out black")

    schemes = open_colour_menu(t)
    chosen = t.item(schemes, SCHEME)

    t.check(t.state(chosen, "checked") is False,
            "%r is not the scheme in use before it is picked" % SCHEME)
    t.click(chosen)

    t.wait(lambda: colour(t, terminal) == BLACK,
           "the terminal to be repainted in the colours of %r" % SCHEME)
    t.log("ok: picking %r turned the terminal black" % SCHEME)

    # And the menu says so the next time it is opened, which is the other half:
    # the scheme was stored, not only applied to the widget.
    schemes = open_colour_menu(t)
    t.check(t.state(t.item(schemes, SCHEME), "checked"),
            "%r is ticked in the menu afterwards" % SCHEME)
    t.escape()


def open_colour_menu(t):
    properties = t.item(t.popup(), "Properties")
    t.click(properties)

    colours = t.item(properties, "Color")
    t.click(colours)

    return colours


def colour(t, terminal):
    """The colour of an empty corner of the terminal.

    The bottom right corner: the shell writes from the top left, so nothing but
    the background is ever drawn there.
    """
    x, y, width, height = ui.extents(terminal)
    return ui.pixel(x + width - 5, y + height - 5)
