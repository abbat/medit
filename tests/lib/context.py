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

    def frames(self):
        """Every main window of the application, in the order the tree lists them.

        self.frame is the one the test started with; this is what a test that
        opens a second window uses to tell them apart. A frame is a window with
        a menu bar and documents in it -- dialogs are toplevels too, and are
        not these.
        """
        return [node for node in a11y.children(self.app)
                if a11y.role_name(node) == "frame"]

    def window_of(self, frame):
        """The X window a frame is drawn in, for the things AT-SPI cannot do."""
        return ui.window_of(frame)

    def place_window(self, frame, x, y, width, height):
        """Put a frame's window somewhere, ask for a size, and say what it got.

        For tests with two windows, which is the only place this is any use.
        Where a window manager puts a second window is its own business --
        xfwm4 cascades it over the first -- and two overlapping windows make a
        click ambiguous and a pixel unreadable, so a test that has two of them
        says where they go rather than hoping.

        The position is waited for and the size is not, because a size is a
        request: a window has a minimum, the window manager clamps to it, and
        the minimum is a property of the theme and the fonts rather than of
        medit -- 680x600 asked for here is 690x634 in the CI container. So the
        extents this returns are what the window actually has, and a test that
        needs the two of them to fit somewhere reads them rather than assuming
        it got what it asked for.

        Waited for rather than assumed even so: the request goes to the window
        manager, which answers when it gets to it, and the coordinates the test
        uses afterwards come from the accessibility tree, which learns the new
        geometry from the toolkit one step further still.
        """
        window = self.window_of(frame)
        ui.resize_window(window, width, height)
        ui.move_window(window, x, y)

        self.wait(lambda: ui.extents(frame)[:2] == (x, y),
                  "%r to be moved to (%d,%d), it is at %s"
                  % (frame.name, x, y, ui.extents(frame)))

        got = ui.extents(frame)
        self.log("%r is at %s, having been asked for %s"
                 % (frame.name, got, (x, y, width, height)))

        return got

    def activate(self, frame):
        """Hand a window to the window manager as the active one.

        What a test with two windows uses before typing into one of them or
        closing it: the keys follow the active window, and so does everything
        the window manager itself does. Only for tests that asked for a window
        manager -- with none there is nothing to ask, and xdotool says so and
        fails rather than quietly leaving the keys where they were.
        """
        window = self.window_of(frame)
        ui.activate_window(window)
        self.log("%r is the active window" % frame.name)

        return window

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

    def attributes(self, node, offset):
        """The text attributes at one character: what the tags there say.

        For anything drawn with a GtkTextTag rather than built out of widgets
        -- a highlighted range, an underlined diagnostic -- which has nothing
        in the accessibility tree of its own.
        """
        return a11y.attributes_of(node, offset)

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

    def click_range(self, node, start, end, times=1, button=1):
        """Click where a range of the node's text is drawn."""
        x, y = ui.click_range(node, start, end, button=button, times=times)
        self.log("click %s%s[%d:%d] at (%d,%d)%s"
                 % (a11y.role_name(node), "" if button == 1 else " (button %d)" % button,
                    start, end, x, y, ", twice" if times == 2 else ""))
        return x, y

    def click_link(self, label, link):
        """Click one of the links returned by t.links()."""
        x, y = ui.click_range(label, link["start"], link["end"])
        self.log("click link %d %r at (%d,%d), from %s"
                 % (link["index"], link["uri"], x, y, link["source"]))
        return link["uri"]

    def drag(self, node, dx, dy, button=1):
        """Drag from the centre of a widget by an offset, and say where it went.

        Everything in medit that is moved rather than clicked goes through
        here: the splitter between a pane and the document, a notebook tab
        being reordered, a pane button carried to another edge of the window.
        """
        x, y = ui.drag_node(node, dx, dy, button=button)
        self.log("drag %s %r by (%+d,%+d), to (%d,%d)"
                 % (a11y.role_name(node), a11y.name(node), dx, dy, x, y))
        return x, y

    def drag_to(self, x0, y0, x1, y1, button=1):
        """Drag between two points the test worked out for itself."""
        ui.drag(x0, y0, x1, y1, button=button)
        self.log("drag (%d,%d) -> (%d,%d)" % (x0, y0, x1, y1))
        return x1, y1

    def extents(self, node):
        """Where the widget is on the screen, as (x, y, width, height)."""
        return ui.extents(node)

    def value(self, node):
        """What a scrollbar or a slider says it is at, as (value, min, max).

        A scroll bar is the one widget whose whole state is a number, and the
        number is the evidence that a view is scrollable at all: a range of
        zero means nothing can be scrolled, whatever the view holds.
        """
        v = node.queryValue()
        return v.currentValue, v.minimumValue, v.maximumValue

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

    def focus(self, frame=None):
        """Point the X input focus back at the application's window.

        There is no window manager, so nothing hands the focus on when a window
        goes away: after a menu is dismissed it belongs to the menu's dead
        window, and the keys that follow reach nobody. t.popup() does this for
        itself; a test that types straight after using a menu has to say so.

        With a window manager and two windows there is a second reason to call
        it: which of them has the keys is then a matter of what was clicked
        last, and a test that means the other one says so with frame=.
        """
        return ui.focus_window(
            window=self.window_of(frame) if frame is not None else None)

    def hover(self, node, start, end):
        """Rest the pointer over a range of the node's text and let it settle."""
        x, y = ui.hover_range(node, start, end)
        self.log("hover %s[%d:%d] at (%d,%d)" % (a11y.role_name(node), start, end, x, y))
        return x, y

    def key(self, *keys):
        ui.key(*keys)

    def type_text(self, text):
        ui.type_text(text)

    def menu(self, *path, frame=None):
        """Walk a menu path, clicking each step.

        The first name is a menu on the menu bar, the rest are items inside it,
        and a step that has a submenu is opened rather than activated. The menu
        bar is the one of self.frame unless a test with two windows says which
        window it means: frame=.
        """
        node = self.need(frame or self.frame, role="menu", name=path[0],
                         what="the %r menu" % path[0])
        self.click(node)

        for depth, label in enumerate(path[1:]):
            parent = node
            node = self.wait(lambda p=parent, want=label: self._menu_item(p, want),
                             "the %r item under %r"
                             % (label, path[depth] if depth else path[0]))
            self.click(node)
            self._open_submenu(node)

        return node

    def _open_submenu(self, item):
        """Pop up the submenu of a menu item that has one.

        Clicking a submenu's parent item is not enough without a window
        manager: the item takes the click, the submenu stays unmapped, and its
        items are in the tree with no position -- so the next step of the path
        finds nothing and waits out its timeout. What opens it is the same key
        a person would use, and GTK's own menu navigation is what handles it.

        An item that has a submenu is the submenu: AT-SPI gives it the role
        "menu" and hangs the items off it directly, where an ordinary item is
        a "menu item" with nothing under it. So the role is the whole test.
        """
        if a11y.role_name(item) != "menu":
            return

        ui.key("Right")

    def popup(self, timeout=a11y.TIMEOUT, frame=None):
        """Open the context menu of whatever has the focus, and return it.

        Shift+F10 and not a right click: a click needs coordinates, and the
        widget the menu belongs to may be one AT-SPI cannot point at. A menu
        pops up in a toplevel window of its own rather than inside the frame,
        which is where this looks for it -- so it finds a menu the menu bar has
        open just as readily, and is meant to be called when none is.
        """
        # The focus first: a menu dismissed earlier took the X input focus
        # into a window that no longer exists, and without a window manager
        # nothing gives it back, so the key below would go nowhere. With two
        # windows the caller says which one it means, since the default -- the
        # last window xdotool lists -- is the other one as often as not.
        self.focus(frame)
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

    def popup_at(self, node, start, end, timeout=a11y.TIMEOUT):
        """Right-click where a range of the node's text is drawn, and take the menu.

        Not the same thing as t.popup(), which asks the focused widget for its
        menu from the keyboard. GtkTextView leaves the cursor where it was on a
        right click, so an entry that goes by the click rather than by the
        cursor can only be told apart from one that does not by opening the menu
        somewhere the cursor is not.
        """
        self.click_range(node, start, end, button=3)

        return self.wait(self._popup_menu, "a context menu", timeout)

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
