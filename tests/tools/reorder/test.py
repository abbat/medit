"""Tools moved and deleted in the preferences, and the menu that follows.

# requires: MOO_GTK3

The buttons beside the list on the Tools page are not the page's own: they are
MooTreeHelper, src/mooutils/mooutils-treeview.cpp, which every list of things in
medit's preferences uses -- the tools, the Open With programs, the file filters.
New is driven by tests/tools/new_tool; this drives the other three, which are the
ones that rearrange a list rather than add to it.

What is asserted is the Tools menu, in order. The order of the list is what the
menu is built from, so a Down that moved the wrong row, or a Delete that removed
one and wrote the others back unchanged, shows up there.
"""

FIRST = "Aaa"
SECOND = "Bbb"

TOOLS_XML = """<moo-user-tools version="1.0">
  <command id="Aaa">
    <name>Aaa</name>
    <type>exe</type>
    <exe:code><![CDATA[
true
]]></exe:code>
  </command>
  <command id="Bbb">
    <name>Bbb</name>
    <type>exe</type>
    <exe:code><![CDATA[
true
]]></exe:code>
  </command>
</moo-user-tools>
"""


def setup(s):
    s.write_data("menu.xml", TOOLS_XML)
    s.open(s.write("workdir/notes.txt", "hello\n"))


def run(t):
    t.check(tools(t) == [FIRST, SECOND],
            "the menu offers both tools, in the order the file names them: %s"
            % tools(t))

    dialog = t.preferences("Tools")

    # Down, on the first of them.
    t.click(row(t, dialog, FIRST))
    t.click(page_buttons(t, dialog)[2])

    t.wait(lambda: rows(t, dialog) == [SECOND, FIRST],
           "Down to move the row past the other; the list holds %s" % rows(t, dialog))

    # Up, which puts it back.
    t.click(page_buttons(t, dialog)[3])

    t.wait(lambda: rows(t, dialog) == [FIRST, SECOND],
           "Up to bring it back; the list holds %s" % rows(t, dialog))

    # And Delete, on the other one.
    t.click(row(t, dialog, SECOND))
    t.click(page_buttons(t, dialog)[1])

    t.wait(lambda: rows(t, dialog) == [FIRST],
           "Delete to take the row away; the list holds %s" % rows(t, dialog))

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    t.check(tools(t) == [FIRST],
            "and the menu is what the list was left as: %s" % tools(t))


def page_buttons(t, dialog):
    """The four icon buttons under the list, left to right: new, delete, down, up."""
    found = [b for b in t.on_screen(t.find_all(dialog, role="push button", depth=30))
             if not b.name]

    return sorted(found, key=lambda node: t.extents(node)[0])


def row(t, dialog, name):
    return t.need(dialog, role="table cell", name=name, depth=30,
                  what="the %r row" % name)


def rows(t, dialog):
    """The tools the list holds, top to bottom.

    The page list of the dialog is cells too, so what is kept is the cells of the
    tools -- the two the test made are the only ones there are.
    """
    found = [(t.extents(cell)[1], cell.name)
             for cell in t.on_screen(t.find_all(dialog, role="table cell", depth=30))
             if cell.name in (FIRST, SECOND)]

    return [name for y, name in sorted(found)]


def tools(t):
    """The tools the Tools menu offers, in order, read in one opening."""
    menu = t.menu("Tools")

    names = [item.name for item in t.on_screen(t.find_all(menu, depth=1))
             if item.name in (FIRST, SECOND)]

    t.escape()

    return names
