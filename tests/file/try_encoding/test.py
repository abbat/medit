"""Opening a file no encoding on the list can read: medit asks which to try.

# requires: MOO_GTK3

medit guesses the encoding of a file it opens by trying the ones on the
autodetect list in turn, and when none of them produces valid text there is
nothing sensible left to do -- so _moo_edit_try_encoding_dialog() puts up
mootryencoding.ui, "Could not open file … try to select another encoding below",
and the loop in convert_file_data_to_utf8_with_prompt() tries again with what was
chosen. Nothing had ever produced a file it could not read.

The list is a preference, so the test sets it to UTF-8 alone and writes a file of
single-byte Cyrillic, which is not valid UTF-8 in any part. Both answers are
given: Cancel opens nothing at all, and choosing the encoding opens the file with
its text readable, which is the loop having gone round.
"""

from lib.notebook import order

UNREADABLE = "letter.txt"

CYRILLIC = "привет\n"

CHOSEN = "WINDOWS-1251"

OPEN_ALREADY = "notes.txt"


def setup(s):
    # The encodings medit tries by itself, and nothing else: without this the
    # guess falls back to a single-byte encoding that reads any bytes at all.
    s.pref("Editor/encodings", "UTF-8")

    s.write_bytes("workdir/" + UNREADABLE, CYRILLIC.encode("windows-1251"))
    s.open(s.write("workdir/" + OPEN_ALREADY, "hello\n"))


def run(t):
    # Cancelled: the file is not opened.
    open_by_path(t, t.sandbox.path("workdir", UNREADABLE))

    dialog = t.wait(lambda: the_dialog(t), "the question about the encoding")

    asked = " ".join(label.name or "" for label in t.find_all(dialog, role="label"))
    t.check(UNREADABLE in asked,
            "the question names the file it could not read: %r" % asked)

    t.click(t.button(dialog, "Cancel"))
    t.wait(lambda: the_dialog(t) is None, "the question to go")

    t.check(order(t) == [OPEN_ALREADY],
            "Cancel opened nothing: the strip holds %s" % order(t))

    # And again, with an encoding that can read it.
    open_by_path(t, t.sandbox.path("workdir", UNREADABLE))
    dialog = t.wait(lambda: the_dialog(t), "the question again")

    # The combo has an entry of its own, so the encoding is typed rather than
    # picked out of the list.
    entry = t.need(dialog, role="text", what="the encoding entry of the dialog")
    t.focus()
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(CHOSEN)

    t.click(t.button(dialog, "OK"))
    t.wait(lambda: the_dialog(t) is None, "the question to go")

    t.wait(lambda: sorted(order(t)) == sorted([OPEN_ALREADY, UNREADABLE]),
           "the file to be opened with the encoding that was chosen; "
           "the strip holds %s" % order(t))

    view = t.document()
    t.check(t.text(view) == CYRILLIC,
            "and its text is readable: %r" % t.text(view))


def the_dialog(t):
    """The try-encoding dialog, which is the only dialog this test opens."""
    found = [node for node in t.find_all(t.app, role="dialog", depth=2)]

    return found[0] if found else None


def open_by_path(t, path):
    """Open the File/Open chooser and give it a path through its location entry."""
    t.menu("File", "Open...")
    t.need(t.app, role="file chooser", depth=2, what="the Open dialog")

    t.focus()
    t.key("ctrl+l")
    t.type_text(path)
    t.key("Return")

    t.wait(lambda: t.find(t.app, role="file chooser", depth=2) is None,
           "the Open dialog to close")
