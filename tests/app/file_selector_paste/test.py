"""Paste a copied file into a second directory.

# requires: MOO_GTK3
"""

import os

from app.file_selector_common import first_row_menu, go, open_pane


def setup(s):
    s.plugin("FileSelector")
    s.write("source/a.txt", "copy me")
    s.write("destination/keep.txt", "keep")
    s.open(s.write("source/notes.txt", "notes"))


def run(t):
    view = open_pane(t)
    go(t, view, t.sandbox.path("source"))
    t.choose(first_row_menu(t, view), "Copy")
    go(t, view, t.sandbox.path("destination"))
    t.choose(empty_menu(t, view), "Paste")
    t.wait(lambda: t.sandbox.read("destination", "a.txt") == "copy me",
           "the copied file in destination")
    t.check(t.sandbox.read("source", "a.txt") == "copy me",
            "Copy leaves the source in place")
    t.check(os.path.exists(t.sandbox.path("destination", "keep.txt")),
            "Paste preserves existing files")


def empty_menu(t, view):
    x, y, width, height = t.extents(view)
    t.click_at(x + width - 10, y + height - 10)
    return t.popup()
