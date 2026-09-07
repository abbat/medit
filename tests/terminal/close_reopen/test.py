"""Closing the pane and opening it again keeps the shell that is running.

# requires: MOO_BUILD_TERMINAL

The shell is started when the pane is first shown and not every time it is
shown, which is what makes the pane a place to leave something running rather
than a fresh shell each time it is looked at.
"""

import shlex


def setup(s):
    s.pref("Plugins/Terminal/shell",
           s.script("counting-shell",
                    "#!/bin/sh\nprintf x >> %s\nexec /bin/sh\n"
                    % shlex.quote(s.path("starts"))))


def run(t):
    t.menu("Tools", "Terminal")
    terminal = t.need(t.frame, role="terminal", what="the terminal")
    t.wait(lambda: t.text(terminal).strip(), "the shell to draw a prompt")

    button = t.need(t.frame, role="toggle button", name="Terminal",
                    what="the button that opens and closes the pane")

    t.click(button)
    t.wait(lambda: not t.state(terminal, "showing"), "the pane to close")
    t.log("ok: the button closed the pane")

    t.click(button)
    t.wait(lambda: t.state(terminal, "showing"), "the pane to open again")

    t.check(t.sandbox.read("starts") == "x",
            "the shell was started once, and opening the pane again did not start another")

    # The same shell, and still alive: it answers.
    t.click(terminal)
    t.type_text("echo again$((21*2))\n")
    t.wait_text(terminal, "again42", what="the answer of the shell that was there all along")
