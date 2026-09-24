# AGENTS.md — medit

Fork of medit (GTK+ text editor) **ported from GTK+2 to GTK+3**; both builds are kept
alive. Work branch: `main`.

The port was largely done by an AI and is buggy. It left 53 blocks marked
`/* FIXME: This code was written by AI and requires review */`, a marker that said
only who wrote the code, not what was wrong with it. All 53 have been read against
their GTK+2 branch: 33 were faithful translations and lost the marker, 19 named a
defect, and those defects are fixed — the story is in `doc/bugs.md`.

None of those markers is left in `src/`. The last one was in `moonotebook.cpp`, on
the current tab's bottom line, and went with the widget when GTK+'s own
`GtkNotebook` took its place. A blanket marker at the top of `mootextview.cpp`
warned about the whole file; all 26 of its GTK+3 blocks have since been read against
their GTK+2 branch, the two that still made a cairo context of their own are fixed,
and that marker is gone too. The `FIXME:`s that remain are all in `src/vendor/` and
in the language files, and are upstream's.

**The GTK+2 branch of every `#if GTK_CHECK_VERSION(3,0,0)` is the specification.**
When GTK+3 misbehaves, read the `#else` branch first and ask what it achieved, then find
the GTK+3 way to achieve the same. Do not invent new behaviour.

**The tree's own sources are compiled as C++** — 150 `.cpp` and 205 `.h` under `src/`,
against three `.c` files left in the ctags plugin. What that buys is a stricter compiler
over a codebase that is still GObject C and calls the GTK+ C API on nearly every line.

The style stays GTK+/GNOME C and a wholesale rewrite is not wanted, but **C++ is there to
be used where it makes a function shorter to read or harder to get wrong**: RAII for a
local freed on several exits, `ObjectPtr` in place of a matching `g_object_unref`, `gstr`
in place of a `char *` whose owner nobody can name. `src/moocpp/` already holds those, and
`array.h` from it is the one part that went tree-wide. Where the resource has a C free
function and no wrapper, `g_autofree`/`g_autoptr` costs less than a class and still reads
as C. What stays out: nothing throws or catches, `-fno-rtti` is on, the standard is C++11,
and public headers stay C inside `G_BEGIN_DECLS` — the ABI of the whole tree is behind
them. Templates live in seven headers, all under `src/moocpp/` and `src/mooutils/`.

Two mechanics decide most of these changes. A `goto` cannot jump over a local with a
destructor — C++ forbids it — so a cleanup label converts only by turning every
`goto out` into a `return`, which is the bulk of such a diff. And GObject allocates
instances itself, so a non-trivial member of a private struct needs a placement `new` in
`_init` and an explicit destructor call in `_finalize`; `mooedit.cpp:315` and
`mooeditwindow.cpp:855` are the two that do it.

**New code, and any function being rewritten anyway, should take the C++ shape.** Declare
a variable where it is first given a value, not in a block at the top — a name that cannot
be read before it means something is one fewer thing to track. Give every owned resource an
owner that releases it: `g_autofree`, `g_autoptr(T)`, `gstr`, `ObjectPtr`, a `std::` container
where the data is ours. Return on failure instead of jumping to a label, and hand ownership
out with `g_steal_pointer`. Prefer `const`, prefer the narrowest scope, and let the type say
what the C idiom used to say in a comment. `git show c3f37b7` is the worked example:
nine `goto`s into a seventeen-line cleanup block became nine `return`s and no block at all.
The function, `parse_ini_file`, has since gone with the loader of external plugins it
served. For a small edit inside a function that still looks like C, match what is
around it — the mixed file is worse than either style.

**Leave the testable part testable.** Every test in this tree drives the real binary through
the UI, so anything tangled into a widget, a signal handler or a global is reachable only by
a whole run of medit, and the awkward cases — a parse error, a path that does not exist, the
third branch of a state machine — are usually not reachable at all. When writing a new
function or reworking an old one, lift the part that only computes into a `static` function
of its own that takes what it needs and returns what it decided, and leave the plumbing
around it. It reads better, and it is the difference between a branch a test can reach and
one only a user can. `tests/` and `tests/coverage.floor` say what the suite covers today;
`doc/testing-panes.md` explains both.

