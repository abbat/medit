"""The Help menu: the web site, and the bug report that asks first.

# requires: MOO_GTK3

The two entries of the Help menu that are not About both end in a URL handed to
the desktop, which in a sandbox is a script that writes the URL down -- so what
they did is a string comparison rather than a browser.

Report a Bug is the one with something to it. moo_app_report_bug() shows the URL
and asks before opening it, and remembers the answer for that URL in the state
preferences, so the question is asked once and never again. All three of those
are asserted: cancelled and nothing is opened, accepted and it is, and asked for
a second time it opens without asking.
"""

WEBSITE = "https://github.com/abbat/medit"
BUG_URL = "https://github.com/abbat/medit/issues/new"

HELP = ("Help", "Help")
REPORT = ("Help", "Report a Bug...")

QUESTION = "Open URL?"


def setup(s):
    s.open(s.write("workdir/notes.txt", "hello"))


def run(t):
    t.menu(*HELP)
    t.wait_url(WEBSITE)

    # Cancelled: the question is asked, and answering no opens nothing.
    t.menu(*REPORT)
    dialog = t.wait(lambda: the_question(t), "the question about the bug report")

    asked = " ".join(label.name or "" for label in t.find_all(dialog, role="label"))
    t.check(BUG_URL in asked, "the question shows the URL it means: %r" % asked)

    t.click(t.button(dialog, "Cancel"))
    t.wait(lambda: the_question(t) is None, "the question to go")

    t.check(BUG_URL not in t.urls(),
            "nothing was opened after Cancel: %s" % ", ".join(t.urls()))

    # Accepted: the URL is opened, and the answer is remembered.
    t.menu(*REPORT)
    dialog = t.wait(lambda: the_question(t), "the question again")
    t.click(t.button(dialog, "OK"))

    t.wait_url(BUG_URL)

    t.menu(*REPORT)
    t.wait(lambda: t.urls().count(BUG_URL) == 2,
           "the bug URL to be opened a second time; the browser has %s"
           % ", ".join(t.urls()))

    t.check(the_question(t) is None,
            "and it was not asked about again, the answer being remembered")


def the_question(t):
    """The question, which is a message dialog and so an alert."""
    found = [node for node in t.find_all(t.app, role="alert", depth=2)]

    return found[0] if found else None
