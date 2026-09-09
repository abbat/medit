"""The Tools page of the preferences: the list of tools, and switching one off.

# requires: MOO_GTK3

src/plugins/usertools/moousertools-prefs.cpp is where the tools of the Tools menu
are edited, and nothing had opened it. Selecting a tool fills the page in from
its description -- name, files, what it requires, what it saves, its type and its
code -- and OK writes the lot back and rebuilds the menu.

Switching a tool off is the shortest way through all of that which can be
asserted from outside: the tool is in the menu, its Enabled box is ticked, and
after unticking it and pressing OK the menu does not offer it any more. A page
that read the tool but wrote nothing back, or wrote it back without rebuilding
the menu, fails the last step.
"""

TOOL = "Shout"

TOOLS_XML = """<moo-user-tools version="1.0">
  <command id="Shout">
    <name>Shout</name>
    <options>need-doc</options>
    <type>exe</type>
    <exe:input>lines</exe:input>
    <exe:output>insert</exe:output>
    <exe:code><![CDATA[
tr a-z A-Z
]]></exe:code>
  </command>
</moo-user-tools>
"""

ENABLED = "Enabled"


def setup(s):
    s.write_data("menu.xml", TOOLS_XML)
    s.open(s.write("workdir/notes.txt", "hello\n"))


def run(t):
    t.check(TOOL in tools_menu(t), "the tool is in the Tools menu to start with")

    dialog = t.preferences("Tools")

    row = t.need(dialog, role="table cell", name=TOOL,
                 what="the %s row of the tools list" % TOOL)
    t.click(row)

    box = t.need(dialog, role="check box", name=ENABLED, what="the Enabled box")
    t.wait(lambda: t.state(box, "checked"),
           "the page to fill in from the tool, which is enabled")

    t.click(box)
    t.wait(lambda: not t.state(box, "checked"), "the box to be unticked")

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")

    listed = tools_menu(t)
    t.check(TOOL not in listed,
            "and the menu no longer offers the tool: %s" % ", ".join(listed))


def tools_menu(t):
    """What the Tools menu lists, read in one opening.

    A click on a menu that is already open closes it, so anything that looks
    twice has to look inside one opening.
    """
    menu = t.menu("Tools")

    names = [item.name for item in t.on_screen(t.find_all(menu, depth=1)) if item.name]

    t.escape()

    return names
