"""Save a dropped document here, save a copy, and cancel the drop.

# requires: MOO_GTK3

The drop test covers the no-modifier menu and chooses Move Here. The other
actions call different document operations and were not reached: Save Here
changes the document's file, Save Copy leaves it alone, and cancelling the
menu must leave both the document and the files untouched. With no modifier,
both save actions use the dropped document's basename directly and do not ask
for another name.
"""

from lib import input as ui
from lib.notebook import spans, strip

INNER = "inner"
CONTENT = "the document being dropped\n"

SAVE_HERE = "save-here.txt"
SAVE_COPY = "save-copy.txt"
NAMES = (SAVE_HERE, SAVE_COPY)


def setup(s):
    s.write("workdir/%s/deep.txt" % INNER, "keep the directory visible\n")
    for name in NAMES:
        s.open(s.write("workdir/" + name, CONTENT))


def run(t):
    view = open_the_pane(t)
    enter_inner(t, view)

    cancel(t, view)
    save_here(t, view)
    save_copy(t, view)


def save_here(t, view):
    drop(t, view, SAVE_HERE)
    menu = t.wait(lambda: dropped_menu(t), "the Save Here drop menu")
    t.choose(menu, "Save Here")

    t.wait(lambda: t.sandbox.exists("workdir", INNER, SAVE_HERE),
           "Save Here to write the target file")
    t.check(t.sandbox.read("workdir", INNER, SAVE_HERE) == CONTENT,
            "Save Here wrote the document contents")
    t.check(t.sandbox.exists("workdir", SAVE_HERE),
            "Save Here left the original file in place")
    t.check(INNER + "/" + SAVE_HERE in (t.frame.name or ""),
            "Save Here changed the document path to the target")


def save_copy(t, view):
    drop(t, view, SAVE_COPY)
    menu = t.wait(lambda: dropped_menu(t), "the Save Copy drop menu")
    t.choose(menu, "Save Copy")

    t.wait(lambda: t.sandbox.exists("workdir", INNER, SAVE_COPY),
           "Save Copy to write the target file")
    t.check(t.sandbox.read("workdir", INNER, SAVE_COPY) == CONTENT,
            "Save Copy wrote the document contents")
    t.check(t.sandbox.exists("workdir", SAVE_COPY),
            "Save Copy left the original file in place")
    t.check(INNER + "/" + SAVE_HERE not in (t.frame.name or ""),
            "Save Copy did not change the document to the previous target")


def cancel(t, view):
    drop(t, view, SAVE_HERE)
    t.wait(lambda: dropped_menu(t), "the drop menu to cancel")
    t.escape()

    t.check(not t.sandbox.exists("workdir", INNER, SAVE_HERE),
            "cancelling left the target file absent")
    t.check(t.sandbox.exists("workdir", SAVE_HERE),
            "cancelling left the original file in place")
    t.check(t.text(t.document()) == CONTENT,
            "cancelling left the document contents unchanged")


def open_the_pane(t):
    t.menu("View", "Panes", "File Selector")
    t.pin_pane()
    t.click(t.need(t.frame, role="push button", name="Jump to",
                   what="the button that goes to the document's directory"))
    return t.wait(lambda: icon_view(t), "the file selector icon view")


def enter_inner(t, view):
    entry = path_entry(t, view)
    target = "%s/inn" % t.sandbox.path("workdir")

    t.click(entry)
    t.key("ctrl+a")
    t.type_text(target)
    t.key("Tab")
    t.key("Return")
    t.wait(lambda: t.text(entry).rstrip("/").endswith("/" + INNER),
           "the file selector to enter the target directory")


def drop(t, view, name):
    tabs = spans(t, len(NAMES))
    left, _ = tabs[name]
    x, y, width, height = t.extents(view)
    t.drag_to(left + 12, strip(t), x + width // 2, y + height // 2)


def dropped_menu(t):
    for top in t.find_all(t.app, depth=1):
        for menu in t.find_all(top, role="menu", depth=1):
            if ui.on_screen(menu):
                return menu
    return None


def path_entry(t, view):
    vx, vy, _, _ = t.extents(view)
    for node in t.find_all(t.frame, role="text", depth=30):
        if ui.on_screen(node):
            x, y, _, _ = t.extents(node)
            if abs(x - vx) <= 4 and y < vy:
                return node
    return t.fail("the file selector has no path entry")


def icon_view(t):
    found = [node for node in t.find_all(t.frame, role="unknown", depth=30)
             if ui.on_screen(node)]
    return found[0] if len(found) == 1 else None
