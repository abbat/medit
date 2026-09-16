"""A file watch error when stat fails: medit marks the document changed so the user is asked before overwriting.

# requires: MOO_GTK3

When file_watch_callback() receives a MOO_FILE_EVENT_ERROR (monitor->alive was set
to FALSE by do_stat()), it sets modified_on_disk and clears file_monitor_id, so
check_file_status() will prompt the user before a save overwrites.

This test makes stat fail by removing read permission on the file's directory,
so the next stat attempt gets EACCES and emits an ERROR event. The document is
then marked with a "!" on the tab, same as when it was deleted.
"""

import os
import stat

from lib.notebook import order

NAME = "data.txt"
CONTENT = "original\n"


def setup(s):
    s.open(s.write("workdir/" + NAME, CONTENT))


def run(t):
    view = t.document()

    t.check(order(t) == [NAME], "the document is open and unmarked: %s" % order(t))

    # Make the directory unreadable so stat will fail with EACCES (not ENOENT).
    # The file watch polls every half second, so eventually stat will fail.
    workdir = t.sandbox.path("workdir")
    os.chmod(workdir, 0o000)

    try:
        # medit looks every half second; wait for it to detect the stat error.
        t.wait(lambda: order(t) == ["!" + NAME],
               "the tab to be marked when stat fails; the strip holds %s" % order(t))

        t.check(t.text(view) == CONTENT,
                "the document still holds the original text: %r" % t.text(view))

    finally:
        # Restore permissions so cleanup works.
        os.chmod(workdir, 0o755)
