"""A file deleted while it is open: medit says so, and a save brings it back.

# requires: MOO_GTK3

The third thing the file watch can report, after the change tests/file/reload uses
and the change behind tests/file/overwrite_modified: the file is gone.
file_deleted() in mooedit-fileops.cpp gives the document a status of its own and
stops watching, and the window says so in two places -- an exclamation mark in
front of the name on the tab, and "[deleted]" in the title.

Both are asserted, and then the way out of it: the document still holds the text,
so saving writes the file again and the marks go away. Which is the point of
telling the user at all -- what is on screen is now the only copy.
"""

import os

from lib.notebook import order

NAME = "notes.txt"

CONTENT = "the only copy\n"

TYPED = "!"


def setup(s):
    s.open(s.write("workdir/" + NAME, CONTENT))


def run(t):
    view = t.document()

    t.check(order(t) == [NAME], "the document is open and unmarked: %s" % order(t))

    os.remove(t.sandbox.path("workdir", NAME))

    # medit looks every half second; a file that is gone is gone whatever the
    # clock says, so nothing has to be arranged about times here.
    t.wait(lambda: order(t) == ["!" + NAME],
           "the tab to be marked; the strip holds %s" % order(t))

    t.wait(lambda: "[deleted]" in (t.frame.name or ""),
           "and the title to say so; it says %r" % t.frame.name)

    t.check(t.text(view) == CONTENT,
            "the document still holds the text, which is now the only copy: %r"
            % t.text(view))

    # And saving writes the file again.
    t.focus()
    t.click(view)
    t.key("ctrl+End")
    t.type_text(TYPED)
    t.key("ctrl+s")

    t.wait(lambda: order(t) == [NAME],
           "saving to put the file back and the mark to go; the strip holds %s"
           % order(t))

    t.check(t.sandbox.read("workdir", NAME) == CONTENT + TYPED,
            "and the file holds what the document had: %r"
            % t.sandbox.read("workdir", NAME))
