"""The main menu: items that have an icon show it.

An item of the menu bar is a MooAction proxy, and the icon is the action's
icon-name. When the stock ids went away the proxy stopped drawing it, so the
menu came up as plain text while the toolbar, built from the same actions, kept
its icons. Paste and Increase Indent have an icon in the toolbar or the theme, so
the menu item has to have one too.

The accessibility tree does not list the image of a menu item, so this reads the
screen: the text of a menu is grey, and an icon of these two has colour in it, so
a pixel with colour in the strip at the left of the item is the icon.
"""

ITEMS = (("Edit", "Paste"), ("Edit", "Increase Indent"),
         # its icon name was one the theme does not have, so it drew nothing
         ("Document", "Toggle Bookmark"))

# How far apart the channels of a pixel are, at least, for it to be colour and
# not the grey of text or of the menu.
COLOURED = 60


def coloured(rgb):
    channels = [int(rgb[i:i + 2], 16) for i in (1, 3, 5)]
    return max(channels) - min(channels) >= COLOURED


def run(t):
    for title, name in ITEMS:
        menu = t.need(t.frame, role="menu", name=title, what="the %s menu" % title)
        t.click(menu)
        item = t.wait(lambda m=menu, n=name: t._menu_item(m, n),
                      "the %r item of the %s menu" % (name, title))
        x, y, w, h = t.extents(item)

        found = [t.pixel(px, py)
                 for px in range(x + 28, x + 58, 3)
                 for py in range(y + 4, y + h - 4, 3)]
        t.check(any(coloured(c) for c in found),
                "the %r item at %r has an icon at its left: %s"
                % (name, (x, y, w, h), sorted(set(found))))

        t.key("Escape")
