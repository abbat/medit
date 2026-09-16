"""The session file: what medit writes into it, and what it brings back.

command_line and second_instance cover what the startup path does with what it
is given; this is the other half of it, the state it keeps for itself. The
documents of a run are written to the session file when medit stops -- whether
it is asked to quit or killed -- and are opened again by the next start, which
is what makes closing the editor and coming back to the same tabs work.

The harness's own medit is no use for this: its session is written when it
quits, which is after the test is over. So the test starts copies of its own
with --app-name, and each of them keeps its session in a file of that name --
session-NAME.xml -- next to, and not on top of, the harness's session.xml.

A copy is an application in its own right on the accessibility bus, and t.frame
leads only to the harness's window, so a copy's window is found by its title:
the title carries the document, and this document is open in no other window.
"""

import os
import signal

NAME = "reopened"
KEPT = "the document the session is expected to bring back\n"
DOC = "kept.txt"

# Long enough for a medit to start, open a document and be stopped again.
STARTUP = 30

SESSION = ("cache", "medit", "session-%s.xml" % NAME)


def setup(s):
    # Written but not opened: this window must not have it, or the title the
    # copies are found by would name two windows rather than one.
    s.write("workdir/" + DOC, KEPT)


def run(t):
    path = t.sandbox.path("workdir/" + DOC)
    session = t.sandbox.path(*SESSION)

    told = t.medit("--app-name", NAME, "--use-session=yes", path)
    window(t, "the copy medit was told to open the document")
    t.check(not os.path.exists(session),
            "no session is written while that copy is running")

    quit_copy(t, told)
    t.check(os.path.exists(session), "quitting wrote %s" % os.path.join(*SESSION))
    t.check(path in t.sandbox.read(*SESSION),
            "and the document it had open is in it:\n%s" % t.sandbox.read(*SESSION))

    # Nothing on the command line this time: whatever comes up came from the
    # file the first copy left behind.
    reopened = t.medit("--app-name", NAME, "--use-session=yes")
    window(t, "the document open again in a copy that was given none")

    # Away, so that what is there afterwards is this copy's own work rather
    # than the file the first one wrote.
    os.unlink(session)

    os.kill(reopened.pid, signal.SIGINT)
    t.check(reopened.wait(timeout=STARTUP) != 0,
            "a killed medit exited, with %d" % reopened.returncode)
    t.check(os.path.exists(session) and path in t.sandbox.read(*SESSION),
            "and wrote its documents to the session on the way out")


def window(t, what):
    """The copy's window, waited for and named by the document it has open."""
    return t.wait(lambda: next(iter(t.frames_titled(DOC)), None), what, STARTUP)


def quit_copy(t, proc):
    """Quit another copy of medit the way a person would, and wait for it.

    Through the accelerator rather than the File menu: a menu is clicked at a
    point on the screen, and with the copy's window over the harness's the
    point belongs to whichever of them the X server has on top. The keys go
    where the input focus is, which the test says. A signal is no substitute
    either, since the exit code here is the whole point -- this is the clean
    quit, and the killed one is checked further down.
    """
    t.focus(window(t, "the copy to quit"))
    t.key("ctrl+q")
    t.check(proc.wait(timeout=STARTUP) == 0,
            "the copy quit through Ctrl+Q, with %d" % proc.returncode)

    # Its window can outlive it on the accessibility bus for a moment, and the
    # next copy is found by the same title: without this the test could go on
    # looking at the window of the copy that has already gone.
    t.wait(lambda: not t.frames_titled(DOC), "that window to be gone", STARTUP)
