"""Synthetic input.

AT-SPI can describe a widget but it cannot reliably operate one: calling the
"click" action on a menu item produces

    Gtk-WARNING: no trigger event for menu popup
    Gdk-CRITICAL: gdk_window_get_window_type: assertion 'GDK_IS_WINDOW (window)'

and leaves the application with no toplevel at all -- gtk wants a real event to
open a menu from, and the action interface has none to give it. So AT-SPI is
asked where the widget is and xdotool is asked to click there, which goes
through the X server the same way a user's pointer would.

The coordinates always come from the tree, never from a table of screen
positions: a fixed coordinate is only correct for one window size, one theme and
one toolkit.
"""

import re
import subprocess
import time

import pyatspi


# What medit sets as the WM_CLASS of its windows; matched as a regex, since
# the two halves of a class differ in case.
APP_CLASS = "[Mm]edit"

SETTLE = 0.4

_size = None

# Between each step of a click. A click is three things to a toolkit, not one:
# the pointer arrives, the button goes down, the button comes up, and what the
# toolkit acts on is decided by the first of those.
#
# It matters more than it sounds. "xdotool mousemove x y click 1" warps the
# pointer and presses in the same instant, and on GTK+2 the press then acts on
# whatever the toolkit thought was under the pointer beforehand: a link in the
# About dialog does not open on the first click after a button in the same
# dialog was clicked, and opens on the second. Measured, one variant per row:
#
#   move to the link and click                     does not open
#   the same click again                           opens
#   park the pointer elsewhere first, then click   opens
#   move onto the label twice, then click          opens
#
# So the pointer is parked away from the target before every click. What that
# buys is a crossing event into the target that the toolkit has time to
# process, which is what a person's hand produces and a warp does not.
POINTER = 0.15


def _xdotool(*args):
    subprocess.run(["xdotool"] + [str(a) for a in args], check=True)


def _xdotool_out(*args):
    done = subprocess.run(["xdotool"] + [str(a) for a in args],
                          capture_output=True, text=True)
    return done.stdout.strip()


def windows(pattern=APP_CLASS):
    """The X windows of the application, as xdotool lists them."""
    return _xdotool_out("search", "--onlyvisible", "--class", pattern).split()


def window_of(frame):
    """The X window a toplevel of the accessibility tree is drawn in.

    By title and not by position in the list: with more than one window open,
    which of them is "the first" is not something either X or AT-SPI promises,
    and the title is what the two views of the same window agree on. medit puts
    the document in it, so the second window of a fresh instance is "medit -
    Untitled 2" -- unique, and unique is the whole requirement.
    """
    found = _xdotool_out("search", "--onlyvisible", "--name",
                         "^%s$" % re.escape(frame.name)).split()

    if len(found) != 1:
        raise AssertionError("%r names %d X windows, expected one"
                             % (frame.name, len(found)))

    return found[0]


def activate_window(window, settle=SETTLE):
    """Make a window the one the window manager considers active.

    Not the same as focus_window(), and the difference is what a window manager
    is: XSetInputFocus, which is what that does, moves the keys and tells the
    window manager nothing, so the window it thinks is active -- the one it
    would close on Alt+F4, the one it raises -- is still the other one. This
    asks the window manager instead, and there has to be one to ask.
    """
    _xdotool("windowactivate", "--sync", window)
    time.sleep(settle)


def resize_window(window, width, height, settle=SETTLE):
    """Give a window a size of the test's choosing."""
    _xdotool("windowsize", window, width, height)
    time.sleep(settle)


def move_window(window, x, y, settle=SETTLE):
    """Put a window where the test wants it.

    Windows are placed by the window manager, and where it puts a second one is
    its business rather than anything a test may rely on: xfwm4 cascades it
    over the first -- measured at (600,300) against (0,0), two 800x600 windows
    overlapping over most of both -- and an overlap is a click that lands in
    whichever of them X stacked on top. A test with two windows says where it
    wants them.
    """
    _xdotool("windowmove", window, x, y)
    time.sleep(settle)


def focus_window(pattern=APP_CLASS, window=None):
    """Point the X input focus at the application's window.

    There is no window manager in the sandbox, and nothing else hands the focus
    on when a window goes away: after a menu has been dismissed the input focus
    belongs to the menu's window, which no longer exists, "xdotool
    getwindowfocus" answers nothing at all, and the application stops receiving
    keys -- it does not even see the key that would open the next menu. A window
    manager, which is to say everywhere except here, deals with this.

    Which window, when there are several, is the caller's to say: the default
    is the last one xdotool lists, and a test driving two windows names the one
    it means.
    """
    if window is None:
        found = windows(pattern)

        if not found:
            return None

        window = found[-1]

    _xdotool("windowfocus", window)
    time.sleep(POINTER)

    return window


