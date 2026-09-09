"""medit's own page of the Print dialog, and what its options do to the paper.

# requires: MOO_GTK3

The options are the page moo_print_operation_create_custom_widget() puts inside
GTK's Print dialog, and they reach the settings through get_options() when the
dialog is answered rather than when a box is ticked. tests/file/print_dialogs
only looks for the page; this one changes it, prints, and reads what came out.

Printing here is real: GTK's "Print to File" printer, which is the only one a
sandbox has, renders the document through the whole of mootextprint.c --
begin_print(), draw_page() for every page, the header and the footer -- into a
PDF this test counts the pages of. It writes output.pdf into medit's current
directory, which is the sandbox.

The page count is what is asserted, because it is the one number a print option
moves that survives into the file. The document is long lines and the settings
start it ellipsized, one printed line per line of the document; ticking "Wrap
long lines" makes each of them take several, so the same document needs more
paper. The count in between says the tick got out of the dialog and onto the
page.
"""

NAME = "long-lines.txt"

# Long enough that a line takes several across a page when it is wrapped and one
# when it is not, and enough lines to fill a few pages either way.
LINE = "the quick brown fox jumps over the lazy dog, and keeps on jumping " * 4
LINES = 90

# What GTK's file printer calls what it writes, in the directory medit was
# started in.
WRITTEN = "output.pdf"

# What GTK calls the tab medit's options are on, after the application.
PAGE = "medit"

WRAP = "Wrap long lines"

PRINTER = "Print to File"


def setup(s):
    s.open(s.write("workdir/" + NAME, (LINE + "\n") * LINES))

    # Ellipsized to begin with, which is not what medit defaults to: one printed
    # line per line of the document, so that ticking Wrap has somewhere to go.
    s.pref("Editor/print/wrap", False)
    s.pref("Editor/print/ellipsize", True)


def run(t):
    ellipsized = print_it(t, tick=None, before=b"")
    t.log("ellipsized, the document is %d pages" % pages(ellipsized))

    wrapped = print_it(t, tick=WRAP, before=ellipsized)
    t.log("wrapped, it is %d pages" % pages(wrapped))

    t.check(pages(wrapped) > pages(ellipsized),
            "wrapping the lines took more paper than ellipsizing them: %d pages "
            "against %d" % (pages(wrapped), pages(ellipsized)))


def print_it(t, tick, before):
    """Print the document, ticking one of medit's options on the way.

    before is the PDF the last print wrote, so that what this one wrote can be
    told from it: both go to the same name.
    """
    t.menu("File", "Print...")

    dialog = t.need(t.app, role="dialog", name="Print", depth=2,
                    what="the Print dialog")

    choose_printer(t, dialog)

    if tick is not None:
        # The page has to be the one on top before anything on it can be
        # clicked: a widget on a tab that is not showing has no position.
        t.click(t.need(dialog, role="page tab", name=PAGE, depth=30,
                       what="medit's own page of the Print dialog"))

        box = t.need(dialog, role="check box", name=tick, depth=30,
                     what="the %r option on medit's page of the dialog" % tick)

        if not t.state(box, "checked"):
            t.click(box)

        t.wait(lambda: t.state(box, "checked"), "%r to be ticked" % tick)

    t.click(t.button(dialog, "Print"))

    # The second print is asked whether it may overwrite what the first wrote.
    replace(t)

    t.no_toplevel("Print")

    written = t.wait(lambda: pdf(t, before), "the PDF the print wrote")

    t.check(written.startswith(b"%PDF"),
            "what came out of the printer is a PDF: it starts %r" % written[:8])

    return written


def replace(t):
    """Answer the chooser's question about the file that is already there."""
    for _ in range(4):
        alert = t.find(t.app, role="alert", depth=2)

        if alert is not None:
            t.click(t.button(alert, "Replace"))
            return

        t.settle(0.5)


def pdf(t, before):
    """The file, once it is written, complete, and not the one from last time.

    A PDF ends with %%EOF, which is what says the print has finished rather than
    that it has started.
    """
    written = t.sandbox.read_bytes(WRITTEN)

    if written == before or not written.rstrip().endswith(b"%%EOF"):
        return None

    return written


def pages(written):
    """How many pages the PDF has.

    Counted from the page objects rather than from /Count: cairo writes the
    dictionaries uncompressed, so this needs no PDF library on the machine the
    tests run on.
    """
    return written.count(b"/Type /Page\n") + written.count(b"/Type /Page ")


def choose_printer(t, dialog):
    """Pick the printer that writes a file, and wait until it is the one.

    Clicked more than once if need be: the list is filled in as the printers of
    the machine are discovered, and a selection made while that is going on is
    moved back to the default printer when the next one arrives.
    """
    for _ in range(10):
        row = t.need(dialog, role="table cell", name=PRINTER, depth=30,
                     what="the %r printer in the Print dialog" % PRINTER)

        if t.state(row, "selected"):
            return

        t.click(row)
        t.settle(0.5)

    t.fail("the %r printer never took the selection" % PRINTER)
