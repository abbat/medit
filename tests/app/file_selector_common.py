"""Small, display-size independent helpers shared by file selector tests."""

from lib import input as ui

FIRST_ROW = 12
INTO_ROW = 20


def open_pane(t):
    t.menu("View", "Panes", "File Selector")
    t.pin_pane()
    t.click(t.need(t.frame, role="push button", name="Jump to",
                   what="the button that goes to the document's directory"))
    return t.wait(lambda: icon_view(t), "the icon view of the file selector")


def icon_view(t):
    found = [n for n in t.find_all(t.frame, role="unknown", depth=30)
             if ui.on_screen(n)]
    return found[0] if len(found) == 1 else None


def path_entry(t, view):
    vx, vy, vwidth, vheight = t.extents(view)
    for node in t.find_all(t.frame, role="text", depth=30):
        if not ui.on_screen(node):
            continue
        x, y, width, height = t.extents(node)
        if abs(x - vx) <= 4 and y < vy:
            return node
    return t.fail("the file selector has no path entry")


def go(t, view, path):
    entry = path_entry(t, view)
    t.focus()
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(path)
    t.key("Return")
    t.wait(lambda: t.text(entry).rstrip("/") == path.rstrip("/"),
           "the file selector to go to %s" % path)


def first_row_menu(t, view):
    x, y, width, height = t.extents(view)
    t.click_at(x + INTO_ROW, y + FIRST_ROW)
    return t.popup()


def properties(t, view):
    t.choose(first_row_menu(t, view), "Properties")
    dialog = t.wait(lambda: properties_dialog(t), "the properties dialog")
    name = dialog.name.rsplit(" Properties", 1)[0]
    t.click(t.button(dialog, "Cancel"))
    t.wait(lambda: properties_dialog(t) is None, "the properties dialog to close")
    return name


def properties_dialog(t):
    for node in t.find_all(t.app, role="dialog", depth=2):
        if node.name and node.name.endswith(" Properties"):
            return node
    return None


def select_row(t, view, row=0, modifiers=()):
    x, y, width, height = t.extents(view)
    t.click_at(x + INTO_ROW, y + FIRST_ROW + row * 21, modifiers=modifiers)


def where(t, view):
    return t.text(path_entry(t, view)).rstrip("/")
