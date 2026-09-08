"""Save As writes the new file, renames the document, and leaves the old file alone.

# requires: MOO_GTK3

_moo_edit_save_as_dialog() in mooeditdialogs.cpp. The dialog is a
GtkFileChooserDialog, but what medit does with it is its own: it seeds the name
entry with the document's basename, adds the character-encoding combo the
chooser has no notion of, and then has to write the document to the file that
came back and give the document its new identity -- while the file it used to be
is left exactly as it was.

That last part is the one worth a test. A Save As that also wrote the old file,
or that wrote the new one and left the document pointing at the old, would look
right on screen; what says otherwise is the bytes of both files afterwards.
"""

OLD = "one.txt"
NEW = "renamed.txt"

BODY = "one\n"
TYPED = "changed "


def setup(s):
    # So the chooser opens in the document's own directory and the test can type
    # a bare name into it. Left off, the chooser starts wherever it last was --
    # nowhere, in a fresh sandbox -- and where a bare name would land is not
    # something a test should have to guess.
    s.pref("Editor/open_dialog_follows_doc", True)
    s.open(s.write("workdir/" + OLD, BODY))


def run(t):
    view = t.document()

    t.focus()
    t.click(view)
    t.key("ctrl+Home")
    t.type_text(TYPED)
    t.wait(lambda: t.text(t.document()) == TYPED + BODY, "the typing to reach the document")

    t.menu("File", "Save As")

    dialog = t.need(t.app, role="file chooser", name="Save As", depth=2,
                    what="the Save As dialog")

    entry = the_name_entry(t, dialog)
    t.check(t.text(entry) == OLD,
            "the name entry starts as the document's own name: %r" % t.text(entry))

    # medit's own addition to the chooser, and the reason it asks for one.
    encoding = t.need(dialog, role="combo box", name="UTF-8",
                      what="the character-encoding combo medit adds to the chooser")
    t.check(t.state(encoding, "showing"), "the encoding combo is on screen")

    t.click(entry)
    t.key("ctrl+a")
    t.type_text(NEW)
    t.wait(lambda: t.text(entry) == NEW,
           "the new name to be in the entry; it holds %r" % t.text(entry))
    t.log("the entry holds %r" % t.text(entry))

    # Enter rather than the Save button: the chooser is taller than the screen
    # the tests run on, so its buttons are below the bottom edge and a click on
    # one lands outside the window. Enter is the default response, and is what a
    # person typing a name presses anyway.
    t.key("Return")
    t.no_toplevel("Save As", role="file chooser")

    t.wait(lambda: t.sandbox.exists("workdir", NEW), "the new file to be written")

    t.check(t.sandbox.read("workdir", NEW) == TYPED + BODY,
            "the new file holds what the document held")
    t.check(t.sandbox.read("workdir", OLD) == BODY,
            "and the file it used to be is untouched")

    t.wait(lambda: NEW in (t.frame.name or ""),
           "the window to name the document after the new file; it says %r"
           % t.frame.name)
    t.check("[modified]" not in (t.frame.name or ""),
            "and the document does not count as modified any more")


def the_name_entry(t, dialog):
    """The entry the name is typed into: the one beside the "Name:" label.

    Not "the only entry that is drawn": the chooser keeps a search entry and the
    folder-name entry of its create-folder panel, and both answer with a
    position. The label is what says which of them is the name of the file.
    """
    for panel in t.find_all(dialog, role="panel", depth=8):
        children = t.find_all(panel, depth=1)
        labels = [c.name for c in children if t.role(c) == "label"]

        if "Name:" not in labels:
            continue

        for child in children:
            if t.role(child) == "text":
                return child

    return t.fail("no entry beside a \"Name:\" label in the Save As dialog:\n%s"
                  % t.dump(dialog))
