"""A file watch error when stat fails: the document is marked, so a save asks first.

# requires: MOO_GTK3

When file_watch_callback() receives a MOO_FILE_EVENT_ERROR (monitor->alive was set
to FALSE by do_stat()), it sets modified_on_disk and clears file_monitor_id, so
check_file_status() will prompt the user before a save overwrites.

This test makes stat fail by putting a symlink to itself where the file's
directory was, so the next stat gets ELOOP (not ENOENT, which is a deletion)
and emits an ERROR event. Not a chmod: CI runs as root, which no permission
stops. The document is then marked with a "!" on the tab, same as when it was
deleted.
"""

import os

from lib.notebook import order

NAME = "data.txt"
CONTENT = "original\n"


def setup(s):
    s.open(s.write("workdir/" + NAME, CONTENT))


def run(t):
    view = t.document()

    t.check(order(t) == [NAME], "the document is open and unmarked: %s" % order(t))

    # The directory moved aside and a loop in its place: stat fails with ELOOP.
    workdir = t.sandbox.path("workdir")
    moved = workdir + ".moved"
    os.rename(workdir, moved)
    os.symlink(os.path.basename(workdir), workdir)

    try:
        # medit looks every half second; wait for it to detect the stat error.
        t.wait(lambda: order(t) == ["!" + NAME],
               "the tab to be marked when stat fails; the strip holds %s" % order(t))

        t.check(t.text(view) == CONTENT,
                "the document still holds the original text: %r" % t.text(view))

    finally:
        # The directory back, so cleanup finds it.
        os.remove(workdir)
        os.rename(moved, workdir)
