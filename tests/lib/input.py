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

import subprocess
import time

import pyatspi


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


def click_at(x, y, button=1, settle=SETTLE):
    park_pointer()
    time.sleep(POINTER)
    _xdotool("mousemove", x, y)
    time.sleep(POINTER)
    _xdotool("click", button)
    time.sleep(settle)


def click_range(label, start, end, settle=SETTLE):
    """Click a range of a label's text.

    This is how a hyperlink inside a label is clicked: it has no extents of its
    own, so the label's Text interface is asked where the characters it covers
    are drawn.
    """
    box = label.queryText().getRangeExtents(start, end, pyatspi.DESKTOP_COORDS)
    x, y = box[0] + box[2] // 2, box[1] + box[3] // 2
    click_at(x, y, settle=settle)
    return x, y


def key(*keys, settle=SETTLE):
    for k in keys:
        _xdotool("key", k)
    time.sleep(settle)


def type_text(text, delay=25, settle=SETTLE):
    _xdotool("type", "--delay", delay, text)
    time.sleep(settle)


def screenshot(path):
    """Best effort -- ImageMagick is useful here but not worth requiring."""
    try:
        subprocess.run(["import", "-window", "root", path],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        return path
    except (FileNotFoundError, subprocess.CalledProcessError):
        return None
