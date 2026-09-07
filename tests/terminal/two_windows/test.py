"""Two windows are two panes on one list, and a preference reaches both.

# requires: MOO_BUILD_TERMINAL, MOO_UI_TEST_WM

Every pane the plugin builds goes on a static list of its own, terminal_panes,
and everything that changes a setting walks it: picking a colour scheme in one
window's context menu calls _moo_terminal_apply_prefs(), which repaints every
terminal there is. With one window on that list, walking it and reading a
variable are the same thing, and so is removing the only element -- which is
what closing a window does, while a spawn callback is still holding a pointer
into the list and checking whether its pane is still on it.

This is the one test that asks for a window manager, and the reason the harness
can start one at all: two toplevels without one are placed at the same
coordinates, so a click reaches whichever X stacked on top and a pixel belongs
to neither in particular. A window manager also puts _NET_CLIENT_LIST_STACKING
on the root window, which is where medit looks for the window a new document
should go to (_moo_get_top_window) -- without one that lookup does not run at
all, it reports a critical and falls back on the first window it knows.
"""

from lib import input as ui

# What the header above is about: the harness starts a window manager for this
# test and for no other. See lib/sandbox.py.
NEEDS_WM = True

SCHEME = "Green on Black"
BLACK = "#000000"

# The second scheme is picked for its background and not for its name: it has
# to be a colour the first one is not, or "the pane was repainted" and "the
# pane was left alone" look the same.
AFTER = "Black on Light Yellow"
YELLOW = "#ffffdd"

# Side by side on the 1400x900 screen, with nothing overlapping: a pixel read
# out of one window has to belong to that window, and a click meant for one of
# them must not be able to land in the other.
LEFT = (0, 0, 680, 600)
RIGHT = (700, 0, 680, 600)


def setup(s):
    s.pref("Plugins/Terminal/shell", "/bin/sh")


def run(t):
    first = t.frame
    t.place_window(first, *LEFT)

    # The second window before either terminal, because Ctrl+Shift+N is an
    # accelerator and the pane eats it: a focused vte widget sees a key before
    # the window's accelerators do, so the same keystroke that opens a window
    # from the document view does nothing at all from the terminal.
    second = new_window(t, first)
    t.place_window(second, *RIGHT)

    terminal = open_terminal(t, first)
    other = open_terminal(t, second)

    t.check(t.state(terminal, "showing") and t.state(other, "showing"),
            "both windows have a terminal of their own on screen")

    # Two shells, not one shell drawn twice: each answers what was typed into
    # it, and neither answers what was typed into the other.
    answers(t, first, terminal, "left")
    answers(t, second, other, "right")
    t.check("right" not in t.text(terminal), "the first terminal ran the first shell")

    # The list, walked: the scheme is picked in the left window and the right
    # window is repainted, having been told nothing itself.
    before = (colour(terminal), colour(other))
    t.check(BLACK not in before, "neither terminal starts out black, they are %s"
            % (before,))

    pick_scheme(t, first, terminal, SCHEME)

    t.wait(lambda: colour(terminal) == BLACK,
           "the terminal of the window the scheme was picked in to turn black")
    t.wait(lambda: colour(other) == BLACK,
           "the terminal of the other window to turn black as well, it is %s"
           % colour(other))
    t.log("ok: one scheme, both panes")

    # And the list, shortened. The pane of a closed window comes off it; the
    # one that is left is applied to, and the one that is gone is not -- which
    # is a use-after-free when it goes wrong, and the sanitizer is watching.
    close_window(t, second)

    pick_scheme(t, first, terminal, AFTER)
    t.wait(lambda: colour(terminal) == YELLOW,
           "the terminal of the window that is left to be repainted, it is %s"
           % colour(terminal))

    answers(t, first, terminal, "still")
    t.log("ok: the pane of the closed window is off the list, the other one is not")


def open_terminal(t, frame):
    """Open the pane in one window and wait for its shell to say something."""
    t.menu("Tools", "Terminal", frame=frame)
    terminal = t.need(frame, role="terminal", what="the terminal of %r" % frame.name)
    t.wait(lambda: t.text(terminal).strip(),
           "the shell of %r to draw a prompt" % frame.name)

    return terminal


def new_window(t, frame):
    """File/New Window, and the frame it opens."""
    before = {f.name for f in t.frames()}

    t.activate(frame)
    t.key("ctrl+shift+n")

    t.wait(lambda: len(t.frames()) == len(before) + 1, "a second window")
    opened = [f for f in t.frames() if f.name not in before]

    t.check(len(opened) == 1, "one window opened, %r" % [f.name for f in opened])

    return opened[0]


def close_window(t, frame):
    """Close a window the way a user does, through the window manager.

    Alt+F4 is xfwm4's own binding for it, so what medit sees is the
    WM_DELETE_WINDOW that a title bar's close button sends -- a path nothing
    else in these tests takes, there being no window manager to take it.

    The window manager closes the window it considers active, which is why the
    window is activated rather than focused: with the input focus moved by hand
    and nothing said to the window manager, Alt+F4 closes nothing at all.
    """
    name = frame.name

    t.activate(frame)
    t.key("alt+F4")

    t.wait(lambda: name not in [f.name for f in t.frames()],
           "%r to close" % name)
    t.log("ok: %r closed" % name)


def pick_scheme(t, frame, terminal, scheme):
    """Pick a colour scheme in the context menu of one window's terminal."""
    t.click(terminal)

    properties = t.item(t.popup(frame=frame), "Properties")
    t.click(properties)

    colours = t.item(properties, "Color")
    t.click(colours)

    t.click(t.item(colours, scheme))
    t.log("picked %r in %r" % (scheme, frame.name))


def answers(t, frame, terminal, word):
    """Type a command into one terminal and wait for that terminal's answer.

    The window first and the widget second. Clicking alone is enough under
    xfwm4 -- measured, with this test passing either way -- but which window
    the keys go to is the window manager's policy and not something a test
    about the terminal should rest on, and one call says which window this is
    about. Without a window manager it is not a matter of policy at all: both
    windows are at the same coordinates, and the click cannot even be aimed.
    """
    t.activate(frame)
    t.click(terminal)
    t.type_text("echo %s$((21*2))\n" % word)
    t.wait_text(terminal, "%s42" % word,
                what="the answer of the shell in that window")


def colour(terminal):
    """The colour of an empty corner of the terminal, as color_scheme reads it."""
    x, y, width, height = ui.extents(terminal)
    return ui.pixel(x + width - 5, y + height - 5)
