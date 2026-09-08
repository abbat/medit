# Writing and running the tests

*For agents working in this tree. Read `AGENTS.md` first. What the terminal, LSP and
shortcut tests know is in `doc/testing-panes.md`.*



## The unit tests

Inside medit, behind a hidden option:

```bash
cmake -S . -B buildu3 -DGTK_VERSION=3 -DENABLE_UI_TESTS=ON \
      -DENABLE_SANITIZERS=address,undefined      # unit tests follow UI tests
cmake --build buildu3 -j"$(nproc)"

buildu3/src/medit --unit-test                    # all of them, ~30 ms
buildu3/src/medit --unit-test /lsp/position      # the four tests under it
buildu3/src/medit --unit-test-list               # what there is
cd buildu3 && ctest -L unit                      # the same, as one ctest entry
```

A path names the tests **directly** under it and not the whole subtree beneath: measured,
`/lsp/position` runs its four and `/lsp` runs nothing at all, printing `1..0` and looking
for all the world like a pass. That is glib's `-p`, not ours; `--unit-test-list` is the
way to find out what a path would select.

There is no test binary and no second build. `ENABLE_UNIT_TESTS` compiles the tests into
medit itself, `--unit-test` runs them before anything else happens — before the single
instance is looked for, before the session is read, before `gtk_init()` — and the process
exits with the result. Which means the binary running them is the sanitized binary the UI
tests already build, so the address sanitizer is watching what they touch and a leak in a
test fails it. An ordinary build has none of it compiled: `--unit-test` there is an
unknown option, and no package build has ever heard of any of this.

The framework is glib's own (`g_test_add_func`, `g_assert_cmpint`), so it costs no
dependency at all — which is the reason this fork can have unit tests again after the lua
ones went with the interpreter they needed.

**What belongs here is what a UI test cannot reach**: arithmetic, the shapes a reply can
take, a parser. Not widgets. Drawing, events and the GTK+2/GTK+3 split are what `tests/`
is for, and a unit test that mocks a toolkit tests the mock.

**No display is needed, and that is a measured fact rather than a hope**: `GtkTextBuffer`
and `GtkTreeStore` are objects rather than widgets and work with no `gtk_init()` and no
`DISPLAY`, which is what makes the text side of the LSP client testable this way.

**A `GtkTextTag` is not one of them.** Its class installs properties of gdk's colour
types, so creating one before `gtk_init()` is a fatal
`g_param_spec_boxed: assertion 'G_TYPE_IS_BOXED (boxed_type)' failed` — with a display as
much as without one, since it is the initialisation that is missing and not the screen.
Anything about tags therefore splits in two: the part that is a decision (which tag a
highlight kind gets) is a function returning a name and is tested here, and the part that
is a buffer wearing tags is a UI test.

**A build with the unit tests keeps its assertions live.** glib's test framework refuses
to start when it is compiled with `G_DISABLE_ASSERT` — rightly, since `g_assert_cmpint()`
would be nothing and the suite would report that no-ops passed — so `CompilerFlags.cmake`
leaves that one definition out when `ENABLE_UNIT_TESTS` is on. `NDEBUG` and
`G_DISABLE_CAST_CHECKS` stay.

**Everything inside an assertion had therefore never been compiled**, and turning them on
found three things in one afternoon, all of the same shape and none of them a regression:
two undefined symbols (the Debug link above) and, from clang only,
`-Wtautological-constant-out-of-range-compare` on `g_assert (type < N_FILTERS)` and
`g_assert (type < N_TOOLS)`. A C++ enum holds the smallest bit-field that fits its
enumerators, so with two of them the comparison is true by construction; with three it is
not, which is why `MOO_ACTION_CHECK_*` says nothing. In **C** an enum has the range of its
underlying type, so the same shape in `moowindow.c` is silent — the same rule the
`operator~` note in `doc/running.md` turns on. `(int) type < N_TOOLS` is what the rest of the tree
already writes.

Two things follow for anyone adding to this. A strict build **with gcc proves less than it
looks**: that whole class is clang's, and the ui job is where it lands. And `g_return_*` is
not `g_assert` — the checks are compiled in every build, so only what an assertion guards
is at risk of having rotted.

