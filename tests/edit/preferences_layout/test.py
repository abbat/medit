"""Edit / Preferences: the spacing of the dialog and of its General page.


Two regressions of the GTK3 port that no other test could see, since both are a
matter of where things are drawn rather than of what they do.

The list of pages used to touch the dialog's edge and the Help button under it:
the box that holds the list and the notebook was packed into the content area
with no border, and GTK2's dialog had put one there itself.

The General page used to have its first section pushed far below the top. In
GTK3 a widget that wants vertical space makes every box above it want it too,
whatever the box's own expand says, and the spin buttons of the tab and indent
widths ask for it; so the Keyboard section was stretched and, not being asked to
fill, centred in the space it was given.
"""

# What the old GTK2 dialog had: a border of this many pixels around the content.
MARGIN = 6

# The page has a border of 12 of its own, so its first heading is at about that
# far from the top of the page; this much is slack for the theme.
HEADING_OFFSET = 16


def run(t):
    dialog = t.preferences("General")

    dx, dy, dw, dh = t.extents(dialog)

    pages = [node for node in t.on_screen(t.find_all(dialog, role="table"))]
    t.check(pages, "the list of pages is on the screen")
    lx, ly, lw, lh = t.extents(pages[0])

    t.check(lx - dx >= MARGIN,
            "the list of pages is %dpx from the left edge of the dialog" % (lx - dx))

    help_button = t.button(dialog, "Help")
    bx, by, bw, bh = t.extents(help_button)
    t.check(by - (ly + lh) >= MARGIN,
            "the list of pages is %dpx above the Help button" % (by - (ly + lh)))

    heading = t.need(dialog, role="label", name="Keyboard",
                     what="the Keyboard heading of the General page")
    hx, hy, hw, hh = t.extents(heading)
    t.check(hy - ly <= HEADING_OFFSET,
            "the Keyboard heading is %dpx below the top of the page" % (hy - ly))

    t.click(t.button(dialog, "Cancel"))
    t.no_toplevel("Preferences")
