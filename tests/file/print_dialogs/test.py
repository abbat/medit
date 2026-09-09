"""The print dialogs open, and the Print one carries medit's own options.

Page Setup and Print are GTK's dialogs and medit only has to put them up --
_moo_edit_page_setup() and _moo_edit_print() -- which is exactly why nothing here
asserts anything about their contents beyond one thing. A dialog that does not
appear at all is the failure this catches: it needs no print backend, no printer
and no paper, and a MooEditWindow that cannot open one is broken for every user
who ever prints.

The one thing about the contents is medit's: moo_print_operation_create_custom_widget()
puts a page of medit's own options into GTK's Print dialog, built from
mooprint.ui, and it is where the header, footer, wrapping and font settings live.
There is no Print Options item in the File menu to reach them by -- see the
comment at the foot of this file -- so the Print dialog is the only way in and
worth asserting.
"""

BODY = "something to print\n"

# One of the check buttons on medit's page of the Print dialog, from mooprint.ui.
AN_OPTION = "Print using text styles"


def setup(s):
    s.open(s.write("workdir/notes.txt", BODY))


def run(t):
    items = [item.name for item in t.find_all(the_file_menu(t), depth=1) if item.name]
    t.log("the File menu offers %s" % ", ".join(items))

    for wanted in ("Page Setup...", "Print...", "Export as PDF..."):
        t.check(wanted in items, "the File menu offers %r" % wanted)

    t.escape()

    page_setup(t)
    print_dialog(t)


def the_file_menu(t):
    menu = t.need(t.frame, role="menu", name="File", what="the File menu")
    t.click(menu)

    return menu


def page_setup(t):
    t.menu("File", "Page Setup...")

    dialog = t.need(t.app, role="dialog", name="Page Setup", depth=2,
                    what="the Page Setup dialog")

    t.check(t.state(dialog, "showing"), "the Page Setup dialog is on screen")

    t.escape()
    t.no_toplevel("Page Setup")
    t.log("ok: Page Setup opens and closes")


def print_dialog(t):
    t.menu("File", "Print...")

    dialog = t.need(t.app, role="dialog", name="Print", depth=2,
                    what="the Print dialog")

    t.check(t.state(dialog, "showing"), "the Print dialog is on screen")

    # medit's own page of it, wherever GTK put the tab.
    t.need(dialog, role="check box", name=AN_OPTION,
           what="medit's own print options inside GTK's Print dialog")
    t.log("ok: the Print dialog carries medit's options page")

    t.escape()
    t.no_toplevel("Print")
    t.log("ok: Print opens and closes")


# medit.xml has <item action="PrintOptions"/> in the File menu and no such action
# exists: _moo_edit_print_options_dialog() in mootextprint.c is inside #if 0, and
# nothing registers the action, so the line names nothing and the menu shows no
# item for it. The options themselves are not lost -- they are the page inside the
# Print dialog that this test looks for.
