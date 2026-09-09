"""Increase and Decrease Indent from the Edit menu, with Tab turned off.

# requires: MOO_GTK3

The menu items call moo_text_view_indent() and its twin directly, where the Tab
key reaches the same code through handle_tab() and only when the "tab indents"
preference is on. So the preference is off here: Tab would insert a tab character
and replace the selection, and the menu still shifts the lines. That is the whole
reason for a menu test beside tests/editor/indent_selection, which drives the key
with the preference at its default.

Unindenting a line that has no indent is asserted as well -- it leaves the text
alone rather than eating the first character of it, which is the mistake the
shift-by-one code is one line away from.

Spaces and a width of 4, so that what the buffer holds is what the test says
rather than a tab character whose width is a preference.
"""

CONTENT = "alpha\nbeta\ngamma\n"

INDENTED = "    alpha\n    beta\ngamma\n"

# alpha 0-4, \n 5; beta 6-9, \n 10; gamma 11-15, \n 16
THIRD_LINE = 11

INCREASE = ("Edit", "Increase Indent")
DECREASE = ("Edit", "Decrease Indent")


def setup(s):
    s.pref("Editor/spaces_instead_of_tabs", True)
    s.pref("Editor/indent_width", 4)
    s.pref("Editor/tab_indents", False)
    s.open(s.write("workdir/indent.txt", CONTENT))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)
    t.key("ctrl+Home")
    t.key("shift+Down")
    t.key("shift+Down")

    t.wait_selection(view, (0, THIRD_LINE),
                     "the first two lines are selected, up to the start of the third")

    t.menu(*INCREASE)
    t.wait(lambda: t.text(view) == INDENTED,
           "the menu to shift the selected lines with the Tab key disabled;\n"
           "the document holds %r" % t.text(view))

    t.menu(*DECREASE)
    t.wait(lambda: t.text(view) == CONTENT,
           "and to shift them back;\nthe document holds %r" % t.text(view))

    t.menu(*DECREASE)
    t.wait(lambda: t.text(view) == CONTENT,
           "a second Decrease Indent to leave lines that have no indent alone;\n"
           "the document holds %r" % t.text(view))

    # A modified document asks about itself when the runner quits medit.
    t.focus()
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")
