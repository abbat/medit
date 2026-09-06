"""Help/About: the credits dialog, the license, and the two links.

Everything in this dialog leads somewhere else -- another dialog, or a browser --
which is why it is the first test: it exercises a dialog opening over a dialog,
a dialog closing without taking its parent with it, and a hyperlink handed to
the desktop, and it does all of that identically on both toolkits.
"""

TABS = ("Thanks", "Written by", "Translated by")


def run(t):
    t.menu("Help", "About")
    about = t.dialog("About")

    credits_dialog(t, about)
    license_button(t, about)
    links(t, about)

    t.click(t.button(about, "Close"))
    t.no_toplevel("About")


def credits_dialog(t, about):
    """Credits opens a notebook of three tabs, and closes without closing About."""
    t.click(t.button(about, "Credits"))
    credits = t.dialog("Credits")

    tabs = t.find_all(credits, role="page tab")
    t.check([tab.name for tab in tabs] == list(TABS),
            "the credits notebook has the tabs %s" % ", ".join(TABS))

    for tab in tabs:
        t.click(tab)
        view = t.need(tab, role="text", what="the text view of the %r tab" % tab.name)
        body = t.text(view).strip()

        if tab.name == "Translated by":
            # Deliberately asserted the other way round. The tab is filled from
            # _("translator-credits"), and credits.c only writes it when the
            # lookup returns something other than the msgid -- so in an
            # untranslated locale, which is the one every test runs in, the tab
            # is there and empty. Asserting that it is filled would mean pinning
            # a locale whose catalog happens to be complete, and asserting
            # nothing would let a real regression through.
            t.check(body == "", "the %r tab is empty without a translation" % tab.name)
        else:
            t.check(body != "", "the %r tab has text" % tab.name)

    t.click(t.button(credits, "Close"))
    t.no_toplevel("Credits")
    t.check(t.find(t.app, role="dialog", name="About", depth=2) is not None,
            "About is still open after Credits closed")


def license_button(t, about):
    """License hands the COPYING file to a browser rather than opening a window."""
    t.click(t.button(about, "License"))

    opened = t.wait(lambda: [u for u in t.urls() if u.endswith("/COPYING")],
                    "the license to be opened in a browser")
    t.check(len(opened) == 1, "the license opened once, at %s" % opened[0])


def links(t, about):
    """Each link in the dialog opens the address it claims to point at."""
    labels = t.link_labels(about)
    t.check(labels != [], "the dialog has at least one label with links")

    seen = []

    for label in labels:
        for link in t.links(label):
            t.click_link(label, link)
            t.wait_url(link["uri"])
            seen.append(link["uri"])

    t.check(len(set(seen)) == len(seen), "no link was counted twice")
    t.check(len(seen) == 2, "both links in the dialog were followed")