**Upstream code carried verbatim lives under `src/vendor/`**: `gtksourceview`,
`eggsmclient`, and ctags' `readtags.c`. It is excluded from `--target analyze`
(`cmake/Analyze.cmake`), exempt from `.editorconfig`, and outside the style measurements
below. Do not reformat or restyle it, and keep a change there to what our build needs —
anything else belongs upstream.

---

## How to read this file

This file is the part that applies to every change. The rest of what this tree has
learned is in `doc/`, one file per kind of work — read the one you need, not all of
them.

| you are about to | read |
|---|---|
| configure a build, touch CI, add a dialog or a plugin, run an analyzer | `doc/build.md` |
| change a package, upload to OBS, cut a release | `doc/packaging.md` |
| run medit and decide whether it works; touch translations | `doc/running.md` |
| write or fix a test, or read the UI harness | `doc/testing.md` |
| add or change a language definition under `src/mooedit/langs` | `doc/testing.md`, "The highlighting goldens" |
| write a test for the terminal, LSP or shortcuts; look at coverage | `doc/testing-panes.md` |
| hunt a defect and want to know what this tree already got wrong | `doc/bugs.md` |
| say what could be improved, or pick what to do next | `doc/todo.md`, "What to suggest first" |

Below, in this file: the rules that cost real time when broken, the environment traps,
the three GTK+3 porting mistakes to check first when anything looks wrong, and the
conventions for code and commits.

## The six that cost real time when broken

1. **A crash looks like success.** medit exits in ~0.15s with no stderr when it
   segfaults, so grepping the log reports a pass. Always
   `timeout 15 ./src/medit --new-app FILE >log 2>&1; echo "exit=$?"` — 124 is good.
   `doc/running.md` has the rest.
2. **Build with `-DENABLE_STRICT=ON`.** Every job that compiles anything in CI does,
   including the package builds. A warning you never see locally is a red CI. `doc/build.md`.
3. **The GTK+2 branch is the specification** — the paragraph above says it, and it is
   the rule most often skipped under time pressure.
4. **Measure the widget, do not assume the value.** Borders, thicknesses and style
   contexts differ per widget and per theme; several defects here came from a number
   that looked obvious. See the porting mistakes below.
5. **A test that passes on broken code is worse than no test.** Check the assertion
   actually fails before the fix, not only that it passes after. `doc/testing.md`.
6. **The version lives in seven files** and nothing derives one from another.
   `doc/packaging.md`, "Cutting a release".

---

## What costs money

The bill is cache reads: every tool call re-reads the whole context, so the cost of a
session is its context size times its number of tool calls. What gets written is noise
next to that — one long session here spent about $26 on everything it produced and five
times that on re-reading what it already had.

1. **One task per session.** Once the commit is pushed, `/clear`: the state is in git
   and in `doc/`, not in the transcript. A session that has drifted to 400k tokens pays
   ten times per call what a fresh one does, for the same work. `--autocompact 200k`
   puts a ceiling on the drift when the work really is one long task.
2. **Never let raw output into the transcript — this includes the build.** `make ... >log
   2>&1; echo "exit=$?"`, then grep the log only if the exit isn't 0 or you're specifically
   checking for warnings — with `-DENABLE_STRICT=ON` a clean exit already means no warnings,
   since strict turns them into errors. Same for tests: `ctest ... | grep -E "ok:|FAIL"`,
   never a whole `-V` run. Anything long goes to the scratchpad and is grepped there — a
   `lean-ctx-optimize` pass found the shell the single costliest tool by $ (33% of spend)
   precisely because raw dumps compress worst (~11%, against ~83% for reads). A 30k dump
   costs 30k on every later call, not once.
3. **Wait for CI in one background loop**, then read one filtered summary. Polling
   `gh run view` by hand is a full-price request that buys no information.
4. **Mechanical sweeps** — a whole-suite run, log triage, a batch experiment — belong in
   a fresh subagent or a cheaper model, and come back as counts rather than as logs.
   `.claude/agents/` has two for that: `suite-run` runs ctest here and `ci-triage`
   waits on a GitHub Actions run, both reporting names and numbers only.
5. **Read the shape before the body.** `mode=signatures`/`map` gets the API surface at a
   fraction of a `full`/`raw` read's cost, and is enough for orientation — most reads in
   this tree pull files in whole even when only their shape was needed (a large file read
   in full runs several thousand tokens for one look). Drop to `full` only for the function
   you are about to touch.

---

