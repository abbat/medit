"""Tools/Terminal: the pane opens, a shell starts in it, and it takes input.

# requires: MOO_BUILD_TERMINAL

The pane is built for the GTK+3 build only -- vte dropped GTK+2 in 0.30 -- so
on a GTK+2 build this test is registered and disabled rather than missing.
"""

# The command is written so that what the shell echoes back is not what it
# prints: the typed line contains "ready$((21*2))" and only the answer contains
# "ready42", so waiting for the answer cannot match the echo of the question.
COMMAND = "echo ready$((21*2))\n"
ANSWER = "ready42"


def run(t):
    lazy_until_shown(t)

    t.menu("Tools", "Terminal")
    terminal = t.need(t.frame, role="terminal", what="the terminal")

    t.check(t.state(terminal, "showing"), "the terminal is on screen")
    t.check(t.state(terminal, "focused"), "the terminal took the focus")

    # Any prompt, not a particular one: it is drawn by the shell, and the
    # tests run as whoever runs them -- a developer gets "$", the root of a CI
    # container gets "#".
    prompt = t.wait(lambda: t.text(terminal).strip(), "the shell to draw a prompt")
    t.log("the shell came up with %r" % prompt)

    t.type_text(COMMAND)
    t.wait_text(terminal, ANSWER, what="the answer to a command typed into it")


def lazy_until_shown(t):
    """The shell starts when the pane is first shown, not with the window.

    The widget is there from the start -- the plugin builds it when the window
    is created -- so the test is that it holds nothing, not that it is missing.
    """
    terminal = t.need(t.frame, role="terminal",
                      what="the terminal, before the pane has ever been opened")

    t.check(not t.state(terminal, "showing"),
            "the terminal is not on screen before the pane is opened")
    t.check(t.text(terminal).strip() == "",
            "no shell has run in it yet")
