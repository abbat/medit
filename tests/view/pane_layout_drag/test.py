"""Several documents across split notebooks and split views.

# requires: MOO_GTK3

The individual tests cover splitting a view, moving one tab to the split
notebook, and reordering tabs. This scenario combines those operations in the
order a user reaches them: move two documents to the second notebook, reorder
its tabs, then split the document that is showing in that notebook twice.

The assertions use the accessibility tree for ownership and text, while the
tab drag uses the notebook's painted strip. No document is modified, so the
test needs no save dialog and creates no files outside its normal sandbox.
"""

from lib import a11y
from lib import input as ui

DOCUMENTS = ("alpha.txt", "bravo.txt", "charlie.txt", "delta.txt")
CONTENTS = {name: name + "\n" for name in DOCUMENTS}

MOVE = ("View", "Move to Split Notebook")
HORIZONTAL = ("View", "Split View Horizontally")
VERTICAL = ("View", "Split View Vertically")

STRIP_OFFSET = 17
STEP = 10


def setup(s):
    for name in DOCUMENTS:
        s.open(s.write("workdir/" + name, CONTENTS[name]))


def run(t):
    found = notebooks(t)
    t.check(len(found) == 1, "the window starts with one notebook")
    left = found[0]
    t.check(pages(left) == list(DOCUMENTS),
            "all four documents start in one notebook: %s" % ", ".join(pages(left)))

    # The last document is current. Move it, then click a document left behind
    # and move that one too. This exercises the notebook lookup after the first
    # notebook has acquired a sibling.
    t.menu(*MOVE)
    t.wait(lambda: len(notebooks(t)) == 2, "a second notebook to appear")
    left, right = notebooks(t)
    t.check(pages(left) == list(DOCUMENTS[:3]) and pages(right) == [DOCUMENTS[3]],
            "moving the current tab leaves %s and carries %s"
            % (pages(left), pages(right)))

    click_tab(t, left, DOCUMENTS[1], 3)
    t.wait(lambda: showing(t, left) == DOCUMENTS[1],
           "the document left behind to become current")
    t.menu(*MOVE)
    t.wait(lambda: pages(notebooks(t)[1]) == [DOCUMENTS[3], DOCUMENTS[1]],
           "the second document to join the split notebook")

    left, right = notebooks(t)
    t.check(pages(left) == [DOCUMENTS[0], DOCUMENTS[2]],
            "the other two documents remain in the first notebook: %s"
            % pages(left))

    # Reorder the two tabs in the second notebook through its painted tab strip.
    click_tab(t, right, DOCUMENTS[3], 2)
    t.wait(lambda: showing(t, right) == DOCUMENTS[3],
           "the first tab in the split notebook to be selected")
    reorder_tab(t, right, DOCUMENTS[3], DOCUMENTS[1])
    t.check(pages(right) == [DOCUMENTS[1], DOCUMENTS[3]],
            "dragging a tab reordered the split notebook: %s" % pages(right))
    t.check(showing(t, right) == DOCUMENTS[3],
            "the dragged document remains the one showing")

    # Split the currently shown document in two directions. It must remain one
    # document with several views, not become copies with independent buffers.
    t.menu(*HORIZONTAL)
    t.wait(lambda: len(views(t, right)) == 2, "a second view in the split notebook")
    t.menu(*VERTICAL)
    t.wait(lambda: len(views(t, right)) == 3, "a third view after the nested split")
    t.menu(*HORIZONTAL)
    t.wait(lambda: len(views(t, right)) == 4, "a fourth view after the second nested split")

    t.check(all(t.text(view) == CONTENTS[DOCUMENTS[3]] for view in views(t, right)),
            "all four views show the same document contents")


def notebooks(t):
    found = [node for node in t.find_all(t.frame, role="page tab list", depth=25)
             if ui.on_screen(node)]
    return sorted(found, key=lambda node: t.extents(node)[0])


def pages(notebook):
    return [page.name for page in a11y.children(notebook) if page.name]


def showing(t, notebook):
    for page in a11y.children(notebook):
        if page.name and ui.on_screen(page):
            return page.name
    return None


def click_tab(t, notebook, name, count):
    spans = tab_spans(t, notebook, count)
    left, right = spans[name]
    t.click_at((left + right) // 2, tab_y(t, notebook))


def reorder_tab(t, notebook, dragged, target):
    spans = tab_spans(t, notebook, 2)
    source = spans[dragged]
    destination = spans[target]
    t.drag_to((source[0] + source[1]) // 2, tab_y(t, notebook),
              (destination[0] + destination[1]) // 2, tab_y(t, notebook))


def tab_spans(t, notebook, count):
    x0, _, width, _ = t.extents(notebook)
    found = {}
    y = tab_y(t, notebook)

    for x in range(x0 + 2, x0 + width, STEP):
        t.click_at(x, y)
        current = showing(t, notebook)
        if current is None:
            continue
        low, high = found.get(current, (x, x))
        found[current] = (min(low, x), max(high, x))
        if len(found) == count:
            break

    return found


def tab_y(t, notebook):
    return t.extents(notebook)[1] + STRIP_OFFSET


def views(t, notebook):
    x, _, _, _ = t.extents(notebook)
    return [view for view in t.find_all(t.frame, role="text", depth=30)
            if ui.on_screen(view) and t.state(view, "editable")
            and t.extents(view)[0] >= x]