## Environment traps that cost tokens

| trap | do this |
|---|---|
| `command -v A B` returns non-zero even when A exists | test one name per call |
| `xwininfo -root -children \| grep medit` finds a 10x10 group-leader window; `import` on it fails with "resource temporarily unavailable" | `xwininfo -root -tree \| grep '"medit - '`, or `xdotool search --name "^medit - "` |
| system locale is Russian; `apt-cache policy` prints `Установлен:`/`Кандидат:` | prefix `LC_ALL=C` before parsing |
| `grep` is `ugrep`; a pattern starting with `-` is parsed as an option | `grep -n -e "->field"` |
| gdb `-batch` breakpoint commands: an error **aborts the script and kills the app** | only read struct fields; never call libgtk functions (no debug info → "unknown return type"); break *after* locals are assigned, not at function entry |
| `Xvfb`/`import` need `DISPLAY=:99` on **every** invocation | it is not exported between Bash tool calls |
| Writing a marker into the log the app is writing to (`echo MARK >> log`) — the app's own fd has its own offset and **overwrites the marker**, so `sed -n '/MARK/,$p'` silently yields nothing and looks like "the code never ran" | record `N=$(wc -l < log)` before the action and read `tail -n +$((N+1)) log` after |
| `g_print` to a redirected file is block-buffered, so a tail of the log lags reality | use `g_printerr`, or run under `stdbuf -o0` |
| `git add -A` sweeps in hundreds of build artifacts — the tree is full of `.o`, `.deps/`, generated `*-gxml.h`, `src/medit`, and `.gitignore` does not cover them | `git add -u` (tracked files only), or name paths explicitly; check `git status --short \| grep -v '^??'` before committing |
| The pane buttons are not always on the right — their side is remembered in the sandbox `state.xml`, so a coordinate that worked last run can miss entirely | screenshot first and locate the button; never reuse coordinates across sessions |
| `cmd \| tail -n` reports the **exit code of `tail`**, so a failed build looks like `exit=0` | `set -o pipefail` before any pipeline whose status you intend to read |
| `G_ENABLE_DIAGNOSTIC=` with an **empty** value turns the diagnostics **on**: glib compares the value against `"0"`, and empty is not `"0"`. A run meant as the control therefore has the feature enabled, and an A/B says the variable does nothing | `env -u G_ENABLE_DIAGNOSTIC` to turn it off |
| Without `xmllint` on PATH, `glib-compile-resources` does not fail on `preprocess="xml-stripblanks"` — it prints one line and bundles the file **unchecked**, so malformed markup reaches the binary and surfaces only when the dialog is opened. `libxml2-utils` is merely *Suggested* by the glib dev package | the configure step requires `xmllint` by name now; do not make it optional again |
| LSan stacks default to `fast_unwind_on_malloc=1` and system libraries have no frame pointers, so every stack is two frames deep and *no* leak appears to involve our code | `ASAN_OPTIONS=fast_unwind_on_malloc=0:malloc_context_size=25`, and expect it to be slow |
| glib frees a test's data when that test has run, so data given to `g_test_add_data_func_full()` is still allocated at exit for every test the run did **not** select — invisible while the whole suite ran in one process, a leak in every entry once ctest runs them one at a time | hand over something immortal instead: `g_intern_string()` where the data is a string |
| A per-line `grep` over a `-j8` build log miscounts: two compilers writing at once interleave mid-line, so one warning's text lands inside another's and a filter like `grep warning: \| grep -v deprecated` reports a warning that does not exist | check the surrounding lines before believing a count of one |
| `gtk-builder-tool validate` stops at the **first** error, and 13 of our 30 `.ui` files fail immediately on `Invalid object type 'MooEntry'` and friends, because the standalone tool does not know the Moo widgets. Everything after that line in those files goes unchecked | it is still worth running on the 17 it can read; a full check needs a validator that registers the types first |
| gcc 12 accepts C constructs that gcc 9/10 reject (unnamed parameters), so a clean local build says nothing about the oldest target | no compiler in the current matrix rejects them; see "Debian package build (old distros)" |
| `pkg_check_modules(<prefix> …)` writes `<prefix>_VERSION` into the **cache**, so a prefix of `GTK` overwrote the `GTK_VERSION` entry that selects the toolkit — the first configure worked, the second failed with "Unsupported GTK version: 3.24.38" | fixed: the prefix is `GTKPKG`, and `GTK_VERSION` is again the toolkit choice and safe to branch on. Never give `pkg_check_modules` a prefix that names an option |
| A key name in an accelerator string is **case sensitive**: `"<Ctrl>Space"` does not parse and `"<Ctrl>space"` does. `_moo_accel_register()` drops an unparsable accelerator without a word, so the action simply has no key | test it: `gtk_accelerator_parse()` returns key 0. `MOO_EDIT_ACCEL_COMPLETE` carried this mistake unused since 1.2.92 |
| `_moo_get_accel()` and `_moo_get_default_accel()` read **different maps**: the first holds accelerators that were actually set, the second the defaults registered with the action. An accelerator that has only ever had its default reads as empty from the first | ask the first, fall back to the second — that is what a plugin matching its own accelerator by hand has to do |
| The focused widget sees a key before the accelerators (`moo_window_key_press_event`), so a plugin action whose key the text view consumes — `Ctrl+Space` — never fires | match the accelerator by hand in the view's `key-press-event`, as the terminal and the LSP completion do |
| An action holds **one** accelerator, and `<Ctrl>plus` is the main-row key only: the numeric keypad sends `KP_Add` and `KP_Subtract`, other keysyms, so zoom worked for everyone but the people on the keypad. An xdotool test with `ctrl+plus` passes and says nothing about it | handle the keypad keys by hand in the view's `key-press-event` (`moo_edit_view_key_press_event`), and test them with `t.key("ctrl+KP_Add")` |
| `MooMarkup` turns a `<![CDATA[…]]>` section into a **comment node**, where `moo_markup_get_content()` cannot see it | put the text in as ordinary escaped element content; `GMarkup` unescapes it on the way in |
| `GMarkup` accepts a `--` **inside an XML comment**; expat and every other conforming parser reject it. A comment mentioning a command line like `clangd --background-index` therefore loads in medit and fails everywhere else | check any xml the user is meant to edit with a real parser: `python3 -c "import xml.dom.minidom as m; m.parse('f.xml')"` |
| GTK+3 hides images in menus unless `gtk-menu-images` is on. It is off in a bare sandbox and commonly on in a real desktop, so a screenshot from the sandbox showing no icon says nothing about what the user sees, and GTK+2 shows them always | to check an icon on GTK+3, write `[Settings]\ngtk-menu-images=1` into `$XDG_CONFIG_HOME/gtk-3.0/settings.ini` for the run |
| A hover tooltip and a synthetic right click do not mix: with the pointer left resting on the target, the tooltip comes up and the context menu does not, and the run reads as a regression in whatever the menu was going to do (mechanism not established — the click may be swallowed, or the menu covered and dismissed) | move the pointer and click in the same breath, without a dwell, then screenshot and confirm the menu is up before clicking an item in it |
| A build-tree run also reads data from an **installed** medit package (`/usr/share/medit/`), so its stale `menu.xml` produces warnings about our tree | reproduce with `MOO_DATA_DIRS=<dir>` holding the tree's own xml — but note it *replaces* the whole search list, so style schemes and the file-selector plugin stop loading; use it to attribute a warning, not to test the UI |
| `-Wstrict-null-sentinel` fires on every `g_object_set()`-style variadic call in the tree — 166 of them, none a defect — because it warns about an uncast `NULL` where the sentinel is expected, which is only a portability concern where `NULL` is the integer `0`. Under `-Werror` that is a red build, and silencing it with `-Wno-error=format` throws away `-Werror=format` for the whole tree | the flag is deliberately absent from `cmake/CompilerFlags.cmake`; the comment there says why. Do not add it back |
| `--target analyze` does not run on this machine: the local `run-clang-tidy` is too old for `-warnings-as-errors=*` and stops with a usage message before analyzing anything. The clang here is 14; CI's is 19, and clang 14's `valist.Uninitialized` reports `create_named_icon()` in `moofileicon.cpp` although `va_start()` is two lines above the loop | run `clang-tidy -p builda -checks=… --extra-arg=-w <files>` over the files you touched instead, and read a `valist` finding as the old checker rather than as a defect. CI is the gate that counts |
| A `# requires: MOO_BUILD_X` line says the feature was **compiled in**, not that the machine can run it. `MOO_BUILD_CTAGS` is unconditionally 1, so `ctags.pane_navigation` failed rather than skipped on a host with no `ctags` program — the plugin shells out to it | requirements on the machine get a `find_program()` in `tests/CMakeLists.txt` and their own name, the way `MOO_HAVE_CTAGS` does |
| The same run takes **languages from the first** directory of the search path and **style schemes from the last**: `gtksourcelanguagemanager.c` keeps the first `.lang` it sees for an id, `gtksourcestyleschememanager.c` lets a later file replace an earlier scheme of the same id. A corpus dropped into `$XDG_DATA_HOME/medit/language-specs` is therefore used while the schemes sitting next to it are still overridden by the installed `/usr/share/medit/` — the new languages appear, the styles they need do not, and it reads as "the new lang file does not work" | point `MOO_DATA_DIRS` at a directory whose `language-specs` is a symlink to `src/mooedit/langs`: that drops the install prefix from the list, so both halves come from the tree |

