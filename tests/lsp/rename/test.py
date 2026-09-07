"""Rename: one word, several places at once, and one undo step for each file.

# requires: MOO_BUILD_LSP

What the server sends back is a WorkspaceEdit -- a set of ranges to replace,
possibly in files that are not open -- and applying one has three things in it
that a screenshot would not show:

* two edits on the same line must be applied from the back, or the first one
  moves the second and the text comes out mangled;
* a file nobody has opened has to be opened to be edited, and left open, so
  that what happened to it can be looked at and undone;
* the edits of one file are one undo step, not one per range.

So this is asserted from the files on disk, saved by hand at each step, which
also makes it a test both toolkits can run: nothing here reads the text out of
the document's accessible.

Nothing is saved by the rename itself. A rename touching files the user never
opened is exactly the operation worth looking at before it is on disk.
"""

CONTENT = "alpha beta alpha\ngamma\n"
OTHER = "one alpha two\n"

# Longer than what it replaces, so that applying the two edits of the first
# line in the order the server listed them mangles it rather than failing
# outright: the first replacement moves everything after it by a character, and
# the second range then covers " alph", leaving "omegaX betaomegaXa".
NEW = "omegaX"

RENAMED = "omegaX beta omegaX\ngamma\n"
OTHER_RENAMED = "one omegaX two\n"

REFUSED = "cannot rename that"


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))

    # Not opened: the rename has to open it.
    s.write("workdir/other.txt", OTHER)

    s.lsp_server(filter="globs:*.txt", replies={"textDocument/rename": {
        "changes": {
            # In the order a server naturally lists them, which is the order
            # they must not be applied in.
            "file://" + s.path("workdir/hello.txt"): [change(0, 0, 5),
                                                      change(0, 11, 16)],
            "file://" + s.path("workdir/other.txt"): [change(0, 4, 9)]}}})


def change(line, start, end):
    """One TextEdit of a WorkspaceEdit."""
    return {"range": {"start": {"line": line, "character": start},
                      "end": {"line": line, "character": end}},
            "newText": NEW}


def run(t):
    t.wait_lsp("textDocument/didOpen")

    t.focus()
    t.key("ctrl+Home")

    # Cancelled first. The dialog offers the word the cursor is in -- a rename
    # nearly always keeps most of the name -- and Escape has to leave both the
    # document and the server alone.
    dialog = ask(t)
    entry = t.need(dialog, role="text", what="the entry of the Rename dialog")

    t.check(t.text(entry) == "alpha",
            "the dialog offers the word under the cursor: %r" % t.text(entry))

    t.escape()
    t.no_toplevel("Rename")

    t.check(t.lsp("textDocument/rename") == [],
            "a cancelled rename asks the server nothing")

    # And now for real.
    rename(t, NEW)

    asked = t.wait_lsp("textDocument/rename")["params"]

    t.check(asked["newName"] == NEW,
            "the server was asked for the new name: %r" % asked.get("newName"))
    t.check(asked["position"] == {"line": 0, "character": 0},
            "about the position of the cursor: %s" % asked["position"])

    t.wait(lambda: "other.txt" in (t.frame.name or ""),
           "the file that was not open to be opened for its edit")
    t.log("ok: a file the edit names is opened rather than written behind the user")

    save(t, "hello.txt")
    save(t, "other.txt")

    t.wait(lambda: t.sandbox.read("workdir/hello.txt") == RENAMED,
           "both places in the open document to be replaced; it holds %r"
           % t.sandbox.read("workdir/hello.txt"))
    t.wait(lambda: t.sandbox.read("workdir/other.txt") == OTHER_RENAMED,
           "and the place in the other file; it holds %r"
           % t.sandbox.read("workdir/other.txt"))
    t.log("ok: every range the server named was replaced, back to front")

    # One undo step for the file, not one per range: a rename the user does not
    # like is one Ctrl+Z, and a rename that left half of itself behind would be
    # worse than one that never happened.
    t.menu("Window", "hello.txt")
    t.focus()
    t.key("ctrl+z")
    t.key("ctrl+s")

    t.wait(lambda: t.sandbox.read("workdir/hello.txt") == CONTENT,
           "one undo to take both edits of the file back; it holds %r"
           % t.sandbox.read("workdir/hello.txt"))
    t.log("ok: the edits of one file are one undo step")

    # A server that refuses has to be heard. It answers an error rather than an
    # empty edit, and the client that swallowed it would leave the user with a
    # dialog they filled in and a document nothing happened to.
    t.sandbox.lsp_scenario("test", errors={"textDocument/rename":
                                           {"code": -32603, "message": REFUSED}})

    t.focus()
    t.key("ctrl+Home")
    rename(t, "whatever")

    reported = t.wait(lambda: alert(t, REFUSED),
                      "a dialog repeating what the server said")
    t.log("ok: a rename the server refuses says so")

    t.click(t.button(reported, "Close"))
    t.wait(lambda: alert(t, REFUSED) is None, "the dialog to close")


def ask(t):
    """Open the Rename dialog on whatever the cursor is in."""
    t.menu("Document", "Rename")

    return t.dialog("Rename")


def rename(t, name):
    """Open it, put a name in it, and press the button."""
    dialog = ask(t)
    entry = t.need(dialog, role="text", what="the entry of the Rename dialog")

    # Clicking the entry puts the caret in it and takes the selection off what
    # was offered, so the old name is selected again by hand.
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(name)

    t.click(t.button(dialog, "Rename"))
    t.no_toplevel("Rename")


def save(t, name):
    """Save one of the open documents, by the name the Window menu lists it under."""
    t.menu("Window", name)
    t.focus()
    t.key("ctrl+s")

    t.wait(lambda: "[modified]" not in (t.frame.name or ""),
           "%s to be saved; the title is %r" % (name, t.frame.name))


def alert(t, needle):
    """The message dialog carrying that text, whatever at-spi calls it here.

    Not t.dialog(): a GtkMessageDialog has no title to look it up by, and its
    role is not "dialog" but "alert" -- on both toolkits, measured, which is
    the only reason this says so.
    """
    for top in t.on_screen(t.find_all(t.app, role="alert", depth=2)):
        if t.find(top, role="label", depth=8,
                  pred=lambda node: needle in (node.name or "")) is not None:
            return top

    return None
