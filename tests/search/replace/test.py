"""Replace does all of them at once, or asks about each one.

# requires: MOO_GTK3

moo_text_view_run_replace() has two halves and the "Don't prompt on replace" box
chooses between them: run_replace_silent() replaces every match and says how many
it did, and run_replace_interactive() puts up the "Replace?" prompt for each one,
with Replace, Replace All, Find Next and Stop.

Both are driven here, and the count in the status bar is read as well as the text
in the document: a replace that did two of three and said three would be a bug
nobody would notice from the buffer alone.

The prompt half is the more interesting one, because it is stateful -- the answer
to one match decides whether the next is even offered -- so the test replaces one
and then stops, and expects exactly one word to have changed.
"""

CONTENT = "alpha one\nalpha two\nalpha three\n"

ALL_OMEGA = "omega one\nomega two\nomega three\n"

ONE_BACK = "alpha one\nomega two\nomega three\n"

DONT_PROMPT = "Don't prompt on replace"


def setup(s):
    s.open(s.write("workdir/words.txt", CONTENT))


def run(t):
    view = t.document()

    # Every one of them, in one go.
    replace(t, "alpha", "omega", prompt=False)

    t.wait(lambda: t.text(view) == ALL_OMEGA,
           "every match to be replaced; the document holds %r" % t.text(view))
    t.log("ok: with the prompt off, Replace does all of them")

    t.check("3 replacements made" in said(t),
            "and says how many it did: %r" % said(t))

    # And back the other way, one at a time, stopping after the first.
    replace(t, "omega", "alpha", prompt=True)

    prompt = t.wait(lambda: t.find(t.app, role="dialog", name="Replace?", depth=2),
                    "the prompt to ask about the first match")
    t.click(t.button(prompt, "Replace"))

    prompt = t.wait(lambda: t.find(t.app, role="dialog", name="Replace?", depth=2),
                    "the prompt to come back for the second match")
    t.click(t.button(prompt, "Stop"))

    t.wait(lambda: t.text(view) == ONE_BACK,
           "one match to have been replaced and the rest left alone; the document "
           "holds %r" % t.text(view))
    t.log("ok: with the prompt on, Stop leaves the rest of them alone")

    # Saved, or the quit at the end of the test turns into a dialog about it.
    t.focus()
    t.key("ctrl+s")
    t.wait(lambda: "[modified]" not in (t.frame.name or ""), "the document to be saved")


def replace(t, term, replacement, prompt):
    """Open Replace, fill both entries, set the prompt box, and accept."""
    t.menu("Search", "Find and Replace")

    dialog = t.wait(lambda: the_dialog(t), "the Replace dialog")

    entries = t.on_screen(t.find_all(dialog, role="text", depth=8))

    t.check(len(entries) == 2,
            "the dialog has the two entries: what to find and what to put there")

    for entry, text in zip(entries, (term, replacement)):
        t.click(entry)
        t.key("ctrl+a")
        t.type_text(text)

    box = t.need(dialog, role="check box", name=DONT_PROMPT, what="the prompt box")

    if t.state(box, "checked") == prompt:
        t.click(box)
        t.wait(lambda: t.state(box, "checked") != prompt,
               "the prompt box to be %s" % ("clear" if prompt else "ticked"))

    t.key("Return")
    t.wait(lambda: the_dialog(t) is None, "the Replace dialog to close")


def the_dialog(t):
    """The Replace dialog, which is the Find dialog with more in it: found by the
    box only the replacing one has."""
    for top in t.find_all(t.app, role="dialog", depth=2):
        if t.find(top, role="check box", name=DONT_PROMPT) is not None:
            return top

    return None


def said(t):
    """What the status bar is showing."""
    bar = t.find(t.frame, role="status bar", depth=25)

    return "" if bar is None else "%s %s" % (bar.name or "", t.text(bar))
