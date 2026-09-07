"""One server per project root, found by the markers the entry names.

# requires: MOO_BUILD_LSP

A server is started per root and kept for as long as a document of that project
is open, which is what makes clangd's index worth building: two files of one
project share a process, and a file of another project gets one of its own.

The root is the nearest directory at or above the file holding one of the
markers, and the file's own directory when nothing does. All three cases are
here, and they are told apart by the pid the fake server writes into its log --
the two servers share a log, as they would share a configuration file.

Both toolkits: nothing is read from the screen.
"""

MARKER = "gitdir: elsewhere\n"


def setup(s):
    s.plugin("Lsp")

    # A project with a marker at its root, and a file one directory down.
    s.write("proj/.git", MARKER)
    s.open(s.write("proj/sub/one.txt", "one\n"))
    s.open(s.write("proj/two.txt", "two\n"))

    # And a file with no marker anywhere above it.
    s.open(s.write("elsewhere/three.txt", "three\n"))

    s.lsp_server(filter="globs:*.txt", root=".git")


def run(t):
    t.wait(lambda: len(t.lsp("initialize")) == 2,
           "two servers to be started, one per root; %d were"
           % len(t.lsp("initialize")))

    t.check(t.lsp_starts() == 2, "two processes, and no more")

    roots = sorted(message["params"]["rootUri"] for message in t.lsp("initialize"))

    t.check(roots == ["file://" + t.sandbox.path("elsewhere"),
                      "file://" + t.sandbox.path("proj")],
            "the roots are the marked directory and the lone file's own: %s"
            % ", ".join(roots))

    # Which document went to which process. The two files of the project must
    # have been announced to the same one.
    where = {}

    for message in t.lsp("textDocument/didOpen"):
        where[message["params"]["textDocument"]["uri"].rsplit("/", 1)[-1]] = message["pid"]

    t.check(sorted(where) == ["one.txt", "three.txt", "two.txt"],
            "all three documents were announced: %s" % ", ".join(sorted(where)))
    t.check(where["one.txt"] == where["two.txt"],
            "the two files of the project went to one server")
    t.check(where["three.txt"] != where["one.txt"],
            "and the file outside it went to the other")
