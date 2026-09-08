"""Reopen Using Encoding reads the file again, and the choice sticks when it is saved.

# requires: MOO_GTK3

The encoding menu under File is medit's own, and what it does is re-read the
bytes on disk through the encoding that was picked. It matters twice. Read
wrongly, a document shows mojibake; saved after being read wrongly, the file is
rewritten from that mojibake and the text on disk is destroyed -- which is why
this test does not stop at what the window shows and goes on to check the bytes.

The file here is Cyrillic in windows-1251, which medit cannot read as UTF-8 and
falls back to a single-byte encoding for -- so the document starts out wrong,
which is the situation the menu exists for.
"""

TEXT = "Привет, мир\n"

ENCODING = "windows-1251"

MENU = ("File", "Reopen Using Encoding", "East European", "Cyrillic (Windows-1251)")

NAME = "cyrillic.txt"

TYPED = "!"


def setup(s):
    s.open(s.write_bytes("workdir/" + NAME, TEXT.encode(ENCODING)))


def run(t):
    view = t.document()

    t.check(t.text(view) != TEXT,
            "the file does not read as UTF-8, so the document starts out wrong: %r"
            % t.text(view))

    t.menu(*MENU)

    t.wait(lambda: t.text(t.document()) == TEXT,
           "the document to hold the text the file means; it holds %r"
           % t.text(t.document()))
    t.log("ok: reopening with the right encoding read the file properly")

    t.check("[modified]" not in (t.frame.name or ""),
            "and re-reading the file is not a change to the document")

    # The encoding it was read with is the encoding it is written with. Anything
    # else would rewrite the file in another encoding behind the user's back.
    t.focus()
    t.click(t.document())
    t.key("ctrl+Home")
    t.type_text(TYPED)
    t.wait(lambda: t.text(t.document()) == TYPED + TEXT, "the typing to reach it")

    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")

    t.check(t.sandbox.read_bytes("workdir", NAME) == (TYPED + TEXT).encode(ENCODING),
            "the file was written back in %s, not in something else" % ENCODING)
