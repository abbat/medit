"""Saving text an encoding cannot hold: medit offers UTF-8 instead.

# requires: MOO_GTK3

A document remembers the encoding it was read as and writes itself back in it, so
a character that encoding has no room for is a save that cannot happen.
moo_editor_save() catches exactly that error and asks --
_moo_edit_save_error_enc_dialog(), "Could not save file … in encoding … Do you
want to save it in UTF-8 encoding instead?" -- and either writes UTF-8 or writes
nothing at all. Nothing had reached it.

Which makes it the fourth of medit's data-loss paths, beside the save dialogs of
tests/file/close_changes, the reload dialog of tests/file/reload and the overwrite
dialog of tests/file/overwrite_modified. Both answers are given here too: Cancel
leaves the file as it was, and OK writes the text as UTF-8 -- and the bytes on
disk say which happened.
"""

NAME = "letter.txt"

CYRILLIC = "привет\n"

# A character no single-byte Cyrillic encoding has any room for.
OUTSIDE = "日"

ENCODING = ("Document", "Encoding")

GROUP = "East European"
CP1251 = "Cyrillic (Windows-1251)"


def setup(s):
    s.open(s.write_bytes("workdir/" + NAME, CYRILLIC.encode("utf-8")))


def run(t):
    view = t.document()

    # The document is written back in whatever it was read as, so first it is
    # given an encoding that cannot hold what is about to be typed.
    t.menu(*ENCODING, GROUP, CP1251)

    t.focus()
    t.click(view)
    t.key("ctrl+End")
    t.type_text(OUTSIDE)

    t.wait(lambda: t.text(view) == CYRILLIC + OUTSIDE,
           "the character to be typed; the document holds %r" % t.text(view))

    # Cancelled: nothing is written.
    t.key("ctrl+s")
    dialog = t.wait(lambda: the_question(t), "the question about the encoding")

    asked = " ".join(label.name or "" for label in t.find_all(dialog, role="label"))
    # The dialog names the encoding as the document holds it, which is the name
    # from the table rather than the label the menu shows.
    t.check(NAME in asked and "WINDOWS-1251" in asked,
            "the question names the file and the encoding: %r" % asked)

    t.click(t.button(dialog, "Cancel"))
    t.wait(lambda: the_question(t) is None, "the question to go")

    t.check(t.sandbox.read_bytes("workdir", NAME) == CYRILLIC.encode("utf-8"),
            "Cancel wrote nothing: the file is as it was, %r"
            % t.sandbox.read_bytes("workdir", NAME))

    # Accepted: the whole document goes to disk as UTF-8.
    t.focus()
    t.key("ctrl+s")
    dialog = t.wait(lambda: the_question(t), "the question again")
    t.click(t.button(dialog, "OK"))

    t.wait(lambda: t.sandbox.read_bytes("workdir", NAME)
           == (CYRILLIC + OUTSIDE).encode("utf-8"),
           "OK to write the document as UTF-8; the file holds %r"
           % t.sandbox.read_bytes("workdir", NAME))


def the_question(t):
    """The question, which is a message dialog and so an alert."""
    found = t.find_all(t.app, role="alert", depth=2)

    return found[0] if found else None
