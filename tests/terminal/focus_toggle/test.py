"""Ctrl-` moves the focus into the terminal and back out of it.

# requires: MOO_BUILD_TERMINAL

A window hands a key to the widget that has the focus before it looks at its
accelerators, so while the terminal has the focus every editor shortcut goes to
the shell instead. That is what a terminal is for, but it also means the pane's
own accelerator cannot fire, and there would be no way back to the document
without the mouse -- so the pane takes that one key itself and gives the focus
back. Both directions are one key, and this is the test of both.
"""

TYPED = "hello"
COMMAND = "echo focus$((21*2))\n"
ANSWER = "focus42"


def setup(s):
    s.open(s.write("workdir/notes.txt", ""))
    s.pref("Plugins/Terminal/shell", "/bin/sh")


def run(t):
    t.menu("Tools", "Terminal")
    terminal = t.need(t.frame, role="terminal", what="the terminal")
    t.wait(lambda: t.text(terminal).strip(), "the shell to draw a prompt")

    t.check(t.state(terminal, "focused"), "opening the pane put the focus in it")

    t.type_text(COMMAND)
    t.wait_text(terminal, ANSWER, what="what was typed while the terminal had it")
    t.check(chars(t) == 0, "and nothing of it reached the document")

    t.key("ctrl+grave")
    t.check(not t.state(terminal, "focused"), "Ctrl-` took the focus out again")

    t.type_text(TYPED)
    t.wait(lambda: chars(t) == len(TYPED),
           "what is typed now to go into the document")
    t.log("ok: the document has the %d characters typed after Ctrl-`" % len(TYPED))

    t.key("ctrl+grave")
    t.check(t.state(terminal, "focused"), "and Ctrl-` brings it back to the terminal")

    # The document was modified, and a modified document makes the quit at the
    # end of the test into a dialog.
    t.key("ctrl+grave")
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")


def chars(t):
    """How many characters the document holds, as the status bar counts them."""
    label = t.find(t.frame, role="label", name_prefix="Chars:", depth=25)
    return int(label.name.split(":")[1]) if label is not None else None
