"""Find selects the match, Find Next walks them, and both ends wrap.

# requires: MOO_GTK3

moo_text_view_run_find() and the MooFind dialog behind it, then
moo_text_view_run_find_next() for the walk. Nothing in the tests had ever typed
into that dialog.

What is asserted is which occurrence is selected at each step, because that is
the whole of what a search does: the first match, the next one, the one after
that, round to the start when there are no more, and back the other way with
Find Previous. The case-sensitive box is ticked at the end, where it has to turn
three matches into one.
"""

CONTENT = "alpha beta\ngamma alpha\ndelta Alpha\n"

#  alpha 0-4, beta 6-9, \n10 | gamma 11-15, alpha 17-21, \n22 | delta 23-27,
#  Alpha 29-33, \n34

FIRST = (0, 5)
SECOND = (17, 22)
THIRD = (29, 34)

TERM = "alpha"
CAPITAL = "Alpha"


def setup(s):
    s.open(s.write("workdir/words.txt", CONTENT))


def run(t):
    view = t.document()

    search_for(t, TERM)

    t.wait_selection(view, FIRST, "the first match is selected: %s" % (FIRST,))

    step(t, view, "Find Next", SECOND, "the second match")
    step(t, view, "Find Next", THIRD, "the third, which differs only in case")
    step(t, view, "Find Next", FIRST, "round to the first, there being no fourth")
    step(t, view, "Find Previous", THIRD, "and backwards from the first wraps to the last")

    # Case matters when it is asked to: one match instead of three.
    search_for(t, CAPITAL, case_sensitive=True)

    t.wait_selection(view, THIRD,
                     "with case sensitivity on, %r matches only the capital one: %s"
                     % (CAPITAL, THIRD))

    t.menu("Search", "Find Next")
    t.settle(1)
    t.log("after Find Next the selection is %s and the toplevels are %s"
          % (t.selection(view),
             [t.role(n) + " " + repr(n.name) for n in t.find_all(t.app, depth=1)]))
    t.wait_selection(view, THIRD, "Find Next stays on the only match")


def search_for(t, term, case_sensitive=False):
    """Open Find, type the term, set the case box, and accept."""
    t.menu("Search", "Find")

    dialog = t.dialog("Find")
    entry = t.need(dialog, role="text", what="the entry to type the search into")

    t.click(entry)
    t.key("ctrl+a")
    t.type_text(term)

    box = t.need(dialog, role="check box", name="Case sensitive",
                 what="the case sensitive box")

    if t.state(box, "checked") != case_sensitive:
        t.click(box)
        t.wait(lambda: t.state(box, "checked") == case_sensitive,
               "the case sensitive box to be %s"
               % ("ticked" if case_sensitive else "clear"))

    t.key("Return")
    t.no_toplevel("Find")


def step(t, view, entry, expected, what):
    t.menu("Search", entry)
    t.wait_selection(view, expected, "%s is selected: %s" % (what, expected))
