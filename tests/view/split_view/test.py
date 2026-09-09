"""Splitting the view gives two windows onto one document.

# requires: MOO_GTK3

_moo_edit_tab_set_split_horizontal() and its vertical twin, through the View
menu's two toggles. What comes out is a second MooEditView over the same MooEdit,
which is the point: both are views of one buffer, so what is typed in one appears
in the other and there is still only one document open. A split that made a copy
of the text instead would look identical until something was typed.

Unsplitting takes the extra view away again and the document survives it, which
is worth asserting because it is where the code has to detach a view from a
document that is staying.

Focus Next Split View moves between them; where the focus is is read from where
typing lands, since that is the only thing about a focus that matters.
"""

from lib.notebook import order

HORIZONTAL = "Split View Horizontally"
VERTICAL = "Split View Vertically"
CYCLE = "Focus Next Split View"

NAME = "notes.txt"

BODY = "one line\n"

TYPED = "x"


def setup(s):
    s.open(s.write("workdir/" + NAME, BODY))


def run(t):
    t.check(len(views(t)) == 1, "one view of the document to start with")

    t.menu("View", HORIZONTAL)

    t.wait(lambda: len(views(t)) == 2,
           "a second view to appear; there are %d" % len(views(t)))
    t.log("ok: splitting the view gives a second one")

    t.check(pages(t) == [NAME],
            "and it is still one document: the strip has %s" % order(t))

    first, second = views(t)

    t.check(t.text(first) == BODY and t.text(second) == BODY,
            "both views show the document")

    # One buffer, not two copies of the text.
    t.focus()
    t.click(first)
    t.key("ctrl+Home")
    t.type_text(TYPED)

    t.wait(lambda: t.text(views(t)[1]) == TYPED + BODY,
           "what was typed in one view to appear in the other; it holds %r"
           % t.text(views(t)[1]))
    t.log("ok: the two views are two windows onto one buffer")

    # The focus moves between them, which is readable from where typing goes.
    t.key("ctrl+Home")
    t.menu("View", CYCLE)
    t.type_text("y")

    t.wait(lambda: t.text(views(t)[0]).startswith("y"),
           "the cursor of the other view to be where the typing went; the document "
           "holds %r" % t.text(views(t)[0]))
    t.log("ok: Focus Next Split View moves the keyboard to the other view")

    # And back to one, with the document intact.
    t.menu("View", HORIZONTAL)

    t.wait(lambda: len(views(t)) == 1,
           "the extra view to go away; there are %d" % len(views(t)))
    t.log("ok: switching the split off leaves one view")

    t.check(pages(t) == [NAME],
            "the document is still open: the strip has %s" % order(t))
    t.check(t.text(t.document()).endswith(BODY),
            "and still holds its text: %r" % t.text(t.document()))

    # Vertically, which is a different pair of panes underneath.
    t.menu("View", VERTICAL)

    t.wait(lambda: len(views(t)) == 2, "a second view from the vertical split")
    t.log("ok: the vertical split gives a second view too")

    t.menu("View", VERTICAL)
    t.wait(lambda: len(views(t)) == 1, "and switching that off leaves one")

    # Saved, or the quit at the end of the test turns into a dialog about it.
    t.focus()
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")


def pages(t):
    """The documents on the strip, without the mark an unsaved one carries.

    order() gives what is drawn on the tab, and a document with unsaved changes
    is drawn with an asterisk in front of its name -- which this test creates
    for itself by typing, so the name alone is what it wants to compare.
    """
    return [name.lstrip("*") for name in order(t)]


def views(t):
    """The editable text views on screen, in the order the tree has them."""
    return [view for view in t.on_screen(t.find_all(t.frame, role="text", depth=30))
            if t.state(view, "editable")]
