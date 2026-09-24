"""Quitting with several tabs open must not trip a GTK-CRITICAL.

# requires: MOO_GTK3

_moo_edit_window_remove_doc() removes a tab's page with
gtk_notebook_remove_page(), which can synchronously fire GTK's own
"switch-page" signal for whatever page ends up active next. That runs
notebook_switch_page() -> edit_changed() -> update_tab_labels() ->
update_tab_label(), and update_tab_label() called gtk_notebook_get_tab_label()
on every remaining tab without checking it was still a page of the notebook --
failing GTK's own "assertion 'list != NULL' failed" for a tab whose removal
was still in flight. This only shows up while quitting (File > Quit closes
every window, not just the active document), not while closing tabs with
Close All -- so the test drives Quit itself instead of waiting for the
framework's own post-run() quit_medit() to do it.

The menu call is wrapped in try/except like runner.quit_medit() does: medit
may already be tearing down its a11y tree by the time it returns -- by then
the criticals are already in medit.log, so it is read straight after.
"""

COUNT = 26
NAMES = ["file-%02d.txt" % i for i in range(COUNT)]


def setup(s):
    for name in NAMES:
        s.open(s.write("workdir/" + name, name + "\n"))


def run(t):
    try:
        t.menu("File", "Quit")
    except Exception as error:
        print("    could not quit through the menu: %s" % error)

    t.check("CRITICAL" not in t.medit_log(),
            "no GTK-CRITICAL while quitting: %s" % t.medit_log())
