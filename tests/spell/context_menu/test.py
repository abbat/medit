"""A misspelled word offers its corrections, and the dictionary learns.

# requires: MOO_BUILD_SPELL, MOO_GTK3

The plugin is off until it is switched on, and checks only the extensions it is
told about, so "notes.txt" is checked and "notes.log" is not. The dictionary is
a stub (MOO_SPELL_STUB, set by the runner) that knows "hello", "help", "world"
and a few more, and suggests the words that start with the same letter.

Underlines are not in the accessibility tree, so what is asserted is the menu:
the entries only exist for a word that is wrong, and Add to Dictionary makes it
right, which the menu of the same word then shows.

GTK+3 only, as the click has to land on a particular word.
"""

CONTENT = "hello wrold\n"

# "wrold" starts at offset 6; clicked a quarter into its first letter, as the
# middle of a glyph is the boundary with the next one.
WORD = 6


def setup(s):
    s.plugin("Spell")
    s.open(s.write("workdir/notes.log", CONTENT))
    s.open(s.write("workdir/notes.txt", CONTENT))


def run(t):
    # The document opened last is on screen: the checked one. The word is found
    # after a short delay, so the menu is asked until it has the entry.
    def offered():
        names = menu_of(t)
        t.escape()
        return names if "Add to Dictionary" in names else None

    names = t.wait(offered, "the misspelled word to be offered for adding")

    t.check("world" in names,
            "the stub's word is offered as a correction: %s" % ", ".join(names))
    t.check("Ignore All" in names and "Ignore in This Document" in names,
            "with both ways of ignoring it: %s" % ", ".join(names))

    menu = t.popup_at(document(t), WORD, WORD + 1, at=0.25)
    t.click(t.item(menu, "Add to Dictionary"))

    names = menu_of(t)
    t.escape()
    t.check("Add to Dictionary" not in names,
            "an added word is right, and offers nothing: %s" % ", ".join(names))

    t.menu("Window", "notes.log")

    names = menu_of(t)
    t.escape()
    t.check("Add to Dictionary" not in names,
            "a file with an extension nobody asked for is not checked: %s" % ", ".join(names))


def menu_of(t):
    return entries(t, t.popup_at(document(t), WORD, WORD + 1, at=0.25))


def document(t):
    """The text view of the document that is on screen."""
    views = [view for view in t.on_screen(t.find_all(t.frame, role="text", depth=30))
             if t.state(view, "editable")]

    return views[0]


def entries(t, menu):
    return [item.name for item in t.on_screen(t.find_all(menu, role="menu item", depth=3))]
