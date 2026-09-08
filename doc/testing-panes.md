# What the pane tests know

*For agents working in this tree. Read `AGENTS.md` and `doc/testing.md` first: this
file assumes the harness those describe.*

## What the terminal tests know

`tests/terminal/` drives the pane through the shell running in it. GTK+3 only, so every
test there carries `# requires: MOO_BUILD_TERMINAL`.

**vte's accessible is the oracle**: `VteTerminal` implements `AtkText`, so
`t.text(terminal)` is the screen. A word on it is no more a widget than a link in a label,
so `t.click_range(terminal, start, end, times=2)` double-clicks where those characters are
drawn — that is how the copy test selects one.

**Pin the shell.** `Plugins/Terminal/shell` is read when the pane is first shown, so
`setup()` sets it; otherwise a test runs the login shell of whoever runs it (fish here,
root's bash in CI). `/bin/sh` where only the answer matters, a script that appends a byte
and `exec`s a shell where starts must be counted, a script that exits at once where the
test is about a shell that fails.

**Nothing may depend on the prompt** — root gets `#` where a developer gets `$`. Wait for
any non-empty text, then for an answer whose echo cannot be mistaken for it:
`echo ready$((21*2))` prints `ready42`.

**The pushd item is offered by the shell's name** (`basename == "bash"`), there being no
way to ask a shell what it supports. The context menu test uses that: its shell is a script
called `bash`, which also starts the real bash outside the document's directory, so "cd
went there" is a change rather than a coincidence.

**Two windows needed a window manager, and now have one.** Without one the second toplevel
lands on the first, at the same coordinates, and a click goes to whichever X stacked on
top; `tests/terminal/two_windows` therefore carries `NEEDS_WM = True`, places the two
windows side by side itself (`t.place_window()` — where a window manager puts a second
window is its own business, and xfwm4 cascades it over the first), and is the only test
that does. What it is for is `terminal_panes`, the plugin's static list of live panes:
picking a colour scheme in one window repaints the terminal in the other, and closing a
window takes its pane off the list while the pane that is left keeps working — a
use-after-free when it goes wrong, with the sanitizer watching.

It also runs two things nothing else here reaches. `_moo_get_top_window()` reads
`_NET_CLIENT_LIST_STACKING` off the root window, which only a window manager sets: with
none, a second window makes it print `Moo-CRITICAL: !nitems_return` and fall back on the
first window it knows, so the branch that picks the top window has never run in these
tests. And a window closed with `Alt+F4` is closed through `WM_DELETE_WINDOW`, which is
the only way medit's own UI closes a window at all — there is no menu item for it.

**A colour is the one thing read off the screen** — nothing in the tree says what colour
anything is drawn in. `input.pixel()` reads a pixel of the corner the shell never writes
in, so the scheme test asserts a repaint rather than a stored setting. `import` on
ImageMagick 6, `magick import` on 7; both are tried.

**The pane's deprecations are ours**, counted on debian 13 where `G_ENABLE_DIAGNOSTIC=1`
is loudest: `VteTerminal::window-title-changed` twice per run (deprecated in vte 0.68, and
vte 0.80 has moved the whole window-title API to `vtedeprecated.h` — the replacement,
termprops, needs 0.78, so fixing it means a version branch);
`GtkImageMenuItem:use-stock` and `:accel-group` from `gtk_image_menu_item_new_from_stock()`
in `create_popup_menu()`, whose replacement drops the icons. `GtkFontButton:font-name` was
the third and is fixed. None of them fails a test, as criticals do not.

## What the LSP tests know

`tests/lsp/` drives the language server client against a server the harness carries,
`tests/lib/fake_lsp.py`. A test about the client cannot be a test of clangd as well: what
a real server answers depends on its version, on the index it built and on the machine it
runs on, and the CI container has no server at all.

**Both toolkits**, unlike the terminal — the client is compiled for GTK+2 too, and only
the tests that need the document's own accessible or a pane's carry
`# requires: MOO_GTK3`: clicking a particular word, reading the text attributes of one,
looking inside a pane. Everything that can be asserted from the server's log, from a
dialog, from a popup window or from the bytes of a saved file runs on both. All of them carry `# requires: MOO_BUILD_LSP`, which is off where
json-glib is missing.

**The client is off until it is asked for.** It runs other people's programs, so it
registers itself disabled; every test says `s.plugin("Lsp")` except `plugin_toggle`,
which drives the toggle in Preferences → Plugins the way a user would. That toggle's cell
exposes no checked state over AT-SPI, and it takes effect on Apply rather than on the
click, so what it did is read from what followed: a process, the items in the Tools menu,
the shutdown reaching the server.

**`lsp.xml` is read once**, by `lsp_manager_init()`, when the plugin is switched on — so
`s.lsp_server()` writes it in `setup()`, as the terminal writes its shell. The two things
that make medit read it again are `Tools → Restart Language Servers` and the button on the
preferences page, and both are covered.

**Half of the protocol never reaches the screen**, and there the server's own log is the
oracle: `t.lsp(method)`, `t.wait_lsp(method)`, `t.lsp_starts()`. A document announced to a
server looks exactly like one that was not; a burst of typing coalesced into one
`didChange` looks exactly like six of them; the position a context-menu entry asks about
is invisible by construction. The scenario is re-read before every message, so the same
question can get a different answer without restarting anything, and every record carries
the pid — which is how `root_markers` tells two servers sharing one log apart.

**Activating a line in the diagnostics pane closes it**, which is the general pane
behaviour above arriving where it costs most: the click hands the focus to the document,
so the next click at coordinates taken from the pane lands in the text. One click per
opening, or `t.pin_pane()` — which is what the symbol tree needs, since it asks the server
for nothing while it is not mapped.

**The completion popup is a toplevel window of its own**, so it is looked for among the
application's children rather than inside the frame, and *on screen* rather than merely in
the tree: a closed popup is hidden, not destroyed, and its accessible outlives it.

**What is offered depends on the prefix under the cursor**, not only on what the server
sent. `Ctrl+Space` after `alpha` narrows the reply to the words that start with it; the
same key after a space offers everything. A test that means to assert the narrowing has to
put the cursor at the end of a word — `End`, not a word motion, which lands *before* the
next one.

**A tooltip is not on the accessibility bus.** Measured on GTK+3: the pointer resting on a
word produces the `textDocument/hover` requests and no node with the "tool tip" role
anywhere in the tree. So the hover test asserts the question and the preference that gates
it, and says so rather than pretending to check the answer.

**The debug log names the program, not the entry.** `Plugins/Lsp/debug` (or
`MEDIT_LSP_DEBUG`) prints every message as `lsp: <argv[0]> <- {...}` — the interpreter or
the wrapper, never the `id` from `lsp.xml`. It is also read where a server is *started*,
so ticking it does nothing until the servers are restarted.

**A capability is not always named after its method.** `lsp_can_ask()` derives one from
the other — `textDocument/definition` asks about `definitionProvider` — and that holds for
every question the client asks except formatting, whose capability is
`documentFormattingProvider` and not `formattingProvider`. The symptom is a menu item that
does nothing and a test that times out waiting for a request that was never sent; the fix
is to ask `lsp_server_has_provider()` for the name itself, which is what `lsp_format()`
does and says why.

**A tag is not in the accessibility tree, but it is in the text attributes.** A highlighted
range, an underlined diagnostic: none of them is a widget, has a name or has a position, so
`t.attributes(view, offset)` reads what the tags at one character say about it instead —
which is the only evidence short of reading pixels. Two things about it, both measured.
The defaults have to be left out (`getAttributeRun(offset, False)`), or at-spi answers with
the colours of the widget itself and every character in the document has a background. And
the value is useless on GTK+3: the colour reported for any tag at all is `0,0,0`, so a test
can say a character is marked and not how. Hence `highlight`, which asserts that the two
uses are marked and the word between them is not, while which of the two marks a *write*
gets is a name compared in `lsp-tests.cpp`.

**The signature popup is a window with a label in it**, looked for among the application's
toplevels the way the completion popup is — and a menu is a toplevel window too, whose
items gail describes with labels of their own, so the search skips any window with a menu
anywhere in it. What the popup says is asserted from the text: the signature, and under it
the documentation of the parameter being typed, which is what changes when the server moves
`activeParameter` along. That the parameter is emboldened *inside* the signature is a
markup string in `lsp-tests.cpp`, since pango markup does not survive into the
accessibility tree.

**The server ends that popup, not the client.** While it is up every keystroke asks again
and an empty answer is what closes it: the alternative is a client guessing where a call
ends, and a popup describing a call that stopped being typed three lines ago. With nothing
open it is the other way round — only the characters the server named in
`signatureHelpProvider.triggerCharacters` open it, or every key pressed in a document is a
request. `signature_help` asserts both halves, which is the whole of the policy.

**Renaming and formatting are the two things here that write**, and they are one file
(`lsp-edits.cpp`) because the reply is the same thing: a rename answers with edits to
several files, `textDocument/formatting` with edits to one, and applying either is the same
three rules. The edits of a file go in **back to front**, or the first replacement moves the
ranges of the ones after it and the second lands in the wrong place; a file that is not open
is opened rather than written behind the user's back; and a file's edits are one undo step,
not one per range. Nothing is saved, deliberately -- a rename reaching files the user never
chose is exactly what should be looked at before it is on disk -- so `rename` saves each
document by hand and asserts on the bytes in the sandbox (`t.sandbox.read()`), which is also
what makes it a test both toolkits run: nothing in it reads the document's text out of the
accessibility tree.

`formatting` is the same test one size smaller, plus the half that is its own: what medit
*sends*. A formatter told nothing about the editor's settings undoes them, so the request
carries the document's own indent width and tabs-or-spaces, and the two options that say
what medit does when it saves. The reply shapes are cheaper to cover in `lsp-tests.cpp` than
through the UI: a WorkspaceEdit written as `changes` and one written as `documentChanges`
are one scenario each in a UI run and both in a millisecond there, and so is the ordering.

**The References pane reads the files it lists.** A reply is positions in files, and a
position on its own says nothing to read; the line comes from the open document when the
file is open and off the disk when it is not — which is also how the column gets out of the
server's UTF-16 counting and into the character medit counts everywhere else. The path is
relative to the project root that server was started for. So `references` has a file it
never opens and asserts the line that came back from it; GTK+3 only, being a pane.

**A message dialog cannot be looked up by name.** `moo_error_dialog()` produces a
GtkMessageDialog, which has no title at all and whose role is `alert` rather than `dialog`
— both toolkits, measured — so `t.dialog("...")` finds nothing. The rename test looks for
the text of its labels among the toplevels with that role instead. It is worth having: a
server that refuses a rename has to say so, and a client that swallowed the refusal would
leave the user with a dialog they filled in and a document nothing happened to.

Three things the tests found, all of them the client's rather than the harness's, and all
three fixed here — with the test that failed first written down beside each:

* **`lsp_server_get_error()` had no caller.** `set_failed()` has always written a sentence
  saying what went wrong; nothing read it back, so the client went quiet and an editor
  that never mentions language servers again looks exactly like one with none configured.
  It is said twice now, the way the terminal says it: on medit's own output and in the
  diagnostics pane. Two things about where, both learned by getting them wrong first — the
  log line belongs in `set_failed()`, not in the state callback, because a server that
  fails again while already failed changes no state and it is the *last* message ("exited
  immediately 3 times in a row, check the command") that a user needs; and the pane has to
  fill itself on `::map`, because a server that gave up before anyone opened it has
  nothing left to notify anybody with. `server_gives_up` covers the log on both toolkits,
  `failure_pane` the pane.
* **An entry whose program was not installed shadowed the entries after it.**
  `find_config()` returned the first entry whose *filter* matched and
  `lsp_manager_add_doc()` then gave up on finding no program, so a fallback entry after a
  preferred one never ran. The check is inside the matching loop now — `lsp.xml`'s "skipped
  in silence" means the entry is skipped, not the document. `starts_server` puts an
  uninstalled entry for `*.txt` before the working one and watches which gets the file.
* **The diagnostics preference did not reach the pane.** "Underline problems and list them
  in the Diagnostics pane" took the marks off the document and left the pane listing them;
  `fill_pane()` reads the preference now. A failed server still says so with the setting
  off — that is not a diagnostic, it is the reason there are none. In `diagnostics_pane`.

## What the shortcuts know

Every command in medit is a `GtkAction` with a name, and every action that is not
marked `no-accel` is in **Edit → Configure Shortcuts**, whether or not it was given a
default key. That dialog is the whole of the answer to "can I change that key": there is
no per-plugin list anywhere, and a command that is not an action is not in it.

**A binding is a preference**, written as `Shortcuts/<window id>/<group>/<action name>` —
the editor window's id is `Editor` and each plugin puts its commands in a group of its
own, so Ctrl+Space is `Shortcuts/Editor/Lsp/LspComplete` and the terminal's copy is
`Shortcuts/Editor/Terminal/TerminalCopy`. An action in no group has no middle part. The
`<MooAction>/` that starts an accel path is stripped on the way in
(`accel_path_to_prefs_key()`), which is worth knowing before writing the key by hand: with
the prefix left on, the value is loaded, registered and never looked at. A test rebinds a
key by writing that preference in `setup()`, which is exactly what the dialog writes, and
`tests/lsp/shortcuts` and `tests/terminal/copy_paste` both do.

**A group is a heading in that dialog and a segment of the accel path**, and the two come
together: `moo_window_class_new_group()` before the actions and the group's name as the
third argument of `moo_window_class_new_action()`, after which the plugin's commands stop
being a few rows among the editor's hundred. The user tools have done this since long
before the plugins did. Moving an existing action into a group changes its accelerator
path, so a binding somebody had customised under the old path is forgotten -- a one-time
cost paid here for the terminal's ``Ctrl+` `` and nothing else, every other grouped action
having been new.

**A key the focused widget swallows has to be matched by hand**, and there are three of
them: the terminal pane takes Ctrl+`, its copy and its paste before the shell sees
anything, and the LSP client takes Ctrl+Space and Ctrl+Shift+Space before the text view
does. `_moo_accel_check_action_event()` is the one place that does it, for both plugins:
it reads the accelerator the action has **now**, so a rebinding is obeyed, and answers no
when the accelerator is empty, so a shortcut somebody cleared stays cleared. The client
used to fall back on the default it was compiled with, which meant a cleared Ctrl+Space
went on completing.

**That comparison cannot see a Ctrl+Shift+letter unless it looks twice**, which is the
trap under all of this. Measured on a plain us layout: `gtk_accelerator_parse
("<Ctrl><Shift>C")` gives the **lower-case** c with both modifiers, while pressing those
keys produces an **upper-case** C with the shift already consumed by the keymap — so
`moo_accel_check_event()`, which compares the translated event, never matches, and any
Ctrl+Shift+letter anybody binds does nothing. That is why the terminal's copy and paste
were two hard-coded cases in a switch for years. The matcher tries the raw event as well
now, and `tests/terminal/copy_paste` presses a rebound Ctrl+Shift+Y to prove it.

**`connect-accel FALSE` is how a key belongs to a pane and not to the window.** The
terminal's copy and paste are actions so that the dialog can edit them and the pane can
look them up, and are not connected to the window's accelerator group, because copying the
terminal's selection while the document has the focus is not what the key means. They are
in no menu either: the dialog lists actions, not menu items.

**The dialog itself is driven in `tests/app/shortcuts`**, on both toolkits. Three things
about doing that. The list is a tree, expanded, whose rows are the actions' display names —
which is how that test can assert that every command of the client is configurable at all.
The shortcut is set through a `MooAccelButton`, which opens a dialog with nothing to type
into: it listens for the keys themselves and commits half a second after the last one, so
the test presses the combination and waits for the dialog to go rather than pressing OK.
And with no window manager the keys follow the pointer, so the test clicks the catching
dialog's label first — otherwise the keystroke is delivered to the dialog underneath.

Two things in that dialog were broken for as long as it has existed, and the test would
have caught either. Its Search box was invisible — the box and its label say
`visible=True` in the `.ui` file and the `GtkHBox` holding them says nothing, and a
container that is not shown does not show its children — and it was wired to nothing, so
the search column made typing into the *list* work while the box that says "Search:" did
not exist. And the three radio buttons read `Shortcut|None`, `Shortcut|Default` and
`Shortcut|Custom` in every language without a translation for them: the `.ui` marked them
`context="yes"`, which GtkBuilder reads as the message context *being the word* "yes"
rather than as "this string uses the | convention", so nothing ever stripped the prefix.
Both are fixed; the po files keep their translations, the msgctxt in them having been
moved to `Shortcut` along with the msgid.

## Coverage

What the tests executed, measured by clang's own instrumentation — the same binary the UI
tests drive and the unit tests run in, so one build answers both what went wrong and what
was reached.

```bash
cmake -S . -B buildc3 -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DGTK_VERSION=3 -DENABLE_UI_TESTS=ON -DENABLE_COVERAGE=ON \
      -DENABLE_SANITIZERS=address,undefined
cmake --build buildc3 -j"$(nproc)"

cmake --build buildc3 --target ui-test         # or ctest -R lsp, or ctest -L unit
cmake --build buildc3 --target coverage        # merge, export, print the TOTAL line
cmake --build buildc3 --target coverage-html   # the same, plus annotated source

python3 tests/coverage.py buildc2/coverage/medit.info buildc3/coverage/medit.info \
        --floor tests/coverage.floor           # both toolkits as one number
```

`tests/run.sh` is not part of this: it drives `buildu2` and `buildu3`, which are the
ordinary test build directories and are better left uninstrumented. A coverage run is
`ctest` or the `ui-test` target inside the coverage build directory, and then the target
above.

`ENABLE_COVERAGE` needs clang and refuses gcc outright: gcc's `--coverage` writes a format
`llvm-cov` cannot read, and a build that quietly measured something else would be worse
than one that stops. It also needs `llvm-profdata` and `llvm-cov` of **the same version as
the compiler** — a `.profraw` carries a version number and the tools refuse anything else.
Debian and Fedora ship them in `llvm`, next to `clang`; `cmake/Coverage.cmake` asks for the
versioned name first, so a machine with several llvms still gets the matching one.

**The target reads profiles, it does not produce them.** Every run of the program leaves a
`.profraw` in `<build>/coverage/raw`; ctest names each one after the test (and the runtime
adds the pid, because the harness may start medit twice). `coverage` merges whatever is
there, exports lcov, prints the summary — and takes the raw files away, so the next run
answers about the next run rather than about everything since the build directory was
made. Run it after `ctest -R lsp` and it is the LSP tests' number, which is often the
question actually being asked.

The counters are written at exit, which is the same property the leak checker has and the
same reason both work here: a test quits medit through File/Quit and waits for its exit
code. A test that hangs and is killed contributes nothing to the number — and it has
failed anyway.

Two toolkits are two builds, hence two profiles and two reports, and merging them needs
neither binary: `tests/coverage.py` unions the lcov files line by line. That is why
`llvm-cov` is told to name files relative to the top of the tree — the two halves were
built in two containers, and an absolute path would have made them two different files.
GTK+2 is not the lesser half: it is what runs the `#else` branch of every
`GTK_CHECK_VERSION` split, and a line only it executed is a covered line. The script lives
under `tests/` because that is where the harness lives and where `flake8` already reads.

Vendored code is not measured — `src/gtksourceview`, `src/xdgmime`, `src/eggsmclient` and
`readtags.c`, the same four the `analyze` target skips — and neither is anything the build
generates or any system header glib inlines into every file.

**`tests/coverage.floor` is the gate.** One number, in the tree, next to the tests it is
about: line coverage must not fall below it. Not a cache, not a service — this is the only
arrangement where what the gate compares against is visible in the same commit as the code,
survives a week of quiet, and gives the same answer on a developer's machine as in CI. The
`coverage` job in `ui.yml` writes the table into the run's summary and fails when the
number is under the floor.

The floor sits a little under what the tests actually reach, and 0.3 pp is the margin: a UI
run is not deterministic to the hundredth — medit is started again when it could not open
the display, timers and idle handlers fire or do not, and the tests run in parallel.

**Raise it when a full point has opened up above it**, to the new number less the same 0.3
margin, in the commit that earned the rise; the job says so in as many words and prints the
line to write. A point rather than a tenth because a tenth is jitter and would have the
floor chasing noise, and rather than five because the whole purpose of the file is that the
ground already taken stays taken — a point is the most this arrangement ever leaves
undefended. Adding a test is usually worth more than that on its own: `tests/lsp` moved
`src/plugins/lsp` from nothing to 82%.

Lowering it is a legitimate commit too — covered code was deleted, a test was retired — and
the reason belongs in the file beside the number.

**What it says today**, from the first run of the whole thing: 41.57% of lines and 27.45%
of functions, GTK+2 at 38.56% and GTK+3 at 41.37%. Merging the two is worth only 0.2 pp
over GTK+3 alone — the lines only the GTK+2 build runs are few, which is a fact about this
tree rather than a reason to stop measuring it, since they are exactly the `#else` branches
nothing else exercises.

The interesting part is not the total but where it is spent, and it maps onto what has been
written recently rather than onto what matters:

| | lines |
|---|---|
| `src/plugins/terminal` | 85.6% |
| `src/plugins/lsp` | 82.2% |
| `src/mooapp` | 70.1% |
| `src/mooutils` | 46.9% |
| `src/mooedit` | 41.9% |
| `src/moofileview` | 26.4% |
| `src/plugins/usertools` | 6.5% |
| `src/plugins/ctags` | 3.1% |

The two subsystems with tests of their own are at 80%+; the file view, which every
open-file dialog goes through, is at a quarter; the user tools and the ctags plugin are
effectively unmeasured. That is where a test buys the most, and the floor is what keeps the
number from quietly going the other way while features are added.