**Pin nothing to an English string.** The kind names in the symbol tree are translated, so
a test comparing against "function" passes in the C locale and fails on a Russian machine;
compare against `lsp_symbol_kind_name()` instead. The ctest entry pins `LC_ALL=C.UTF-8`
anyway, and the point is that the test should not need it.

**In CI they run in `ui.yml` and nowhere else**, as part of the same `ctest` the UI tests
go through — the binary is already built there, already sanitized, and the unit tests add
milliseconds to a job that takes minutes. They were briefly wired into `build.yml` as well,
for the oldest distribution medit supports, and taken back out: turning them on there meant
compiling that job with assertions live, which stops it being the build a distribution
does, and the point of `build.yml` is that it is exactly that build.

What is in them, besides the LSP client below: `_moo_parse_file_line()`, `moo_splitlines()`,
`_moo_accel_parse()` and `MooFileWriter`. The last three are the suites this fork used to
have — `moo_test_mooaccel`, `moo_test_mooutils_misc` and `moo_test_moo_file_writer` went
with the lua interpreter that ran them, while the functions they were about stayed exactly
where they were. The first is new, and is why the file exists: see `doc/build.md` on `file.c:42`.

`moo_splitlines()` is worth knowing about before using it. A terminator ends a line and
starts another, so `"one\n"` is **two** lines, the second empty — split-on-separator, not
python's `splitlines()`. `moocmdview` writes every element it gets into the output pane,
which is why a chunk of output ending in a newline draws a blank line. The test pins the
behaviour rather than changing it.

For the LSP client there are three more, each of which needed a few lines moved somewhere
a test could reach them: `lsp_config_parse_file()` (an `lsp.xml` written into a temp
directory: the command split the way a shell would, semicolon-separated markers, `<env>`,
`initialization-options` as ordinary text, `enabled="false"`, and a malformed file, which
must be a warning and an empty list rather than half a server), `lsp_config_find_root()`
(the walk up to a marker, which used to be `find_root_dir()` in the manager), and
`lsp_completion_word_start()` (what counts as the word being completed — the rule that
makes `obj.` offer everything and `obj.fi` offer what starts with `fi`, and that a cursor
just after a space has no prefix at all, which is what a UI test asserting the narrowing
has to know). `lsp_diagnostic_detail()` is the fourth and needed no walk: the bracketed
`[source code]` after a message has four cases and a UI test can drive one per run.

What is in them for the rest of the LSP client is its arithmetic and reply shapes: the UTF-16
crossing (an emoji is one character and two code units), the UTF-8 one (Cyrillic is one
character and two bytes), the clamps for a line or a column the document does not have,
and `documentSymbol` in both the flat `SymbolInformation` and the nested `DocumentSymbol`
form — the flat one being what older servers answer with and what no UI test drives. Every
UI test under `tests/lsp` is written in ASCII, where all three encodings agree, so none of
that was covered by anything before.

**Extracting for a test is worth it when the extraction is a walk over strings.** Every one
of those four was a `static` function or an inline block that no test could call, and each
came out as a free function with no widget in its signature. What did *not* come out is
anything that would have lost something on the way: the pane still writes its line in
pieces, because the severity is coloured and a single string would have taken the colour
with it.

`gdk/gdkkeysyms.h` has to be included for `GDK_KEY_s` on GTK+2 — the compat names live
there, and a test that only ever built against GTK+3 does not find out.

**`event->x` and `event->y` are in the coordinates of `event->window`**, and a
GtkTextView has several windows: the text, and a border window for each side that
is in use. Converting them with `gtk_text_view_window_to_buffer_coords (…,
GTK_TEXT_WINDOW_WIDGET, …)` when the click landed on the text moves the position
left by the width of whatever is down the left — the line numbers, most of all —
and a right click on a short word then asks the language server about the word
before it. Pass `event->window` along and convert from `GTK_TEXT_WINDOW_TEXT`; a
click that landed anywhere else names no character at all.

`::query-tooltip` is the other way round: its x and y *are* the widget's, so the
hover path was right all along. Which coordinates a callback is handed is worth
checking rather than copying from the one next to it.

