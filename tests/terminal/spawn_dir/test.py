"""The shell starts in the directory of the document being edited.

# requires: MOO_BUILD_TERMINAL

The python plugin let the shell inherit medit's own working directory, which
for a program started from a desktop file is the home directory -- never the
one the user is working in. This is the fix for that, and the reason the shell
is not started until the pane is first shown: a window has no document yet when
its plugins are created, so there would be no directory to start in.
"""

import shlex


def setup(s):
    """A document in a directory of its own, and a shell that reports its own."""
    s.open(s.write("workdir/hello.txt", "hello\n"))
    s.pref("Plugins/Terminal/shell",
           s.script("reporting-shell",
                    "#!/bin/sh\npwd > %s\nexec /bin/sh\n"
                    % shlex.quote(s.path("cwd"))))


def run(t):
    document(t)

    t.menu("Tools", "Terminal")
    t.need(t.frame, role="terminal", what="the terminal")

    started_in = t.wait(lambda: t.sandbox.read("cwd").strip(),
                        "the shell to report the directory it started in")

    t.check(started_in == t.sandbox.path("workdir"),
            "the shell started in %s, the directory of the open document"
            % started_in)


def document(t):
    """The file was opened from the command line; make sure it is really there.

    Without a document there is no directory to start in and the shell falls
    back to medit's own, which is exactly the behaviour this test is about --
    so an empty window would make it pass for the wrong reason.
    """
    t.wait(lambda: "hello.txt" in (t.frame.name or ""),
           "the document to be open, as the window title says")
