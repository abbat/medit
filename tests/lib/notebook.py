"""Where the document tabs are, for the tests that click them.

The tabs are GtkNotebook's, and GtkNotebookAccessible puts every page in the
tree as a page tab named after its label -- so a tab can be asked where it is
drawn and whether it is the current one, and none of it has to be found by
clicking along the strip and watching what comes forward.

Shared because the answers are the same for every test that reads the strip,
and each of them would otherwise carry its own guesses about where one is.
"""

from . import input as ui


def the_notebook(t, frame=None):
    """The notebook the documents are in -- the one of that window that is drawn.

    frame says which window, for a test that has more than one; the window the
    test started with by default.
    """
    return [n for n in t.find_all(frame or t.frame, role="page tab list", depth=25)
            if ui.on_screen(n)][0]


def tabs(t, frame=None):
    """The tabs of that notebook, in the order the notebook holds them."""
    return t.find_all(the_notebook(t, frame), role="page tab", depth=1)


def drawn(t, frame=None):
    """The tabs that are on screen, in order.

    A strip too narrow for them all scrolls, and the tabs that are past either
    end of it are in the tree like any other but are not drawn.
    """
    return [tab for tab in tabs(t, frame) if ui.on_screen(tab)]


def strip(t, frame=None):
    """Where along the height of the window the tabs are drawn."""
    _, y, _, height = t.extents(drawn(t, frame)[0])
    return y + height // 2


def order(t, frame=None):
    """The documents as the notebook holds them, named after their tabs."""
    return [tab.name for tab in tabs(t, frame) if tab.name]


def showing(t, frame=None):
    """The current document: the one tab of the notebook that is selected."""
    for tab in tabs(t, frame):
        if tab.name and t.state(tab, "selected"):
            return tab.name

    return None


def tab_icon(t, name):
    """Where the document icon of a tab is drawn, as a point to drag from.

    Dragging a document out of the window starts at the icon -- the event box
    it sits in is the drag source, tab_icon_start_drag() in mooeditwindow.cpp
    -- while the rest of the tab drags the tab along the strip instead.

    The icon is not in the accessibility tree, and what a tab answers for its
    own extents is the extents of its label alone: GtkNotebookPageAccessible
    looks through the tab's box for the first label in it and reports that. So
    the icon is where the label is not, ICON to the left of where it starts.
    """
    for tab in drawn(t):
        if tab.name == name:
            x, y, _, height = t.extents(tab)
            return x - ICON, y + height // 2

    return t.fail("no tab named %s is drawn" % name)


# From the left edge of a tab's label to the middle of the icon before it: half
# a menu-size icon, and the spacing of the box the two are in.
ICON = 11


def spans(t, count):
    """Where each tab that is drawn begins and ends along the strip.

    count is how many are wanted, and the scan stops there. A notebook whose
    tabs do not all fit answers with the ones that are on screen: a caller asks
    for spans in order to click them, and a tab that is scrolled out of the
    strip is not there to be clicked.
    """
    found = {}

    for tab in drawn(t):
        if not tab.name:
            continue

        x, _, width, _ = t.extents(tab)
        found[tab.name] = (x, x + width - 1)

        if len(found) == count:
            break

    return found