def extents(node):
    """Where the widget is on the screen, as (x, y, width, height)."""
    box = node.queryComponent().getExtents(pyatspi.DESKTOP_COORDS)
    return box.x, box.y, box.width, box.height


def on_screen(node):
    """Whether the node has a position at all.

    A widget that is in the tree but not realised reports INT_MIN for its
    origin, and xdotool then rejects the coordinate rather than clicking
    anywhere. That happens with menu items of a menu that has not popped up
    yet, so it is a "not ready" answer rather than an error -- the callers wait
    on it.
    """
    try:
        x, y, w, h = extents(node)
    except Exception:
        return False

    return w > 0 and h > 0 and -32768 < x < 32768 and -32768 < y < 32768


def centre(node):
    x, y, w, h = extents(node)
    return x + w // 2, y + h // 2


def click(node, button=1, settle=SETTLE):
    x, y = centre(node)
    click_at(x, y, button, settle)
    return x, y


def display_size():
    """The screen size, asked once and remembered."""
    global _size

    if _size is None:
        out = subprocess.run(["xdotool", "getdisplaygeometry"],
                             capture_output=True, text=True, check=True).stdout
        width, height = out.split()
        _size = (int(width), int(height))

    return _size


def park_pointer():
    """Move the pointer to the far corner, where nothing is."""
    width, height = display_size()
    _xdotool("mousemove", width - 1, height - 1)


def click_at(x, y, button=1, settle=SETTLE, times=1):
    park_pointer()
    time.sleep(POINTER)
    _xdotool("mousemove", x, y)
    time.sleep(POINTER)
    if times > 1:
        _xdotool("click", "--repeat", times, button)
    else:
        _xdotool("click", button)
    time.sleep(settle)


def click_range(node, start, end, button=1, settle=SETTLE, times=1):
    """Click a range of a node's text.

    This is how a hyperlink inside a label is clicked: it has no extents of its
    own, so the label's Text interface is asked where the characters it covers
    are drawn. Words on a terminal have no widget of their own either, and are
    reached the same way -- twice over, to select one.
    """
    box = node.queryText().getRangeExtents(start, end, pyatspi.DESKTOP_COORDS)
    x, y = box[0] + box[2] // 2, box[1] + box[3] // 2
    click_at(x, y, button=button, settle=settle, times=times)
    return x, y


def hover_range(node, start, end, settle=1.0):
    """Rest the pointer where a range of a node's text is drawn.

    Parked elsewhere first and then moved, like a click, because what a
    tooltip waits for is the pointer arriving and then staying still: a warp
    straight onto the target produces no crossing event to start the timer.
    """
    box = node.queryText().getRangeExtents(start, end, pyatspi.DESKTOP_COORDS)
    x, y = box[0] + box[2] // 2, box[1] + box[3] // 2

    park_pointer()
    time.sleep(POINTER)
    _xdotool("mousemove", x, y)
    time.sleep(settle)

    return x, y


def key(*keys, settle=SETTLE):
    for k in keys:
        _xdotool("key", k)
    time.sleep(settle)


def type_text(text, delay=25, settle=SETTLE):
    _xdotool("type", "--delay", delay, text)
    time.sleep(settle)


# ImageMagick 6 installs "import"; in 7 the same thing is "magick import", and
# which one a distribution ships is not something a test should have to know.
_IMPORT = (["import"], ["magick", "import"])


def _run_import(argv):
    return subprocess.run(argv, capture_output=True, text=True, check=True)


def _import(*args):
    """Run whichever spelling of ImageMagick's import this machine has.

    Every spelling but the last is tried and forgiven; the last one is run
    outside the loop, so its failure is what the caller is shown and there is
    no way out of this function that neither returns nor raises. Keeping the
    error in a variable and raising it afterwards read as though the loop might
    not run at all, and then it is a TypeError from "raise None" that arrives
    in place of the failure being reported.
    """
    argv = [str(a) for a in args]

    for command in _IMPORT[:-1]:
        try:
            return _run_import(command + argv)
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue

    return _run_import(_IMPORT[-1] + argv)


def pixel(x, y):
    """The colour of one pixel of the screen, as "#rrggbb".

    The one thing in these tests that is read off the screen rather than out of
    the accessibility tree, and only because a colour has no other evidence:
    nothing in the tree says what colour anything is drawn in, so the choice is
    between looking and testing that a setting was written rather than that it
    did something.
    """
    out = _import("-window", "root", "-crop", "1x1+%d+%d" % (x, y),
                  "-depth", "8", "txt:-").stdout

    found = re.search(r"#[0-9A-Fa-f]{6}", out)

    if not found:
        raise AssertionError("could not read the pixel at (%d,%d): %r" % (x, y, out))

    return found.group(0).lower()


def screenshot(path):
    """Best effort -- ImageMagick is useful here but not worth requiring."""
    try:
        _import("-window", "root", path)
        return path
    except (FileNotFoundError, subprocess.CalledProcessError):
        return None
