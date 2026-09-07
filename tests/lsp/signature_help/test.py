"""The signature of the call being typed, in a popup under the cursor.

# requires: MOO_BUILD_LSP

Asked for by the characters the server names -- "(" and "," for most of them --
and by Ctrl+Shift+Space, which is what a call already typed needs. The popup is
a window of the plugin's own, the way the completion popup is, so both toolkits
can read it: a plain GtkWindow with a GtkLabel in it.

Which parameter of the signature is the one being typed is the whole point of
the feature, and the only part of it a test can see from outside is the line
underneath: the parameter's own documentation, which changes when the server
moves activeParameter along. That the parameter is emboldened inside the
signature is asserted in lsp-tests.cpp, where the markup is a string rather
than a screenful of pixels.
"""

CONTENT = "add\n"

SIGNATURE = {
    "label": "add(alpha: int, beta: int) -> int",
    "documentation": "Adds two numbers",
    "parameters": [
        {"label": "alpha: int", "documentation": "the number to start from"},
        {"label": "beta: int", "documentation": "the number to add"},
    ],
}

FIRST = "the number to start from"
SECOND = "the number to add"

SETTING = "Show the parameters of a call"


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))
    s.lsp_server(filter="globs:*.txt", replies={"textDocument/signatureHelp": help_at(0)})


def help_at(parameter):
    """A SignatureHelp with that parameter as the one being typed."""
    return {"signatures": [SIGNATURE],
            "activeSignature": 0,
            "activeParameter": parameter}


def run(t):
    t.wait_lsp("textDocument/didOpen")

    t.focus()
    t.key("ctrl+Home")
    t.key("End")
    t.type_text("(")

    asked = t.wait_lsp("textDocument/signatureHelp")["params"]

    t.check(asked["position"] == {"line": 0, "character": 4},
            "the server was asked about the position after the bracket: %s"
            % asked["position"])
    t.check(asked.get("context", {}).get("triggerCharacter") == "(",
            "and told which character asked for it: %s" % asked.get("context"))
    t.check(asked.get("context", {}).get("isRetrigger") is False,
            "and that nothing was open when it did")

    popup = t.wait(lambda: hints(t), "the signature popup")

    t.wait_text(popup, SIGNATURE["label"], what="the signature of the call")
    t.wait_text(popup, FIRST, what="the documentation of the first parameter")
    t.log("ok: the popup describes the parameter the cursor is in")

    # The next parameter. The server is the one that says which it is -- the
    # client sends the position and gets activeParameter back -- so the
    # scenario moves it along and the comma asks again.
    t.sandbox.lsp_scenario("test", replies={"textDocument/signatureHelp": help_at(1)})

    t.type_text(",")

    asked = t.wait_lsp("textDocument/signatureHelp", count=2)["params"]

    t.check(asked.get("context", {}).get("isRetrigger") is True,
            "the second question says the popup was already open: %s" % asked.get("context"))

    t.wait_text(popup, SECOND, what="the documentation of the second parameter")
    t.check(FIRST not in t.text(popup), "and the first parameter's is gone")

    # Every keystroke while it is up asks again, and the answer is what closes
    # it: the server is the one that knows the call has ended, and a popup
    # describing a call that is no longer being typed is worse than a question
    # per key. An empty answer is how it says so.
    t.sandbox.lsp_scenario("test", replies={"textDocument/signatureHelp": {"signatures": []}})

    t.type_text("x")

    t.wait(lambda: hints(t) is None,
           "the popup to close when the server answers with no signatures")
    t.log("ok: the server ends the popup, the client does not guess")

    # With nothing open it is the other way round: only the characters the
    # server named ask anything, or every keystroke in the document is a
    # request.
    count = len(t.lsp("textDocument/signatureHelp"))
    t.type_text("y")
    t.settle(1)

    t.check(len(t.lsp("textDocument/signatureHelp")) == count,
            "with no popup open, an ordinary character asks the server nothing")

    t.sandbox.lsp_scenario("test", replies={"textDocument/signatureHelp": help_at(0)})

    # Ctrl+Shift+Space, for a call that is already written. Like Ctrl+Space it
    # is matched by hand, the text view having swallowed it first.
    t.key("ctrl+shift+space")

    asked = t.wait_lsp("textDocument/signatureHelp", count=count + 1)["params"]

    t.check(asked.get("context", {}).get("triggerKind") == 1,
            "a question nobody typed a bracket for says it was invoked: %s"
            % asked.get("context"))
    t.wait(lambda: hints(t), "the popup, asked for from the keyboard")

    t.key("Escape")
    t.wait(lambda: hints(t) is None, "the popup to close again")

    # And the setting that turns the whole thing off.
    setting(t, False)

    count = len(t.lsp("textDocument/signatureHelp"))
    t.focus()
    t.type_text("(")
    t.settle(1)

    t.check(len(t.lsp("textDocument/signatureHelp")) == count,
            "nothing is asked once the setting is off")
    t.check(hints(t) is None, "and no popup is shown")

    # Saved, or the quit at the end of the test turns into a dialog about it.
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")


def setting(t, on):
    """Tick or untick the popup's setting on the client's page of the preferences."""
    dialog = t.preferences("Language Servers")
    box = t.need(dialog, role="check box", name=SETTING,
                 what="the %r check box" % SETTING)

    if t.state(box, "checked") != on:
        t.click(box)

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")
    t.log("the setting is now %s" % ("on" if on else "off"))


def hints(t):
    """The popup: a toplevel window of the application with a label in it.

    The menus are toplevel windows too, and a menu that is still closing has
    items in it that gail describes with labels of their own, so a window with
    a menu anywhere in it is not this one.
    """
    for top in t.find_all(t.app, role="window", depth=1):
        if t.find(top, role="menu", depth=2) is not None:
            continue

        # On screen, not merely in the tree: a popup that has been closed is
        # hidden rather than destroyed, and its accessible outlives it.
        labels = t.on_screen(t.find_all(top, role="label", depth=3))

        if labels:
            return labels[0]

    return None
