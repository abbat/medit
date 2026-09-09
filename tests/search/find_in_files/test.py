"""Find in Files runs the search and puts the matches in a pane.

# requires: MOO_GTK3

The Find plugin, src/plugins/moofind.cpp, which nothing had ever driven. It is
not a search of its own: it builds a grep command line out of the dialog -- the
pattern, the file globs, the directory, the globs to skip and the case box --
runs it through a MooCmdView in the Search Results pane, and parses what comes
back into results that can be clicked.

So what the test asserts is what came out of it: the file that holds the word is
in the pane with the line it is on, the file that does not is absent, and the
file that the globs excluded is absent as well. That is the command line built
correctly, the run, and the output all at once -- and a grep that matched
everything would fail the second half.
"""

WORD = "needle"

FILES = {
    "workdir/hay.txt": "first line\n%s in the hay\nlast line\n" % WORD,
    "workdir/clean.txt": "nothing to find here\n",
    "workdir/skipped.log": "%s in a log file\n" % WORD,
}

# The globs the dialog is given: the .log file matches the word and is excluded
# by them, which is what tells a search that read the globs from one that did not.
GLOBS = "*.txt"


def setup(s):
    for name, body in FILES.items():
        s.write(name, body)

    s.open(s.path("workdir/clean.txt"))


def run(t):
    t.menu("Search", "Find In Files")

    dialog = t.dialog("Find in Files")

    fill(t, dialog, WORD, GLOBS, t.sandbox.path("workdir"))

    t.click(t.button(dialog, "Find"))
    t.no_toplevel("Find in Files")

    pane = t.wait(lambda: results_pane(t), "the Search Results pane to open")

    t.wait_text(pane, "hay.txt", what="the file that holds the word to be listed")

    text = t.text(pane)

    t.check("%s in the hay" % WORD in text,
            "the matching line is listed with the file: %r" % text)
    t.check("clean.txt" not in text,
            "and the file without the word is not: %r" % text)
    t.check("skipped.log" not in text,
            "nor the one the file globs left out: %r" % text)

    t.check("*** 1 match found ***" in text,
            "and the plugin counted the one match: %r" % text)

    # A result is clickable: MooLineView activates the line the click landed on,
    # and the plugin's handler opens the file there.
    line = text.index("%s in the hay" % WORD)
    x, y, _, _ = t.range_extents(pane, line, line + 1)
    t.click_at(x + 2, y + 2)

    view = t.wait(lambda: opened(t), "the match to be opened in a document")
    t.wait_caret(view, len("first line\n"),
                 "with the cursor on the line the match was found on")


def opened(t):
    """The document view, once it is showing the file the match was in."""
    view = t.document()

    return view if view is not None and t.text(view) == FILES["workdir/hay.txt"] else None


def results_pane(t):
    """The pane's text view: the one on screen that cannot be typed into."""
    views = [view for view in t.on_screen(t.find_all(t.frame, role="text", depth=30))
             if not t.state(view, "editable")]

    return views[0] if len(views) == 1 else None


def fill(t, dialog, pattern, globs, directory):
    """Type into the dialog's four combos, which are one per row and in order."""
    entries = t.on_screen(t.find_all(dialog, role="text"))
    entries.sort(key=lambda node: t.extents(node)[1])

    t.check(len(entries) == 4,
            "the dialog has its four entries: pattern, files, directory, skip")

    for entry, text in zip(entries, (pattern, globs, directory)):
        t.focus()
        t.click(entry)
        t.key("ctrl+a")
        t.type_text(text)
