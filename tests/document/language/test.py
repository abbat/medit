"""The Language menu says what a document is, and changing it changes the editor.

# requires: MOO_GTK3

create_lang_action() builds the menu out of whatever language definitions are
installed and update_lang_menu() keeps its radio on the language of the document
on screen. The choice is not decoration: MooEdit reads the comment syntax out of
the language, so Edit -> Comment is insensitive with no language and comments with
the right marker once there is one -- which is what this test uses to see the
change from outside.

The language is the test's own. A sandbox has no installed medit to borrow
definitions from (tests/lib/sandbox.py says why), so the language is written into
medit's user data directory before it starts: one glob, one comment marker, one
keyword. That also keeps the menu small enough to assert whole. The schema every
definition is validated against has to be put there with it -- it is data too,
and the sandbox took it away along with the rest.
"""

NAME = "poem.haiku"

CONTENT = "frog\npond\nsplash\n"

SPEC = """<?xml version="1.0" encoding="UTF-8"?>
<language id="haiku" name="Haiku" version="2.0" _section="Others">
  <metadata>
    <property name="globs">*.haiku</property>
    <property name="line-comment-start">#</property>
  </metadata>

  <styles>
    <style id="word" _name="Word"/>
  </styles>

  <definitions>
    <context id="haiku">
      <include>
        <context id="word" style-ref="word">
          <keyword>frog</keyword>
        </context>
      </include>
    </context>
  </definitions>
</language>
"""

LANGUAGE = ("Document", "Language")

NONE = "None"
HAIKU = "Haiku"
SECTION = "Others"

COMMENT = ("Edit", "Comment")
UNCOMMENT = ("Edit", "Uncomment")


def setup(s):
    s.copy_data("language-specs/language2.rng", "src/mooedit/langs/language2.rng")
    s.write_data("language-specs/haiku.lang", SPEC)
    s.open(s.write("workdir/" + NAME, CONTENT))


def run(t):
    view = t.document()

    # Read in one opening and read once: t.menu() clicks its way in, and a click
    # on a menu that is already open closes it again.
    state = ticked(t, SECTION)
    t.check(state == [HAIKU],
            "the file's glob picked the language, and the menu says so: %s" % state)

    t.check(sensitive(t, *COMMENT),
            "Comment is available, the language having a comment syntax")

    t.focus()
    t.key("ctrl+a")
    t.menu(*COMMENT)
    t.wait_text(view, "# frog", what="the selection to be commented")

    t.check(t.text(view) == "# frog\n# pond\n# splash\n",
            "with the marker the language named, and a space after it: %r" % t.text(view))

    t.menu(*UNCOMMENT)
    t.wait(lambda: t.text(view) == CONTENT, "the comments to be taken off again")

    # Saved because the document is modified either way -- the text is back to
    # what it was, the buffer is not -- and a modified document asks about itself
    # when the runner quits medit at the end.
    t.focus()
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")

    # No language: the same document, and nothing to comment it with.
    t.menu(*LANGUAGE, NONE)

    state = ticked(t)
    t.check(state == [NONE], "the menu follows the choice to no language: %s" % state)

    state = ticked(t, SECTION)
    t.check(state == [], "and the language is no longer ticked in its section: %s" % state)

    t.check(not sensitive(t, *COMMENT),
            "and Comment goes insensitive with no comment syntax to use")

    t.check(t.text(view) == CONTENT,
            "the text itself is untouched by any of it: %r" % t.text(view))

    # And back, through the section the language is filed under.
    t.menu(*LANGUAGE, SECTION, HAIKU)

    state = ticked(t, SECTION)
    t.check(state == [HAIKU], "the language is back on the radio: %s" % state)

    t.check(sensitive(t, *COMMENT), "and Comment with it")


def ticked(t, *section):
    """What the menu has its radio on, in the Language menu or in one section.

    "None" is an item of the Language menu itself and every language is an item
    of the section it is filed under, so which of the two is asked matters: an
    item inside a submenu that was never opened is in the tree with no position
    and reads as nothing.
    """
    menu = t.menu(*LANGUAGE, *section)

    state = [item.name for item in t.on_screen(t.find_all(menu, depth=1))
             if item.name and t.state(item, "checked")]

    # One per level opened: a submenu takes the first Escape and the menu bar the
    # next, and an Escape with nothing open is nothing.
    t.escape()
    t.escape()

    return state


def sensitive(t, *path):
    """Whether the item at that menu path can be activated."""
    item = t.menu(*path[:-1])
    item = t.need(item, role="menu item", name=path[-1],
                  what="the %r item" % path[-1])
    answer = t.state(item, "sensitive")
    t.escape()

    return answer
