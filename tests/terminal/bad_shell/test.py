"""A shell that cannot be run at all says so where the user is looking.

# requires: MOO_BUILD_TERMINAL

There is no child in this case, so nothing will ever exit and nothing will try
again: the message in the pane and the line in the log are all there is, and
without them the pane would just sit there empty.
"""

import os

MISSING = "no-such-shell"


def setup(s):
    s.pref("Plugins/Terminal/shell", s.path(MISSING))


def run(t):
    t.menu("Tools", "Terminal")
    terminal = t.need(t.frame, role="terminal", what="the terminal")

    t.wait_text(terminal, "No such file or directory", squeeze=True,
                what="the reason the shell could not be run, in the pane")

    t.wait(lambda: "could not run the shell" in log(t),
           "the same failure on medit's own output")
    t.log("ok: the failure is in the log as well as in the pane")

    t.check(MISSING in log(t), "and the log names the shell it tried")


def log(t):
    with open(os.path.join(t.log_dir, "medit.log"), errors="replace") as f:
        return f.read()
