"""Exercise invalid paths and cancelling path-entry editing.

# requires: MOO_GTK3
"""

from app.file_selector_common import go, open_pane, path_entry, where


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/file.txt", "file")
    s.open(s.write("workdir/notes.txt", "notes"))


def run(t):
    view = open_pane(t)
    original = where(t, view)
    entry = path_entry(t, view)

    t.click(entry)
    t.key("ctrl+a")
    t.type_text(t.sandbox.path("does-not-exist"))
    t.key("Return")
    t.wait(lambda: where(t, view) == original,
           "an invalid path not to change the current directory")

    t.click(entry)
    t.key("ctrl+a")
    t.type_text(t.sandbox.path("workdir"))
    t.key("Escape")
    t.check(where(t, view) == original,
            "Escape cancels path-entry editing")

    go(t, view, t.sandbox.path("workdir"))
    t.check(where(t, view).endswith("workdir"),
            "a valid path still works after an invalid attempt")
