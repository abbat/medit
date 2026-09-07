"""The document being edited is readable, and what is typed goes into it.

# requires: MOO_BUILD_TERMINAL

The requirement is not about the terminal: it is how this test says GTK+3 only.
The editor's notebook keeps its own pages and inherits GtkNotebook's accessible,
which reads GtkNotebook's -- so until MooNotebookAccessible the document was not
in the tree at all, and on GTK+2, where gail's classes cannot be subclassed, it
still is not.
"""

CONTENT = "hello from the document\n"
TYPED = "and this was typed"


def setup(s):
    s.open(s.write("workdir/hello.txt", CONTENT))


def run(t):
    view = document(t)

    t.check(t.text(view) == CONTENT,
            "the document holds what the file it was opened from holds")
    t.check(chars(t) == len(CONTENT),
            "and the status bar counts the same %d characters" % len(CONTENT))

    t.click(view)
    t.key("ctrl+End")
    t.type_text(TYPED)

    expected = CONTENT + TYPED
    t.wait(lambda: t.text(view) == expected, "what was typed to reach the document")
    t.log("ok: typing at the end of the document appends to it")

    t.check(chars(t) == len(expected),
            "the status bar counts the %d characters now in it" % len(expected))

    # Saved, or the quit at the end of the test turns into a dialog about it.
    t.check("[modified]" in (t.frame.name or ""), "the document counts as modified")
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")

    t.check(t.sandbox.read("workdir/hello.txt") == expected,
            "and the file on disk has what the document had")


def document(t):
    """The text view of the open document.

    The one text widget on screen: the others in the tree belong to panes that
    are not open, and have no position.
    """
    views = t.on_screen(t.find_all(t.frame, role="text", depth=25))

    t.check(len(views) == 1, "the document is the one text widget on screen")

    return views[0]


def chars(t):
    label = t.find(t.frame, role="label", name_prefix="Chars:", depth=25)
    return int(label.name.split(":")[1]) if label is not None else None
