"""Find selects the match, Find Next walks them, and both ends wrap.

# requires: MOO_GTK3

moo_text_view_run_find() and the MooFind dialog behind it, then
moo_text_view_run_find_next() for the walk. Nothing in the tests had ever typed
into that dialog.

What is asserted is which occurrence is selected at each step, because that is
the whole of what a search does: the first match, the next one, the one after
that, round to the start when there are no more, and back the other way with
Find Previous. The case-sensitive box is ticked at the end, where it has to turn
three matches into one -- and then Find Next on that one match says the pattern
was not found, which is medit's answer and not a shortcoming: the wrap searches
up to the start of the current match and so excludes it.
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

    # One match and no other. "Next" means another one, and the wrap search runs
    # from the start of the buffer to the start of the current match -- which
    # excludes it -- so medit says the pattern was not found and puts the cursor
    # where the match ended instead of re-selecting it. That is the behaviour,
    # not a shortcoming: the test pins it so a change of mind about it is a
    # change somebody has to make deliberately.
    t.menu("Search", "Find Next")

    t.wait(lambda: t.selection(view) is None,
           "the selection to be dropped when there is no next match; it is %s"
           % (t.selection(view),))
    t.log("ok: Find Next on the only match reports nothing further and drops the "
          "selection")

    t.check(t.caret(view) == THIRD[1],
            "leaving the cursor where the match ended: %d" % t.caret(view))

    t.check("not found" in said(t),
            "and saying so where medit puts its messages: %r" % said(t))


def said(t):
    """What the status bar is showing.

    moo_window_message() pushes onto a GtkStatusbar, whose text is not a label
    of its own in the tree -- it is the name of the status bar node.
    """
    bar = t.find(t.frame, role="status bar", depth=25)

    if bar is None:
        return t.fail("no status bar in the window:\n%s" % t.dump(t.frame))

    return "%s %s" % (bar.name or "", t.text(bar))


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
