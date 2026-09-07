"""The client's page of the preferences: five settings and two buttons.

# requires: MOO_BUILD_LSP

The page is only there while the plugin is on -- it belongs to the plugin, and
the framework builds it from _moo_lsp_prefs_page() -- so its being found at all
is part of the assertion.

The debug setting is the one worth driving all the way through, since it is
read where a server is started rather than where it is set: ticking it does
nothing to the servers already running, and the button beside it is how a user
makes it take effect. The protocol on medit's own output is then the evidence.

Both toolkits.
"""

SETTINGS = ("Underline problems and list them in the Diagnostics pane",
            "Complete words",
            "Describe what is under the pointer",
            "Log the protocol to standard error")

# The buttons of the dialog itself, which are not on the page.
DIALOG_BUTTONS = ("Help", "Apply", "Cancel", "OK")


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", "alpha\n"))
    s.lsp_server(filter="globs:*.txt")


def run(t):
    t.wait_lsp("textDocument/didOpen")

    dialog = t.preferences("Language Servers")
    boxes = {name: t.need(dialog, role="check box", name=name,
                          what="the %r check box" % name) for name in SETTINGS}

    for name in SETTINGS[:3]:
        t.check(t.state(boxes[name], "checked"), "%r is on to begin with" % name)

    t.check(not t.state(boxes[SETTINGS[3]], "checked"),
            "and the protocol is not logged until it is asked for")

    delay = t.need(dialog, role="spin button", what="the sync delay")

    t.check(t.text(delay) == "300",
            "the delay before a change is sent is the default 300, not %r"
            % t.text(delay))

    buttons = [button.name for button in t.on_screen(t.find_all(dialog, role="push button"))
               if button.name not in DIALOG_BUTTONS]

    t.check(sorted(buttons) == ["Edit the Server List", "Restart Servers"],
            "the page offers its two buttons: %s" % ", ".join(sorted(buttons)))

    # Ticked here, and it reaches the servers only when they are started again,
    # which is what the button next to it is for.
    t.click(boxes[SETTINGS[3]])
    t.click(t.button(dialog, "Apply"))

    t.check("lsp: " not in t.medit_log(),
            "the server already running is not logging the protocol")

    t.click(t.button(dialog, "Restart Servers"))

    # Every message, in both directions, prefixed "lsp: <the program> ". The
    # name is the program rather than the id from lsp.xml, which for a server
    # started through a wrapper -- or through python, as here -- says less than
    # it looks like it does.
    t.wait(lambda: "lsp: " in t.medit_log(),
           "the protocol of the restarted server to reach medit's output")
    t.check('"method":"initialize"' in t.medit_log(),
            "and it is the whole conversation, starting with the handshake")
    t.log("ok: the debug setting takes effect when the servers are restarted")

    t.click(t.button(dialog, "Cancel"))
    t.no_toplevel("Preferences")
