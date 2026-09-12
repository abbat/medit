"""Save a dropped document here, save a copy, and cancel the chooser.

# requires: MOO_GTK3

The drop test covers the no-modifier menu and chooses Move Here. The other
actions call different document operations and were not reached: Save Here
changes the document's file, Save Copy leaves it alone, and cancelling either
chooser must leave both the document and the files untouched.
"""

from lib import input as ui
from lib.notebook import spans, strip

INNER = "inner"
CONTENT = "the document being dropped\n"

SAVE_HERE = "save-here.txt"
SAVE_COPY = "save-copy.txt"
CANCEL = "cancel.txt"
NAMES = (SAVE_HERE, SAVE_COPY, CANCEL)


def setup(s):
    s.write("workdir/%s/deep.txt" % INNER, "keep the directory visible\n")
    for name in NAMES:
        s.open(s.write("workdir/" + name, CONTENT))


def run(t):
    view = open_the_pane(t)
    enter_inner(t, view)

    save_here(t, view)
    save_copy(t, view)
    cancel(t, view)


def save_here(t, view):
    drop(t, view, SAVE_HERE)
    menu = dropped_menu(t)
    t.choose(menu, "Save Here")
    finish_save_dialog(t, "Save As")

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
    menu = dropped_menu(t)
    t.choose(menu, "Save Copy")
    finish_save_dialog(t, "Save Copy As")

    t.wait(lambda: t.sandbox.exists("workdir", INNER, SAVE_COPY),
           "Save Copy to write the target file")
    t.check(t.sandbox.read("workdir", INNER, SAVE_COPY) == CONTENT,
            "Save Copy wrote the document contents")
    t.check(t.sandbox.exists("workdir", SAVE_COPY),
            "Save Copy left the original file in place")
    t.check(INNER + "/" + SAVE_HERE not in (t.frame.name or ""),
            "Save Copy did not change the document to the previous target")


def cancel(t, view):
    drop(t, view, CANCEL)
    menu = dropped_menu(t)
    t.choose(menu, "Save Here")
    dialog = t.need(t.app, role="file chooser", name="Save As", depth=2,
                    what="the Save As chooser for the cancelled drop")
    t.escape()
    t.no_toplevel("Save As", role="file chooser")

    t.check(not t.sandbox.exists("workdir", INNER, CANCEL),
            "cancelling left the target file absent")
    t.check(t.sandbox.exists("workdir", CANCEL),
            "cancelling left the original file in place")
    t.check(t.text(t.document()) == CONTENT,
            "cancelling left the document contents unchanged")
    t.check(dialog is not None, "the cancelled action opened the Save As chooser")


def open_the_pane(t):
    t.menu("View", "Panes", "File Selector")
    t.pin_pane()
    t.click(t.need(t.frame, role="push button", name="Jump to",
                   what="the button that goes to the document's directory"))
    return t.wait(lambda: icon_view(t), "the file selector icon view")


def enter_inner(t, view):
    x, y, _, _ = t.extents(view)
    t.click_at(x + 20, y + 8, times=2)
    t.wait(lambda: t.text(path_entry(t)).endswith("/" + INNER),
           "the file selector to enter the target directory")


def drop(t, view, name):
    tabs = spans(t, len(NAMES))
    left, _ = tabs[name]
    x, y, width, height = t.extents(view)
    t.drag_to(left + 12, strip(t), x + width // 2, y + height // 2)


def finish_save_dialog(t, title):
    t.need(t.app, role="file chooser", name=title, depth=2,
           what="the %s chooser" % title)
    t.key("Return")
    t.no_toplevel(title, role="file chooser")


def dropped_menu(t):
    for top in t.find_all(t.app, depth=1):
        for menu in t.find_all(top, role="menu", depth=1):
            if ui.on_screen(menu):
                return menu
    return None


def path_entry(t):
    view = icon_view(t)
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