### Getting a backtrace for a warning or critical

`G_DEBUG=fatal-warnings` under gdb is the quick route, but medit prints an unrelated
critical at startup, so `fatal-criticals` kills it before you reach anything. The
general recipe, which survives that:

```gdb
break g_logv          # every g_warning/g_critical passes through here
commands
silent
printf "=== LOG\n"
bt 12
continue
end
run
```

Then locate the message text in the log and take the block printed just above it.

---

## The GTK+3 porting mistakes that account for most bugs

Nearly every visual bug found so far is one of these three. Check for them first.

### a) Dispatching on a window taken from the drawing context

GTK+2 delivered one expose per `GdkWindow` and code branched on `event->window`.
The port kept that shape but took the window from the drawing context:

```c
drawing_context = gdk_cairo_get_drawing_context (cr);
event_window = gdk_drawing_context_get_window (drawing_context);   /* WRONG */
if (event_window == some_window) ...                               /* never true */
```

That window is the **toplevel frame's**, so the comparisons never match and the
code silently does nothing. Same for `gdk_drawing_context_get_clip()`: its region
is in toplevel coordinates, so intersecting it with widget-space rectangles
clips away everything above the widget's origin.

This has now been found three times — the file list, `MooPaned`'s splitter, and
`MooBigPaned`'s drop indicator — and it is worth measuring rather than reading,
because the failure is silent. Instrumenting `moo_paned_draw()` in a window with
four panes in it printed:

