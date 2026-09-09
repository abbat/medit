"""A tool the user defined is in the Tools menu and runs on the document.

# requires: MOO_GTK3

The Tools menu is mostly a place other things put items into: the terminal and
LSP plugins add theirs from code, and the UserTools plugin reads descriptions out
of menu.xml in medit's data directory. This drives the second of those end to end,
because nothing did -- the plugin, the exe command factory under it, and the
input and output plumbing that decides what the command is fed and what is done
with what it prints.

The tool here takes the lines it is given and shouts them, with `tr a-z A-Z`, and
is declared with input "lines" and output "insert". So one line of a two-line
document should come back upper case and the other should be untouched, which is
the whole assertion: it says the command ran, that it was fed the line the cursor
was on rather than the file, and that its output replaced that line rather than
being appended or shown in a pane.

menu.xml is installed rather than bundled, so a build tree has no tools at all
until a test writes one -- which is why this writes its own into the sandbox
instead of driving one of the tools medit ships.
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

CONTENT = "hello\nworld\n"

SHOUTED = "HELLO\nworld\n"


def setup(s):
    s.write_data("menu.xml", TOOLS_XML)
    s.open(s.write("workdir/notes.txt", CONTENT))


def run(t):
    listed = tools_menu(t)
    t.log("the Tools menu offers %s" % ", ".join(listed))

    t.check(TOOL in listed, "the tool the user described is in the menu")

    # Only ever there while something is running, and nothing is.
    t.check("Stop" not in listed, "and the Stop item is not, no job being in flight")

    t.escape()

    view = t.document()
    t.focus()
    t.click(view)
    t.key("ctrl+Home")
    t.wait_caret(view, 0, "the cursor is on the first line")

    t.menu("Tools", TOOL)

    t.wait(lambda: t.text(t.document()) == SHOUTED,
           "the tool to shout the line the cursor was on and leave the other "
           "alone; the document holds %r" % t.text(t.document()))
    t.log("ok: the tool ran, was fed one line, and its output replaced that line")

    # Saved, or the quit at the end of the test turns into a dialog about it.
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")

    t.check(t.sandbox.read("workdir", "notes.txt") == SHOUTED,
            "and what it wrote is what got saved")


def tools_menu(t):
    """The entries of the Tools menu that are drawn."""
    menu = t.need(t.frame, role="menu", name="Tools", what="the Tools menu")
    t.click(menu)

    return [item.name for item in t.on_screen(t.find_all(menu, depth=2))
            if item.name]
