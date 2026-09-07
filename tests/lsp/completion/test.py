"""The completion popup: what the server offers, narrowed as the user types.

# requires: MOO_BUILD_LSP

medit has no completion of its own, so the popup is a window of the plugin's
own with a list in it, placed under the cursor, taking Up, Down, Enter and
Escape before the text view sees them. It is a toplevel of the application
rather than anything inside the window, which is where this looks for it.

Both toolkits: nothing here is a pane, and the popup is a plain GtkWindow with
a GtkTreeView in it, which gail describes as readily as GTK+3 does.

Ctrl+Space is the other half of the reason this is worth a test.
moo_window_key_press_event() hands a key to the focused widget before it tries
the accelerators, so the text view swallows it and the action never fires; the
plugin matches the accelerator by hand, and only a test that presses the keys
can say whether that still works.
"""

CONTENT = "alpha\n"

ITEMS = [
    {"label": "alphabet", "insertText": "alphabet"},
    {"label": "alphanumeric", "insertText": "alphanumeric"},
    {"label": "zebra", "insertText": "zebra"},
]


def setup(s):
    s.plugin("Lsp")
    s.open(s.write("workdir/hello.txt", CONTENT))
    s.lsp_server(filter="globs:*.txt",
                 replies={"textDocument/completion": {"isIncomplete": False,
                                                      "items": ITEMS}})


def run(t):
    t.wait_lsp("textDocument/didOpen")

    # At the end of the word, which is the prefix the list is narrowed by: the
    # server offers zebra as well, and the client never shows it. The end of
    # the line rather than a word motion, since what the cursor has behind it
    # is the whole of what current_prefix() answers with.
    t.focus()
    t.key("ctrl+Home")
    t.key("End")
    t.key("ctrl+space")

    t.wait_lsp("textDocument/completion")
    popup = t.wait(lambda: completion_popup(t), "the completion popup")

    t.wait(lambda: offered(t, popup) == ["alphabet", "alphanumeric"],
           "the two words that start with what is typed; it offers %s"
           % offered(t, popup))
    t.log("ok: the popup offers %s" % ", ".join(offered(t, popup)))

    # Typing narrows what is already open, without asking the server again.
    asked = len(t.lsp("textDocument/completion"))
    t.type_text("b")

    t.wait(lambda: offered(t, popup) == ["alphabet"],
           "the list to narrow to what still matches")
    t.check(len(t.lsp("textDocument/completion")) == asked,
            "and narrowing did not ask the server again")

    t.key("Return")

    t.wait(lambda: completion_popup(t) is None, "the popup to close on Enter")

    t.key("ctrl+s")
    t.wait(lambda: t.sandbox.read("workdir/hello.txt") == "alphabet\n",
           "the word the popup put in to reach the document")
    t.log("ok: Enter inserts the selected word in place of what was typed")

    # And Escape leaves the document alone.
    t.key("ctrl+Home")
    t.key("End")
    t.key("ctrl+space")

    t.wait(lambda: completion_popup(t), "the popup again")
    t.key("Escape")

    t.wait(lambda: completion_popup(t) is None, "the popup to close on Escape")
    t.check(t.sandbox.read("workdir/hello.txt") == "alphabet\n",
            "and Escape put nothing in the document")


def completion_popup(t):
    """The popup window, which is a toplevel of the application of its own.

    A window with a list in it: the menus are windows too, and a test that
    matched on the role alone would find whichever menu was last open.
    """
    for top in t.find_all(t.app, role="window", depth=1):
        # On screen, not merely in the tree: a popup that has been closed is
        # hidden rather than destroyed, and its accessible outlives it.
        tables = t.on_screen(t.find_all(top, role="table", depth=3))

        if tables:
            return tables[0]

    return None


def offered(t, table):
    return [cell.name for cell in t.find_all(table, role="table cell", depth=2)
            if cell.name]
