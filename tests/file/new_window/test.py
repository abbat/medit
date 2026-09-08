"""File/New Window is a second window with its own documents.

# requires: MOO_GTK3

Two windows are two MooEditWindows over one MooEditor, and every open file
belongs to one of them. What has to be right is which one: a file opened from a
window's own File menu goes into that window, and the other window's strip does
not change. Nothing else in the tests has ever had two windows of documents open
at once.

The second thing here is the empty-document rule in moo_editor_open_files(): a
window whose current document is untitled and empty has that document reused
rather than a tab added beside it. That is why a fresh window opening its first
file ends up with one tab and not two, and it is easy to lose without noticing --
the window would simply accumulate an Untitled tab nobody asked for.

No window manager is asked for, but the two windows are moved apart all the same:
xdotool's windowmove is a plain X call and works without one. Without that they
sit on top of each other, and then a click aimed at one window's menu bar by the
coordinates in the other window's tree lands on the wrong window -- which is also
how the harness quits medit at the end, through the first window's File menu.
"""

from lib.notebook import order

OPEN_ALREADY = "notes.txt"
OPENED_LATER = "other.txt"

UNTITLED = "Untitled"


def setup(s):
    s.write("workdir/" + OPENED_LATER, "the other one\n")
    s.open(s.write("workdir/" + OPEN_ALREADY, "notes\n"))


def run(t):
    first = t.frame

    t.check(order(t, first) == [OPEN_ALREADY], "the window starts with one document")

    t.menu("File", "New Window")

    t.wait(lambda: len(t.frames()) == 2, "a second window to appear")

    second = [f for f in t.frames() if f != first][-1]

    # Side by side, so that a click meant for one window reaches that window.
    t.place_window(second, 700, 0, 700, 860)

    t.check(order(t, second) == [UNTITLED],
            "the new window has a document of its own and nothing else: %s"
            % order(t, second))
    t.check(order(t, first) == [OPEN_ALREADY],
            "and the first window is as it was: %s" % order(t, first))

    # Opened from the new window's own menu, so it goes there.
    t.menu("File", "Open...", frame=second)

    t.need(t.app, role="file chooser", depth=2, what="the Open dialog")
    t.focus(second)
    t.key("ctrl+l")
    t.type_text(t.sandbox.path("workdir", OPENED_LATER))
    t.key("Return")

    t.wait(lambda: t.find(t.app, role="file chooser", depth=2) is None,
           "the Open dialog to close")

    t.wait(lambda: order(t, second) == [OPENED_LATER],
           "the new window to hold the file it opened, in place of its empty "
           "document; it holds %s" % order(t, second))
    t.log("ok: the file replaced the new window's empty untitled document")

    t.check(order(t, first) == [OPEN_ALREADY],
            "and the window it was not opened from is untouched: %s" % order(t, first))
