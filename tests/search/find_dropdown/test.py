"""The drop-down button of the Find entry shows the history under the entry.

# requires: MOO_GTK3

The entry of the Find dialog is a MooCombo -- src/mooutils/moocombo.cpp -- which
is an entry and an arrow button side by side rather than a GtkComboBox, and the
list it drops is a popup window it places and sizes itself. tests/search/
find_history walks that list from the keyboard, where a list of the wrong size in
the wrong place still answers Down and Return; this one presses the button and
looks at the window that comes up.

Where it has to be is under the combo and as wide as it: those are the two things
resize_popup() computes, and both were wrong on GTK+3, where the entry has no
window of its own to take an origin from and a scrolled window does not pass on
what its child asked for. The list came up at the corner of the dialog and a
third of the width, over the fields instead of under the entry.

The height is the other half of the same question, and the one with no landmark
to compare against: a popup sized by anything other than its rows has empty space
below the last one, so what the test asks is that the window ends where the rows
do.
"""

CONTENT = "one\nalpha\ntwo\nbeta\nthree\n"

#  one 0-2, \n 3 | alpha 4-8, \n 9 | two 10-12, \n 13 | beta 14-17, \n 18
BETA = (14, 18)

# A window placed and sized by hand is never off by a pixel or two; it is off by
# the height of a title bar or the width of a scrollbar, or it is right.
SLACK = 6


def setup(s):
    s.open(s.write("workdir/words.txt", CONTENT))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)

    # Two searches, so that the button has something to drop.
    for term in ("alpha", "beta"):
        search(t, term)

    t.focus()
    t.key("ctrl+Home")
    t.wait_caret(view, 0, "the cursor is back at the start")

    t.menu("Search", "Find")
    dialog = t.dialog("Find")

    entry = t.need(dialog, role="text", what="the entry of the Find dialog")
    ex, ey, ew, eh = t.extents(entry)

    arrow = drop_down(t, dialog, entry)
    ax, _, aw, _ = t.extents(arrow)

    t.click(arrow)

    popup = t.wait(lambda: history_popup(t), "the history list to drop")
    px, py, pw, ph = t.extents(popup)

    # The combo is the entry and the button together, and the list belongs to
    # the pair rather than to either.
    left, right = min(ex, ax), max(ex + ew, ax + aw)

    t.check(abs(px - left) <= SLACK and abs(px + pw - right) <= SLACK,
            "the list spans the combo: it is at %d..%d, the combo at %d..%d"
            % (px, px + pw, left, right))
    t.check(abs(py - (ey + eh)) <= SLACK,
            "and hangs under it: the list starts at y=%d, the entry ends at y=%d"
            % (py, ey + eh))

    rows = t.on_screen(t.find_all(popup, role="table cell"))

    t.check(len(rows) == 2,
            "both searches are offered: %s" % [row.name for row in rows])

    bottom = max(t.extents(row)[1] + t.extents(row)[3] for row in rows)

    t.check(0 <= py + ph - bottom <= SLACK,
            "and the window ends where its rows do: it ends at y=%d, the last "
            "row at y=%d" % (py + ph, bottom))
    t.log("ok: the list is under the combo, as wide as it and as tall as its rows")

    # And it is a list to pick from, not only something to look at.
    t.click(row_named(t, rows, "beta"))

    t.wait(lambda: t.text(entry) == "beta",
           "the row that was clicked to fill the entry in; it holds %r"
           % t.text(entry))

    accept(t, dialog)

    t.wait_selection(view, BETA, "and the search that ran is the one picked")


def drop_down(t, dialog, entry):
    """The arrow button of the combo: the one button with no name of its own.

    A MooCombo builds it out of a GtkButton holding an arrow, and an arrow has
    nothing to say about itself, so the accessible has no name -- which is what
    tells it apart from Find and Cancel. The dialog holds a second nameless
    button, the one of the Find in Files entry it shares its layout with, and
    that one is not on screen.
    """
    nameless = [b for b in t.on_screen(t.find_all(dialog, role="push button"))
                if not b.name]

    if len(nameless) != 1:
        t.fail("expected one drop-down button beside the entry, found %d"
               % len(nameless))

    x, y, w, h = t.extents(nameless[0])
    ex, ey, ew, eh = t.extents(entry)

    if x < ex + ew:
        t.fail("the drop-down button is at %d, not right of the entry, which "
               "ends at %d" % (x, ex + ew))

    return nameless[0]


def history_popup(t):
    """The popup window holding the history, or None while there is none.

    It is a top level of its own -- a GTK_WINDOW_POPUP -- so it is not under the
    dialog in the tree, and it has no name; what it has is the list.
    """
    for window in t.on_screen(t.find_all(t.app, role="window", depth=2)):
        if t.find(window, role="table cell", depth=6) is not None:
            return window

    return None


def row_named(t, rows, name):
    for row in rows:
        if row.name == name:
            return row

    t.fail("no %r in the list: %s" % (name, [row.name for row in rows]))


def search(t, term):
    """Open Find, type the term, and accept, so the history remembers it."""
    t.menu("Search", "Find")
    dialog = t.dialog("Find")

    entry = t.need(dialog, role="text", what="the entry of the Find dialog")
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(term)

    accept(t, dialog)


def accept(t, dialog):
    """Press Find until the dialog goes.

    The list takes a pointer grab while it is up, and the click that dismisses
    it reaches nothing else -- so with a popup up the first click is spent on it
    and the second presses the button.
    """
    for _ in range(3):
        t.click(t.button(dialog, "Find"))

        if t.find(t.app, role="dialog", name="Find", depth=2) is None:
            return

    t.fail("the Find dialog did not close")
