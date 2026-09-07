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

_MENU_ROLE_CONSTS = None


class Failed(AssertionError):
    pass


def _menu_role_consts():
    return tuple(a11y.role_const(name) for name in MENU_ROLES)


class Test(object):
    def __init__(self, app, gtk, url_log, log_dir, out, sandbox=None):
        self._started = time.time()
        self.app = app
        self.gtk = int(gtk)
        self.log_dir = log_dir
        # what the test's setup function put in place, see lib/setup.py
        self.sandbox = sandbox
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
        return self._or_dump(lambda: a11y.find(root, **kwargs), described, timeout, root)

    def toplevel(self, name, role="dialog", timeout=a11y.TIMEOUT):
        """A toplevel window of the application, by title."""
        return self._or_dump(
            lambda: a11y.find(self.app, role=role, name=name, depth=2),
            "the %s %s" % (name, role), timeout, self.app)

    def _or_dump(self, fn, what, timeout, root):
        """Wait for something, and print the tree if it never arrives.

        A UI test that says only "did not find the Credits button" is nearly
        undiagnosable at a distance -- the button may be missing, or named
        something else, or the search may have been rooted in the wrong place,
        and the screenshot looks the same in all three cases. The tree
        distinguishes them in one line.
        """
        try:
            return self.wait(fn, what, timeout)
        except a11y.NotFound as missing:
            raise Failed("%s\nthe tree it was looked for in:\n%s"
                         % (missing, a11y.dump(root)))

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

    def state(self, node, name):
        """Whether the node carries the named AT-SPI state."""
        return a11y.state(node, name)

    def wait_text(self, node, needle, timeout=a11y.TIMEOUT, what=None, squeeze=False):
        """Wait until the node's text contains needle, and say what it held.

        For anything that fills in by itself -- a terminal waiting for its
        shell, a view waiting for a file -- where the failure is unreadable
        without the text that did arrive.

        squeeze collapses every run of whitespace to one space before looking,
        which is how a sentence is matched in a terminal: the width of the pane
        decides where the line breaks, so any long enough needle would otherwise
        be split by a newline that depends on the size of the window.
        """
        described = what or "%r" % needle

        def holds():
            text = a11y.text_of(node)
            return needle in (" ".join(text.split()) if squeeze else text)

        try:
            self.wait(holds, described, timeout)
        except a11y.NotFound as missing:
            raise Failed("%s\nthe text it was looked for in:\n%s"
                         % (missing, a11y.text_of(node)))

        self.log("ok: %s appeared" % described)

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
        described = "%s %r" % (a11y.role_name(node), a11y.name(node))
        x, y = ui.click(node, button)
        self.log("click %s at (%d,%d)" % (described, x, y))

    def click_range(self, node, start, end, times=1):
        """Click where a range of the node's text is drawn."""
        x, y = ui.click_range(node, start, end, times=times)
        self.log("click %s[%d:%d] at (%d,%d)%s"
                 % (a11y.role_name(node), start, end, x, y,
                    ", twice" if times == 2 else ""))
        return x, y

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

    def popup(self, timeout=a11y.TIMEOUT):
        """Open the context menu of whatever has the focus, and return it.

        Shift+F10 and not a right click: a click needs coordinates, and the
        widget the menu belongs to may be one AT-SPI cannot point at. A menu
        pops up in a toplevel window of its own rather than inside the frame,
        which is where this looks for it -- so it finds a menu the menu bar has
        open just as readily, and is meant to be called when none is.
        """
        ui.key("shift+F10")
        return self.wait(self._popup_menu, "a context menu", timeout)

    def _popup_menu(self):
        for top in a11y.children(self.app):
            if a11y.role_name(top) != "window":
                continue

            menu = a11y.find(top, role="menu", depth=1)

            if menu is not None and ui.on_screen(menu):
                return menu

        return None

    def item(self, menu, label, timeout=a11y.TIMEOUT):
        """One item of a menu that is already open."""
        return self._or_dump(lambda: self._menu_item(menu, label),
                             "the %r item" % label, timeout, menu)

    def _menu_item(self, parent, label):
        global _MENU_ROLE_CONSTS

        if _MENU_ROLE_CONSTS is None:
            _MENU_ROLE_CONSTS = _menu_role_consts()

        # on_screen, because the items of a menu that has not popped up yet are
        # already in the tree and have no position. Filtering them out here is
        # what turns "the menu is still opening" into another poll rather than
        # into a click at INT_MIN.
        items = a11y.find_all(
            parent,
            pred=lambda n: a11y.role(n) in _MENU_ROLE_CONSTS and ui.on_screen(n),
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
