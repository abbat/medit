"""Move to Split Notebook, and Focus Document.

# requires: MOO_GTK3

The two entries of the View menu that no test had used. A window has two
notebooks side by side rather than one, and the second is empty until something
is moved into it: move_tab_to_split_view() takes the tab out of the notebook it
is in and puts it in the other, which is what makes two documents visible at
once. Focus Document is the way back to the text from wherever the keyboard has
got to.

What is asserted is which notebook each document is in, read from the pages
rather than from the tabs: MooNotebookAccessible names each page after its tab,
so the strips can be told apart by what they hold. And Focus Document is asserted
by typing: the keyboard is left in a pane, and afterwards what is typed lands in
the document.
"""

from lib import input as ui

FIRST = "first.txt"
SECOND = "second.txt"

CONTENT = "a line\n"

MOVE = ("View", "Move to Split Notebook")
FOCUS = ("View", "Focus Document")


def setup(s):
    s.open(s.write("workdir/" + FIRST, CONTENT))
    s.open(s.write("workdir/" + SECOND, CONTENT))


def run(t):
    t.check(len(notebooks(t)) == 1, "the window starts with one notebook of documents")

    showing = pages(t, notebooks(t)[0])
    t.check(sorted(showing) == sorted([FIRST, SECOND]),
            "and both documents are in it: %s" % ", ".join(showing))

    moved = current(t)

    t.menu(*MOVE)

    t.wait(lambda: len(notebooks(t)) == 2,
           "the document to move into a notebook of its own")

    left, right = notebooks(t)
    t.check(pages(t, right) == [moved],
            "the document that was in front is the one that moved: %s"
            % ", ".join(pages(t, right)))
    t.check(len(pages(t, left)) == 1,
            "and the other stayed where it was: %s" % ", ".join(pages(t, left)))

    # Focus Document, with the keyboard somewhere else: a pane takes it, and the
    # menu entry is what gives it back.
    t.menu("View", "Panes", "File List")
    t.menu(*FOCUS)

    ui.type_text("x")

    # Whichever of the two is the current one: what matters is that the keyboard
    # went back to a document at all, rather than staying in the pane.
    t.wait(lambda: any(text.startswith("x") for text in texts(t)),
           "what is typed to land in a document; they hold %s" % texts(t))

    # And back: the same entry moves the tab to the other notebook again.
    t.menu(*MOVE)

    t.wait(lambda: len(notebooks(t)) == 1,
           "the document to go back to the notebook it came from")

    t.focus()
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")


def notebooks(t):
    """The document notebooks that are drawn, left to right."""
    found = [n for n in t.find_all(t.frame, role="page tab list", depth=25)
             if ui.on_screen(n)]

    return sorted(found, key=lambda node: t.extents(node)[0])


def pages(t, notebook):
    return [page.name for page in t.find_all(notebook, depth=1) if page.name]


def texts(t):
    """What the documents on screen hold; there are two once one has moved."""
    return [t.text(view) for view in t.find_all(t.frame, role="text", depth=25)
            if ui.on_screen(view) and t.state(view, "editable")]


def current(t):
    """The document in front, which is the page that is drawn."""
    for notebook in notebooks(t):
        for page in t.find_all(notebook, depth=1):
            if ui.on_screen(page):
                return page.name

    return None
