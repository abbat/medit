"""Activate a bookmark from the file selector's toolbar menu.

# requires: MOO_GTK3
"""

from app.file_selector_common import open_pane, path_entry, where
from lib import input as ui


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/inside.txt", "inside")
    s.write("other/outside.txt", "outside")
    s.open(s.write("workdir/notes.txt", "notes"))


def run(t):
    view = open_pane(t)
    workdir = t.sandbox.path("workdir")
    other = t.sandbox.path("other")
    add_bookmark(t)
    go(t, view, other)

    button = t.need(t.frame, depth=30,
                    pred=lambda n: n.description == "Bookmarks" and ui.on_screen(n),
                    what="the Bookmarks button")
    t.click(button)
    t.key("Down")
    t.key("Return")
    t.wait(lambda: where(t, view) == workdir,
           "the bookmark to return to workdir")


def add_bookmark(t):
    button = t.need(t.frame, depth=30,
                    pred=lambda n: n.description == "Bookmarks" and ui.on_screen(n),
                    what="the Bookmarks button")
    t.click(button)
    t.key("Down")
    t.key("Return")


def go(t, view, path):
    entry = path_entry(t, view)
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(path)
    t.key("Return")
    t.wait(lambda: where(t, view) == path, "the file selector to enter %s" % path)
