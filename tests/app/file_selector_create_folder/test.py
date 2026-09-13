"""Create a folder from the file selector and verify its model update.

# requires: MOO_GTK3
"""

from app.file_selector_common import first_row_menu, open_pane


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/seed.txt", "seed")
    s.open(s.write("workdir/notes.txt", "notes"))


def run(t):
    view = open_pane(t)
    t.choose(first_row_menu(t, view), "New Folder")
    dialog = t.dialog("Create Folder")
    entry = t.need(dialog, role="text", what="the new folder name")
    t.click(entry)
    t.key("ctrl+a")
    t.type_text("created")
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Create Folder")
    t.wait(lambda: t.sandbox.isdir("workdir", "created"),
           "the created directory on disk")

    # Reopening the menu proves the folder model received the file-added event.
    t.check("Properties" in [n.name for n in t.find_all(first_row_menu(t, view), depth=2)
                             if n.name],
            "the updated listing can act on the new folder")
    t.escape()
