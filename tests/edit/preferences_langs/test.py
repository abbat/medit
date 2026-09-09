"""The Languages page: which files a language is used for.

# requires: MOO_GTK3

The page is per-language settings -- the globs a language claims, the mime types
it claims, and the document options to use for it -- kept by moolangmgr.cpp
beside the language definitions and written to the preferences rather than into
them. tests/edit/preferences_pages only asks the page to come up; this changes
something on it and opens a file to see it hold.

The language is the test's own, as in tests/document/language: a sandbox has no
installed medit to borrow definitions from. It claims "*.haiku", the page adds
"*.hk" to that, and the file opened afterwards is a .hk -- which nothing but the
page could have made it a Haiku.
"""

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

from lib import input as ui

LANGUAGE = ("Document", "Language")

HAIKU = "Haiku"
NONE = "None"
SECTION = "Others"

OPENED = "second.hk"


def setup(s):
    s.copy_data("language-specs/language2.rng", "src/mooedit/langs/language2.rng")
    s.write_data("language-specs/haiku.lang", SPEC)
    s.write("workdir/" + OPENED, "frog\n")
    s.open(s.write("workdir/first.hk", "frog\n"))


def run(t):
    t.check(ticked(t) == [NONE],
            "a .hk file is nothing to start with: the language claims .haiku")

    claim_the_extension(t)

    # A file opened after the change: the guess is made as a file is opened, so
    # the one that was already open is not the question here.
    open_by_path(t, t.sandbox.path("workdir", OPENED))

    state = ticked(t, SECTION)
    t.check(state == [HAIKU],
            "and the file opened afterwards is a Haiku: %s" % state)


def claim_the_extension(t):
    dialog = t.preferences("Languages")

    # The list is grouped by section, the way the Document menu's is, so the
    # language is a step inside it rather than an entry of the list itself.
    choose(t, combo(t, dialog), SECTION, HAIKU)

    entry = field(t, dialog, "Extensions:")
    t.focus()
    t.click(entry)
    t.key("ctrl+a")
    t.type_text("*.haiku;*.hk")

    t.wait(lambda: t.text(entry) == "*.haiku;*.hk",
           "the extensions to be typed in; the entry holds %r" % t.text(entry))

    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")


def combo(t, dialog):
    """The language combo, which is the only one on the page.

    Waited for: the page is built when it is first shown, and a widget looked up
    while that is happening has no position yet.
    """
    return t.wait(lambda: the_combo(t, dialog), "the language combo of the page")


def the_combo(t, dialog):
    found = t.on_screen(t.find_all(dialog, role="combo box", depth=30))

    return found[0] if found else None


def choose(t, node, *path):
    """Drop a combo box and walk to an entry of its list.

    Entries are looked for by where they are drawn rather than in the first menu
    there is: menus that are not up are in the tree too, with no position, and
    the list of a combo box that has just been clicked takes a moment to become
    one of the ones that are.
    """
    t.click(node)

    for step in path[:-1]:
        item = t.wait(lambda name=step: dropped_item(t, name),
                      "the %r section of the combo box's list" % step)
        t.click(item)
        ui.key("Right")

    item = t.wait(lambda: dropped_item(t, path[-1]),
                  "the %r entry of the combo box's list" % path[-1])
    t.click(item)

    # The combo names itself after the language's id rather than its label, so
    # what it says is compared without case.
    t.wait(lambda: (node.name or "").lower() == path[-1].lower(),
           "the combo box to say %r; it says %r" % (path[-1], node.name))


def dropped_item(t, entry):
    """An item of an open list, wherever in it: a section is a submenu."""
    for menu in t.find_all(t.app, role="menu", depth=2):
        for item in t.find_all(menu, depth=3):
            if item.name == entry and ui.on_screen(item):
                return item

    return None


def field(t, dialog, label):
    """The entry beside a label, which is the one on the same row."""
    for node in t.on_screen(t.find_all(dialog, role="label", depth=30)):
        if node.name != label:
            continue

        y = t.extents(node)[1]

        for entry in t.on_screen(t.find_all(dialog, role="text", depth=30)):
            if abs(t.extents(entry)[1] - y) <= 12:
                return entry

    return t.fail("no entry beside %r on the page" % label)


def ticked(t, *section):
    """What the Language menu has its radio on, in the menu or in one section."""
    menu = t.menu(*LANGUAGE, *section)

    state = [item.name for item in t.on_screen(t.find_all(menu, depth=1))
             if item.name and t.state(item, "checked")]

    t.escape()
    t.escape()

    return state


def open_by_path(t, path):
    """Open the File/Open chooser and give it a path through its location entry."""
    t.menu("File", "Open...")
    t.need(t.app, role="file chooser", depth=2, what="the Open dialog")

    t.focus()
    t.key("ctrl+l")
    t.type_text(path)
    t.key("Return")

    t.wait(lambda: t.find(t.app, role="file chooser", depth=2) is None,
           "the Open dialog to close")
