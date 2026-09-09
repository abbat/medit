"""Previous Tab and Next Tab walk the strip and wrap round at both ends.

# requires: MOO_GTK3

action_previous_tab() and action_next_tab() in mooeditwindow.cpp. Walking one
step is plumbing; the wrap is the decision, and it is written twice, once at each
end -- next from the last tab goes to the first, and previous from the first goes
to the last through switch_to_tab(window, -1), which reads as "no tab" and means
"the last one". Both ends are asserted here, and so is the ordinary step between
them, because a wrap that worked and a step that did not would look the same
from one press.

The menu is what is used rather than Alt+Left and Alt+Right, since this is a test
of the Window menu; the accelerators reach the same two functions.

The menu also holds a "No Documents" placeholder, hidden by
"!has-open-document", and the last part of the test is about why it stays hidden:
closing every document does not leave the window empty.
moo_editor_close_docs() puts a fresh untitled one in when the last goes, unless
the editor allows an empty window, which medit does not. So the placeholder is
for a configuration medit is not, and what a user sees after closing everything
is a new empty document.

Whether an item is in the menu has to be read as whether it is drawn: an item a
condition has hidden is still in the accessibility tree, with its name.
"""

from lib.notebook import order, showing

DOCS = ["one.txt", "two.txt", "three.txt"]

NO_DOCUMENTS = "No Documents"

UNTITLED = "Untitled"


def setup(s):
    for name in DOCS:
        s.open(s.write("workdir/" + name, name + "\n"))


def run(t):
    t.check(order(t) == DOCS, "three documents, in the order they were opened")
    t.check(showing(t) == DOCS[-1], "the last is showing")

    t.check(NO_DOCUMENTS not in items(t),
            "the menu says nothing about having no documents while three are open")
    t.escape()

    # From the last tab, forwards: round to the first.
    step(t, "Next Tab", DOCS[0], "Next Tab from the last tab wraps to the first")

    # An ordinary step, so that the wrap above is not the only thing proved.
    step(t, "Next Tab", DOCS[1], "Next Tab from the first goes to the second")

    # And back off the front: round to the last.
    step(t, "Previous Tab", DOCS[0], "Previous Tab goes back to the first")
    step(t, "Previous Tab", DOCS[-1], "Previous Tab from the first wraps to the last")

    # Closing everything does not empty the window: moo_editor_close_docs() puts a
    # fresh document in when the last one goes, unless the editor was told to
    # allow an empty window, which medit does not do. So the "No Documents"
    # placeholder in this menu is not reachable this way, and the test says what
    # does happen instead.
    #
    # The focus first, because the menu above took it into a window that no
    # longer exists and there is no window manager to hand it back -- Ctrl+W
    # would go nowhere.
    t.focus()

    for left in (2, 1):
        t.key("ctrl+w")
        t.wait(lambda n=left: len(order(t)) == n,
               "a document to close, leaving %d; the strip is %s" % (left, order(t)))

    t.key("ctrl+w")

    t.wait(lambda: order(t) == [UNTITLED],
           "the last close to leave a fresh untitled document; the strip is %s"
           % order(t))
    t.log("ok: closing the last document leaves a new one rather than nothing")

    t.check(showing(t) == UNTITLED, "and it is the document showing")
    t.check(t.text(t.document()) == "", "and it is empty")

    t.check(NO_DOCUMENTS not in items(t),
            "so the menu still does not offer %r: there is a document open"
            % NO_DOCUMENTS)

    t.escape()


def step(t, entry, expected, what):
    t.menu("Window", entry)
    t.wait(lambda: showing(t) == expected, "%s; %s is showing" % (what, showing(t)))
    t.log("ok: %s" % what)


def the_menu(t):
    menu = t.need(t.frame, role="menu", name="Window", what="the Window menu")
    t.click(menu)

    return menu


def items(t):
    """The entries of the open menu that are actually drawn.

    Filtered by that on purpose: an item hidden by a condition -- "No Documents"
    is hidden by "!has-open-document" -- stays in the accessibility tree with
    its name, so being in the tree says nothing about being in the menu.
    """
    return [item.name for item in t.on_screen(t.find_all(the_menu(t), depth=1))
            if item.name]
