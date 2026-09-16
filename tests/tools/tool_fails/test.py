"""A tool whose command is not installed says so, and leaves the document alone.

# requires: MOO_GTK3

Both failure paths of moocommand-exe.cpp used to end in the same g_message():
a tool whose program is not there, or whose command line does not parse, was a
menu item that did nothing and explained nothing, because a medit started from
a desktop file has no terminal for a g_message() to land in. run_command()
passed NULL for the exit status and for the standard error as well, so the
other half -- a tool that starts and then fails -- was lost twice over.

This drives that half: a tool declared with output "insert" whose command does
not exist. The shell it runs under exits 127 and says so on its standard error,
which is what the dialog has to show. And the document must still hold what it
held: the empty output of a command that failed is not something to paste over
what the user wrote.

menu.xml is installed rather than bundled, so a build tree has no tools until a
test writes one -- the same reason tests/tools/user_tool writes its own.
"""

TOOL = "Missing"

MISSING = "no-such-command-here"

TOOLS_XML = """<moo-user-tools version="1.0">
  <command id="Missing">
    <name>Missing</name>
    <options>need-doc</options>
    <type>exe</type>
    <exe:input>none</exe:input>
    <exe:output>insert</exe:output>
    <exe:code><![CDATA[
%s
]]></exe:code>
  </command>
</moo-user-tools>
""" % MISSING

CONTENT = "hello\nworld\n"


def setup(s):
    s.write_data("menu.xml", TOOLS_XML)
    s.open(s.write("workdir/notes.txt", CONTENT))


def run(t):
    view = t.document()
    t.focus()
    t.click(view)

    t.menu("Tools", TOOL)

    dialog = t.wait(lambda: the_alert(t), "the dialog about the tool that failed")

    said = " ".join(label.name or "" for label in t.find_all(dialog, role="label"))
    t.log("the dialog says %r" % said)

    t.check("failed" in said.lower(),
            "the dialog says the command failed")
    t.check(MISSING in said,
            "and shows what the command said for itself: %r" % said)

    t.click(t.button(dialog, "Close"))
    t.wait(lambda: the_alert(t) is None, "the dialog to go")

    t.check(t.text(t.document()) == CONTENT,
            "the document is what it was: %r" % t.text(t.document()))


def the_alert(t):
    """The dialog, which is a message dialog and so an alert."""
    found = t.find_all(t.app, role="alert", depth=2)

    return found[0] if found else None
