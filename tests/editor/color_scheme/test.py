"""The colour scheme paints the text view: text, selection and line numbers.

The scheme gives these to the widget rather than to a text tag, which is
_gtk_source_style_scheme_apply()'s part: the "text" style is the background of
the text window, "selection" the background of selected text, and
"line-numbers" the background of the gutter. cobalt is used because none of
its colours is anything a theme would give by itself.

The pixels are read where nothing but the background is drawn: below the
text, between spaces, and at the gutter's outer edge.
"""

TEXT_BG = "#001b33"          # cobalt.xml: text, background="dark_blue"
SELECTION_BG = "#0088ff"     # selection, background="sky_blue"
LINE_NUMBERS_BG = "#000d1a"  # line-numbers, background="black_blue"


def setup(s):
    # The sandbox holds no installed data, so the scheme is copied in.
    s.copy_data("language-specs/cobalt.xml", "src/mooedit/langs/cobalt.xml")
    s.pref("Editor/color_scheme", "cobalt")
    s.pref("Editor/show_line_numbers", True)
    s.pref("Editor/highlight_current_line", False)


def run(t):
    t.menu("File", "New")
    view = t.document()
    t.click(view)
    # Spaces: a selection over them is the selection background and nothing
    # else, with no glyph to land on.
    t.type_text(" " * 20)

    view_x, view_y, view_width, view_height = t.extents(view)
    below = view_y + view_height - 10

    t.wait(lambda: t.pixel(view_x + view_width - 10, below) == TEXT_BG,
           "the text window to be painted in the scheme's background %s, "
           "not %s" % (TEXT_BG, t.pixel(view_x + view_width - 10, below)))
    t.log("ok: text background is %s" % TEXT_BG)

    t.check(t.pixel(view_x + 1, below) == LINE_NUMBERS_BG,
            "the line-number gutter is %s, not %s"
            % (LINE_NUMBERS_BG, t.pixel(view_x + 1, below)))
    t.log("ok: line-number gutter is %s" % LINE_NUMBERS_BG)

    t.key("ctrl+a")
    x, y, width, height = t.range_extents(view, 0, 20)
    middle = (x + width // 2, y + height // 2)
    t.wait(lambda: t.pixel(*middle) == SELECTION_BG,
           "the selection to be painted in %s, not %s"
           % (SELECTION_BG, t.pixel(*middle)))
    t.log("ok: selection background is %s" % SELECTION_BG)

    t.key("ctrl+w")
    dialog = t.need(t.app, role="alert", depth=2,
                    what="the dialog asking about the unsaved new document")
    t.click(t.button(dialog, "Discard"))
