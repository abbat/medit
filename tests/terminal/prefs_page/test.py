"""The Terminal page of the preferences: the font check box and the shell.

# requires: MOO_BUILD_TERMINAL

The check box says "use the system monospace font", and while it is ticked the
font button beside it means nothing and is insensitive. The shell entry is the
setting the pane reads when it starts a shell, and it is bound to the
preference rather than read out of the widget, so the test sets it through the
dialog and then looks at which shell actually ran.
"""

import shlex

# The buttons of the dialog itself, which are not on the page.
DIALOG_BUTTONS = ("Help", "Apply", "Cancel", "OK")


def setup(s):
    s.script("chosen-shell",
             "#!/bin/sh\nprintf x >> %s\nexec /bin/sh\n" % shlex.quote(s.path("started")))


def run(t):
    dialog = open_page(t)

    default_font, font = font_widgets(t, dialog)
    t.check(t.state(default_font, "checked"), "no font is configured, so Default is ticked")
    t.check(not t.state(font, "sensitive"), "and the font button is insensitive")

    t.click(default_font)
    t.wait(lambda: t.state(font, "sensitive"),
           "the font button to become sensitive when Default is unticked")
    t.log("ok: unticking Default hands the font over to the button")

    t.click(default_font)
    t.wait(lambda: not t.state(font, "sensitive"),
           "and to go back when it is ticked again")
    t.log("ok: ticking it again takes the font back")

    set_shell(t, dialog, t.sandbox.path("chosen-shell"))
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    t.menu("Tools", "Terminal")
    t.need(t.frame, role="terminal", what="the terminal")

    t.wait(lambda: t.sandbox.read("started") == "x",
           "the pane to start the shell that was named in the preferences")
    t.log("ok: the shell from the preferences is the one that ran")


def open_page(t):
    dialog = t.preferences("Terminal")

    # The shell entry is the last thing on the page, so waiting for its label
    # is waiting for the page to be drawn.
    t.need(dialog, role="label", name_prefix="Shell", what="the Terminal page")

    return dialog


def font_widgets(t, dialog):
    """The Default check box and the font button beside it."""
    check = t.need(dialog, role="check box", name="Default",
                   what="the Default font check box")

    # By elimination: the button carries the name of the font it holds, which
    # is whatever the system happens to consider monospace.
    buttons = [b for b in t.on_screen(t.find_all(dialog, role="push button"))
               if b.name not in DIALOG_BUTTONS]

    t.check(len(buttons) == 1,
            "the page has one button besides the dialog's own: %s"
            % ", ".join(sorted(b.name for b in buttons)))

    return check, buttons[0]


def set_shell(t, dialog, path):
    entries = t.on_screen(t.find_all(dialog, role="text"))
    t.check(len(entries) == 1, "the page has one entry, for the shell")

    t.click(entries[0])
    t.key("ctrl+a")
    t.type_text(path)
