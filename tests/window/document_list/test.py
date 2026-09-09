"""The Window menu lists the open documents by name, and ticks the current one.

# requires: MOO_GTK3

populate_window_menu() in mooeditwindow.cpp. Three things in it are decisions
rather than plumbing, and nothing watched any of them.

The list is sorted by name -- compare_docs_for_menu() collates the basenames --
and not by the order the documents are in on the tab strip. The test opens three
files in an order that is not their alphabetical one, so the menu and the strip
disagree, and asserts both.

Each entry is a check item drawn as a radio with the current document ticked, so
the menu says which document you are in as well as which are open. And choosing
one switches to it, which is the only reason the menu exists.
"""

from lib.notebook import order, showing

# Opened in this order, so the strip reads zulu, alpha, mike ...
OPENED = ["zulu.txt", "alpha.txt", "mike.txt"]

# ... and the menu, sorted, reads alpha, mike, zulu.
SORTED = sorted(OPENED)


def setup(s):
    for name in OPENED:
        s.open(s.write("workdir/" + name, name + "\n"))


def run(t):
    t.check(order(t) == OPENED, "the strip is in the order the files were opened")
    t.check(showing(t) == OPENED[-1], "the one opened last is showing")

    listed = documents_in_menu(t)

    t.check(listed == SORTED,
            "the menu lists them by name: %s" % ", ".join(listed))
    t.check(listed != order(t),
            "which is not the order the strip has them in: %s" % ", ".join(order(t)))

    # Read once: ticked() opens the menu to look, and opening a menu that is
    # already open closes it.
    state = ticked(t)
    t.check(state == [OPENED[-1]],
            "and the current document is the one ticked: %s" % state)

    # Choosing one is what the menu is for.
    t.escape()
    t.menu("Window", OPENED[0])

    t.wait(lambda: showing(t) == OPENED[0],
           "the document chosen from the menu to come forward; %s is showing"
           % showing(t))
    t.log("ok: choosing a document from the menu switches to it")

    state = ticked(t)
    t.check(state == [OPENED[0]], "and the tick moved with it: %s" % state)


def the_menu(t):
    """The Window menu, opened. Its items are only right once it is up."""
    menu = t.need(t.frame, role="menu", name="Window", what="the Window menu")
    t.click(menu)

    return menu


def documents_in_menu(t):
    """The document entries, in the order the menu has them.

    They are the check items: Previous Tab and Next Tab are plain ones, and
    "No Documents" is hidden while any document is open.
    """
    return [item.name for item in t.find_all(the_menu(t), depth=1)
            if t.role(item) == "check menu item" and item.name]


def ticked(t):
    """Those of them that are ticked, which should be exactly the current one."""
    return [item.name for item in t.find_all(the_menu(t), depth=1)
            if t.role(item) == "check menu item" and t.state(item, "checked")]
