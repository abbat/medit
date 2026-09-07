"""A shell that dies as soon as it starts is not restarted forever.

# requires: MOO_BUILD_TERMINAL

The plugin restarts the shell when it exits, which is what makes the pane feel
like a terminal emulator. The python plugin this one was ported from did it
unconditionally, so a bad Plugins/Terminal/shell turned into a fork loop that
only ended when medit was killed. This is the test for the counter that stops
it, and for the message that says where to fix the setting.
"""

import shlex

# TERMINAL_MAX_QUICK_EXITS in terminal-plugin.cpp
MAX_STARTS = 3

GAVE_UP = "The shell keeps exiting immediately"


def setup(s):
    """A shell that records the fact that it ran and exits at once."""
    s.pref("Plugins/Terminal/shell",
           s.script("quick-exit-shell",
                    "#!/bin/sh\nprintf x >> %s\n" % shlex.quote(s.path("starts"))))


def run(t):
    t.menu("Tools", "Terminal")
    terminal = t.need(t.frame, role="terminal", what="the terminal")

    # squeeze, because the message is longer than the pane is wide and vte
    # breaks it wherever the window happens to end.
    t.wait_text(terminal, GAVE_UP, what="the message that it gave up", squeeze=True)

    started = len(t.sandbox.read("starts"))
    t.check(started == MAX_STARTS,
            "the shell was started %d times, the limit being %d" % (started, MAX_STARTS))

    # Nothing restarts it afterwards: the count is the whole point, and a loop
    # that kept going would keep going now.
    t.settle(2)
    started = len(t.sandbox.read("starts"))
    t.check(started == MAX_STARTS, "and not once more after it gave up")
