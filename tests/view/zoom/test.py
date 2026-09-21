"""The text is zoomed from the View menu and with Ctrl and the wheel, in every document.

# requires: MOO_GTK3

_moo_edit_zoom() in mooeditprefs.cpp. The zoom is one for the whole editor, so a
document that was not in front when it changed shows the new size once it is, and
"Reset Zoom" puts the size of the preferences back. The size is read off the
screen, as how tall the first line is drawn.

The wheel without Ctrl scrolls, and does not zoom; it is only asserted that the
size stays, as there is nothing to scroll in a short document.
"""

ZOOM_IN = "Zoom In"
ZOOM_OUT = "Zoom Out"
ZOOM_RESET = "Reset Zoom"

FIRST = "first.txt"
SECOND = "second.txt"

TEXT = "some text\n"


def setup(s):
    s.pref("Editor/font", "Monospace 10")

    s.open(s.write("workdir/" + SECOND, TEXT))
    s.open(s.write("workdir/" + FIRST, TEXT))


def run(t):
    base = height(t)
    t.log("the line is %d high" % base)

    # -- the menu ----------------------------------------------------------
    for _ in range(3):
        t.menu("View", ZOOM_IN)

    t.wait(lambda: height(t) > base,
           "the line to grow with Zoom In; it is %d high against %d" % (height(t), base))
    larger = height(t)
    t.log("ok: Zoom In makes the text larger: %d" % larger)

    t.menu("View", ZOOM_OUT)
    t.wait(lambda: height(t) < larger,
           "the line to shrink with Zoom Out; it is %d high against %d" % (height(t), larger))
    t.log("ok: Zoom Out makes it smaller")

    # -- it is one for all the documents -----------------------------------
    t.menu("Window", SECOND)
    t.wait(lambda: height(t) > base,
           "the other document to be zoomed too; it is %d high against %d" % (height(t), base))
    t.log("ok: the other document is zoomed as well")

    # -- the wheel ---------------------------------------------------------
    view = t.document()
    before = height(t)

    t.hover(view, 0, 1)
    t.wheel(-1)
    t.check(height(t) == before,
            "the wheel alone does not zoom: %d against %d" % (height(t), before))

    t.wheel(3, "ctrl")
    t.wait(lambda: height(t) > before,
           "Ctrl and the wheel up to make the text larger; it is %d high against %d"
           % (height(t), before))
    t.log("ok: Ctrl and the wheel zoom in")

    t.wheel(-3, "ctrl")
    t.wait(lambda: height(t) == before,
           "Ctrl and the wheel down to bring it back; it is %d high against %d"
           % (height(t), before))
    t.log("ok: and out again")

    # -- reset -------------------------------------------------------------
    t.menu("View", ZOOM_RESET)
    t.wait(lambda: height(t) == base,
           "Reset Zoom to bring back the size of the preferences; it is %d high against %d"
           % (height(t), base))
    t.log("ok: Reset Zoom puts the size back")


def height(t):
    """How tall the first line is drawn, in pixels."""
    return t.range_extents(t.document(), 0, len(TEXT) - 1)[3]
