"""The popup opens on the server's trigger character, unless it was turned off.

# requires: MOO_BUILD_LSP

Two things at once, because they are two ends of the same path: the character
the server named in its capabilities brings the popup up by itself, and the
preference switches the whole of it -- the trigger and the accelerator both --
without a restart.
"""

CONTENT = "obj\n"

ITEMS = [{"label": "field", "insertText": "field"},
         {"label": "method", "insertText": "method"}]


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))
    s.lsp_server(filter="globs:*.txt",
                 capabilities={"completionProvider": {"triggerCharacters": ["."]}},
                 replies={"textDocument/completion": {"isIncomplete": False,
                                                      "items": ITEMS}})


def run(t):
    t.wait_lsp("textDocument/didOpen")

    t.focus()
    t.key("ctrl+Home")
    t.key("End")
    t.type_text(".")

    t.wait_lsp("textDocument/completion")
    popup = t.wait(lambda: completion_popup(t), "the popup the trigger character opened")

    t.check(offered(t, popup) == ["field", "method"],
            "everything the server offers is listed after the trigger character")

    t.key("Escape")
    t.wait(lambda: completion_popup(t) is None, "the popup to close")

    switch_off(t)

    asked = len(t.lsp("textDocument/completion"))

    t.focus()
    t.type_text(".")
    t.key("ctrl+space")
    t.settle(2)

    t.check(completion_popup(t) is None,
            "with completion switched off, neither the trigger nor Ctrl+Space opens it")
    t.check(len(t.lsp("textDocument/completion")) == asked,
            "and the server was not asked what could go there")

    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")


def switch_off(t):
    """Untick "Complete words" on the client's own page of the preferences."""
    dialog = t.preferences("Language Servers")

    box = t.need(dialog, role="check box", name="Complete words",
                 what="the completion check box")

    t.check(t.state(box, "checked"), "completion is on until it is turned off")

    t.click(box)
    t.click(t.button(dialog, "OK"))
    t.no_toplevel("Preferences")


def completion_popup(t):
    for top in t.find_all(t.app, role="window", depth=1):
        tables = t.on_screen(t.find_all(top, role="table", depth=3))

        if tables:
            return tables[0]

    return None


def offered(t, table):
    return [cell.name for cell in t.find_all(table, role="table cell", depth=2)
            if cell.name]
