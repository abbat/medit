"""A server that gave up says why in the pane, whenever the pane is opened.

# requires: MOO_BUILD_LSP, MOO_GTK3

The pane is where a user looks when the client has gone quiet, and until the
message was put in it the pane was empty in exactly the way it is for a
document with nothing wrong with it.

Opened after the failure, on purpose. The pane is filled when diagnostics
arrive and when the active document changes, and a server that gave up before
anyone opened the pane has neither of those left to offer -- so the pane fills
itself when it is mapped, and this is the test for that as much as for the
message.
"""

# LSP_MAX_QUICK_EXITS in lsp-server.cpp, and the sentence set_failed() writes.
MAX_STARTS = 3
GAVE_UP = "exited immediately %d times in a row" % MAX_STARTS


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", "alpha\n"))
    s.lsp_server(filter="globs:*.txt", mode="exit-at-once")


def run(t):
    # Nothing is opened until the client has finished giving up: what is being
    # tested is a pane opened after the fact, with nothing left to notify it.
    t.wait(lambda: t.lsp_starts() == MAX_STARTS, "the client to stop trying")
    t.settle(1)

    t.menu("Tools", "Diagnostics")

    pane = t.wait(lambda: diagnostics_pane(t), "the diagnostics pane")

    t.wait_text(pane, GAVE_UP, squeeze=True,
                what="the reason the client stopped trying, in the pane")
    t.check("check the command in the LSP configuration file"
            in " ".join(t.text(pane).split()),
            "and the pane says where to fix it")


def diagnostics_pane(t):
    views = [view for view in t.on_screen(t.find_all(t.frame, role="text", depth=30))
             if not t.state(view, "editable")]

    return views[0] if len(views) == 1 else None
