"""Where the document tabs are, for the tests that click them.

The tab strip is MooNotebook's own, drawn on a GdkWindow rather than built out
of widgets, so nothing in the accessibility tree points at a tab. What is in the
tree is the pages, named after their tabs by MooNotebookAccessible -- so which
document is which is readable, and where its tab is drawn is not. It has to be
found by clicking along the strip and asking which page came forward.

Shared because three tests need the same four answers out of it, and each of
them would otherwise carry the same guesses about where a strip is.
"""

from . import input as ui


# The strip runs from the top of the notebook to the top of the page. Sampled
# at a fixed offset into it rather than at the middle of a tab: a tab that is
# not the current one is drawn a couple of pixels lower, and this is inside
# both.
STRIP = 17

# How finely to look along the strip for the edges of a tab. The step bounds how
# well an edge is known, and a caller should stay well inside what it finds.
STEP = 12


def the_notebook(t):
    """The notebook the documents are in -- the one that is on screen."""
    return [n for n in t.find_all(t.frame, role="page tab list", depth=25)
            if ui.on_screen(n)][0]


def strip(t):
    """Where along the height of the window the tabs are drawn."""
    return t.extents(the_notebook(t))[1] + STRIP


def order(t):
    """The documents as the notebook holds them, named after their tabs."""
    return [n.name for n in t.find_all(the_notebook(t), depth=1)]


def showing(t):
    """The one page of the notebook that is drawn: the current document."""
    for page in t.find_all(the_notebook(t), depth=1):
        if ui.on_screen(page):
            return page.name

    return None


def spans(t, count):
    """Click along the strip and note which page each x brings forward.

    Stops as soon as that many tabs have answered, so the span of every tab but
    the last one found is complete and nothing further along is clicked. How far
    to look is not fixed: the scan runs to the width of the notebook, so a
    machine whose font makes the tabs wider costs a few more clicks rather than
    a failure.

    Read from the page that is showing rather than from the window title: the
    title follows the document that has the focus, which after a run of clicks
    on the strip is not reliably the one whose tab was last clicked.
    """
    x0, _, width, _ = t.extents(the_notebook(t))
    found = {}

    for x in range(x0 + 2, x0 + width, STEP):
        t.click_at(x, strip(t))
        name = showing(t)

        if name is None:
            continue

        low, high = found.get(name, (x, x))
        found[name] = (min(low, x), max(high, x))

        if len(found) == count:
            break

    return found