```
PROBEPANED handle_visible=1 event_window=0x614000095640
           handle_window=0x61400009a440 should_handle=1
```

one `event_window` for all four, never equal to any child, while
`gtk_cairo_should_draw_window()` says TRUE for the child. A `g_print` in the
dispatch and one UI test that opens the pane settles it in a minute.

A window only reaches a widget's `::draw` if it carries that widget as its user
data. `MooBigPaned` created the indicator as a child of `outer`'s window but set
the user data to the big paned, so `gtk_cairo_should_draw_window()` on `outer`'s
context answered FALSE for it whatever else was right.

GTK+3 emits **one `::draw` for the whole widget**. The correct shape is:

```c
if (win && gtk_cairo_should_draw_window (cr, win))
{
    cairo_save (cr);
    gtk_cairo_transform_to_window (cr, widget, win);   /* now widget/window coords */
    ... paint ...
    cairo_restore (cr);
}
```

Use `gdk_cairo_get_clip_rectangle (cr, &rect)` for the damage region.
Coordinates passed to cairo are relative to the widget/window, never
`allocation.x/y` (that offset is GTK+2's, and adding it puts the drawing outside
the clip).

`gdk_cairo_create()` on a window inside `::draw` does still paint, but it is
deprecated since 3.22 and bypasses the clip, so transform the context you were
handed instead: `cairo_save()`, `gtk_cairo_transform_to_window (cr, widget,
window)`, `cairo_restore()`. No GTK+3 path in the tree makes a context of its own
any more. Where it appeared not to work, the real cause was ordering:
`GtkTextView` fills the border windows in its own `::draw` and wiped out what had
been painted before the chain-up.

Ordering matters too: `GtkTextView` fills its windows' backgrounds in its own
`::draw`, so anything painted *before* chaining up is wiped out. Line numbers are
painted after the chain-up; line backgrounds that must sit under the text are
painted after it with `CAIRO_OPERATOR_MULTIPLY`.

### b) Style calls that are silently dead on GTK+3

