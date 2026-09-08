"""Copy and paste, on whatever keys they have been given.

# requires: MOO_BUILD_TERMINAL

Ctrl-C and Ctrl-V belong to the shell -- one interrupts it, the other is a
control character -- so the pane moves copy and paste one modifier up, the way
every terminal emulator does. The clipboard the two ends of that share is the
system one, which is what makes the round trip through the document below a
test of it rather than of something the pane keeps to itself.

Both are rebound before medit starts, to keys medit ships with nothing on, and
the test presses those: the pane looks the accelerator up rather than knowing
it, and the two keys it used to have hard-coded in a switch are now ordinary
actions that Configure Shortcuts can move. The shipped keys are pressed too,
and have to do nothing at all -- a shortcut somebody moved that goes on working
where it was is the failure this is about.
"""

MARKER = "copyme42"
COMMAND = "echo copyme$((21*2))\n"

# What the pane's copy and paste are in this run, and what they are not.
COPY = "ctrl+shift+y"
PASTE = "ctrl+shift+u"
SHIPPED_COPY = "ctrl+shift+c"


def setup(s):
    s.open(s.write("workdir/notes.txt", ""))

    # Not the login shell of whoever runs the tests: that is a shell this test
    # has never seen, and one of them will not say "not found" at the end.
    s.pref("Plugins/Terminal/shell", "/bin/sh")

    # The same file and the same keys Configure Shortcuts writes.
    s.pref("Shortcuts/Editor/TerminalCopy", "<Ctrl><Shift>Y")
    s.pref("Shortcuts/Editor/TerminalPaste", "<Ctrl><Shift>U")


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

    # The key medit ships with, which this run has moved elsewhere.
    t.key(SHIPPED_COPY)
    nothing_was_copied(t)

    t.menu("Tools", "Terminal")
    select_word(t, terminal)

    t.key(COPY)

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


def nothing_was_copied(t):
    """The clipboard is still empty, so there is nothing to paste into the document.

    The one assertion here that a rebound shortcut needs: a key that was moved
    has to stop working where it was, and "the pane copied anyway" would look
    exactly like a pass in every other check of this test.
    """
    t.key("ctrl+grave")
    t.key("ctrl+v")
    t.settle(1)

    view = document(t)

    t.check(t.text(view) == "",
            "the shipped key copied nothing, having been rebound: the document "
            "holds %r" % t.text(view))


def document(t):
    return t.on_screen(t.find_all(t.frame, role="text", depth=25))[0]


def into_the_document(t):
    """The copy key put it on the clipboard: the document can paste it."""
    t.key("ctrl+grave")
    t.key("ctrl+v")

    view = document(t)
    t.wait(lambda: t.text(view) == MARKER,
           "the document to hold the word that was copied")
    t.log("ok: what the terminal copied arrived in the document, character for character")

    # Saved rather than left dirty: a modified document turns the quit at the
    # end of every test into a dialog asking about it, and the test would wait
    # for the window to close instead of finishing.
    t.check("[modified]" in (t.frame.name or ""),
            "the pasted document counts as modified")

    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""),
           "the document to be saved, as the window title says")


def back_into_the_shell(t, terminal):
    """And the paste key takes it the other way, into the shell's input."""
    t.menu("Tools", "Terminal")
    t.key(PASTE)
    t.key("Return")

    t.wait_text(terminal, "not found", squeeze=True,
                what="the shell's answer to the pasted word")
