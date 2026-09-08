"""Close All with several changed documents saves the ones that are ticked.

# requires: MOO_GTK3

_moo_edit_save_multiple_changes_dialog(). It is medit's own dialog, built from
mooeditsavemult.ui: a list of the documents with a tick beside each, all ticked
to begin with, and three answers -- Save None, Cancel, Save Selected. Only the
ticked ones are written, and that list is the whole point of the dialog.

So the test unticks one of two changed documents and takes Save Selected, then
reads both files: the ticked one has the change, the unticked one does not. A
dialog that ignored the ticks, or that read them the wrong way round, would look
exactly the same on screen and would either lose a change the user asked to keep
or write one they asked to drop.
"""

FIRST = "one.txt"
SECOND = "two.txt"

BODIES = {FIRST: "one\n", SECOND: "two\n"}

TYPED = "x"


def setup(s):
    for name in (FIRST, SECOND):
        s.open(s.write("workdir/" + name, BODIES[name]))


def run(t):
    # The one opened last is showing; change it, then go to the other and change
    # that one too.
    change(t, SECOND)
    t.menu("Window", FIRST)
    t.wait(lambda: t.text(t.document()) == BODIES[FIRST], "the other document to come up")
    change(t, FIRST)

    t.menu("File", "Close All")

    dialog = the_dialog(t)

    said = " ".join(label.name or "" for label in t.find_all(dialog, role="label"))
    t.check("2 documents" in said,
            "the dialog says how many documents are waiting: %r" % said)

    table = t.need(dialog, role="table", what="the list of documents in the dialog")
    cells = t.find_all(table, role="table cell", depth=1)
    listed = [cell.name for cell in cells if cell.name]

    t.check(sorted(listed) == sorted([FIRST, SECOND]),
            "both changed documents are listed: %s" % ", ".join(listed))

    # Untick the first one. The tick is the cell before the name in its row.
    tick = cells[cells.index(next(c for c in cells if c.name == FIRST)) - 1]

    t.check(t.state(tick, "checked"), "every document starts ticked")
    t.click(tick)
    t.wait(lambda: not t.state(tick, "checked"), "the tick to come off %s" % FIRST)
    t.log("ok: %s is unticked" % FIRST)

    t.click(t.button(dialog, "Save Selected"))

    t.wait(lambda: t.sandbox.read("workdir", SECOND) == TYPED + BODIES[SECOND],
           "the ticked document to be written; the file holds %r"
           % t.sandbox.read("workdir", SECOND))
    t.log("ok: the ticked document was saved")

    t.check(t.sandbox.read("workdir", FIRST) == BODIES[FIRST],
            "and the unticked one was not: its file is as it was")

    t.wait(lambda: ".txt" not in (t.frame.name or ""),
           "both documents to be closed; the window says %r" % t.frame.name)
    t.log("ok: Close All closed them both either way")


def change(t, name):
    view = t.document()

    t.check(t.text(view) == BODIES[name], "%s is the document on screen" % name)

    t.focus()
    t.click(view)
    t.key("ctrl+Home")
    t.type_text(TYPED)
    t.wait(lambda: t.text(t.document()) == TYPED + BODIES[name],
           "the typing to reach %s" % name)


def the_dialog(t):
    """The dialog that lists the documents. It has no title, so it is found by
    the button only it has."""
    def found():
        for top in t.find_all(t.app, role="dialog", depth=2):
            if t.find(top, role="push button", name="Save Selected") is not None:
                return top

        return None

    return t.wait(found, "the dialog listing the documents with unsaved changes")
