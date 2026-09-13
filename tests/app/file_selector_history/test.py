"""Exercise branching Back/Forward history in the file selector.

# requires: MOO_GTK3
"""

from app.file_selector_common import go, open_pane, where


def setup(s):
    s.plugin("FileSelector")
    s.write("a/one.txt", "a")
    s.write("b/two.txt", "b")
    s.write("c/three.txt", "c")
    s.open(s.write("start.txt", "start"))


def run(t):
    view = open_pane(t)
    a = t.sandbox.path("a")
    b = t.sandbox.path("b")
    c = t.sandbox.path("c")

    go(t, view, a)
    go(t, view, b)
    go(t, view, c)
    choose_empty_space(t, view, "Back")
    t.wait(lambda: where(t, view) == b, "Back to return to b")
    choose_empty_space(t, view, "Back")
    t.wait(lambda: where(t, view) == a, "Back to return to a")
    choose_empty_space(t, view, "Forward")
    t.wait(lambda: where(t, view) == b, "Forward to return to b")
    go(t, view, c)
    choose_empty_space(t, view, "Forward")
    t.check(where(t, view) == c, "Forward is a no-op after a new branch")


def choose_empty_space(t, view, item):
    x, y, width, height = t.extents(view)
    t.click_at(x + width - 10, y + height - 10)
    t.choose(t.popup(), item)
