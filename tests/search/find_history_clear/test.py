"""The drop-down of the Find entry offers to forget what it remembers.

# requires: MOO_GTK3

The history of the Find entry used to be a one-way street: everything typed into
it stayed there for good, and a mistyped or a private search came back under the
arrow button on every later find. The drop-down now ends with a button that
empties the list -- src/mooutils/moohistorycombo.cpp puts it there and
moo_history_list_clear() in src/mooutils/moohistorylist.cpp does the forgetting,
prefs included.

The button lives inside the popup, which holds a pointer grab while it is up, so
that a click anywhere else only dismisses it; whether a click on the button
reaches the button at all is the first thing worth asking. The second is whether
the history is really gone rather than merely out of sight, and the answer to
that is the drop-down itself: with nothing left to offer it does not come up, and
the next search fills it again from scratch.
"""

import time

CONTENT = "one\nalpha\ntwo\nbeta\nthree\n"

#  one 0-2, \n 3 | alpha 4-8, \n 9 | two 10-12, \n 13 | beta 14-17, \n 18
ALPHA = (4, 9)

# A window placed and sized by hand is never off by a pixel or two.
SLACK = 6


def setup(s):
    s.open(s.write("workdir/words.txt", CONTENT))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)

    # Two searches, so that the history has something to forget.
    for term in ("alpha", "beta"):
        search(t, term)

    dialog, entry, arrow = find_dialog(t)
    t.click(arrow)

    popup = t.wait(lambda: history_popup(t), "the history list to drop")
    px, _, pw, _ = t.extents(popup)

    rows = t.on_screen(t.find_all(popup, role="table cell"))

    t.check(len(rows) == 2,
            "both searches are offered: %s" % [row.name for row in rows])

    clear = t.need(popup, role="push button",
                   what="the button that clears the history")

    cx, cy, cw, _ = t.extents(clear)
    bottom = max(t.extents(row)[1] + t.extents(row)[3] for row in rows)

    t.check(cy >= bottom - SLACK,
            "the button is under the list: it starts at y=%d, the last row ends "
            "at y=%d" % (cy, bottom))
    t.check(cx >= px - SLACK and cx + cw <= px + pw + SLACK,
            "and did not widen the popup: the button spans %d..%d, the popup "
            "%d..%d" % (cx, cx + cw, px, px + pw))

    t.click(clear)

    t.wait(lambda: history_popup(t) is None,
           "the list to go down with the history it was showing")

    # Nothing left to offer, so the arrow has nothing to drop.
    t.click(arrow)
    time.sleep(1)

    t.check(history_popup(t) is None,
            "the drop-down stays down once the history is empty")
    t.log("ok: the button emptied the history the drop-down was offering")

    # And the entry goes on remembering from there.
    t.click(entry)
    t.key("ctrl+a")
    t.type_text("alpha")
    accept(t, dialog)

    t.wait_selection(view, ALPHA, "the search after the clearing still runs")

    dialog, _, arrow = find_dialog(t)
    t.click(arrow)

    popup = t.wait(lambda: history_popup(t),
                   "the drop-down to come back with the search that followed")
    names = [row.name for row in t.on_screen(t.find_all(popup, role="table cell"))]

    t.check(names == ["alpha"],
            "and it offers that search alone: %s" % names)

    accept(t, dialog)


def find_dialog(t):
    """Open Find, and hand back the dialog, its entry and its arrow button."""
    t.menu("Search", "Find")
    dialog = t.dialog("Find")
    entry = t.need(dialog, role="text", what="the entry of the Find dialog")
    return dialog, entry, drop_down(t, dialog, entry)


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

    x, _, _, _ = t.extents(nameless[0])
    ex, _, ew, _ = t.extents(entry)

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