Two things about writing a test for this, both learnt the expensive way. A word
is too wide a target — the first version asserted "inside `delta`" and passed
while the click was landing two characters early, because `delta` is five
characters long. And the *middle* of a single character is not that character: a
text view resolves a point to the nearest place a caret could go, so a glyph
eight pixels wide answers with itself for the first four and with its neighbour
for the rest. `t.click_range()`, `t.popup_at()` and `t.hover()` take `at=0.25`
for a test that means one particular character.

**Two dialogs in one `.ui` file must not share an id.** A GtkBuilder reads the whole
file at once, so the second `id="entry"` is not a second scope — it is an error, and
then *nothing* in the file is built. Both dialogs in `moofileselector.ui` were dead
that way: Create File said

```
could not build /ui/moofileselector.ui: Duplicate object ID 'entry'
Condition '*builder != nullptr' failed
```

and Save As, which no test can reach, said nothing at all. Underneath it the same
duplicate had pointed the second dialog's `<action-widget>` at the first dialog's
button, so once the file built, OK had no response and the dialog would not close.
`cmake/CheckBuilderIds.cmake` refuses both shapes now — a duplicated id, and an
`<action-widget>` naming an id the file does not declare — at build time and without a
display, which is where this belongs: the check already existed and only compared the
ids the C asks for against the ids the file declares, and every id here existed.

## The UI tests

```bash
cmake -S . -B buildu3 -DGTK_VERSION=3 -DENABLE_UI_TESTS=ON \
      -DENABLE_SANITIZERS=address,undefined
cmake --build buildu3 -j"$(nproc)"

cd buildu3 && ctest -j8                       # every test of this toolkit
ctest -R about_dialog --output-on-failure      # one test
ctest -L app                                   # one subsystem
cmake --build buildu3 --target ui-test         # ctest -j UI_TEST_PARALLEL

tests/run.sh --gtk both                        # both toolkits, from the source tree
tests/run.sh --gtk 3 -L terminal                # one subsystem of one toolkit
tests/run.sh --verbose --gtk 3                 # every line, as ctest prints it
```

**`tests/run.sh` is quiet on purpose.** A passing run is one line per toolkit —
`GTK+3  50/50 passed  96s` — and the exit code; the compile and the per-test `Passed`
lines go to `<build dir>/run.log`. A failing run prints what the failing tests printed,
the list at the end, and where the log is. `--verbose` restores the old behaviour, and
`ctest` in the build directory was never quiet in the first place. The reason is
arithmetic: a full run is 50 tests over two toolkits, so reading the roll call to learn
what one number already said costs about 120 lines every time.

Build directories of their own, `buildu2` and `buildu3` beside `build2` and `build3`: a
sanitized binary is three times the size and visibly slower, which is not what an
ordinary build should become.

A test is `tests/<subsystem>/<name>/test.py` — `app`, `editor`, `terminal`, `lsp` so far — one
`run(t)` function, and it imports nothing: the whole vocabulary is on `t`
(`tests/lib/context.py`). ctest labels each test with its subsystem and its toolkit. One
file serves both toolkits wherever the two trees agree, which for dialogs they do, gail's
and GTK+3's being the same tree there; the panes, the document and the terminal are GTK+3
only, and those tests say so.

A test may also define `setup(s)`, run **before medit starts** (`tests/lib/setup.py`):
`s.pref()` writes a setting into `prefs.xml`, `s.script()` an executable, `s.open()` a
file for medit's command line, `s.plugin()` switches a plugin on and `s.lsp_server()`
writes an entry in `lsp.xml`. `s.pref()` writes the type the key was registered with —
a boolean written as `type="string"` still arrives, through `item_set_type()`, which
converts it and prints `oops` as a critical in every run that does it. Nothing later would do — the terminal reads its shell when
the pane is first shown. The same object is `t.sandbox` in `run(t)`, so both halves name a
file the same way.

A test names what this build may lack in its header — `# requires: MOO_BUILD_TERMINAL`,
`# requires: MOO_GTK3` — and a build without it registers the test **disabled**, so
`ctest -N` lists the same tests in every build and says which cannot run.

