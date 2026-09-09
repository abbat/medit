"""The Encoding menu says what a document will be written as, and changes it.

# requires: MOO_GTK3

create_doc_encoding_action() hangs _moo_encodings_menu_new()'s menu off the
Document menu and update_doc_encoding_item() keeps its first item on the encoding
of the document on screen. Choosing another one is not a re-reading of the file --
that is File -> Reload with an encoding, tested elsewhere -- it says what the next
save will write, so the bytes on disk after a save are what proves it took.

The text is Cyrillic on purpose: it is the shortest thing that is different in
UTF-8 and in a single-byte encoding, so the file on disk says which one was used
without any inspection of medit at all.
"""

NAME = "letter.txt"

CONTENT = "привет\nмир\n"

ENCODING = ("Document", "Encoding")

UTF8 = "UTF-8"
CP1251 = "Cyrillic (Windows-1251)"

GROUP = "East European"
UNICODE = "Unicode"


def setup(s):
    s.open(s.write_bytes("workdir/" + NAME, CONTENT.encode("utf-8")))


def run(t):
    view = t.document()

    t.check(t.text(view) == CONTENT,
            "the file was read as UTF-8, which it is: %r" % t.text(view))

    # Read in one opening and read once: t.menu() clicks its way in, and a click
    # on a menu that is already open closes it again.
    said = current(t)
    t.check(said == UTF8, "and the menu says so: %r" % said)

    t.menu(*ENCODING, GROUP, CP1251)

    said = current(t)
    t.check(said == CP1251, "the choice moves the menu to it: %r" % said)

    t.check(t.text(view) == CONTENT,
            "and does not re-read the text: %r" % t.text(view))

    body = save(t, "!")

    t.check(body == (CONTENT + "!").encode("windows-1251"),
            "the save wrote single-byte Cyrillic: %r" % body)

    # And back, so that the menu is shown to work in both directions.
    t.menu(*ENCODING, UNICODE, UTF8)

    said = current(t)
    t.check(said == UTF8, "the menu goes back to UTF-8: %r" % said)

    body = save(t, "?")

    t.check(body == (CONTENT + "!?").encode("utf-8"),
            "and the save wrote UTF-8 again: %r" % body)


def current(t):
    """The encoding the menu names first, which is the document's own."""
    menu = t.menu(*ENCODING)

    items = [item.name for item in t.on_screen(t.find_all(menu, depth=1))
             if item.name]

    # One per level opened: a submenu takes the first Escape and the menu bar the
    # next, and an Escape with nothing open is nothing.
    t.escape()
    t.escape()

    return items[0] if items else None


def save(t, tail):
    """Type that at the end of the document, save, and read the file back."""
    t.focus()
    t.click(t.document())
    t.key("ctrl+End")
    t.type_text(tail)
    t.key("ctrl+s")

    t.wait(lambda: "[modified]" not in (t.frame.name or ""),
           "the document to be saved")

    return t.sandbox.read_bytes("workdir", NAME)
