"""File Selector: resting the pointer on a button of its toolbar shows the tooltip.


The pane is a MooPaned, and the order in which it lists its widgets decides which
of them GTK+3 hands the pointer to. Listed after the document, which spans the
whole window, the pane lost it: its buttons lit up and no tooltip came. The main
toolbar, which is not in a pane, always had its tooltips. It only shows with the
pane floating, not pinned.


The tooltip window is not on the accessibility bus, so this reads the screen:
the tooltip is a dark box, and under the pointer there is only the light
background of the pane.
"""

from lib import input as ui

TIP = "Go to current document directory"

# Where to look for the box, from the pointer: below it and to its left, where
# GTK+ puts a tooltip. A dark pixel is one with no channel above this.
BELOW = 32
DARK = 70
# How many of the points looked at have to be dark. The box is a few dozen
# pixels across, so nearly all of them are; the text of the pane's entry is a
# handful at most.
NEEDED = 8


def setup(s):
    s.plugin("FileSelector")
    s.open(s.write("workdir/notes.txt", "hello"))


def dark(rgb):
    return all(int(rgb[i:i + 2], 16) <= DARK for i in (1, 3, 5))


def run(t):
    t.menu("View", "Panes", "File Selector")

    button = t.need(t.app, depth=30,
                    pred=lambda node: node.description == TIP and ui.on_screen(node),
                    what="the %r button" % TIP)
    x, y, width, height = t.extents(button)
    px, py = x + width // 2, y + height // 2

    def tooltip_shown():
        return sum(dark(t.pixel(px + dx, py + BELOW)) for dx in range(-100, 0, 8))

    # parked first: a tooltip waits for the pointer to arrive and then rest
    ui.park_pointer()
    ui._xdotool("mousemove", px, py)

    t.check(t.wait(lambda: tooltip_shown() >= NEEDED, "the tooltip %r to appear" % TIP),
            "resting the pointer on the button shows its tooltip")