**Reading and acting are different mechanisms, on purpose.** Everything asserted comes
from AT-SPI, so a test says "the Credits button is there" rather than comparing pixels,
and says it identically on both toolkits. Input is `xdotool` at coordinates AT-SPI has
just given, and there is not one fixed coordinate anywhere.

**Except where there is nothing in the tree to read**, and the widgets the port broke are
mostly of that kind: a container drawing on a `GdkWindow` of its own has no accessible for
what it draws. `t.pixel(x, y)` reads one pixel and `t.pixel_row(x, y, width)` a whole line
in one process — a hundred `t.pixel()` calls is a hundred ImageMagick invocations and is
slower than the thing being watched. Ask such a test for a *property* rather than a
colour, so it is not a test of the theme:

| what is being asked | how it is put |
|---|---|
| is anything drawn here at all | the spread between the lightest and darkest pixel across the strip (`app/pane_resize`: 0 before the fix, 195 after) |
| is this a picture of that | the mean brightness of the two compared (`editor/tab_drag`: a tab at rest is 240, in the air it was 122 and is now 239) |
| is this a frame or a block | how much of a scanned line changed (`app/pane_move`: 3 pixels out of a span of 798) |

**Some widgets are not in the tree at all.** `MooIconView` draws its own cells, so AT-SPI
calls it `unknown` and it has no children — a test finds it as *the* on-screen node of that
role and then asks which document a double-click at some coordinate opened, which says more
than a name would: that those coordinates really were that file. `MooNotebook`'s tab strip
is the same; what `MooNotebookAccessible` does expose is the pages, named after their tabs
and in the order the notebook holds them, which is what reordering changes. Where a tab is
is found by clicking along the strip and asking which page came forward — and read the page
that is *showing*, not the window title, which follows the document that has the focus and
after a run of clicks on the strip is not the one whose tab was last clicked.

**The file selector is six tests and one idea**: nothing in that pane names a
file. `MooIconView` draws its own cells, so the way to read a listing is to select
a row and ask the properties dialog whose file that is — a dialog per reading, and
worth it. Everything else about the pane is read from the entry above the listing,
which says the folder and also takes one when typed into, so a test points the pane
where it wants without clicking through the listing at all.

Three measurements that pane cost, so nobody pays twice:

- **An entry is only as wide as its own name.** A click a quarter of the way across
  the view lands beside a short one and does nothing: a double click 66 px in did
  not open a folder called `inner`, and 20 px in did. Click from the left.
- **The listing puts directories before files**, so the first row is a folder
  wherever there is one, and a test that wants a particular file either arranges
  the names or does without folders in that directory.
- **The folder is watched.** A file written from outside is in the listing about a
  second later, with nothing asked of the pane, so there is no backlog for Reload to
  clear and no way to tell Reload's work from the monitor's. That test asserts what
  is left: that Reload keeps the folder rather than losing it.

**`s.script("bin/xdg-open", …)` intercepts a program medit runs.** The sandbox has a
`bin` directory at the front of `PATH`, so what "Open With / Default Application"
launches is a script the test wrote, and what it was asked to open is a string
comparison instead of a dependency on what the machine has installed. The same trick
is waiting for the user tools, which are 6% covered and are nothing but commands.

**`t.choose(menu, *path)`** walks a context menu the way `t.menu()` walks the menu
bar, opening submenus on the way. And `t.item()` will bring an entry into view with
the arrow keys if it has no position: a long menu keeps entries in the tree that it
is not showing, and clicking the coordinates of something that is nowhere goes
nowhere.

**Dragging.** `t.drag(node, dx, dy)` and `t.drag_to(x0, y0, x1, y1)` walk the journey in
steps with the button down: a toolkit decides a drag has begun from the motion it sees, and
one jump from press to release is a single event most drag handlers treat as noise. Pass
`during=` to look at the screen while the button is still down — what a drag draws exists
only then. Real X drag-and-drop is a different matter: dropping a file onto a folder in the
file view *starts* (the drag icon and the drop highlight appear) but does not land
reliably, so the menu it would raise is not covered by anything.

