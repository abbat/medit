"""The font dialog behind the Preferences font button.

# requires: MOO_GTK3

src/mooutils/moofontsel.c is medit's own copy of GTK's font selection, kept
because GTK's has no way to show only the fonts an editor can use, and no test
had ever opened it. The button on the General page of the preferences is the only
way in.

What the filter does is the assertion that matters: the editor's button asks for
monospace, so the dialog comes up with "Show only fixed width fonts" ticked, and
unticking it has to change what the list holds -- that is
moo_font_selection_filter_changed() rebuilding the model, which is the whole
reason this file is not GTK's. What is compared is the names on screen rather
than a count of the whole model: a GtkTreeView describes the rows it is showing,
so a list that grows below the fold looks the same from here, and a list that
starts with different families does not. The rest is the round trip: pick a
family, accept it, and the button says what was picked.
"""

FILTER = "Show only fixed width fonts"

# The buttons of the preferences dialog itself, which are not on the page.
DIALOG_BUTTONS = ("Help", "Apply", "Cancel", "OK")


def setup(s):
    s.open(s.write("workdir/notes.txt", "alpha\n"))


def run(t):
    prefs = t.preferences("General")

    button = font_button(t, prefs)
    t.click(button)

    dialog = t.dialog("Pick a Font")

    check = t.need(dialog, role="check box", name=FILTER, what="the fixed width box")
    t.check(t.state(check, "checked"),
            "the editor's button asks for monospace, so the filter starts on")

    fixed = families(t, dialog)
    t.check(fixed, "the filtered list has something in it: %s" % ", ".join(fixed))

    t.click(check)
    every = t.wait(lambda: differs_from(t, dialog, fixed),
                   "unticking the filter to change what the list holds")
    t.log("ok: with the filter off the list starts %s" % ", ".join(every[:4]))

    t.click(check)
    back = t.wait(lambda: differs_from(t, dialog, every),
                  "and ticking it again to filter the list once more")
    t.log("ok: with the filter on again the list starts %s" % ", ".join(back[:4]))

    # Which of them are fixed width is not something the accessibility tree can
    # say, and the toggling above is what this test can prove: the model is
    # rebuilt each way round. The list is scrolled to wherever the selection is,
    # so it is not the same rows twice, which is why the two lists are compared
    # for being different rather than for being equal to anything.

    chosen = back[0]
    t.click(cell(t, dialog, chosen))

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Pick a Font")

    t.wait(lambda: chosen in (button.name or ""),
           "the button to say which font was picked; it says %r" % button.name)

    t.click(t.button(prefs, "OK"))
    t.no_toplevel("Preferences")


def font_button(t, prefs):
    """The button on the page, which is the one that is not the dialog's own."""
    buttons = [b for b in t.on_screen(t.find_all(prefs, role="push button"))
               if b.name not in DIALOG_BUTTONS]

    t.check(len(buttons) == 1,
            "the General page has one button, for the font: %s"
            % ", ".join(sorted(b.name for b in buttons)))

    return buttons[0]


def cells(t, dialog):
    """Every list cell of the dialog that is drawn, with where it is drawn."""
    return [(t.extents(node)[0], node)
            for node in t.on_screen(t.find_all(dialog, role="table cell"))
            if node.name]


def families(t, dialog):
    """The names in the leftmost of the dialog's three lists.

    Families, styles and sizes are three lists side by side, so the column a cell
    is in is which list it belongs to.
    """
    found = cells(t, dialog)

    if not found:
        return []

    left = min(x for x, _ in found)

    return [node.name for x, node in found if x == left]


def cell(t, dialog, name):
    for x, node in cells(t, dialog):
        if node.name == name:
            return node

    raise AssertionError("no %r in the dialog's lists" % name)


def differs_from(t, dialog, before):
    found = families(t, dialog)

    return found if found and found != before else None
