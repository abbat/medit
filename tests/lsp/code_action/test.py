"""Code actions: what the server offers here, and what happens when one is taken.

# requires: MOO_BUILD_LSP

A code action is the one LSP feature that can arrive in three shapes, and all
three have to work the same way from the user's side:

* an edit, which is applied the way a rename is;
* a command, which is sent back with workspace/executeCommand and whose result
  is nothing -- what it changed arrives afterwards as a request of the
  server's own, workspace/applyEdit, which medit has to answer;
* an action that cannot be used, which is shown anyway, because saying why a
  fix does not apply is the point of the "disabled" member.

The diagnostics on the line are part of the question: a server picks its quick
fixes out of the ones it is handed, and hands them back untouched -- including
the members medit never reads, which is how it recognises its own.
"""

CONTENT = "alpha beta\ngamma delta\n"

FIXED = "omega beta\ngamma delta\n"
COMMANDED = "omega beta\ngamma sorted\n"

# The diagnostic the server published, with a member medit knows nothing about.
DIAGNOSTIC = {
    "range": {"start": {"line": 0, "character": 0},
              "end": {"line": 0, "character": 5}},
    "message": "alpha is not defined",
    "severity": 1,
    "code": "E101",
    "data": {"fixId": 17},
}

# One somewhere else, which the question must not carry.
ELSEWHERE = {
    "range": {"start": {"line": 1, "character": 0},
              "end": {"line": 1, "character": 5}},
    "message": "gamma is unused",
    "severity": 2,
}

WITH_EDIT = {
    "title": "Replace alpha with omega",
    "kind": "quickfix",
    "isPreferred": True,
}

WITH_COMMAND = {
    "title": "Sort the second line",
    "kind": "source.organizeImports",
    "command": {"title": "Sort", "command": "fake.sort", "arguments": ["second"]},
}

DISABLED = {
    "title": "Extract method",
    "kind": "refactor.extract",
    "disabled": {"reason": "Select a statement first"},
}


def edit(path, line, start, end, text):
    """A WorkspaceEdit replacing one range of one file."""
    return {"changes": {"file://" + path: [
        {"range": {"start": {"line": line, "character": start},
                   "end": {"line": line, "character": end}},
         "newText": text}]}}


def setup(s):
    s.plugin("Lsp")

    path = s.write("workdir/hello.txt", CONTENT)
    s.open(path)

    with_edit = dict(WITH_EDIT, edit=edit(path, 0, 0, 5, "omega"))

    s.lsp_server(filter="globs:*.txt",
                 diagnostics=[DIAGNOSTIC, ELSEWHERE],
                 replies={"textDocument/codeAction":
                          [with_edit, WITH_COMMAND, DISABLED]},
                 apply_edit={"workspace/executeCommand":
                             edit(path, 1, 6, 11, "sorted")})


def run(t):
    t.wait_lsp("textDocument/didOpen")

    t.focus()
    t.key("ctrl+Home")

    menu = actions(t)

    for title in ("Replace alpha with omega", "Sort the second line",
                  "Extract method"):
        t.item(menu, title)
    t.log("ok: every action the server offered is in the menu")

    # What was asked, which is the half of this feature that has no pixels.
    asked = t.wait_lsp("textDocument/codeAction")["params"]

    t.check(asked["range"] == {"start": {"line": 0, "character": 0},
                               "end": {"line": 0, "character": 0}},
            "with no selection the question is an empty range at the cursor: %s"
            % asked["range"])

    carried = asked["context"]["diagnostics"]

    t.check(len(carried) == 1 and carried[0]["message"] == DIAGNOSTIC["message"],
            "only the diagnostic the cursor is in is handed back: %s"
            % [d.get("message") for d in carried])
    t.check(carried[0].get("data") == DIAGNOSTIC["data"],
            "and it is handed back as it arrived, unread members and all: %s"
            % carried[0])
    t.check(asked["context"].get("triggerKind") == 1,
            "a person asked for it, which is triggerKind 1: %s"
            % asked["context"].get("triggerKind"))

    # An action with an edit changes the document and nothing else.
    t.click(t.item(menu, "Replace alpha with omega"))
    t.no_menu()

    save(t)
    t.wait(lambda: t.sandbox.read("workdir/hello.txt") == FIXED,
           "the edit of the action to be applied; the file holds %r"
           % t.sandbox.read("workdir/hello.txt"))
    t.check(t.lsp("workspace/executeCommand") == [],
            "an action that is only an edit runs no command")
    t.log("ok: an action carrying an edit applies it")

    # An action with a command sends it, and what the command changed comes
    # back as the server's own request, which medit has to answer and apply.
    t.focus()
    t.click(t.item(actions(t), "Sort the second line"))
    t.no_menu()

    ran = t.wait_lsp("workspace/executeCommand")["params"]

    t.check(ran.get("command") == "fake.sort",
            "the command of the action is what was sent: %s" % ran.get("command"))
    t.check(ran.get("arguments") == ["second"],
            "with the arguments of its Command: %s" % ran.get("arguments"))

    save(t)
    t.wait(lambda: t.sandbox.read("workdir/hello.txt") == COMMANDED,
           "the edit the server asked for afterwards to be applied; the file "
           "holds %r" % t.sandbox.read("workdir/hello.txt"))

    answered = t.wait(lambda: replied(t), "medit to answer workspace/applyEdit")

    t.check(answered.get("applied") is True,
            "and to say it applied it, which is what the protocol asks: %s"
            % answered)
    t.log("ok: a command runs, and what it changed is applied and acknowledged")

    # An action the server says cannot be used is shown, and cannot be used.
    t.focus()
    menu = actions(t)
    disabled = t.item(menu, "Extract method")

    t.check(not t.state(disabled, "sensitive"),
            "the disabled action is offered but not selectable")
    t.log("ok: an action the server disabled is offered but cannot be taken")

    t.escape()
    t.no_menu()

    # A server with nothing to offer says so rather than leaving an empty menu.
    t.sandbox.lsp_scenario("test", replies={"textDocument/codeAction": []})

    t.focus()
    t.key(ACCEL)

    empty = t.wait(lambda: alert(t, "No code actions here"),
                   "a dialog saying there is nothing here")
    t.log("ok: nothing to offer is said rather than shown as an empty menu")

    t.click(t.button(empty, "Close"))


ACCEL = "alt+Return"


def actions(t):
    """Ask for the code actions of wherever the cursor is, and take the menu."""
    t.key(ACCEL)

    return t.open_menu()


def save(t):
    t.focus()
    t.key("ctrl+s")

    t.wait(lambda: "[modified]" not in (t.frame.name or ""),
           "the document to be saved; the title is %r" % t.frame.name)


def replied(t):
    """What medit answered the server's workspace/applyEdit with."""
    for message in t.lsp(None):
        if message.get("method") is None and message.get("result") is not None:
            return message["result"]

    return None


def alert(t, needle):
    """The message dialog carrying that text; a GtkMessageDialog has no title."""
    for top in t.on_screen(t.find_all(t.app, role="alert", depth=2)):
        if t.find(top, role="label", depth=8,
                  pred=lambda node: needle in (node.name or "")) is not None:
            return top

    return None
