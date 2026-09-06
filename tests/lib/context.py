"""What a test is handed.

A test file is one function::

    def run(t):
        t.menu("Help", "About")
        about = t.dialog("About")
        ...

and everything it needs is on t. Tests import nothing, so the whole vocabulary
of a test is in this one class, and a test reads as the scenario rather than as
plumbing.
"""

import os
import time

from . import a11y
from . import browser
from . import input as ui


MENU_ROLES = ("menu item", "check menu item", "radio menu item", "menu")


class Failed(AssertionError):
    pass


class Test(object):
    def __init__(self, app, gtk, url_log, log_dir, out):
        self._started = time.time()
        self.app = app
        self.gtk = int(gtk)
        self.log_dir = log_dir
        self._url_log = url_log
        self._out = out

        self.frame = self.wait(
            lambda: a11y.find(self.app, role="frame", depth=2), "the main window")

    # -- output ------------------------------------------------------------

    def log(self, message):
        # Timestamped, because the other half of the evidence -- medit's own
        # output in medit.log -- is timestamped too, and a warning is only
        # attributable to a step if the two can be lined up.
        self._out.write("    %6.2f  %s\n" % (time.time() - self._started, message))
        self._out.flush()

    def check(self, condition, message):
        """Assert, and say what was expected either way."""
        if not condition:
            raise Failed(message)
        self.log("ok: %s" % message)

    def fail(self, message):
        raise Failed(message)

    def dump(self, node=None):
        return a11y.dump(node if node is not None else self.app)

    # -- finding things ----------------------------------------------------

    def wait(self, fn, what, timeout=a11y.TIMEOUT):
        return a11y.wait(fn, what, timeout)

    def find(self, root, **kwargs):
        return a11y.find(root, **kwargs)

    def find_all(self, root, **kwargs):
        return a11y.find_all(root, **kwargs)

    def need(self, root, what=None, timeout=a11y.TIMEOUT, **kwargs):
        """The first match, waited for, or a failure naming what was missing."""
        described = what or ", ".join("%s=%r" % kv for kv in sorted(kwargs.items()))
        return self.wait(lambda: a11y.find(root, **kwargs), described, timeout)

    def toplevel(self, name, role="dialog", timeout=a11y.TIMEOUT):
        """A toplevel window of the application, by title."""
        return self.wait(
            lambda: a11y.find(self.app, role=role, name=name, depth=2),
            "the %s %s" % (name, role), timeout)

    def dialog(self, name, timeout=a11y.TIMEOUT):
        return self.toplevel(name, "dialog", timeout)

    def no_toplevel(self, name, role="dialog", timeout=a11y.TIMEOUT):
        """Wait until a toplevel with that title is gone."""
        self.wait(
            lambda: a11y.find(self.app, role=role, name=name, depth=2) is None,
            "the %s %s to close" % (name, role), timeout)

    def button(self, root, label, timeout=a11y.TIMEOUT):
        return self.need(root, role="push button", name=label,
                         what="the %r button" % label, timeout=timeout)

    def text(self, node):
        return a11y.text_of(node)

    def links(self, node):
        return a11y.links_of(node)

    def link_labels(self, root):
        """Every label in the subtree that carries at least one hyperlink."""
        return [n for n in a11y.find_all(root, role="label") if a11y.links_of(n)]

    # -- acting ------------------------------------------------------------

    def click(self, node, button=1):
        # Read the description before clicking. A button that closes its dialog
        # takes its own accessible with it, and by the time the line is printed
        # the node is defunct and answers with an empty name.
        described = "%s %r" % (a11y.role(node), a11y.name(node))
        x, y = ui.click(node, button)
        self.log("click %s at (%d,%d)" % (described, x, y))

    def click_link(self, label, link):
        """Click one of the links returned by t.links()."""
        x, y = ui.click_range(label, link["start"], link["end"])
        self.log("click link %d %r at (%d,%d), from %s"
                 % (link["index"], link["uri"], x, y, link["source"]))
        return link["uri"]

    def key(self, *keys):
        ui.key(*keys)

    def type_text(self, text):
        ui.type_text(text)

    def menu(self, *path):
        """Walk a menu path, clicking each step.

        The first name is a menu on the menu bar, the rest are items inside it.
        """
        node = self.need(self.frame, role="menu", name=path[0],
                         what="the %r menu" % path[0])
        self.click(node)

        for label in path[1:]:
            node = self.wait(lambda parent=node, want=label: self._menu_item(parent, want),
                             "the %r item under %r" % (label, path[0]))
            self.click(node)

        return node

    def _menu_item(self, parent, label):
        # on_screen, because the items of a menu that has not popped up yet are
        # already in the tree and have no position. Filtering them out here is
        # what turns "the menu is still opening" into another poll rather than
        # into a click at INT_MIN.
        items = a11y.find_all(
            parent,
            pred=lambda n: a11y.role(n) in MENU_ROLES and ui.on_screen(n),
            depth=3)

        exact = [n for n in items if a11y.name(n) == label]
        if exact:
            return exact[0]

        prefixed = [n for n in items if a11y.name(n).startswith(label)]
        if len(prefixed) > 1:
            raise Failed("%r matches several menu items: %s"
                         % (label, ", ".join(sorted(a11y.name(n) for n in prefixed))))

        return prefixed[0] if prefixed else None

    def escape(self):
        """Close whatever popup is open."""
        ui.key("Escape")

    def settle(self, seconds=0.5):
        time.sleep(seconds)

    # -- the outside world -------------------------------------------------

    def urls(self):
        """Every URL medit has asked a browser to open during this test."""
        return browser.urls(self._url_log)

    def wait_url(self, url, timeout=10):
        """Wait until that URL has been handed to a browser."""
        self.wait(lambda: url in self.urls(),
                  "%r to be opened in a browser" % url, timeout)
        self.log("ok: browser opened %s" % url)

    def screenshot(self, name):
        return ui.screenshot(os.path.join(self.log_dir, name))
