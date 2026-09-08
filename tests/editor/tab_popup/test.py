"""A tab's context menu is about that tab, whichever document is showing.

# requires: MOO_GTK3

moo_notebook_maybe_popup() finds the tab under the pointer and hands it to the
menu; the button-3 branch of moo_notebook_button_press() does not switch to it
first. So a right click on a tab that is not the current one opens a menu about
a document that is not on screen, and every entry in it has to act on that
document rather than on the one the user is looking at.

That is the whole test, and it is a distinction nothing else would catch: a menu
wired to the current document instead would look identical, work correctly every
time the user right-clicked the tab they were already in, and close the wrong
document the first time they did not.

The entries come from notebook_populate_popup() in mooeditwindow.cpp, and two of
them are only there when more than one document is open.
"""

from lib.notebook import order, showing, spans, strip

FIRST = "alpha.txt"
SECOND = "bravo.txt"
THIRD = "charlie.txt"

ENTRIES = ["Close", "Close All Others", "Copy Full Path", "Detach"]


def setup(s):
    for name in (FIRST, SECOND, THIRD):
        s.open(s.write("workdir/" + name, name + "\n"))


def run(t):
    tabs = spans(t, 3)
    t.log("the tabs are at %s" % tabs)

    # The scan left some document showing; make it the last one, so that the tab
    # right-clicked below is not the one on screen.
    click_tab(t, tabs, THIRD)
    t.wait(lambda: showing(t) == THIRD, "the last document to be the one showing")

    menu = right_click_tab(t, tabs, FIRST)

    t.check(showing(t) == THIRD,
            "a right click on another tab did not switch to it: %s is still showing"
            % THIRD)

    labels = [name for name in (item.name for item in t.find_all(menu, depth=1)) if name]
    t.log("the menu offers %s" % ", ".join(labels))

    for entry in ENTRIES:
        t.check(entry in labels, "the tab menu offers %r" % entry)

    # And it acts on the tab it was opened over, not on the document on screen.
    t.choose(menu, "Close")

    t.wait(lambda: order(t) == [SECOND, THIRD],
           "the right-clicked document to be the one that closed")
    t.check(showing(t) == THIRD,
            "and %s, which was showing and was not the tab clicked, is still open "
            "and still showing; it is %s" % (THIRD, showing(t)))


def click_tab(t, tabs, name):
    left, right = tabs[name]
    t.click_at((left + right) // 2, strip(t))


def right_click_tab(t, tabs, name):
    left, right = tabs[name]
    return t.popup_at_point((left + right) // 2, strip(t))
