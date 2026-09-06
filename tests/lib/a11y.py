"""Reading the accessibility tree.

Everything a test asserts is read through AT-SPI: the widget hierarchy, the
names, the text, the hyperlinks. Nothing is read from a screenshot, so a test
says "the Credits button is there" rather than "these pixels are there", and it
says the same thing for both toolkits -- GTK+2 through libgail and GTK+3
natively expose the same tree.

Input is the exception, see input.py.
"""

import re
import time

import pyatspi


TIMEOUT = 20
INTERVAL = 0.2


class NotFound(AssertionError):
    pass


def wait(fn, what, timeout=TIMEOUT, interval=INTERVAL):
    """Poll fn until it returns something truthy, or fail naming what was missed.

    Every lookup goes through this. The tree is built by another process while
    the toolkit is still laying widgets out, so a plain search races: it finds
    an empty dialog, or no dialog at all, depending on how fast the machine is.
    """
    deadline = time.time() + timeout
    last = None

    while True:
        try:
            last = fn()
        except Exception:
            last = None

        if last:
            return last

        if time.time() >= deadline:
            raise NotFound("timed out after %gs waiting for %s" % (timeout, what))

        time.sleep(interval)


def children(node):
    """The children of a node, skipping any that disappear while we look."""
    out = []
    try:
        count = node.childCount
    except Exception:
        return out

    for i in range(count):
        try:
            child = node[i]
        except Exception:
            continue
        if child is not None:
            out.append(child)

    return out


def role(node):
    try:
        return node.getRoleName()
    except Exception:
        return ""


def name(node):
    try:
        return node.name or ""
    except Exception:
        return ""


def _matches(node, want_role, want_name, name_prefix, pred):
    if want_role is not None and role(node) != want_role:
        return False
    if want_name is not None and name(node) != want_name:
        return False
    if name_prefix is not None and not name(node).startswith(name_prefix):
        return False
    if pred is not None and not pred(node):
        return False
    return True


def find_all(root, role=None, name=None, name_prefix=None, pred=None, depth=16):
    """Breadth-first search of the subtree, root itself excluded."""
    out = []
    queue = [(root, 0)]

    while queue:
        node, level = queue.pop(0)

        if node is not root and _matches(node, role, name, name_prefix, pred):
            out.append(node)

        if level < depth:
            queue += [(child, level + 1) for child in children(node)]

    return out


def find(root, **kwargs):
    """The first match, or None. Pass it to wait() to make it a requirement."""
    found = find_all(root, **kwargs)
    return found[0] if found else None


_name_of = name


def application(name="medit", timeout=TIMEOUT):
    def look():
        for app in pyatspi.Registry.getDesktop(0):
            if app is not None and _name_of(app) == name:
                return app
        return None

    return wait(look, "the %s application to appear on the a11y bus" % name, timeout)


def text_of(node):
    """The text of a node, through the Text interface or the name."""
    try:
        text = node.queryText()
        return text.getText(0, text.characterCount)
    except NotImplementedError:
        return name(node)
    except Exception:
        return ""


# A bare URL in a label's text, for the GTK+2 fallback below. The trailing
# class excludes the punctuation a sentence puts after a URL.
_URL = re.compile(r"https?://[^\s<>]+?(?=[\s<>]|[.,;:)]?$)", re.MULTILINE)


def links_of(node):
    """The hyperlinks in a label, as dicts of index, uri, start, end and source.

    A link inside a GtkLabel is not a widget: it has no Component interface and
    therefore no extents of its own, which is why it cannot be found by looking
    for something clickable. It is a range of the label's text, and its position
    on screen comes from the label's Text interface.

    On GTK+3 the range and the href come from AtkHypertext. GTK+2 has neither:
    gail's label accessible implements AtkText but not AtkHypertext, so the same
    dialog reports zero links there even though the links are on screen and
    clickable. The fallback finds the URLs in the label's own text instead,
    which works because the labels medit puts links in show the address as the
    link text -- and it makes the test a little stricter on GTK+2 than on GTK+3,
    since it then also proves that what the label shows is where it goes.
    """
    try:
        hypertext = node.queryHypertext()
    except Exception:
        hypertext = None

    if hypertext is not None:
        out = []
        for i in range(hypertext.getNLinks()):
            link = hypertext.getLink(i)
            out.append({
                "index": i,
                "uri": link.getURI(0),
                "start": link.startIndex,
                "end": link.endIndex,
                "source": "hypertext",
            })
        return out

    text = text_of(node)
    return [
        {
            "index": i,
            "uri": match.group(0),
            "start": match.start(),
            "end": match.end(),
            "source": "text",
        }
        for i, match in enumerate(_URL.finditer(text))
    ]


def dump(node, depth=0, maxdepth=8, out=None):
    """The subtree as indented text, for a failure message or a log."""
    if out is None:
        out = []

    out.append("%s%s: %r" % ("  " * depth, role(node) or "?", name(node)))

    if depth < maxdepth:
        for child in children(node):
            dump(child, depth + 1, maxdepth, out)

    return "\n".join(out)
