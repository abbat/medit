"""The context menu sends the shell to the directory of the document.

# requires: MOO_BUILD_TERMINAL

Two items, and the second of them only for a shell that has pushd -- which the
plugin decides by the name of the shell, since there is no way to ask a shell
what it supports. The test uses that: its shell is a script called bash, so the
item is offered, and the script starts the real bash somewhere else than the
document, so that going to the document is a change and not a coincidence.
"""

import os
import shlex

CD = "“cd” to current file directory"
PUSHD = "“pushd” to current file directory"


def setup(s):
    s.open(s.write("workdir/hello.txt", "hello\n"))
    s.script("bash", "#!/bin/bash\ncd /\nexec /bin/bash\n")
    s.pref("Plugins/Terminal/shell", s.path("bash"))


def run(t):
    t.menu("Tools", "Terminal")
    terminal = t.need(t.frame, role="terminal", what="the terminal")
    t.wait(lambda: t.text(terminal).strip(), "the shell to draw a prompt")

    t.check(ask(t, terminal, "pwd", "cwd") == "/",
            "the shell started in / and not in the document's directory")

    menu = t.popup()

    # Nothing is selected in the terminal, and Copy says so. The item is made
    # sensitive from vte_terminal_get_has_selection() every time the menu is
    # built, which is why the menu is built every time it is opened.
    t.check(not t.state(t.item(menu, "Copy"), "sensitive"),
            "Copy is insensitive while nothing is selected")

    t.click(t.item(menu, CD))
    t.check(ask(t, terminal, "pwd", "cwd") == t.sandbox.path("workdir"),
            "cd took the shell to the directory of the document")

    t.click(t.item(t.popup(), PUSHD))
    t.check(ask(t, terminal, "dirs -p | wc -l", "dirs") == "2",
            "pushd went there too, and kept where it came from on the stack")


def ask(t, terminal, command, name):
    """Run a command in the terminal and read its answer out of a file.

    Out of a file rather than off the screen: an answer on the screen is broken
    wherever the pane happens to end, and a path is long enough for that to
    matter.
    """
    path = t.sandbox.path(name)

    if os.path.exists(path):
        os.unlink(path)

    t.type_text("%s > %s\n" % (command, shlex.quote(path)))

    return t.wait(lambda: t.sandbox.read(name).strip(),
                  "the answer to %r" % command)
