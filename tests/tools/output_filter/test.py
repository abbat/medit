"""A user tool's output filter turns a line of output into a place in a file.

# requires: MOO_GTK3

src/plugins/usertools/moooutputfilterregex.cpp reads filters.xml out of medit's
data directory, and each filter is a set of regular expressions with named groups
-- file, line -- that are matched against what a command prints. A line that
matches becomes a result: it is styled in the Output pane and it can be clicked,
and the click opens the file at the line the expression found. Nothing had ever
loaded that file, so the whole of it was dead as far as the tests were concerned.

The filter here is the test's own, in the sandbox's data directory, for the same
reason the tools are: nothing is installed, so nothing is there. It matches
"<path>:<line>:" and the tool prints exactly that, naming the document it was run
on and its second line -- so the cursor landing on the second line is the filter
having parsed both halves, and the pane's own text is what says the command ran
at all.
"""

NAME = "notes.txt"

LINES = ("alpha", "beta", "gamma")

CONTENT = "".join(line + "\n" for line in LINES)

WIDTH = len("alpha\n")

TOOL = "Complain"

FILTERS_XML = """<?xml version="1.0" encoding="UTF-8"?>
<medit-filters version="1.0">
  <filter id="test" _name="Filter|Test">
    <match what="stdout" pattern="^(?P&lt;file&gt;[^:]+):(?P&lt;line&gt;\\d+):" style="output-error"/>
  </filter>
</medit-filters>
"""

TOOLS_XML = """<moo-user-tools version="1.0">
  <command id="Complain">
    <name>Complain</name>
    <options>need-file</options>
    <type>exe</type>
    <exe:input>none</exe:input>
    <exe:output>pane</exe:output>
    <exe:filter>test</exe:filter>
    <exe:code><![CDATA[
echo "$DOC_PATH:2: something is wrong here"
]]></exe:code>
  </command>
</moo-user-tools>
"""

COMPLAINT = "2: something is wrong here"


def setup(s):
    s.write_data("filters.xml", FILTERS_XML)
    s.write_data("menu.xml", TOOLS_XML)
    s.open(s.write("workdir/" + NAME, CONTENT))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)
    t.key("ctrl+Home")
    t.wait_caret(view, 0, "the cursor starts at the top of the document")

    t.menu("Tools", TOOL)

    pane = t.wait(lambda: output_pane(t), "the Output pane to open")
    t.wait_text(pane, COMPLAINT, what="the command's complaint to be in the pane")

    text = t.text(pane)
    printed = "%s:2:" % t.sandbox.path("workdir", NAME)

    t.check(printed in text,
            "the pane holds the line the command printed: %r" % text)

    # The click is what the filter is for: the line is a result, and activating
    # it goes to the file and the line the expression pulled out of it.
    #
    # The expanded path is what is looked for rather than the message: the pane
    # opens with the command line itself, which holds the message too and would
    # be the line found first.
    where = text.index(printed)
    x, y, _, _ = t.range_extents(pane, where, where + 1)
    t.click_at(x + 2, y + 2)

    t.wait_caret(view, WIDTH,
                 "the cursor to be on the second line, which the filter read out "
                 "of the text")


def output_pane(t):
    """The pane's text view: the one on screen that cannot be typed into."""
    views = [view for view in t.on_screen(t.find_all(t.frame, role="text", depth=30))
             if not t.state(view, "editable")]

    return views[0] if len(views) == 1 else None
