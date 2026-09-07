"""What the server says is wrong with the document, listed and clickable.

# requires: MOO_BUILD_LSP, MOO_GTK3

GTK+3 only because it is a pane: a pane and its button are internal children of
MooPaned, which GtkContainerAccessible does not list, and MooPanedAccessible --
the class that puts them on the bus -- cannot exist on GTK+2, where those types
live inside the gail module.

The line the pane draws is the whole answer: the position resolved in the
buffer rather than in the server's own UTF-16 counting, the severity by name,
the message, and the source and code the server gave, in brackets.
"""

CONTENT = "alpha beta\ngamma delta\n"

DIAGNOSTICS = [
    {"range": {"start": {"line": 0, "character": 0},
               "end": {"line": 0, "character": 5}},
     "severity": 1, "source": "fake", "code": "E1",
     "message": "alpha is undefined"},
    {"range": {"start": {"line": 1, "character": 6},
               "end": {"line": 1, "character": 11}},
     "severity": 2,
     "message": "delta is never used"},
    {"range": {"start": {"line": 1, "character": 0},
               "end": {"line": 1, "character": 5}},
     "severity": 3, "source": "fake",
     "message": "gamma could be shorter"},
]

EXPECTED = [
    "1:1  error: alpha is undefined  [fake E1]",
    "2:7  warning: delta is never used",
    "2:1  information: gamma could be shorter  [fake]",
]


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))
    s.open(s.write("workdir/notes.md", "nothing here\n"))
    s.lsp_server(filter="globs:*.txt", diagnostics=DIAGNOSTICS)


def run(t):
    t.wait_lsp("textDocument/didOpen")

    # The document opened last is the one on screen, and it is the one no
    # server handles, so switch to the one that has diagnostics first.
    t.menu("Window", "hello.txt")
    t.menu("Tools", "Diagnostics")

    pane = t.wait(lambda: diagnostics_pane(t), "the diagnostics pane")
    t.wait_text(pane, EXPECTED[0], what="the first diagnostic")

    lines = [line for line in t.text(pane).splitlines() if line.strip()]

    t.check(lines == EXPECTED,
            "every diagnostic is listed, in the server's order:\n      %s"
            % "\n      ".join(lines))

    # Clicking a line goes there. The second one is the one worth clicking: the
    # server counted its character in UTF-16 code units from the start of the
    # line, and 2:7 is that resolved against the buffer.
    #
    # It is also the last thing that can be done with the pane in one opening:
    # activating a line hands the focus to the document, and a pane that is not
    # sticky hides itself when it loses the focus -- after which its accessible
    # is still readable but the document is drawn where it was, so a second
    # click at coordinates taken from it would land in the text.
    line = t.text(pane).index(EXPECTED[1])
    t.click_range(pane, line, line + len(EXPECTED[1]))

    t.wait(lambda: cursor(t) == "Line: 2 Col: 7",
           "the cursor to go where the line points; it went to %s" % cursor(t))
    t.log("ok: clicking a diagnostic moves the cursor to it")

    t.check(not t.state(pane, "showing"),
            "and the pane closed itself, having handed the focus over")

    # The pane follows the document rather than the window: the other one is
    # open in the same window and has no server at all.
    t.menu("Window", "notes.md")
    t.menu("Tools", "Diagnostics")

    t.wait(lambda: t.text(pane).strip() == "",
           "the pane to empty for a document no server handles")
    t.log("ok: the pane follows whichever document is active")


def diagnostics_pane(t):
    """The pane's text view: the one on screen that cannot be typed into.

    Told apart from the document by that rather than by what it holds, since an
    empty pane is one of the things asserted here.
    """
    views = [view for view in t.on_screen(t.find_all(t.frame, role="text", depth=30))
             if not t.state(view, "editable")]

    return views[0] if len(views) == 1 else None


def cursor(t):
    label = t.find(t.frame, role="label", name_prefix="Line:", depth=25)
    return label.name if label is not None else None
