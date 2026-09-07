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

import json
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

    def preferences(self, page, timeout=a11y.TIMEOUT):
        """Open Edit/Preferences at one of its pages, and return the dialog.

        The pages are a list beside a notebook, so the page is picked by its
        row rather than by a tab; the caller gets the dialog, since everything
        it will want -- the widgets of the page, and the dialog's own buttons --
        hangs off that.
        """
        self.menu("Edit", "Preferences")
        dialog = self.dialog("Preferences", timeout)

        self.click(self.need(dialog, role="table cell", name=page,
                             what="the %s row of the preferences" % page))

        return dialog

    def on_screen(self, nodes):
        """Those of the nodes that are drawn somewhere.

        A dialog holds every page of it at once, and the widgets of the pages
        that are not shown are in the tree with no position -- so anything
        looked up across a dialog has to be filtered by this or it will find
        the same widget on four other pages.
        """
        return [node for node in nodes if ui.on_screen(node)]

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

    def pin_pane(self):
        """Make the open pane sticky, so that it stays open when it loses focus.

        A pane hides itself the moment the document takes the focus back, which
        is what the panes are for and is also why a test that wants to watch one
        while typing has to pin it first. The button is the one in the pane's own
        toolbar; it carries no name, only the tooltip the description comes from.
        """
        button = self.need(
            self.frame, role="toggle button", depth=30,
            pred=lambda node: node.description == "Sticky" and ui.on_screen(node),
            what="the Sticky button of the open pane")

        if not self.state(button, "checked"):
            self.click(button)
            self.wait(lambda: self.state(button, "checked"), "the pane to be pinned")

        self.log("ok: the pane is pinned open")

    def focus(self):
        """Point the X input focus back at the application's window.

        There is no window manager, so nothing hands the focus on when a window
        goes away: after a menu is dismissed it belongs to the menu's dead
        window, and the keys that follow reach nobody. t.popup() does this for
        itself; a test that types straight after using a menu has to say so.
        """
        ui.focus_window()

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
        # The focus first: a menu dismissed earlier took the X input focus
        # into a window that no longer exists, and without a window manager
        # nothing gives it back, so the key below would go nowhere.
        ui.focus_window()
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

    # -- what medit said to a language server ------------------------------

    def lsp(self, method=None, server="test"):
        """Every message medit has sent that server, oldest first.

        The other half of what an LSP test can assert. Half of the protocol
        never reaches the screen -- a document announced, a change sent after
        the quiet the preferences ask for, a document closed -- and the log the
        fake server keeps is the only place it can be seen.
        """
        messages = []

        try:
            with open(self.sandbox.lsp_log_path(server), errors="replace") as f:
                lines = f.readlines()
        except FileNotFoundError:
            return messages

        for line in lines:
            line = line.strip()

            if not line:
                continue

            try:
                message = json.loads(line)
            except ValueError:
                # The last line of a log being written while it is read.
                continue

            if method is None or message.get("method") == method:
                messages.append(message)

        return messages

    def wait_lsp(self, method, server="test", timeout=a11y.TIMEOUT, count=1):
        """Wait until medit has sent that message, and return the last of them."""
        described = "%s to reach the %s server" % (method, server)

        def arrived():
            found = self.lsp(method, server)
            return found[-1] if len(found) >= count else None

        try:
            message = self.wait(arrived, described, timeout)
        except a11y.NotFound as missing:
            raise Failed("%s\nwhat did reach it:\n    %s"
                         % (missing, "\n    ".join(self.lsp_methods(server)) or "nothing"))

        self.log("ok: medit sent %s" % method)

        return message

    def lsp_methods(self, server="test"):
        """The methods medit has sent, in order, for a failure to print."""
        return [m.get("method") or "(reply)" for m in self.lsp(None, server)]

    def lsp_starts(self, server="test"):
        """How many times that server's process has been started."""
        return len(self.sandbox.read_path(self.sandbox.lsp_starts_path(server)))

    def medit_log(self):
        """Everything medit has printed, which is where its own diagnosis is."""
        try:
            with open(os.path.join(self.log_dir, "medit.log"), errors="replace") as f:
                return f.read()
        except FileNotFoundError:
            return ""

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
