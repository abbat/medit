"""A server that dies is started again, and told about the documents once more.

# requires: MOO_BUILD_LSP

Language servers crash; the client's answer is to start another one. What has
to happen with it is the part worth testing: the new process knows nothing
about the documents the old one had open, so the set is cleared and every
document is announced again -- and a client that forgot to would go on asking
about files its server has never heard of, and get nothing back for the rest of
the session.

Both toolkits.
"""


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", "alpha\n"))

    # Alive long enough not to count as a quick exit -- LSP_QUICK_EXIT_USEC is
    # two seconds, and three quick ones in a row make the client give up.
    s.lsp_server(filter="globs:*.txt", exit_after=3)


def run(t):
    first = t.wait_lsp("textDocument/didOpen")

    t.check(t.lsp_starts() == 1, "one server was started")

    t.wait(lambda: t.lsp_starts() == 2,
           "the server that died to be started again", timeout=30)
    t.log("ok: the server was started again after it exited")

    second = t.wait_lsp("textDocument/didOpen", count=2, timeout=30)

    t.check(second["pid"] != first["pid"],
            "the document was announced to the new process, not the old one")

    # Settled down before the end: the scenario the next process reads no
    # longer tells it to die, so the quit at the end of the test is not racing
    # a server that is on its way out.
    t.sandbox.lsp_scenario("test")
