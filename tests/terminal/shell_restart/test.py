"""A shell that exits is replaced by a new one.

# requires: MOO_BUILD_TERMINAL

Typing "exit" in a terminal emulator normally closes the window; here there is
no window to close, so the pane starts another shell instead. That is what
makes the pane usable, and it is also the behaviour the give-up counter had to
be added to -- see the shell_gives_up test for the other end of it.
"""

import shlex

ANSWER = "ready42"
COMMAND = "echo ready$((21*2))\n"


def setup(s):
    """A shell that records that it ran and then behaves like a shell."""
    s.pref("Plugins/Terminal/shell",
           s.script("counting-shell",
                    "#!/bin/sh\nprintf x >> %s\nexec /bin/sh\n"
                    % shlex.quote(s.path("starts"))))


def run(t):
    t.menu("Tools", "Terminal")
    terminal = t.need(t.frame, role="terminal", what="the terminal")

    prompt(t, terminal)
    t.check(t.sandbox.read("starts") == "x", "one shell was started for the pane")

    t.type_text("exit\n")

    t.wait(lambda: t.sandbox.read("starts") == "xx",
           "the shell that exited to be replaced by another")
    t.log("ok: the shell was started again after it exited")

    # And the new one is a shell, not just a process: it answers.
    prompt(t, terminal)
    t.type_text(COMMAND)
    t.wait_text(terminal, ANSWER, what="the answer of the second shell")


def prompt(t, terminal):
    """Wait for whatever prompt this shell draws, so that typing is not lost."""
    return t.wait(lambda: t.text(terminal).strip(), "the shell to draw a prompt")
