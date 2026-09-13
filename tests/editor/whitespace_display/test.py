"""Show spaces and Show trailing spaces change what the text view paints.

# requires: MOO_GTK3

The preferences are reachable through the View page, while the marks themselves
are painted by MooTextView and are not exposed in the accessibility tree.  The
test compares the pixels occupied by a space before and after each setting is
enabled.  The document is made with File/New and is never saved.
"""

CONTENT = "a b  \n"


def run(t):
    t.menu("File", "New")
    view = t.document()
    t.click(view)
    t.type_text(CONTENT)

    # Normalize both switches before taking the baseline.  The sandbox starts
    # with defaults, but making that assumption would let a persistent default
    # turn the first comparison into a no-op.
    dialog = t.preferences("View")
    tick(t, dialog, "Show spaces", False)
    tick(t, dialog, "Show trailing spaces", False)
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    # The space between two visible characters is invisible by default.  Keep
    # the initial strip so the assertion proves that enabling the setting paints
    # something, rather than merely finding a non-background pixel.
    space = t.range_extents(view, 1, 2)
    before = pixels(t, space)

    dialog = t.preferences("View")
    tick(t, dialog, "Show spaces", True)
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    t.wait(lambda: pixels(t, space) != before,
           "the space marker to be painted")

    # Show spaces also includes trailing spaces.  Turn it back off before the
    # second baseline, otherwise enabling Show trailing spaces would have
    # nothing new to paint.
    dialog = t.preferences("View")
    tick(t, dialog, "Show spaces", False)
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    # The two trailing spaces have their own setting.  They are kept separate
    # from the ordinary-space assertion because a regression could accidentally
    # make only the middle marker visible.
    trailing = t.range_extents(view, 3, 5)
    trailing_before = pixels(t, trailing)

    dialog = t.preferences("View")
    tick(t, dialog, "Show trailing spaces", True)
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    t.wait(lambda: pixels(t, trailing) != trailing_before,
           "the trailing-space markers to be painted")

    t.key("ctrl+w")
    dialog = t.need(t.app, role="alert", depth=2,
                    what="the dialog asking about the unsaved new document")
    t.click(t.button(dialog, "Discard"))


def tick(t, dialog, name, on):
    box = t.need(dialog, role="check box", name=name, what="the %r check box" % name)

    if t.state(box, "checked") != on:
        t.click(box)
        t.wait(lambda: t.state(box, "checked") == on,
               "the %r box to be %s" % (name, "ticked" if on else "clear"))


def pixels(t, box):
    """Read the whole glyph cell, including the row where the marker is drawn."""
    x, y, width, height = box
    return tuple(t.pixel_row(x, y + row, width) for row in range(height))
