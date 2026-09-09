"""Edit / Preferences: the pages it is assembled from, and OK against Cancel.

# requires: MOO_GTK3

moo_app_create_prefs_dialog() puts the editor's five pages into one dialog and
then lets every plugin add its own, so the list down the side is medit's whole
answer to "where is that setting". The first assertion is that list.

The rest is one setting driven twice, because a preferences dialog has two ways
of being wrong and both are silent. OK has to reach the documents that are
already open -- the pages write preferences, and prefs_dialog_apply() is what
turns them into changes in a MooEdit that was loaded before the dialog existed --
and Cancel has to reach nothing at all. The setting is "do not use tabs for
indentation", so what says whether it took is what Tab puts in the buffer.
"""

NAME = "notes.txt"

CONTENT = "alpha\nbeta\n"

WIDTH = 4

# The editor's own pages, in the order moo_app_create_prefs_dialog() adds them.
# Plugins add more, so this is a subset rather than the whole list.
PAGES = ("General", "View", "File", "Languages", "File Filters")

USE_SPACES = "Do not use tabs for indentation"


def setup(s):
    s.pref("Editor/spaces_instead_of_tabs", False)
    s.pref("Editor/indent_width", WIDTH)
    s.pref("Editor/tab_width", WIDTH)
    s.open(s.write("workdir/" + NAME, CONTENT))


def run(t):
    view = t.document()

    dialog = t.preferences("General")

    listed = [cell.name for cell in t.on_screen(t.find_all(dialog, role="table cell"))
              if cell.name]
    missing = [page for page in PAGES if page not in listed]
    t.check(not missing,
            "every page of the editor is in the list: %s" % ", ".join(listed))

    spaces = t.need(dialog, role="check box", name=USE_SPACES,
                    what="the %r check box" % USE_SPACES)
    t.check(not t.state(spaces, "checked"),
            "the preference starts off, as the sandbox set it")

    t.click(spaces)
    t.wait(lambda: t.state(spaces, "checked"), "the check box to take the click")

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    t.focus()
    t.click(view)
    t.key("ctrl+Home")
    t.key("Tab")

    t.wait(lambda: t.text(view) == " " * WIDTH + CONTENT,
           "Tab to insert spaces in a document that was open before the dialog was;\n"
           "the document holds %r" % t.text(view))

    # And the other way: the same box unticked and the dialog cancelled, which
    # has to leave the preference where OK put it.
    dialog = t.preferences("General")

    spaces = t.need(dialog, role="check box", name=USE_SPACES,
                    what="the %r check box" % USE_SPACES)
    t.check(t.state(spaces, "checked"), "the box remembers what OK wrote")

    t.click(spaces)
    t.wait(lambda: not t.state(spaces, "checked"), "the check box to be unticked again")

    t.click(t.button(dialog, "Cancel"))
    t.no_toplevel("Preferences")

    t.focus()
    t.click(view)
    t.key("ctrl+Home")
    t.key("Tab")

    t.wait(lambda: t.text(view) == " " * WIDTH * 2 + CONTENT,
           "Tab to insert spaces still, Cancel having written nothing;\n"
           "the document holds %r" % t.text(view))

    # A modified document asks about itself when the runner quits medit.
    t.focus()
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")
