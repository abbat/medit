"""The View preference makes MooTextView draw its right margin.

# requires: MOO_GTK3

The right margin is painted in the text window and has no accessibility node.
Its position is the width of COLUMN columns, which the test measures off the
drawn text, and it is looked for as a pixel change at exactly that position on
a row the document leaves empty.  The document is created with File/New and is
never saved.
"""

COLUMN = 10


def run(t):
    t.menu("File", "New")
    view = t.document()
    t.click(view)
    # COLUMN characters, so that the width of COLUMN columns can be measured
    # rather than extrapolated from one glyph.
    t.type_text("x" * COLUMN)

    # Take the baseline with the feature explicitly disabled, rather than
    # relying on the preference default.
    dialog = t.preferences("View")
    tick(t, dialog, "Draw right margin", False)
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    # Where the margin has to land, read off the text rather than computed from
    # a single character's width: getRangeExtents() answers with the drawn
    # width of the whole run, so this holds whatever Pango does with the glyphs.
    text_left, _, run_width, _ = t.range_extents(view, 0, COLUMN)
    margin_x = text_left + run_width

    # Read below the text, on a row the document leaves empty: the margin is
    # drawn down the whole height of the text window, and a row with glyphs on
    # it changes for reasons of its own -- the caret, a repaint in another
    # colour -- which a pixel comparison cannot tell from the margin.
    view_x, view_y, view_width, view_height = t.extents(view)
    row_y = view_y + view_height - 10
    before = t.pixel_row(view_x, row_y, view_width)

    dialog = t.preferences("View")
    tick(t, dialog, "Draw right margin", True)

    spin = t.wait(lambda: visible_spin(t, dialog),
                  "the visible right-margin column")
    t.wait(lambda: t.state(spin, "sensitive"),
           "the right-margin column to become editable")
    set_column(t, spin, COLUMN)
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    t.wait(lambda: t.pixel_row(view_x, row_y, view_width) != before,
           "the right margin to be drawn")

    after = t.pixel_row(view_x, row_y, view_width)
    drawn = [view_x + i for i in range(len(after)) if after[i] != before[i]]

    # One column, and that column: scanning the row for any change at all would
    # pass on a margin drawn at the default column 80 just as happily.
    t.check(len(drawn) == 1 and abs(drawn[0] - margin_x) <= 1,
            "the margin is the single line at column %d, where the text ends "
            "(x=%d): the row changed at %r" % (COLUMN, margin_x, drawn))

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


def set_column(t, spin, column):
    """Type a column into the spin button.

    A quarter of the way across rather than in the middle, which is where
    t.click() would go: the middle of a GtkSpinButton this size is one of its
    arrows, and clicking there steps the value instead of putting the cursor in
    the text.
    """
    x, y, width, height = t.extents(spin)

    # The dialog has to be given the keys before any of them are typed: nothing
    # hands the input focus to a window that has just appeared, and without this
    # the digits go into the document behind the dialog and the margin stays at
    # the column it was already at -- which is what the assertion below catches.
    t.focus()
    t.click_at(x + width // 4, y + height // 2)
    t.key("ctrl+a")
    t.type_text(str(column))

    t.check(t.text(spin) == str(column),
            "the right-margin column now reads %d, not %r" % (column, t.text(spin)))


def visible_spin(t, dialog):
    """Ignore spin buttons on the preference pages that are not displayed."""
    spins = t.on_screen(t.find_all(dialog, role="spin button"))
    return spins[0] if len(spins) == 1 else None
