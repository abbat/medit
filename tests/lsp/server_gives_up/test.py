"""A server that dies as soon as it starts is not started forever, and says so.

# requires: MOO_BUILD_LSP

The counter that stops it, and the sentence that explains why nothing is
happening. set_failed() has always written that sentence; for a while nothing
read it back, so the client simply went quiet -- three processes in two seconds
and then an editor that never mentions language servers again.

Here it is asserted on medit's own output, which is the half of the report that
works on both toolkits. The other half, the line in the diagnostics pane, is
what failure_pane tests.
"""

# LSP_MAX_QUICK_EXITS in lsp-server.cpp
MAX_STARTS = 3

GAVE_UP = "exited immediately %d times in a row" % MAX_STARTS


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

    t.wait(lambda: GAVE_UP in t.medit_log(),
           "the reason it stopped trying to reach medit's output")
    t.check("check the command in the LSP configuration file" in t.medit_log(),
            "and the message says where to fix it")

    # The window is still usable, which is not obvious: the client has a failed
    # server attached to the document it is showing.
    t.focus()
    t.key("ctrl+End")
    t.type_text("beta")
    t.key("ctrl+s")

    t.wait(lambda: t.sandbox.read("workdir/hello.txt") == "alpha\nbeta",
           "the document to be editable and saveable with a failed server")