These compile, run, and do nothing — no warning:

| call | status |
|---|---|
| `gtk_style_context_set_background()` | no-op since 3.18 |
| `gdk_window_set_background[_rgba]()` | no-op; GDK does not paint window backgrounds |
| `gtk_style_context_add_region()` | no-op since 3.14 |
| `gtk_style_context_get_background_color()` | returns **fully transparent** on a bare widget context |
| `gtk_style_context_get_border()` | **0** on a container with no CSS border of its own, where GTK+2's `style->xthickness`/`ythickness` were 1 — measured on `MooPaned`, and the reason four thickness translations in it drew nothing. Not a blanket rule: a realized `GtkEntry` answers 1, so measure the widget rather than assuming either way |
| `gtk_render_line()` | draws, but in the context's **foreground** colour — `#2e3436` on this theme. It is not what `gtk_paint_vline()`/`hline()` were: those drew the theme's etched groove. Using it for a separator puts black lines around the document, which is how the `MooPaned` border was noticed after it started working |
| `gtk_style_context_add_class (…, GTK_STYLE_CLASS_SEPARATOR)` and friends | matches nothing since 3.20 on a widget that is not the thing named. A theme matches on the CSS **node**, so the class alone leaves the context exactly as it was |

That last one is worth measuring rather than assuming. On this machine's theme:

```
plain widget context   NORMAL/ACTIVE/SELECTED  -> rgba(0,0,0, a=0)   ← useless
with GTK_STYLE_CLASS_VIEW  NORMAL/ACTIVE       -> white
with GTK_STYLE_CLASS_VIEW  SELECTED            -> the selection blue
```

So: a widget that wants a background must paint it itself in `::draw`
(`gtk_render_background()`), and any colour query needs
`gtk_style_context_save()` + `add_class (GTK_STYLE_CLASS_VIEW)` (or the right
class for that widget) + `set_state()` + `restore()`. Better still, let the theme
draw: `gtk_render_background()` with the state set beats fetching a colour and
filling a rectangle.

**To draw what a widget you do not have would draw**, build a style context on a
widget path of its own rather than adding classes to the one you have:

```c
GtkStyleContext *context = gtk_style_context_new ();
GtkWidgetPath *path = gtk_widget_path_new ();

gtk_widget_path_append_type (path, GTK_TYPE_SEPARATOR);
gtk_widget_path_iter_set_object_name (path, -1, "separator");   /* 3.20+ */
gtk_style_context_set_path (context, path);
gtk_widget_path_unref (path);

gtk_render_background (context, cr, x, y, width, height);       /* the line */
```

Measured: that context's background is `rgba(0,0,0,0.1)`, which over the pane's
surface is `#dedddc` — a separator, where `gtk_render_line()` on the widget's own
context gave `#2e3436`. This is how `MooPaned` draws its border and the two lines
beside the drag grip. Note that a separator on GTK+3 is a **background**, not a
stroked line, so it is `gtk_render_background()` over a rectangle one pixel thick.

Throwaway probes are cheap: a 20-line GTK+3 program in the scratchpad that
prints what these functions return settles such questions in one build. Four of
them settled this section: the border of an entry against a container's, what
`gtk_accelerator_parse()` makes of `"Shift"`, what a separator's background is,
and what it comes out as over white.

### c) Translating GDK drawing primitives to cairo one call at a time

`gdk_draw_polygon (…, FALSE, points, 3)` over three points a pixel apart draws
three pixels. The same path stroked with cairo gets a 1px antialiased line on
*either side* of every edge — the whitespace markers turned from neat dots into
blurry triangles. When the GTK+2 original addressed individual pixels, fill
1x1 rectangles with `CAIRO_ANTIALIAS_NONE` rather than stroking a path.

## Conventions

- Fix both GTK versions in one change where the API allows it, and **delete the
  `#if GTK_CHECK_VERSION` split** when one code path is correct for both.
- Remove the `FIXME:` on any block you fix, and if you review one and find nothing,
  remove it too — a marker that survives a reading it passed costs the next reader the
  same reading. If you find something and are not fixing it now, replace the marker
  with what you found and how you found it.
