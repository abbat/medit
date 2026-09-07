"""Find References: every place the server names, listed and gone to.

# requires: MOO_BUILD_LSP, MOO_GTK3

Going to a definition needs one place out of the answer and can throw the rest
away. This needs all of them, which is a pane rather than a jump, and the pane
is the whole of what is new here: a reply is a list of positions in files, some
of which nobody has opened -- so the line each one points at is read off the
disk, and the position is resolved against that text rather than shown in the
server's own UTF-16 counting.

The path is written relative to the root of the project the server was started
for, which is what makes the lines readable at all; the root here is the
directory the two files are in, there being no root markers in this lsp.xml.

GTK+3 only, being a pane: see diagnostics_pane for why a pane has no accessible
on GTK+2.
"""

CONTENT = "alpha beta\ngamma alpha\n"
OTHER = "one\ntwo alpha\nthree\n"

# Two in the document that is open and one in the file that is not.
EXPECTED = [
    "hello.txt:1:1  alpha beta",
    "hello.txt:2:7  gamma alpha",
    "other.txt:2:5  two alpha",
]

NOTHING = "No references found"


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))

    # Written and not opened. A use in a file nobody has looked at yet is still
    # a use, and it is the case with the work in it: nothing in medit knows
    # what is in that file until the pane reads it.
    s.write("workdir/other.txt", OTHER)

    s.lsp_server(filter="globs:*.txt",
                 replies={"textDocument/references": [
                     at(s, "workdir/hello.txt", 0, 0),
                     at(s, "workdir/hello.txt", 1, 6),
                     at(s, "workdir/other.txt", 1, 4)]})


def at(s, name, line, character):
    """A Location, the way a server answers with one."""
    return {"uri": "file://" + s.path(name),
            "range": {"start": {"line": line, "character": character},
                      "end": {"line": line, "character": character + 5}}}


def run(t):
    t.wait_lsp("textDocument/didOpen")

    t.focus()
    t.key("ctrl+Home")

    t.menu("Document", "Find References")

    asked = t.wait_lsp("textDocument/references")["params"]

    t.check(asked["position"] == {"line": 0, "character": 0},
            "the server was asked about the position of the cursor: %s" % asked["position"])
    t.check(asked.get("context", {}).get("includeDeclaration") is True,
            "and asked to count the declaration as a use: %s" % asked.get("context"))

    # Nobody opened the pane: an answer that arrives with nowhere to be seen is
    # the same as no answer, so the reply presents it.
    pane = t.wait(lambda: references_pane(t), "the References pane to present itself")
    t.wait_text(pane, EXPECTED[0], what="the first reference")

    lines = [line for line in t.text(pane).splitlines() if line.strip()]

    t.check(lines == EXPECTED,
            "every reference is listed, in the server's order:\n      %s"
            % "\n      ".join(lines))

    # The one in the file that is not open: clicking it has to open the file
    # and land on the character the server named. It is also the last thing
    # this opening of the pane is good for -- activating a line hands the focus
    # to the document, and a pane that is not sticky hides itself when it does.
    line = t.text(pane).index(EXPECTED[2])
    t.click_range(pane, line, line + len(EXPECTED[2]))

    t.wait(lambda: "other.txt" in (t.frame.name or ""),
           "the file the reference is in to be opened")
    t.wait(lambda: cursor(t) == "Line: 2 Col: 5",
           "and the cursor to be put on it; it is at %s" % cursor(t))
    t.log("ok: a reference in a file that was not open opens it")

    # An answer with nothing in it, asked for from the context menu this time:
    # the entry there goes through the same code as the menu bar's, and an
    # empty pane is indistinguishable from a question that was never asked.
    t.sandbox.lsp_scenario("test", replies={"textDocument/references": []})

    asked = len(t.lsp("textDocument/references"))

    menu = t.popup()
    t.click(t.item(menu, "Find References"))

    t.wait(lambda: len(t.lsp("textDocument/references")) > asked,
           "the second question to reach the server")
    t.wait(lambda: t.text(pane).strip() == NOTHING,
           "the pane to say there are none; it says %r" % t.text(pane).strip())
    t.log("ok: an empty answer says so instead of leaving the last one up")


def references_pane(t):
    """The pane's text view: the one on screen that cannot be typed into.

    Told apart from the document by that rather than by what it holds, the way
    the diagnostics pane is -- and there is only ever one bottom pane on screen.
    """
    views = [view for view in t.on_screen(t.find_all(t.frame, role="text", depth=30))
             if not t.state(view, "editable")]

    return views[0] if len(views) == 1 else None


def cursor(t):
    label = t.find(t.frame, role="label", name_prefix="Line:", depth=25)
    return label.name if label is not None else None
