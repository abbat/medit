"""Copy and paste, on the modifier a terminal has to use.

# requires: MOO_BUILD_TERMINAL

Ctrl-C and Ctrl-V belong to the shell -- one interrupts it, the other is a
control character -- so the pane moves copy and paste one modifier up, the way
every terminal emulator does. The clipboard the two ends of that share is the
system one, which is what makes the round trip through the document below a
test of it rather than of something the pane keeps to itself.
"""

MARKER = "copyme42"
COMMAND = "echo copyme$((21*2))\n"


def setup(s):
    s.open(s.write("workdir/notes.txt", ""))

    # Not the login shell of whoever runs the tests: that is a shell this test
    # has never seen, and one of them will not say "not found" at the end.
    s.pref("Plugins/Terminal/shell", "/bin/sh")


def run(t):
    t.menu("Tools", "Terminal")
    terminal = t.need(t.frame, role="terminal", what="the terminal")
    t.wait(lambda: t.text(terminal).strip(), "the shell to draw a prompt")

    t.type_text(COMMAND)
    t.wait_text(terminal, MARKER, what="the word to copy")

    select_word(t, terminal)

    # And with something selected the menu offers to copy it, which is the
    # other half of the same check in the context_menu test.
    t.check(t.state(t.item(t.popup(), "Copy"), "sensitive"),
            "Copy is sensitive now that there is a selection")
    t.escape()

    t.key("ctrl+shift+c")

    into_the_document(t)
    back_into_the_shell(t, terminal)


def select_word(t, terminal):
    """Double-click the word the shell printed, and check it took.

    A word on a terminal is not a widget and has no extents of its own; where
    it is drawn comes from the terminal's Text interface, the same way a link
    inside a label is found.
    """
    text = t.text(terminal)
    start = text.rindex(MARKER)

    t.click_range(terminal, start, start + len(MARKER), times=2)

    selected = terminal.queryText()
    t.check(selected.getNSelections() == 1
            and selected.getSelection(0) == (start, start + len(MARKER)),
            "double-clicking the word selected exactly it")


def into_the_document(t):
    """Ctrl-Shift-C put it on the clipboard: the document can paste it."""
    t.key("ctrl+grave")
    t.key("ctrl+v")

    t.wait(lambda: chars(t) == "Chars: %d" % len(MARKER),
           "the document to hold the %d characters that were copied" % len(MARKER))
    t.log("ok: what the terminal copied arrived in the document")

    # Saved rather than left dirty: a modified document turns the quit at the
    # end of every test into a dialog asking about it, and the test would wait
    # for the window to close instead of finishing.
    t.check("[modified]" in (t.frame.name or ""),
            "the pasted document counts as modified")

    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""),
           "the document to be saved, as the window title says")


def back_into_the_shell(t, terminal):
    """And Ctrl-Shift-V takes it the other way, into the shell's input."""
    t.menu("Tools", "Terminal")
    t.key("ctrl+shift+v")
    t.key("Return")

    t.wait_text(terminal, "not found", squeeze=True,
                what="the shell's answer to the pasted word")


def chars(t):
    label = t.find(t.frame, role="label", name_prefix="Chars:", depth=25)
    return label.name if label is not None else None
