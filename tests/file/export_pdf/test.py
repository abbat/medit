"""Export as PDF names the file after the document and writes a real PDF.

# requires: MOO_GTK3

action_print_pdf() in mooeditwindow.cpp offers a name for the file before the
chooser is shown: the document's own basename with its extension replaced by
.pdf, so notes.txt becomes notes.pdf. Then it prints the document through
GtkPrintOperation into whatever came back.

Both halves are here. The offered name is medit's arithmetic on a file name,
which has an off-by-one in it waiting to happen -- the code copies up to the last
dot by hand. And the file that comes out has to actually be a PDF, which the
first bytes say and nothing else does: a print that silently produced an empty
file would leave the document looking exported.
"""

NAME = "notes.txt"

OFFERED = "notes.pdf"

WRITTEN = "exported.pdf"

BODY = "a line to print\nand another\n"


def setup(s):
    s.open(s.write("workdir/" + NAME, BODY))


def run(t):
    t.menu("File", "Export as PDF...")

    dialog = t.need(t.app, role="file chooser", depth=2,
                    what="the chooser for the PDF to write")

    entry = the_name_entry(t, dialog)

    t.check(t.text(entry) == OFFERED,
            "the name offered is the document's with a pdf extension: %r"
            % t.text(entry))

    # An absolute path, since this chooser starts wherever it last was -- nowhere,
    # in a fresh sandbox -- and where a bare name would land is not something a
    # test should have to guess. Enter rather than the button, which is below the
    # bottom of the screen the tests run on.
    t.click(entry)
    t.key("ctrl+a")
    t.type_text(t.sandbox.path("workdir", WRITTEN))
    t.key("Return")

    t.wait(lambda: t.find(t.app, role="file chooser", depth=2) is None,
           "the chooser to close")

    t.wait(lambda: len(t.sandbox.read_bytes("workdir", WRITTEN)) > 0,
           "the PDF to be written")

    written = t.sandbox.read_bytes("workdir", WRITTEN)

    t.check(written.startswith(b"%PDF"),
            "the file that came out is a PDF: it starts %r" % written[:8])
    t.check(len(written) > 500,
            "and it has a document in it: %d bytes" % len(written))


def the_name_entry(t, dialog):
    """The entry beside the "Name:" label, as in the Save As test."""
    for panel in t.find_all(dialog, role="panel", depth=8):
        children = t.find_all(panel, depth=1)

        if "Name:" not in [c.name for c in children if t.role(c) == "label"]:
            continue

        for child in children:
            if t.role(child) == "text":
                return child

    return t.fail("no entry beside a \"Name:\" label in the chooser:\n%s"
                  % t.dump(dialog))