- Verify before claiming: build both **with `-DENABLE_STRICT=ON`**, run both with the
  exit-code rule, screenshot when the change is visual, and state what was *not*
  verified. Strict is not an extra: every job that compiles anything uses it, the
  package builds included, so an ordinary local build is a weaker check than any gate
  and `-Werror=unused-variable` on a line the change orphaned is enough to turn two
  jobs red. On a push CI compiles every
  supported distribution but one, with both compilers, builds every package, and drives
  the program through the UI tests — so what is worth doing by hand is looking at what it
  did, and anything the tests do not cover yet.
- Run `--target analyze` on anything non-trivial before committing, and read the traces
  rather than the summary lines — most of what it says about glib code is wrong, and the
  reasons are listed under "The analyze target".
- One logical fix per commit. Commit message: what the GTK+2 code did, why the GTK+3
  port broke it, the evidence (verbatim critical text / gdb output), and the fix.
- Commit only source files — the tree is full of untracked build artifacts.
- **Language:** the user is addressed in Russian, but everything committed to the
  repository — source code, code comments, commit messages, this file — stays in
  English.
- Trailer:
  ```
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
  ```
- Do not push unless asked.

### Code style

The tree is written in the **GTK+/GNOME C style**, which is the style of the API it
calls on nearly every line: four spaces and never a tab, the brace of a function or a
block on a line of its own and not indented past the statement it opens, a space before
the parenthesis of a call, `char *p` rather than `char* p`, the return type of a
definition on its own line, and multi-line parameter lists aligned into columns.

```c
static BTNode  *bt_node_new         (BTNode     *parent,
                                     guint       n_children,
                                     guint       count,
                                     guint       n_marks);
```

This records the style rather than imposing it — the tree already follows it. Measured
over the 358 own sources under `src/` (158173 lines, `src/vendor/` excluded): 93935
indented lines use spaces and **none** uses a tab, calls are written `foo (x)` in 45795
places against 2965 without the space, 9753 braces sit on a line of their own against
612 trailing a statement, and `char *p` outnumbers `char* p` 11743 to 168. Whitespace
hygiene is now exact — no line carries trailing whitespace, every file ends with a
newline, nothing is CRLF — which is what `.editorconfig` is there to keep true.

**There is no `.clang-format`, and GNOME's own experience with one is the reason.** GTK
ships a `.clang-format` at the top of gtk.git (`BasedOnStyle: GNU` plus eight
overrides), and the job that runs it, `style-check-diff`, is `when: manual`, looks at
the merge request's diff only, and prints this before exiting:

> The style check is not infallible. The clang-format configuration cannot perfectly
> describe GTK's coding style: in particular, it cannot align function arguments. The
> documented coding style for GTK takes priority over clang-format suggestions. […]
> That's why this CI check is OK to fail.

That is not caution about somebody else's tree. Seven core files of GTK itself
(`gtkwidget.c`, `gtkwindow.c`, `gtknotebook.c`, `gtklabel.c`, `gtkentry.c`,
`gtkbutton.c`, `gtkcssnode.c` — 41601 lines) diverge from GTK's own configuration by 6%
of their lines.

The same measurement here, as the share of lines clang-format 19 rewrites:

| configuration | rewritten |
|---|---|
| GNOME's `.clang-format` as it ships | 52% |
| the same, with `IndentWidth: 4` and `BreakBeforeBraces: Allman` | **19%** |
| a configuration fitted to this tree by hand | 20% |
| Microsoft / WebKit / GNU | 38% / 48% / 53% |
| Google / LLVM / Chromium / Mozilla | 61% / 62% / 63% / 63% |

GNOME's file is the right starting point — adapting its two disagreements about indent
width and brace placement beats anything fitted by hand — and 19% is the floor, for the
reason their script names. Of the 28411 lines it removes, 12123 are the column-aligned
parameter lists and 5141 are the second of the two blank lines between functions. The
first group cannot be recovered by trying harder: `AlignConsecutiveDeclarations` builds
an alignment of its own instead, and made the hand-fitted diff larger rather than
smaller, 60010 changed lines against 69860.

The cost of running it anyway is not the noise in one commit. What this fork does most
is read a line against the GTK+2 code it was ported from — the 263 `GTK_CHECK_VERSION`
splits and the blocks still marked `FIXME:` — and a tree-wide reformat puts one
commit on top of every line of that history.

What is enforced is the mechanical half only, through `.editorconfig`: indent width,
tabs, trailing whitespace, final newline, encoding. It needs no tool in CI and reformats
nothing, and it covers the drift that actually happens.
