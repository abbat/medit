"""Rename with prepareRename: the server, not the word at the cursor, names what is renamed.

# requires: MOO_BUILD_LSP

A server announcing renameProvider.prepareProvider is asked first whether
there is anything to rename at the cursor and what range it covers. The dialog
then offers the text of that range, the rename is asked at its start, and a
refusal -- null -- is said to the user before any dialog to type a name into.

The cursor stays on "alpha" while the server points at "beta", so a client
still guessing from the word under the cursor offers the wrong name.
"""

CONTENT = "alpha beta\n"

NOTHING = "There is nothing to rename here."


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))

    s.lsp_server(filter="globs:*.txt", **scenario(s, range_(0, 6, 10)))


def range_(line, start, end):
    return {"start": {"line": line, "character": start},
            "end": {"line": line, "character": end}}


def scenario(s, prepared):
    # A real edit for the rename: a null one is an error dialog of its own.
    uri = "file://" + s.path("workdir/hello.txt")
    edit = {"changes": {uri: [{"range": range_(0, 6, 10), "newText": "omega"}]}}

    return {"capabilities": {"renameProvider": {"prepareProvider": True}},
            "replies": {"textDocument/prepareRename": prepared,
                        "textDocument/rename": edit}}


def run(t):
    t.wait_lsp("textDocument/didOpen")

    t.focus()
    t.key("ctrl+Home")

    t.menu("Document", "Rename")

    asked = t.wait_lsp("textDocument/prepareRename")["params"]
    t.check(asked["position"] == {"line": 0, "character": 0},
            "prepareRename is asked about the cursor: %s" % asked["position"])

    dialog = t.dialog("Rename")
    entry = t.need(dialog, role="text", what="the entry of the Rename dialog")

    t.check(t.text(entry) == "beta",
            "the dialog offers the range the server named: %r" % t.text(entry))

    t.click(entry)
    t.key("ctrl+a")
    t.type_text("omega")
    t.click(t.button(dialog, "Rename"))
    t.no_toplevel("Rename")

    renamed = t.wait_lsp("textDocument/rename")["params"]
    t.check(renamed["position"] == {"line": 0, "character": 6},
            "the rename is asked at the start of that range: %s"
            % renamed["position"])
    t.wait(lambda: "[modified]" in (t.frame.name or ""),
           "the rename's edit to be applied; the title is %r" % t.frame.name)
    t.log("ok: the server's range decides what is renamed")

    # Saved, or quitting at the end stops at the question whether to.
    t.focus()
    t.key("ctrl+s")
    t.wait(lambda: t.sandbox.read("workdir/hello.txt") == "alpha omega\n",
           "the renamed text to be saved; the file holds %r"
           % t.sandbox.read("workdir/hello.txt"))

    # Null: nothing to rename here. Said, and no rename sent.
    t.sandbox.lsp_scenario("test", **scenario(t.sandbox, None))

    t.focus()
    t.key("ctrl+Home")
    t.menu("Document", "Rename")

    t.wait_lsp("textDocument/prepareRename", count=2)

    reported = t.wait(lambda: alert(t, NOTHING),
                      "a dialog saying there is nothing to rename")
    t.no_toplevel("Rename")
    t.check(len(t.lsp("textDocument/rename")) == 1,
            "a refused prepareRename sends no rename")
    t.log("ok: a refused prepareRename says so and renames nothing")

    t.click(t.button(reported, "Close"))
    t.wait(lambda: alert(t, NOTHING) is None, "the dialog to close")


def alert(t, needle):
    """The message dialog carrying that text; see lsp/rename for why not t.dialog()."""
    for top in t.on_screen(t.find_all(t.app, role="alert", depth=2)):
        if t.find(top, role="label", depth=8,
                  pred=lambda node: needle in (node.name or "")) is not None:
            return top

    return None