**A submenu is not opened by clicking its parent.** Without a window manager the item takes
the click and the submenu stays unmapped, its items in the tree with no position. `t.menu()`
presses Right after such a step; an item that has a submenu is recognised by its role, which
is `menu` where an ordinary item is `menu item`.

**Typing into a dialog needs `t.focus()` first**, for the same reason: nothing hands the
input focus to a window that has just appeared, and clicking an entry only moves the caret
within a window that already has it. The symptom is an entry that keeps its old text with
no error anywhere.

Each test gets a temp root, a home directory, an X server (`Xvfb -displayfd`, so no two
tests race for a display number — see the trap below) and a session bus of its own. HOME
is sandboxed too, which only began to matter with the terminal: a shell reads the rc files
of whoever runs the tests, so with the real one it came up with the developer's prompt,
wrote to the developer's history, and behaved differently again in CI.
`UI_TEST_PARALLEL` says how many run at once. Browsers are intercepted, not opened: a
fake `x-scheme-handler/http` desktop entry appends the URL to a file, so "the license
opened in a browser" is a string comparison.
Failures leave everything in `<build>/ui-tests/<test>/` — `medit.log`, the X server's
log, the sanitizer logs, `sanitizer.json`, and `failure.png`. CI uploads that directory
as an artifact, and `gh run download <id> -D <dir>` fetches it.

**Reproducing a CI failure is a container away**, and much faster than pushing again:

```bash
docker run --rm -v "$PWD:/src:ro" -v /tmp/w:/w debian:13 bash -c '
    apt-get update -qq && apt-get install -y -qq --no-install-recommends <ui.yml apt lines>
    dbus-uuidgen --ensure
    export CC=clang CXX=clang++
    cmake -S /src -B /w/build -DGTK_VERSION=3 -DENABLE_STRICT=ON \
          -DENABLE_UI_TESTS=ON -DENABLE_SANITIZERS=address,undefined
    cmake --build /w/build -j"$(nproc)" && cd /w/build && ctest -j4 --output-on-failure'
```

Copy the two apt lines out of `ui.yml` rather than writing a list: `libclang-rt-dev` is in
them and is not obvious — without it clang accepts `-fsanitize=` and then fails to link,
which cmake reports as a compiler that does not accept the flag at all.

This is worth doing rather than guessing: the toolkit and at-spi versions are what UI
tests break on, they are exactly what the local machine cannot vary, and CI compiles with
clang while a local build almost certainly does not.

**No window manager**, which costs one thing: nothing hands the input focus on when a
window disappears. After a menu is dismissed the focus belongs to the menu's dead window,
`xdotool getwindowfocus` answers nothing, and the application stops seeing keys — the next
`Shift+F10` opens no menu. `t.popup()` points the focus back first
(`input.focus_window()`), and `t.focus()` is the same thing for a test that uses the menu
bar and then types — opening a file from a menu and typing into it silently types nowhere.
The manual sandbox below starts `xfwm4` and has none of this.

**Unless the test asks**, which one of them does. `NEEDS_WM = True` in the test module
starts `xfwm4` in that test's sandbox, before medit, and stops it after — `UI_TEST_WM`
names another one, and cmake passes what it found there. It is opt-in because a window
manager is the largest single change that can be made to what these tests run in: it
decides where windows go, who has the keys, and what closing a window means. Thirty-odd
tests that pass without one are not worth re-verifying under one, and a test that needs
one says `# requires: MOO_UI_TEST_WM` so that a machine without it lists the test as
disabled rather than failing it.

With a window manager there is a second thing to know: **activating a window and focusing
it are different**. `t.focus(frame)` is `XSetInputFocus` and tells the window manager
nothing, so the window it still considers active is the other one — and `Alt+F4` then
closes nothing at all, which is an hour spent on a test that seemed to be about closing
windows. `t.activate(frame)` asks the window manager instead (`xdotool windowactivate
--sync`), and that is what a test with two windows uses before typing into one or closing
it. Typing works after a plain click too, xfwm4 giving the keys to what was clicked, but
that is its policy rather than a promise.

What it cost to get there, so nobody pays twice:

