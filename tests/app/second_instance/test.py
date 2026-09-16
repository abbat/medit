"""A second medit hands its files to the first one, unless it is told not to.

# requires: MOO_GTK3

The single instance handshake of src/medit-app: a copy started while another is
running sends what it was given over the socket in the sandbox's temporary
directory and exits, so that clicking a file in a file manager opens a tab
rather than a second editor. With --new-app it starts anyway, which is how the
harness starts every other test.

GTK+3 only, for the reason document_text gives.
"""

OPEN = "the document the window starts with\n"
SENT = "the document the second copy was given\n"
OWN = "the document the copy with --new-app keeps to itself\n"

# Long enough for a medit to come up and open a file: the point of the last
# check is that this one does not, and a check that a thing has not happened is
# only worth what it waited.
STARTUP = 10


def setup(s):
    s.open(s.write("workdir/open.txt", OPEN))
    s.write("workdir/sent.txt", SENT)
    s.write("workdir/own.txt", OWN)


def run(t):
    t.check(pages(t) == ["open.txt"], "the window starts with the one document")

    handed_over = t.medit(t.sandbox.path("workdir/sent.txt"))
    t.check(handed_over.wait(timeout=STARTUP) == 0,
            "the second copy exited, with %d" % handed_over.returncode)

    t.wait(lambda: "sent.txt" in pages(t),
           "the file it was given to open in the running medit")
    t.check(t.text(t.document()) == SENT, "and to be the document on screen")

    on_its_own = t.medit("--new-app", t.sandbox.path("workdir/own.txt"))

    try:
        t.settle(STARTUP)
        t.check(on_its_own.poll() is None,
                "the copy started with --new-app is still running")
        t.check("own.txt" not in pages(t),
                "and the file it was given is not in this window: %s"
                % ", ".join(pages(t)))
    finally:
        on_its_own.terminate()
        on_its_own.wait(timeout=STARTUP)


def pages(t):
    """The names of the open documents, as the notebooks report their pages."""
    found = []

    for notebook in t.find_all(t.frame, role="page tab list", depth=25):
        found += [child.name for child in t.find_all(notebook, depth=1) if child.name]

    return sorted(found)
