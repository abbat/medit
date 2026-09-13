"""The View preference makes MooTextView draw its right margin.

# requires: MOO_GTK3

The right margin is painted in the text window and has no accessibility node.
Its position is derived from the width of ten monospace columns, so the test
looks for a pixel change at that measured position instead of depending on a
theme colour.  The document is created with File/New and is never saved.
"""

COLUMN = 10


def run(t):
    t.menu("File", "New")
    view = t.document()
    t.click(view)
    t.type_text("x")

    # Take the baseline with the feature explicitly disabled, rather than
    # relying on the preference default.
    dialog = t.preferences("View")
    tick(t, dialog, "Draw right margin", False)
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    view_x, view_y, view_width, view_height = t.extents(view)
    row_y = view_y + view_height // 2
    before = t.pixel_row(view_x, row_y, view_width)

    dialog = t.preferences("View")
    tick(t, dialog, "Draw right margin", True)

    spin = t.wait(lambda: visible_spin(t, dialog),
                  "the visible right-margin column")
    t.wait(lambda: t.state(spin, "sensitive"),
           "the right-margin column to become editable")
    t.click(spin)
    t.key("ctrl+a")
    t.type_text(str(COLUMN))
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    t.wait(lambda: t.pixel_row(view_x, row_y, view_width) != before,
           "the right margin to be drawn at column %d" % COLUMN)

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


def visible_spin(t, dialog):
    """Ignore spin buttons on the preference pages that are not displayed."""
    spins = t.on_screen(t.find_all(dialog, role="spin button"))
    return spins[0] if len(spins) == 1 else None
