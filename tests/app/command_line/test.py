"""What the command line says about where to open a file, and where in it.

# requires: MOO_GTK3

The startup path of src/medit-app, which is the least covered code in the tree
and the part of it that runs before there is anything to click on: a name is
resolved against the working directory when it is relative, "name:LINE" is a
file and a line when there is no file by that whole name, and a "+LINE"
argument of its own is the line for the rest of them.

GTK+3 only, for the reason document_text gives.
"""

FIRST = "one\ntwo\nthree\nfour\nfive\n"
SECOND = "alpha\nbeta\ngamma\ndelta\n"


def setup(s):
    s.write("workdir/first.txt", FIRST)
    s.write("workdir/second.txt", SECOND)

    # Relative, because resolving one against the working directory is a branch
    # of parse_file() nothing else here takes: every other test opens a file by
    # the absolute path s.write() returns.
    s.open("workdir/first.txt")
    s.open("workdir/second.txt:3")
    s.open("+2")


def run(t):
    open_documents = pages(t)

    t.check(open_documents == ["first.txt", "second.txt"],
            "both files named on the command line are open: %s"
            % ", ".join(open_documents))

    view = t.document()
    t.check(t.text(view) == SECOND, "the file opened last is the one on screen")
    t.check(line_of(t, view) == 3,
            "'second.txt:3' put the cursor on line %d" % line_of(t, view))

    t.menu("Window", "first.txt")
    t.wait(lambda: t.text(t.document()) == FIRST, "the other document to be shown")

    # The +2 was not a file, and is not open: the loop above found two documents.
    t.check(line_of(t, t.document()) == 2,
            "'+2' put the cursor of the other file on line %d"
            % line_of(t, t.document()))


def line_of(t, view):
    """The line the cursor is on, counting from one."""
    return t.text(view)[:t.caret(view)].count("\n") + 1


def pages(t):
    """The names of the open documents, as the notebooks report their pages."""
    found = []

    for notebook in t.find_all(t.frame, role="page tab list", depth=25):
        found += [child.name for child in t.find_all(notebook, depth=1) if child.name]

    return sorted(found)
