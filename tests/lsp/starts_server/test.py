"""A document that matches an entry gets a server; one that does not gets nothing.

# requires: MOO_BUILD_LSP

The first half of the client: which document is handed to which server, what
the server is told about it, and how loudly medit fails to find a program that
is not installed -- which must be not at all, since the shipped list names nine
servers and nobody has nine.

Everything here is asserted from the log the server keeps rather than from the
screen, because none of it reaches the screen: a document announced to a server
looks exactly like a document that was not.
"""

MATCHING = "alpha beta\ngamma delta\n"
OTHER = "# not a text file\n"


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", MATCHING))
    s.open(s.write("workdir/notes.md", OTHER))

    # Three entries, and the first two are not installed. The shipped file
    # names nine servers and nobody has nine, so an entry for a program that is
    # not there is the ordinary case -- and the first of them matches the same
    # documents as the working one, which is what says a skipped entry does not
    # decide for the entries after it.
    s.lsp_config("""<?xml version="1.0" encoding="UTF-8"?>
<medit-lsp version="1.0">
  <server id="missing-for-txt">
    <filter>globs:*.txt</filter>
    <command>%s/no-such-language-server</command>
  </server>
  <server id="missing-for-md">
    <filter>globs:*.md</filter>
    <command>%s/no-such-language-server-either</command>
  </server>
  <server id="test">
    <filter>globs:*.txt</filter>
    <command>%s</command>
  </server>
</medit-lsp>
""" % (s.root, s.root, s.lsp_command("test")))

    s.lsp_scenario("test")


def run(t):
    t.wait_lsp("initialize")

    t.check(t.lsp_starts() == 1, "one server was started")

    root = t.wait_lsp("initialize")["params"].get("rootUri")
    t.check(root == "file://" + t.sandbox.path("workdir"),
            "and it was told the document's own directory is the root: %s" % root)

    opened = t.wait_lsp("textDocument/didOpen")["params"]["textDocument"]

    t.check(opened["uri"] == "file://" + t.sandbox.path("workdir/hello.txt"),
            "the matching document was announced to it, and not to the entry "
            "before it whose program is not installed")
    t.check(opened["text"] == MATCHING,
            "with the text the file holds")

    # The other document is open in the same window, and the only entry that
    # matches it names a program that is not there. Nothing runs for it, and --
    # the assertion that matters -- nothing is said about it either: a line of
    # complaint per file opened is exactly what the check is there to avoid.
    announced = [m["params"]["textDocument"]["uri"] for m in t.lsp("textDocument/didOpen")]

    t.check(not any(uri.endswith("notes.md") for uri in announced),
            "the document whose every entry is not installed was not announced: %s"
            % announced)

    t.check("no-such-language-server" not in t.medit_log(),
            "and medit said nothing about either program it could not find")
