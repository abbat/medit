"""Two open documents are told apart, and switching between them switches views.

# requires: GTK3

GTK+3 only, for the reason document_text gives.
"""

FIRST = "first document\n"
SECOND = "second document\n"


def setup(s):
    s.open(s.write("workdir/first.txt", FIRST))
    s.open(s.write("workdir/second.txt", SECOND))


def run(t):
    open_documents = pages(t)

    t.check(open_documents == ["first.txt", "second.txt"],
            "both documents are in the tree, by name: %s" % ", ".join(open_documents))

    view = shown(t)
    t.check(t.text(view) == SECOND,
            "the document opened last is the one on screen")

    t.menu("Window", "first.txt")
    t.wait(lambda: t.text(shown(t)) == FIRST,
           "the other document to be shown after switching to it")
    t.log("ok: switching windows switches which document is on screen")

    # The name follows the file, so typing in it renames the page.
    t.type_text("x")
    t.wait(lambda: "*first.txt" in pages(t),
           "the page of a document with unsaved changes to be marked")
    t.log("ok: an edited document is marked in the name of its page")

    t.key("ctrl+z")
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")


def pages(t):
    """The names of the open documents, as the notebooks report their pages.

    Whatever the page happens to be made of -- the role of the widget a page
    holds is no part of what this is about -- so the children of the notebooks
    are taken as they come.
    """
    found = []

    for notebook in t.find_all(t.frame, role="page tab list", depth=25):
        found += [child.name for child in t.find_all(notebook, depth=1) if child.name]

    return sorted(found)


def shown(t):
    views = t.on_screen(t.find_all(t.frame, role="text", depth=25))
    t.check(len(views) == 1, "one document is on screen")
    return views[0]