**The sandbox root must be a short path.** The at-spi bus socket is created under
`XDG_RUNTIME_DIR`, and a UNIX socket path is limited to about 108 bytes. A root under a
build or scratch directory goes over it, and the only symptom is one line on stderr —
`atk-bridge: Couldn't listen on dbus server: Socket name too long` — after which medit
comes up, the accessibility tree never does, and the test times out looking for a window
that is on screen. Hence `mktemp -d /tmp/mui.XXXXXX`, and `UI_TEST_TMP_ROOT` if `/tmp` is
not where it should go.

**A display number is not a promise.** One test in thirteen fails a minute later with
`cannot open display` while the rest pass. Three fixes in `sandbox.py`, two causes found
and one not:

- Xvfb writes the number with a trailing newline; a two-digit number can arrive in two
  writes, and reading `1` out of `12` hands the test somebody else's display. The read
  waits for the newline.
- X creates `/tmp/.X11-unix` when it is missing, and servers starting together race to do
  it: one wins, the rest print `_XSERVTransmkdir: ERROR: Cannot create /tmp/.X11-unix` and
  then find every display taken. This is what kept CI red after the first fix — a
  container starts without that directory, a developer's machine has had it since login.
  It is created once, up front. Server starts are serialised on a lock too, so two cannot
  settle on one number.
- The rest is open. About once in five full parallel runs, here and in CI, one medit is
  refused a display that answered `xdotool` immediately before — in medit's own
  environment — and answers again a minute later; forty sequential starts on one display
  were never refused. `start_medit()` retries up to three times, only on that message,
  logging each retry; anything else that dies at startup is not retried. The retry line in
  a green run means this is still live.

The display is also checked before medit starts, from both phases, so a test without one
says so in a second instead of timing out in a minute.

**AT-SPI can describe a widget but not operate one.** `queryAction().doAction("click")`
on a menu item produces `Gtk-WARNING: no trigger event for menu popup` and
`Gdk-CRITICAL: gdk_window_get_window_type: assertion 'GDK_IS_WINDOW (window)' failed`,
opens nothing, and leaves the application with no toplevel at all. gtk wants a real event
to open a menu from and the action interface has none to give it. That is why the
coordinates come from AT-SPI and the click from `xdotool`.

**Park the pointer before every click.** `xdotool mousemove x y click 1` warps and presses
in the same instant, and GTK+2 then acts on what it thought was under the pointer
beforehand. A link in the About dialog does not open on the first click after a button in
the same dialog was clicked, and opens on the second. Measured, one variant per row:

| what was done | result |
|---|---|
| move to the link, click | does not open |
| the same click again | opens |
| park the pointer elsewhere first, then move and click | opens |
| move onto the label twice, then click | opens |

So `input.click_at()` parks at the far corner, moves to the target, then presses, with a
pause between each — which is what a hand produces and a warp does not.

**A link in a label is not a widget.** It has no `Component` interface, so it has no
extents: `queryComponent()` on it raises `NotImplementedError`. It is a range of the
label's text, and its position comes from `queryText().getRangeExtents(start, end, ...)`.
Worse, GTK+2 does not expose links at all — gail's label accessible implements `AtkText`
but not `AtkHypertext`, so the same dialog reports zero links there. `a11y.links_of()`
falls back to finding URLs in the label's own text, which works because the labels medit
puts links in show the address as the link text, and makes the GTK+2 assertion slightly
stronger than the GTK+3 one.

**Never match on a role name, only on the role constant.** at-spi renames them:
`push button` became `button` in at-spi2-core 2.52, so a test written against debian 12
finds nothing at all on debian 13 while a screenshot of the two is identical. This cost a
red CI run to notice, and about a minute to diagnose once the failure started printing
the tree it had searched — which is why `t.need()` and `t.toplevel()` do that. `a11y.find`
resolves the name to the constant before comparing anything; role names are only ever
printed.

**An unrealised widget reports INT_MIN for its origin**, and `xdotool` then rejects the
coordinate rather than clicking anywhere. The items of a menu that has not popped up yet
are already in the tree, so lookups filter on `input.on_screen()` — that turns "still
opening" into another poll instead of a crash.

