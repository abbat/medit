"""A tool whose output goes to the pane, and the Stop item that kills it.

# requires: MOO_GTK3

The other half of the user tools: output "pane" sends what the command prints to
the Output pane rather than into the document, and MooCmdView there writes a line
of its own when the command ends -- "*** Done ***" for a clean exit and
"*** Aborted ***" for one that was killed. Both are asserted, which is what makes
this a test of the pane rather than of the shell.

The Stop item in the Tools menu is the killer. It is created with
"condition::visible" on there being something that can be stopped and
"condition::sensitive" on a job actually running, so it has three states and the
test walks all three: absent before anything has run, there but dead once a
command has finished, and live while one is in flight.

GTK+3 only because it is a pane -- see tests/lsp/diagnostics_pane for why.
"""

QUICK = "Quick"
SLEEPER = "Sleeper"
STOP = "Stop"

TOOLS_XML = """<moo-user-tools version="1.0">
  <command id="Quick">
    <name>Quick</name>
    <options>need-doc</options>
    <type>exe</type>
    <exe:output>pane</exe:output>
    <exe:code><![CDATA[
echo the first line
echo the second line
]]></exe:code>
  </command>
  <command id="Sleeper">
    <name>Sleeper</name>
    <options>need-doc</options>
    <type>exe</type>
    <exe:output>pane</exe:output>
    <exe:code><![CDATA[
echo waiting now
sleep 20
]]></exe:code>
  </command>
</moo-user-tools>
"""


def setup(s):
    s.write_data("menu.xml", TOOLS_XML)
    s.open(s.write("workdir/notes.txt", "nothing to see\n"))


def run(t):
    t.check(STOP not in tools_menu(t),
            "Stop is not offered before anything has been run")
    t.escape()

    # A command that ends by itself: its output, then the pane's own last word.
    t.menu("Tools", QUICK)

    pane = t.wait(lambda: output_pane(t), "the output pane")
    t.wait_text(pane, "the first line", what="what the command printed")
    t.wait_text(pane, "the second line", what="the rest of what it printed")
    t.wait_text(pane, "*** Done ***", what="the pane's own line for a clean exit")

    t.check(stop_state(t, the_menu(t)) is False,
            "Stop is offered now that there is a pane, and is dead with nothing "
            "running")
    t.escape()

    # A command that does not: Stop comes alive, and killing it says so.
    t.menu("Tools", SLEEPER)

    t.wait_text(output_pane(t), "waiting now", what="the sleeper to start printing")

    # Read out of one opening of the menu rather than reopening it to look: the
    # click that opens a menu closes it again if it is already open, so a poll
    # that opens the menu for itself is on half the time.
    menu = the_menu(t)
    t.wait(lambda: stop_state(t, menu) is True,
           "Stop to come alive while the command is running; it is %s"
           % stop_state(t, menu))
    t.log("ok: Stop is live while a job is in flight")
    t.escape()

    t.menu("Tools", STOP)

    t.wait_text(output_pane(t), "*** Aborted ***",
                what="the pane's own line for a command that was killed")

    menu = the_menu(t)
    t.wait(lambda: stop_state(t, menu) is False,
           "Stop to go dead again once the job is gone; it is %s"
           % stop_state(t, menu))
    t.log("ok: Stop went dead with the job")
    t.escape()


def the_menu(t):
    menu = t.need(t.frame, role="menu", name="Tools", what="the Tools menu")
    t.click(menu)

    return menu


def tools_menu(t):
    """The entries of the Tools menu that are drawn."""
    return [item.name for item in t.on_screen(t.find_all(the_menu(t), depth=2))
            if item.name]


def stop_state(t, menu):
    """Whether the Stop item of an open menu is live, or None if not offered.

    Takes the menu rather than opening one: a click on a menu that is already
    open closes it, so anything that looks more than once has to look inside one
    opening.
    """
    for item in t.on_screen(t.find_all(menu, depth=2)):
        if item.name == STOP:
            return t.state(item, "sensitive")

    return None


def output_pane(t):
    """The pane's text view: the one on screen that cannot be typed into."""
    views = [view for view in t.on_screen(t.find_all(t.frame, role="text", depth=30))
             if not t.state(view, "editable")]

    return views[0] if len(views) == 1 else None
