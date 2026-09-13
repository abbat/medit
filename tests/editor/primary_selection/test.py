"""The primary selection is pasted by a middle click in the text view.

# requires: MOO_GTK3

_moo_text_view_ensure_primary() and the primary-selection branch of the button
handler are only used by the X11 selection convention.  The ordinary clipboard
test does not exercise them: Ctrl+C and Edit/Paste use the CLIPBOARD selection.
This test keeps the document unsaved, so it needs no input file.
"""

CONTENT = "alpha beta"
EXPECTED = "alpha alpha beta"


def run(t):
    t.menu("File", "New")

    view = t.document()
    t.click(view)
    t.type_text(CONTENT)
    t.key("ctrl+Home")
    for _ in range(5):
        t.key("shift+Right")
    t.wait_selection(view, (0, 5), "the first word to become the primary selection")

    # Put the caret between the two words.  A middle click there must paste the
    # PRIMARY selection, rather than the CLIPBOARD or nothing at all.
    x, y, width, height = t.range_extents(view, 6, 7)
    t.click_at(x + width // 2, y + height // 2, button=2)
    t.wait(lambda: t.text(view) == EXPECTED,
           "the primary selection to be pasted at the middle click; the document "
           "holds %r" % t.text(view))

    t.key("ctrl+w")
    dialog = t.need(t.app, role="alert", depth=2,
                    what="the dialog asking about the unsaved new document")
    t.click(t.button(dialog, "Discard"))
