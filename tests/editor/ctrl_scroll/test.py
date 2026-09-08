"""Ctrl with an arrow scrolls the view and leaves the cursor where it was.

# requires: MOO_GTK3

handle_ctrl_up() and handle_ctrl_pgup() in mootextview-input.c. GtkTextView has
no such binding: Ctrl+Up there moves the cursor by a paragraph. Here the cursor
does not move at all and the view goes by one line, or by one page for
Ctrl+Page_Down -- which is the whole point of it, a way to look further down a
file without losing the place being edited.

The distances are read off the scroll bar rather than counted in pixels, because
a line is as tall as the font: what the test asserts is that three presses move
by three equal steps, that a page is further than a line, and that the walk back
stops at the top rather than going past it.
"""

from lib import input as ui

LINES = 200

CONTENT = "".join("line %d\n" % i for i in range(LINES))


def setup(s):
    s.open(s.write("workdir/long.txt", CONTENT))


def run(t):
    view = t.document()
    bar = scroll_bar(t)

    value, low, high = t.value(bar)
    t.check(high > low, "the document is long enough to scroll: %s to %s" % (low, high))

    t.click(view)
    t.key("ctrl+Home")
    t.wait_caret(view, 0, "the cursor is at the start of the document")
    t.wait(lambda: t.value(bar)[0] == low, "the view to be at the top")

    steps = []

    for _ in range(3):
        before = t.value(bar)[0]
        t.key("ctrl+Down")
        t.wait(lambda: t.value(bar)[0] > before,
               "Ctrl+Down to scroll down from %s" % before)
        steps.append(t.value(bar)[0] - before)

    t.check(steps[0] > 0, "Ctrl+Down scrolls down, by %s" % steps[0])
    t.check(steps[1] == steps[0] and steps[2] == steps[0],
            "each press moves the same distance: %s" % ", ".join(str(s) for s in steps))
    t.wait_caret(view, 0, "and the cursor has not moved through any of it")

    before = t.value(bar)[0]
    t.key("ctrl+Page_Down")
    t.wait(lambda: t.value(bar)[0] > before, "Ctrl+Page_Down to scroll down further")

    page = t.value(bar)[0] - before
    t.check(page > steps[0], "a page is further than a line: %s against %s" % (page, steps[0]))
    t.wait_caret(view, 0, "and the cursor is still where it was")

    # Back up, past the top: the clamp is a branch of its own in both functions.
    t.key("ctrl+Page_Up")
    t.key("ctrl+Page_Up")
    t.wait(lambda: t.value(bar)[0] == low,
           "the view to stop at the top rather than scroll past it; it is at %s"
           % t.value(bar)[0])
    t.log("ok: scrolling up stops at the top")

    t.key("ctrl+Up")
    t.check(t.value(bar)[0] == low, "and Ctrl+Up at the top leaves it there")
    t.wait_caret(view, 0, "the cursor never moved")


def scroll_bar(t):
    """The document's vertical scroll bar: the tall one that is on screen."""
    found = [bar for bar in t.find_all(t.frame, role="scroll bar", depth=25)
             if ui.on_screen(bar) and t.extents(bar)[3] > t.extents(bar)[2]]

    t.check(len(found) == 1, "the document has one vertical scroll bar on screen")

    return found[0]
