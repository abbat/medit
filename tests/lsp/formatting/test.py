"""Format Document: the server rewrites the file, medit applies what it says.

# requires: MOO_BUILD_LSP

The reply is a list of TextEdits over the document that was sent, which is the
same thing a rename comes back with for one file -- so the applying, the order
and the single undo step are the same code and are asserted the same way, on
the bytes in the sandbox rather than on the text of a widget. Both toolkits.

What is new here is what goes out: a formatting request carries the settings
medit would indent with, and a server that is not told them formats to its own
defaults and undoes the user's. tabSize and insertSpaces are the document's
own, and the two options that say what medit does when it saves are sent from
the settings that do it.

Nothing is saved by the formatting itself, for the same reason a rename saves
nothing: what a formatter did is worth looking at before it is on disk.
"""

CONTENT = "alpha   beta   gamma\n"
FORMATTED = "alpha beta gamma\n"

# Not the default 8, so that a tabSize of 8 in the request would be the
# preference not being read rather than a coincidence.
WIDTH = 3


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))

    s.pref("Editor/indent_width", WIDTH)
    s.pref("Editor/spaces_instead_of_tabs", True)
    s.pref("Editor/strip", True)

    # Two edits on one line, in the order a formatter reports them and not the
    # order they can be applied in: replacing the first run of spaces moves the
    # second, and applying them front to back leaves "alpha beta   mma".
    s.lsp_server(filter="globs:*.txt", replies={"textDocument/formatting": [
        edit(0, 5, 0, 8, " "),
        edit(0, 12, 0, 15, " "),
    ]})


def edit(line, start, end_line, end, text):
    return {"range": {"start": {"line": line, "character": start},
                      "end": {"line": end_line, "character": end}},
            "newText": text}


def run(t):
    t.wait_lsp("textDocument/didOpen")

    t.menu("Document", "Format Document")

    asked = t.wait_lsp("textDocument/formatting")["params"]
    options = asked.get("options", {})

    t.check(asked["textDocument"]["uri"].endswith("hello.txt"),
            "the server was asked to format the document that is open")
    t.check(options.get("tabSize") == WIDTH,
            "and told how wide medit indents: %s" % options)
    t.check(options.get("insertSpaces") is True,
            "and that it indents with spaces")
    t.check(options.get("trimTrailingWhitespace") is True,
            "and that medit strips trailing whitespace itself: %s" % options)

    t.focus()
    t.key("ctrl+s")

    t.wait(lambda: t.sandbox.read("workdir/hello.txt") == FORMATTED,
           "every edit to reach the file; it holds %r"
           % t.sandbox.read("workdir/hello.txt"))
    t.log("ok: the document is what the server said it should be")

    # One undo step for the whole reformatting: a formatter one does not like
    # is one Ctrl+Z, not one per edit it made.
    t.key("ctrl+z")
    t.key("ctrl+s")

    t.wait(lambda: t.sandbox.read("workdir/hello.txt") == CONTENT,
           "one undo to take the whole formatting back; it holds %r"
           % t.sandbox.read("workdir/hello.txt"))
    t.log("ok: the formatting is one undo step")
