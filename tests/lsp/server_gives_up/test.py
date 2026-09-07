"""A server that dies as soon as it starts is not started forever.

# requires: MOO_BUILD_LSP

The counter that stops it, and nothing else -- because nothing else is
observable. When it gives up, lsp_server set_failed() puts a sentence in
error_message saying which server it was and where to fix it, and
lsp_server_get_error() exists to hand that sentence over, but nothing calls it:
there is no pane, no status bar and no line in the log where a user could find
out that the client has stopped trying. The terminal, which had the same bug in
the same shape, writes it into the pane where the user is looking.

So this test asserts the count of processes, which is the whole of what the
outside world can see. If the message ever reaches the user, this is where the
assertion for it belongs.

Both toolkits.
"""

# LSP_MAX_QUICK_EXITS in lsp-server.cpp
MAX_STARTS = 3


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", "alpha\n"))
    s.lsp_server(filter="globs:*.txt", mode="exit-at-once")


def run(t):
    t.wait(lambda: t.lsp_starts() == MAX_STARTS,
           "the server to be tried %d times; it was tried %d"
           % (MAX_STARTS, t.lsp_starts()))
    t.log("ok: the server was started %d times" % MAX_STARTS)

    # Nothing tries again afterwards: a loop that kept going would keep going
    # now, and this is the one thing that says the counter did its work.
    t.settle(3)

    t.check(t.lsp_starts() == MAX_STARTS, "and not once more after it gave up")

    # The window is still usable, which is not obvious: the client has a failed
    # server attached to the document it is showing.
    t.focus()
    t.key("ctrl+End")
    t.type_text("beta")
    t.key("ctrl+s")

    t.wait(lambda: t.sandbox.read("workdir/hello.txt") == "alpha\nbeta",
           "the document to be editable and saveable with a failed server")
