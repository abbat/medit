"""The pointer over the text is still there after a key has changed the text.

# requires: MOO_GTK3

GTK+3's text view hides the pointer when a key it handles itself changes the
buffer -- an ordinary character, typed -- and keeps the fact in a flag of its
own. Only its own motion handler clears it, and medit's replaced that handler
without chaining up, so medit went on believing its text cursor was up while
the window had none: the pointer stayed invisible over the text until a focus
change put a cursor back.

What the X server shows is read through XFixes, which gives the image of the
cursor the pointer has now, whoever set it. The image over the text before any
typing is what it has to be again once the pointer moves after typing.
"""

import ctypes
import ctypes.util
import hashlib

CONTENT = "alpha beta\ngamma delta\n"


class _CursorImage(ctypes.Structure):
    _fields_ = [("x", ctypes.c_short), ("y", ctypes.c_short),
                ("width", ctypes.c_ushort), ("height", ctypes.c_ushort),
                ("xhot", ctypes.c_ushort), ("yhot", ctypes.c_ushort),
                ("cursor_serial", ctypes.c_ulong),
                ("pixels", ctypes.POINTER(ctypes.c_ulong)),
                ("atom", ctypes.c_ulong), ("name", ctypes.c_char_p)]


def pointer_image():
    """What the pointer looks like now: a digest of its size, hot spot and pixels."""
    x11 = ctypes.CDLL(ctypes.util.find_library("X11") or "libX11.so.6")
    xfixes = ctypes.CDLL(ctypes.util.find_library("Xfixes") or "libXfixes.so.3")
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    x11.XFree.argtypes = [ctypes.c_void_p]
    xfixes.XFixesGetCursorImage.restype = ctypes.POINTER(_CursorImage)
    xfixes.XFixesGetCursorImage.argtypes = [ctypes.c_void_p]

    display = x11.XOpenDisplay(None)
    if not display:
        raise RuntimeError("cannot open the X display")
    try:
        image = xfixes.XFixesGetCursorImage(display)
        if not image:
            raise RuntimeError("XFixesGetCursorImage failed")
        c = image.contents
        pixels = [c.pixels[i] & 0xffffffff for i in range(c.width * c.height)]
        shape = (c.width, c.height, c.xhot, c.yhot, pixels)
        x11.XFree(image)
        return hashlib.sha1(repr(shape).encode()).hexdigest()
    finally:
        x11.XCloseDisplay(display)


def setup(s):
    s.open(s.write("workdir/hello.txt", CONTENT))


def run(t):
    view = t.document()

    t.click_range(view, 1, 2)
    t.hover(view, 13, 14)
    before = pointer_image()

    t.type_text("x")
    t.wait_text(view, "x", what="the typed character in the document")

    # Saved, as it was when the pointer was first seen gone -- the save only
    # made it noticeable, but it also lets the application quit without asking.
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")

    t.hover(view, 19, 20)
    t.check(pointer_image() == before,
            "the pointer moved over the text after typing looks as it did before")
