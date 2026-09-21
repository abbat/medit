"""The file selector's path entry offers several matches in a popup.

# requires: MOO_GTK3

tests/app/file_selector_entry has Tab complete a name that only one folder
fits. Here two do, so Tab has nothing to complete and the popup of
src/moofileview/moofileentry.c has to open instead. Down and Up walk the
matches and put each in the entry; Escape closes the popup and keeps the entry
as it was walked to; Return on a match takes the pane there.
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from file_selector_common import open_pane, path_entry, where

ONE = "inner"
TWO = "island"


def setup(s):
    s.plugin("FileSelector")
    s.write("workdir/%s/a.txt" % ONE, "a")
    s.write("workdir/%s/b.txt" % TWO, "b")
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    view = open_pane(t)
    entry = path_entry(t, view)
    workdir = t.sandbox.path("workdir")

    def text():
        return t.text(entry).rstrip("/")

    def type_prefix():
        t.focus()
        t.click(entry)
        t.key("ctrl+a")
        t.type_text("%s/i" % workdir)
        t.key("Tab")

    # "i" fits both folders: the entry stays, the popup opens.
    type_prefix()
    t.key("Down")
    t.wait(lambda: text() == "%s/%s" % (workdir, ONE),
           "Down to walk to the first match; the entry holds %r" % t.text(entry))
    t.key("Down")
    t.wait(lambda: text() == "%s/%s" % (workdir, TWO),
           "Down to walk to the second match; the entry holds %r" % t.text(entry))
    t.key("Up")
    t.wait(lambda: text() == "%s/%s" % (workdir, ONE),
           "Up to walk back; the entry holds %r" % t.text(entry))
    t.key("Page_Down")
    t.wait(lambda: text() == "%s/%s" % (workdir, TWO),
           "Page_Down to walk to the last match; the entry holds %r" % t.text(entry))
    t.key("Page_Up")
    t.wait(lambda: text() == "%s/%s" % (workdir, ONE),
           "Page_Up to walk to the first match; the entry holds %r" % t.text(entry))
    t.key("Escape")

    # The popup is gone, so a key reaches the entry again.
    t.key("ctrl+a")
    t.type_text("%s/i" % workdir)
    t.wait(lambda: text() == "%s/i" % workdir,
           "the entry to take typing after Escape; it holds %r" % t.text(entry))

    # Walk to the second and take it.
    t.key("Tab")
    t.key("Down")
    t.key("Down")
    t.key("Return")
    t.wait(lambda: where(t, view) == "%s/%s" % (workdir, TWO),
           "Return on a match to take the pane there; the entry holds %r"
           % t.text(entry))
