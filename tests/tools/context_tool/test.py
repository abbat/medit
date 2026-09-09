"""A user tool in the document's context menu.

# requires: MOO_GTK3

The user tools plugin reads two files, not one: menu.xml for the Tools menu and
context.xml for the menu a document has under the right mouse button. Everything
about tools that the tests drive -- tests/tools/user_tool and the rest -- is the
first file, and the second half of moousertools.cpp had never been read.

The tool here is described in context.xml and takes the selection: right-click,
choose it, and the selected word comes back upper case. The context menu is
worth the trip on its own, too -- MooTextView adds Undo and Redo to the top of
what GTK puts there, and that is asserted in the same opening.
"""

TOOL = "Shout"

CONTENT = "hello world\n"

SHOUTED = "HELLO world\n"

CONTEXT_XML = """<moo-user-tools version="1.0">
  <command id="Shout">
    <name>Shout</name>
    <options>need-doc</options>
    <type>exe</type>
    <exe:input>selection</exe:input>
    <exe:output>insert</exe:output>
    <exe:code><![CDATA[
tr a-z A-Z
]]></exe:code>
  </command>
</moo-user-tools>
"""


def setup(s):
    s.write_data("context.xml", CONTEXT_XML)
    s.open(s.write("workdir/notes.txt", CONTENT))


def run(t):
    view = t.document()

    # Select the first word, which is what the tool is fed.
    t.focus()
    t.click(view)
    t.key("ctrl+Home")
    t.key("ctrl+shift+Right")
    # Ctrl+Right stops at the start of the next word, so the blank goes with it.
    t.wait_selection(view, (0, 6), "the first word and the blank after it to be selected")

    menu = t.popup()
    offered = [item.name for item in t.on_screen(t.find_all(menu, depth=1)) if item.name]

    t.check(TOOL in offered,
            "the tool described in context.xml is in the document's menu: %s"
            % ", ".join(offered))
    t.check("Undo" in offered and "Redo" in offered,
            "and MooTextView's own two entries are at the top of it: %s"
            % ", ".join(offered))

    t.choose(menu, TOOL)

    t.wait(lambda: t.text(view) == SHOUTED,
           "the tool to shout the selection; the document holds %r" % t.text(view))

    # A modified document asks about itself when the runner quits medit.
    t.focus()
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")
