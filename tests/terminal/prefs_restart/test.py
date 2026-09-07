"""Fixing the shell in the preferences starts one in the pane that gave up.

# requires: MOO_BUILD_TERMINAL

The pane stops restarting a shell that keeps dying at once, and tells the user
to look at the setting. Applying the preferences is therefore also the moment
to try again -- otherwise the advice would be to fix the shell and then close
and reopen the window, which is not what the message says.
"""

import shlex

GAVE_UP = "The shell keeps exiting immediately"


def setup(s):
    s.pref("Plugins/Terminal/shell",
           s.script("broken-shell", "#!/bin/sh\nexit 1\n"))

    s.script("working-shell",
             "#!/bin/sh\nprintf x >> %s\nexec /bin/sh\n" % shlex.quote(s.path("started")))


def run(t):
    t.menu("Tools", "Terminal")
    terminal = t.need(t.frame, role="terminal", what="the terminal")

    t.wait_text(terminal, GAVE_UP, squeeze=True,
                what="the message that the shell keeps dying")
    t.check(t.sandbox.read("started") == "", "and nothing else has been started")

    dialog = open_page(t)
    set_shell(t, dialog, t.sandbox.path("working-shell"))
    t.click(t.button(dialog, "Apply"))

    t.wait(lambda: t.sandbox.read("started") == "x",
           "applying the preferences to start the shell that was named in them")
    t.log("ok: the pane tried again as soon as the setting was fixed")

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    # A shell, not just a process that was started: it draws a prompt, and the
    # message about giving up is gone, because starting resets the terminal.
    t.wait(lambda: t.text(terminal).strip(), "the new shell to draw a prompt")
    t.check(GAVE_UP not in " ".join(t.text(terminal).split()),
            "the message about giving up is gone")


def open_page(t):
    dialog = t.preferences("Terminal")

    # The shell entry is the last thing on the page, so waiting for its label
    # is waiting for the page to be drawn.
    t.need(dialog, role="label", name_prefix="Shell", what="the Terminal page")

    return dialog


def set_shell(t, dialog, path):
    entries = t.on_screen(t.find_all(dialog, role="text"))
    t.check(len(entries) == 1, "the page has one entry, for the shell")

    t.click(entries[0])
    t.key("ctrl+a")
    t.type_text(path)
