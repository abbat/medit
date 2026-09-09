"""The line endings menu says what the file uses, and changes what is written.

# requires: MOO_GTK3

create_doc_line_end_action() builds the submenu and update_doc_line_end_item()
keeps its radio on whatever the document was read as -- so the menu is also the
only place medit says which endings a file has. Picking another one is a change to
the document, and what proves it happened is the bytes on disk after a save: the
buffer holds "\\n" for a line break either way, so nothing in the text says which
terminator will be written.

Three endings, three files' worth of bytes: LF as read, then CR+LF, then CR.
"""

NAME = "notes.txt"

LINES = ["alpha", "beta", "gamma"]

UNIX = "Unix (LF)"
WINDOWS = "Windows (CR+LF)"
MAC = "Mac (CR)"

MENU = ("Document", "Line Endings")


def setup(s):
    s.open(s.write_bytes("workdir/" + NAME, "\n".join(LINES).encode() + b"\n"))


def run(t):
    view = t.document()

    t.check(t.text(view) == "\n".join(LINES) + "\n",
            "the document reads as three lines whatever the terminators are")

    # Read once and then asserted: ticked() opens the menu to look, and a click
    # on a menu that is already open closes it -- so calling it twice in one
    # statement, once for the test and once for the message, reads an empty menu
    # the second time.
    state = ticked(t)
    t.check(state == [UNIX],
            "the menu says the file has unix endings, which it has: %s" % state)
    t.escape()

    for entry, terminator in ((WINDOWS, b"\r\n"), (MAC, b"\r"), (UNIX, b"\n")):
        t.menu(*MENU, entry)

        t.focus()
        t.key("ctrl+s")
        t.wait(lambda: "[modified]" not in (t.frame.name or ""),
               "the document to be saved after choosing %s" % entry)

        expected = terminator.join(line.encode() for line in LINES) + terminator

        t.check(t.sandbox.read_bytes("workdir", NAME) == expected,
                "%s writes %r between the lines" % (entry, terminator))

        state = ticked(t)
        t.check(state == [entry], "and the menu now says %s: %s" % (entry, state))
        t.escape()

    t.check(t.text(view) == "\n".join(LINES) + "\n",
            "and through all of it the document reads the same")


def ticked(t):
    """The ending the submenu has its radio on."""
    submenu = t.menu(*MENU)

    return [item.name for item in t.on_screen(t.find_all(submenu, depth=1))
            if item.name and t.state(item, "checked")]