**The locale is pinned to `C.UTF-8`**, because tests match on widget names. One
consequence is asserted directly rather than worked around: `credits.c` fills the
"Translated by" tab from `_("translator-credits")` and only when the lookup returns
something other than the msgid, so in an untranslated locale the tab is there and empty.

**The panes reach the bus through an accessible of their own.** A pane and its button are
internal children of `MooPaned`, and `GtkContainerAccessible` lists children from
`gtk_container_get_children()`, which skips internal ones. Before `MooPanedAccessible`
(`moopaned.c`) the paned reported one child, the document area: the file selector, the
file list and the terminal were off the bus entirely, for a screen reader as much as for a
test. GTK+3 only — GTK+2 keeps those types inside the gail module, which cannot be
subclassed by linking against it — so anything inside a pane is a GTK+3 test.

**A pane hides itself when it loses the focus, and its accessible does not.** A pane that
is not sticky is closed the moment the document takes the focus back — which is what the
panes are for — and what a test sees afterwards is a widget that still answers with its
text, still reports its position, and is not on screen: a click at coordinates taken from
it lands in the document drawn where it was. The symptom is a cursor somewhere nobody
clicked and nothing in any log. `t.pin_pane()` presses the Sticky button in the pane's own
toolbar (which has no name, only a tooltip) and is what a test that wants to watch a pane
while typing needs; without it, a pane is good for one click per opening.

**The document reaches the bus the same way**, and for a related reason: `MooNotebook`
inherits from `GtkNotebook` and uses none of it — it keeps its own pages and calls no
`gtk_notebook_` function — so the accessible it inherited read GtkNotebook's empty page
list while the child count came from the container. One child, and nothing returned for
it. `MooNotebookAccessible` takes the container's children instead, and the document is a
`text` node with the text in it. Each page is named after its tab, refreshed at every
lookup, so an open document is `hello.txt` and becomes `*hello.txt` while it has unsaved
changes. A page tab object of its own is what is still missing —
`gtk_notebook_page_accessible_new()` asks `gtk_notebook_get_tab_label()` for the name,
which is that same empty list, with a Gtk-CRITICAL to go with it. GTK+3 only, as above, so
the status bar's `Chars: N` label is still how a GTK+2 test would count characters.

**A test that asserts a fix should be seen failing without it.** The cheapest way, and the
one that costs no thinking: `git stash push -- src`, rebuild the test tree (incremental,
seconds), run the test, `git stash pop`, rebuild. All four assertions written for the three
LSP fixes were confirmed red that way before the fixes were committed. A green new test
next to a new fix proves only that the two were written by the same person.

**A modified document blocks the quit at the end of a test**: File/Quit asks about saving,
nothing answers, and the test fails with "medit did not quit when asked" twenty seconds
after the part it tested passed. A test that types into a document saves it first —
`Ctrl+S` on a file from `s.open()`, so no chooser appears.

**Accessibility itself produces criticals, and they are not medit's.** On this machine
the About test reports three on GTK+3 —
`gtk_notebook_get_tab_label: assertion 'list != NULL' failed`, as the credits notebook is
destroyed — and one on GTK+2, `gail_notebook_real_remove_gtk: assertion 'obj' failed`.
medit calls neither function; both are inside the toolkit's own accessible
implementations, which only run because the bridge is loaded. The count also depends on
the toolkit's version, not only on medit: debian 13's GTK+3 produces none of the three,
and its GTK+2 still produces the one. So the runner counts criticals and prints the
count, and does not gate on it.

**Quit through the UI, never with a signal.** A sanitizer only reports at exit, so a
killed medit reports nothing; the runner clicks File/Quit and waits for the exit code,
and a medit that will not quit fails the test. This is the exit-code rule of `doc/running.md` as a
mechanism rather than a habit.

## The ad-hoc sandbox (headless X + screenshots + synthetic input)

For poking at something by hand, rather than for a test.

Everything needed is installed: `Xvfb`, `xfwm4`, `x11vnc`, `Xephyr`, `xdotool`,
ImageMagick (`import`, `convert`, `compare`), `gcc`, `libX11`/`libXtst` dev.
**No sudo, no installs required.** Never test on the user's real `:0` — synthetic
clicks would seize their pointer.

