"""Clicking a tab brings its document forward with the keyboard, and closing one
goes back to the document that was open before it.

# requires: MOO_GTK3

Two behaviours, neither watched, and the second is not what it looks like.

moo_notebook_button_press() does not only switch the page when a tab is clicked:
it moves the focus into that page's child, so the next key goes to the document
that came forward. A notebook that switched the page and left the focus behind
would look right in a screenshot and lose whatever was typed next.

Closing a document does not go to a neighbour. _moo_edit_window_remove_doc()
sets the active document from window->priv->history, which
moo_edit_window_update_doc_list() keeps as the documents most recently made
active -- two of them, no more. So closing a document returns to the one that
was showing before it, wherever its tab happens to be. The test tells the two
rules apart on purpose: it visits the leftmost document, then the rightmost, and
closes the rightmost, where a neighbour rule would show the middle one and this
one shows the leftmost.

The notebook has an answer of its own for when there is no history --
find_next_visible_page() -- and it is not what decides here. Turning the walk in
it around changes nothing about this test, which is how the mechanism above came
to be read properly rather than guessed at.
"""

from lib.notebook import order, showing, spans, strip

FIRST = "alpha.txt"
SECOND = "bravo.txt"
THIRD = "charlie.txt"

BODIES = {FIRST: "a\n", SECOND: "b\n", THIRD: "c\n"}


def setup(s):
    # In this order, so the strip reads left to right the same way.
    for name in (FIRST, SECOND, THIRD):
        s.open(s.write("workdir/" + name, BODIES[name]))


def run(t):
    t.check(order(t) == [FIRST, SECOND, THIRD],
            "three documents, in the order they were opened")
    t.check(showing(t) == THIRD, "the one opened last is the one showing")

    tabs = spans(t, 3)
    t.log("the tabs are at %s" % tabs)

    click_tab(t, tabs, FIRST)
    t.wait(lambda: showing(t) == FIRST, "the clicked tab's document to come forward")
    t.log("ok: clicking a tab brings its document forward")

    # The focus went with it, so this is typed into that document and not into
    # the one that was showing before.
    t.type_text("x")
    view = t.document()
    t.wait(lambda: t.text(view) == "x" + BODIES[FIRST],
           "what was typed to reach the document whose tab was clicked;\n"
           "it holds %r" % t.text(view))
    t.log("ok: the click moved the focus into the document, not only the page")

    # Undone and saved, or closing below turns into a dialog about the unsaved
    # change rather than a close.
    t.key("ctrl+z")
    t.wait(lambda: t.text(t.document()) == BODIES[FIRST], "the typing to be undone")
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")

    click_tab(t, tabs, THIRD)
    t.wait(lambda: showing(t) == THIRD, "the last document to come forward again")

    t.key("ctrl+w")
    t.wait(lambda: order(t) == [FIRST, SECOND], "the closed document to leave the strip")

    t.check(showing(t) == FIRST,
            "closing it went back to %s, the document open before it, and not to "
            "%s, which is the tab next to the one that closed" % (FIRST, SECOND))


def click_tab(t, tabs, name):
    """Click the middle of what the scan found for that tab."""
    left, right = tabs[name]
    t.click_at((left + right) // 2, strip(t))
