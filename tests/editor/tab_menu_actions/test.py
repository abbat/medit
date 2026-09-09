"""What the rest of a tab's menu does: the path, the others, the new window.

# requires: MOO_GTK3

tests/editor/tab_popup pins which document the menu is about and drives Close;
these are the other three entries of notebook_populate_popup(), and each of them
does something a different piece of medit has to do properly.

Copy Full Path goes through the clipboard, so what says it worked is a paste.
Close All Others closes everything but the tab the menu was opened over. Detach
takes the document out into a window of its own, which is the same machinery as
File / New Window and had only ever been reached the other way.
"""

from lib import input as ui
from lib.notebook import order, showing, spans, strip

FIRST = "alpha.txt"
SECOND = "bravo.txt"
THIRD = "charlie.txt"

CONTENT = "a line\n"


def setup(s):
    for name in (FIRST, SECOND, THIRD):
        s.open(s.write("workdir/" + name, CONTENT))


def run(t):
    tabs = spans(t, 3)

    click_tab(t, tabs, SECOND)
    t.wait(lambda: showing(t) == SECOND, "the middle document to be the one showing")

    # Copy Full Path, read back by pasting it into that same document.
    t.choose(right_click_tab(t, tabs, SECOND), "Copy Full Path")

    view = t.document()
    t.focus()
    t.click(view)
    t.key("ctrl+End")
    t.key("ctrl+v")

    t.wait(lambda: t.text(view) == CONTENT + t.sandbox.path("workdir", SECOND),
           "the path of that document to be what was copied; the document holds %r"
           % t.text(view))

    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")

    # Close All Others, over the same tab.
    t.choose(right_click_tab(t, tabs, SECOND), "Close All Others")

    t.wait(lambda: order(t) == [SECOND],
           "everything but the tab the menu was opened over to close; "
           "the strip holds %s" % order(t))

    # And Detach, which needs a second document to be offered at all -- it and
    # Close All Others are only in the menu while more than one is open. Detach
    # is last because the window it makes comes up over this one, with no window
    # manager to put it anywhere else, and the tabs underneath cannot be clicked
    # after that.
    open_by_path(t, t.sandbox.path("workdir", FIRST))
    t.wait(lambda: sorted(order(t)) == sorted([SECOND, FIRST]),
           "a second document to be open again; the strip holds %s" % order(t))

    tabs = spans(t, 2)
    t.choose(right_click_tab(t, tabs, SECOND), "Detach")

    t.wait(lambda: len(t.frames()) == 2,
           "the document to be detached into a window of its own")

    # The new window comes up exactly over the old one -- there is no window
    # manager to put it anywhere else -- and everything after this, the runner's
    # own File/Quit included, would be clicking the wrong window.
    ui.move_window(ui.windows()[-1], 300, 250)

    holds = [order(t, frame) for frame in t.frames()]
    t.check([SECOND] in holds,
            "the new window holds the document that was detached: %s" % holds)
    t.check([FIRST] in holds,
            "and the old one holds the one that was not: %s" % holds)


def open_by_path(t, path):
    """Open the File/Open chooser and give it a path through its location entry."""
    t.menu("File", "Open...")
    t.need(t.app, role="file chooser", depth=2, what="the Open dialog")

    t.focus()
    t.key("ctrl+l")
    t.type_text(path)
    t.key("Return")

    t.wait(lambda: t.find(t.app, role="file chooser", depth=2) is None,
           "the Open dialog to close")


def click_tab(t, tabs, name):
    left, right = tabs[name]
    t.click_at((left + right) // 2, strip(t))


def right_click_tab(t, tabs, name):
    left, right = tabs[name]

    return t.popup_at_point((left + right) // 2, strip(t))
