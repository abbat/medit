"""Comment and Uncomment with a language that has only block comments.

# requires: MOO_GTK3

moo_edit_comment_selection() has two branches -- line_comment() where the
language names a line comment, block_comment() where it names only a block one --
and which it takes is decided by _moo_edit_has_comments() rather than by the
menu. The line branch is driven by tests/document/language; this is the other
one, and the two together are the whole of the Edit menu's Comment items.

The language is the test's own, as it has to be: a sandbox has no installed medit
to borrow definitions from (tests/lib/sandbox.py says why), and no language in the
tree has block comments and no line comment except by accident. One glob, one pair
of block markers, and no line comment at all.
"""

NAME = "sheet.blk"

CONTENT = "alpha\nbeta\n"

# The end marker goes before the last line break rather than after it: the
# selection Ctrl+A makes runs to the end of the buffer, and block_comment() backs
# up over the trailing newline so that the comment does not end on an empty line.
COMMENTED = "/*alpha\nbeta*/\n"

SPEC = """<?xml version="1.0" encoding="UTF-8"?>
<language id="blocky" name="Blocky" version="2.0" _section="Others">
  <metadata>
    <property name="globs">*.blk</property>
    <property name="block-comment-start">/*</property>
    <property name="block-comment-end">*/</property>
  </metadata>

  <styles>
    <style id="word" _name="Word"/>
  </styles>

  <definitions>
    <context id="blocky">
      <include>
        <context id="word" style-ref="word">
          <keyword>alpha</keyword>
        </context>
      </include>
    </context>
  </definitions>
</language>
"""

COMMENT = ("Edit", "Comment")
UNCOMMENT = ("Edit", "Uncomment")


def setup(s):
    s.copy_data("language-specs/language2.rng", "src/mooedit/langs/language2.rng")
    s.write_data("language-specs/blocky.lang", SPEC)
    s.open(s.write("workdir/" + NAME, CONTENT))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)
    t.key("ctrl+a")

    t.menu(*COMMENT)
    t.wait(lambda: t.text(view) == COMMENTED,
           "the selection to be wrapped in the language's block markers;\n"
           "the document holds %r" % t.text(view))

    t.focus()
    t.key("ctrl+a")

    t.menu(*UNCOMMENT)
    t.wait(lambda: t.text(view) == CONTENT,
           "and unwrapped again;\nthe document holds %r" % t.text(view))

    # A modified document asks about itself when the runner quits medit.
    t.focus()
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")