```bash
S=<scratch>
Xvfb :99 -screen 0 1400x900x24 >$S/xvfb.log 2>&1 &   echo "Xvfb $!"   >>$S/pids
sleep 2
DISPLAY=:99 xfwm4 >$S/xfwm4.log 2>&1 &               echo "xfwm4 $!"  >>$S/pids
sleep 2
DISPLAY=:99 XDG_DATA_HOME=$S/xdg/data XDG_CACHE_HOME=$S/xdg/cache \
  XDG_CONFIG_HOME=$S/xdg/config ./src/medit --new-app FILE >$S/medit.log 2>&1 &
                                                     echo "medit $!"  >>$S/pids
sleep 7
WID=$(DISPLAY=:99 xdotool search --name "^medit - " | head -1)
DISPLAY=:99 xdotool windowsize $WID 1200 800
DISPLAY=:99 xdotool windowmove $WID 40 30
DISPLAY=:99 import -window root $S/shot.png          # then Read the png
```

## Letting the user watch/drive

```bash
x11vnc -display :99 -localhost -nopw -forever -shared -repeat -rfbport 5999 &
```
They connect with `vncviewer localhost::5999` (needs `sudo apt install tigervnc-viewer`;
no viewer is installed on their side). Alternative with zero install: run the session on
`Xephyr` instead of `Xvfb` — it appears as a window on their desktop.

## Click coordinates

Valid **only** after `windowsize 1200 800` + `windowmove 40 30` (client lands at +45+55):

| target | coords |
|---|---|
| "File Selector" pane button (right strip) | `1225 180` |
| location/path entry in the pane | `1070 217` |
| horizontal scrollbar of the list | `~1000 774` |

```bash
DISPLAY=:99 xdotool mousemove 1225 180 click 1                 # toggle the pane
DISPLAY=:99 xdotool mousemove 1070 217 click 1
DISPLAY=:99 xdotool key ctrl+a
DISPLAY=:99 xdotool type --delay 25 "/some/dir/"
DISPLAY=:99 xdotool key Return                                 # navigate
DISPLAY=:99 xdotool mousemove 1150 600 mousedown 1 ; \
DISPLAY=:99 xdotool mousemove 990 300 ; DISPLAY=:99 xdotool mouseup 1   # rubber band
```

Panes **always start closed**: `MooBigPanedConfig.active` is serialised into
`state.xml` but never applied on load (`moobigpaned.c` — `config->active` is only
written, never read back). So a click is always required.

The File Selector's default page is `MOO_FILE_VIEW_ICON` → the widget on screen is
**`MooIconView`** (`moofileview.c:893`), not a `GtkTreeView`.

## Comparing renders

```bash
convert a.png -crop 250x150+930+175 +repage -scale 250% zoom.png   # then Read it
convert a.png -crop 19x1+930+600 +repage txt:                      # exact pixel values
compare -metric AE before.png after.png null:                      # 0 = identical
```
A/B against the GTK+2 build is the fastest way to identify a UI regression — it turns
"looks wrong" into "GTK+2 draws X here, GTK+3 does not".

Crop carefully before concluding anything: a wrong offset once made a fix look like it
had broken the tab labels, and it had done the opposite. `import -window $WID` gives
window coordinates, `import -window root` gives screen coordinates — clicks with
`xdotool` always take the latter. Mixing them up types into the editor instead of the
widget you meant.

**A crop that is a few pixels too wide compares the wrong thing.** Two menu entries were
given the same icon, and `compare -metric AE` on a 20x16 crop of each reported 26
differing pixels — the first letter of the label, `С` against `Я`, had come along for
the ride. On 16x16, the icon and nothing else, it is 0. Find the extent of what is being
compared first: dump the region with `txt:` and look for the rows and columns that are
not background, rather than guessing a box around it.

## Teardown

**Kill by PID from `$S/pids`, never by name.** `pkill -x xfwm4` once killed the user's
desktop window manager. `pkill -f "src/medit --new-app"` matches the agent's own shell
command line and kills the shell (exit 144). `pkill -x medit` is safe.

---
