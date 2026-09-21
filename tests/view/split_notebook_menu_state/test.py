"""The View menu follows the document that has the focus, in either notebook.

# requires: MOO_GTK3

Wrap Text and Show Line Numbers are settings of a document, and the menu shows
those of the one in front. With two notebooks side by side a click can put the
focus in a document of the other notebook without any tab being switched, and
the menu has to follow that too: ticking Wrap Text in one document and clicking
into the other must leave the item unticked.
"""

from lib import input as ui

FIRST = "first.txt"
SECOND = "second.txt"

WRAP = "Wrap Text"
NUMBERS = "Show Line Numbers"


def setup(s):
    s.pref("Editor/wrap_enable", False)
    s.pref("Editor/show_line_numbers", False)

    s.open(s.write("workdir/" + FIRST, "a line\n"))
    s.open(s.write("workdir/" + SECOND, "a line\n"))


def run(t):
    t.menu("View", "Move to Split Notebook")
    t.wait(lambda: len(views(t)) == 2 and len(notebooks(t)) == 2,
           "the document to move into a notebook of its own")

    left, right = views(t)

    # The one that moved has the focus: change both settings there.
    t.click(right)
    t.menu("View", WRAP)
    t.menu("View", NUMBERS)

    t.check(ticked(t, WRAP) and ticked(t, NUMBERS),
            "both items are ticked for the document they were set in")

    # Now into the other notebook, by a click and nothing else.
    t.click(left)

    t.wait(lambda: not ticked(t, WRAP),
           "Wrap Text to be unticked for the document that does not wrap")
    t.check(not ticked(t, NUMBERS),
            "and Show Line Numbers for the one without numbers")

    t.click(right)

    t.wait(lambda: ticked(t, WRAP) and ticked(t, NUMBERS),
           "both to be ticked again back in the first document")


def notebooks(t):
    return [n for n in t.find_all(t.frame, role="page tab list", depth=25)
            if ui.on_screen(n)]


def views(t):
    """The two documents on screen, left to right."""
    found = [v for v in t.find_all(t.frame, role="text", depth=25)
             if ui.on_screen(v) and t.state(v, "editable")]

    return sorted(found, key=lambda node: t.extents(node)[0])


def ticked(t, name):
    """Whether the item is ticked, read with the menu open and closed again."""
    menu = t.need(t.frame, role="menu", name="View", what="the View menu")
    t.click(menu)

    item = t.need(menu, role="check menu item", name=name, what="the %r item" % name)
    state = t.state(item, "checked")

    ui.key("Escape")

    return state
