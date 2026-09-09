"""A pane taken out of the window and put back.

# requires: MOO_GTK3

Every pane has four buttons of its own -- sticky, hide, detach, remove -- and
tests/app/pane_move and tests/app/pane_resize drive the strip and the divider
rather than these. Detach is the one that changes the shape of things: MooPane
takes its contents out of the MooPaned they live in, puts them in a window of
their own, and swaps its own toolbar for one with Attach and Keep on top on it.
Attach undoes all of that.

So what is asserted is which window the pane's contents are in. The file
selector's own path entry is easy to find and belongs to nothing else, so it
stands for the pane, and the toplevel it hangs off is read by walking up to the
application. Rectangles would not do: with no window manager the detached window
comes up at the top left corner, over the editor window and inside its bounds.
"""

from lib import input as ui

DETACH = "Detach pane"
ATTACH = "Attach"


def setup(s):
    s.plugin("FileSelector")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    open_the_pane(t)

    editor = window_of(t, path_entry(t))
    t.check(editor == t.frame.name,
            "the pane starts in the editor window: %r" % editor)

    t.click(button(t, DETACH))

    t.wait(lambda: window_of(t, path_entry(t)) != t.frame.name,
           "the pane's contents to move to a window of their own; they are in %r"
           % window_of(t, path_entry(t)))
    t.log("ok: Detach took the pane out into %r" % window_of(t, path_entry(t)))

    t.click(button(t, ATTACH))

    t.wait(lambda: window_of(t, path_entry(t)) == t.frame.name,
           "the pane's contents to come back into the editor window; they are in %r"
           % window_of(t, path_entry(t)))
    t.log("ok: and Attach put it back")


def button(t, tip):
    """A button of the pane's toolbar, which carries the tooltip as description."""
    return t.need(t.app, depth=30,
                  pred=lambda node: node.description == tip and ui.on_screen(node),
                  what="the %r button" % tip)


def window_of(t, node):
    """The name of the toplevel the node hangs off, or None if it has none."""
    while node is not None:
        parent = node.parent

        if parent is None or t.role(parent) == "application":
            return node.name

        node = parent

    return None


def path_entry(t):
    """The file selector's path entry, which is the only entry on screen."""
    entries = [node for node in t.find_all(t.app, role="text", depth=30)
               if ui.on_screen(node) and not t.state(node, "multi line")]

    return entries[0] if entries else None


def open_the_pane(t):
    t.menu("View", "Panes", "File Selector")
    t.pin_pane()
    t.click(t.need(t.frame, role="push button", name="Jump to",
                   what="the button that goes to the document's directory"))
