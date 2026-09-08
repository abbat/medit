# Building medit

*For agents working in this tree. Read `AGENTS.md` first: it carries the rules that
apply to every change, and this file assumes them.*

CMake, out of source. Two build directories keep both GTK versions alive at the same
time — no copying of the tree, no `distclean`:

```bash
SRC=$(git rev-parse --show-toplevel)              # repository root; every path below is relative to it
cmake -S "$SRC" -B "$SRC/build3" -DGTK_VERSION=3  # once; 3 is the default, the flag is for clarity
cmake --build "$SRC/build3" -j8                   # ~10s for one .o + link
```

The GTK+2 reference build is just another directory, and there the flag is required:

```bash
cmake -S "$SRC" -B "$SRC/build2" -DGTK_VERSION=2
cmake --build "$SRC/build2" -j8
```

The binary lands in `<build dir>/src/medit`, the compiled catalogs in
`<build dir>/locale/`. Full build ≈ 3 min, incremental ≈ 10s. Both GTK+2 2.24.33 and
GTK+3 3.24.38 dev packages are installed. **Every fix must build clean and behave
correctly on both.**

`./clean.sh` removes every build directory (anything holding a CMakeCache.txt), the
leftovers of a package build under `debian/`, the `__pycache__` a test run leaves, the
doxygen output and a hand-made `coverage.info`. It touches nothing else — and it is not
a thing to run to check that it works, because a configured build directory costs
minutes to get back.

Other options: `-DENABLE_NLS=OFF`, `-DENABLE_STRICT=ON` (all warnings and `-Werror`),
`-DCMAKE_BUILD_TYPE=Debug` (the default is RelWithDebInfo, i.e. `-g -O2`),
`-DENABLE_SANITIZERS=address,undefined`, `-DENABLE_UI_TESTS=ON` (`doc/testing.md`),
`-DENABLE_UNIT_TESTS=ON` (same file, and it follows `ENABLE_UI_TESTS` unless set).

**`medit file.c:42` opened nothing for eight months.** The command line takes a place in a
file as `file.c:42` or `file.c(100)`, and the rewrite in "remove debugs" left three lines
in the wrong order — `path = NULL` one line *before* `filename = path` — so every such
argument became a null filename and two criticals. The splitting is
`_moo_parse_file_line()` in `mooutils-fs.cpp` now, where a unit test can reach it, and the
caller is six lines. Anything with this shape is worth suspecting: a `static` helper in
`main.cpp` that no test can call and no user reports on, because the failure looks like
"medit did nothing".

**A Debug build had not linked for months**, and nothing noticed because every job in CI
builds RelWithDebInfo. Two symbols were referenced only from code a release build compiles
away — `model_contains_file()` from four `g_assert()`s in `moofoldermodel.c`, `moo_dmsg()`
from `mooappinput-unix.c`, which calls it without the `MOO_DEBUG_INIT` that defines it —
and both had been deleted as unused, which in a release build they truthfully look. Both
are back, marked `G_GNUC_UNUSED` so the release build stays quiet. Anything that is only
called from an assertion has this shape; `-DCMAKE_BUILD_TYPE=Debug` is the thing to build
before believing otherwise.

**Python is a developer's tool here, never a dependency.** The UI tests under `tests/`
are written in python, and that is the only python in the tree. It is not built, not
installed, not packaged, and nothing in `debian/` or `packages/` mentions it;
`CMakeLists.txt` does not so much as look for an interpreter unless `ENABLE_UI_TESTS=ON`
asks it to, and the default is off. This matters more than it would elsewhere: python2
is why medit was dropped from Debian in the first place, and "no interpreter at build
time, none at run time" is a property of this fork worth not quietly losing. If
something ever needs python to *build*, it does not belong in the build.

That harness is read as well as run: `flake8` in `.flake8`'s configuration (96 columns,
which is what the tree already is), through `cmake --build <dir> --target ui-lint` or the
`harness` job in `ui.yml`, and by CodeQL's python queries in a job of its own. The target
is optional and says so when flake8 is missing — a linter for a developer's tool is
itself a developer's tool, and no test may need one to run.

## A/B comparison of one change

**Do not use a pre-session build as the "GTK+2 reference".** Since the translations fix
the UI language differs, so pixel comparisons against an old build are noise. Compare
*the same tree* with and without the one change, rebuilding in place:

```bash
cmake --build "$SRC/build3" -j8 && <screenshot>   # with
git stash push path/to/file
cmake --build "$SRC/build3" -j8 && <screenshot>   # without
git stash pop
```

## What CI already does, and what it does not

Every job that compiles anything does it with `-DENABLE_STRICT=ON`, so warnings are
errors everywhere — `build.yml` asks for it, and so do `debian/rules`,
`packages/PKGBUILD` and `packages/medit.spec`. Which means the package builds are gates
too: Fedora is where LTO happens, and `-Wodr` has caught defects there that nothing else
sees.

| job | what it covers |
|---|---|
| `deb` | ubuntu 22.04, both toolkits — the low end of everything: gtk 3.24.33, glib 2.72, gcc 11, cmake 3.22 |
| `langs` | `src/mooedit/langs/check.sh` over the 187 language definitions and schemes |

`.github/workflows/package.yml` is the other half of the compiling: the deb on Debian 12
and Ubuntu 26.04 (both toolkits each), the rpm on fedora:44 with LTO, the Arch package,
and a check that the version is the same in all seven places it is written.

`.github/workflows/ui.yml` compiles with clang, runs the static analyzer, and is the only
job that runs the program rather than reading it. Its `harness` job goes first and takes
seconds: `flake8` over `tests/`, because a name that is not defined in a test file is a
failure the ui job reports half an hour later as "the dialog never opened". It builds debian:13 with
`-DENABLE_STRICT=ON -DENABLE_UI_TESTS=ON -DENABLE_SANITIZERS=address,undefined` for both
toolkits, runs `analyze`, then the `ui-test` target — a real X server, a real accessibility bus,
real clicks, sanitizers underneath. It is the only place a dialog that stopped opening
can fail anything. See `doc/testing.md` for what the tests are and how to write one; the evidence a
failure leaves is uploaded as an artifact, because a UI failure is close to
undiagnosable without it.

The same build is instrumented for coverage (`-DENABLE_COVERAGE=ON`, clang's own), so what
the clicks and the unit tests reached is measured without a second compile of anything. A
third job merges the two toolkits' reports, writes the table into the run's summary, and
compares the total with `tests/coverage.floor` — a percentage kept in the tree, raised by
hand with the change that earned it. `doc/testing-panes.md` has the commands and what the number does
not mean.

`.github/workflows/codeql.yml` runs CodeQL over both toolkits on every push, on pull
requests, and weekly. It judges a pull request on the alerts it *introduces*, which is
why it can be a gate while the analyzer's existing findings are not zero. A third job
reads the python of `tests/` (`build-mode: none`, a category of its own so its results
sit beside the toolkits' rather than replacing one) — until it was added, `languages:
c-cpp` meant the harness was analyzed by nothing at all.

**A workflow takes its `schedule` and `workflow_dispatch` triggers from the default
branch and nowhere else.** This one was written with `push: branches: [main]` while it
existed only on a work branch, and the result was not a workflow that ran rarely — it
was one that had never run at all: the cron did not fire off the default branch, the
*Run workflow* button never appeared, and no push ever matched. `build.yml` was green the
whole time, from its bare `push:`, and that is the trap — the Actions tab looks busy.
Ask the API instead, which needs no token on a public repository:

```sh
curl -s https://api.github.com/repos/abbat/medit/actions/workflows   # what is registered
curl -s https://api.github.com/repos/abbat/medit/actions/runs        # what has actually run
```

Two more things about reading its output. Alerts are per branch and the API defaults to
the **default** branch, so `code-scanning/alerts` answers "0" for a fork whose work
lives elsewhere; pass the ref. And `gh` needs to be logged in — `repo` scope is enough
to read them, and enough to dismiss one.

```sh
gh api "/repos/abbat/medit/code-scanning/analyses?per_page=10"          # what was uploaded, per ref
gh api "/repos/abbat/medit/code-scanning/alerts?ref=refs/heads/strict&state=open" --paginate
```

**A python `# codeql[rule-id]` comment suppresses nothing either**, measured the same way
as the C++ one below: four alerts annotated, two left as controls, and the two annotated
alerts that were not `py/empty-except` came back open. The two that did go were the
query's own doing — `py/empty-except` does not fire when the block carries a comment
explaining itself — which the controls then confirmed by going quiet with a plain comment
and no marker. What is left is dismissed in the Security tab with the reason written
there: 1777 on `/tmp/.X11-unix` is what X requires, and the runner's `except BaseException`
is what makes a failed test leave its evidence.

**There is no `NOLINTNEXTLINE` for CodeQL.** `// codeql[rule-id]` on the line above an
alert is real syntax and C++ does have an `AlertSuppression.ql`, but all it produces is a
`suppressions[]` entry in the SARIF, and code scanning does not read that entry —
converting those into dismissals is what `advanced-security/dismiss-alerts` is for, a
separate action that has to run on the default branch. Measured rather than assumed:
three comments, one per rule, each with its siblings left in place as a control, moved
nothing — `cpp/use-after-free` 8 → 8, `cpp/double-free` 1 → 1, and
`cpp/unterminated-variadic-call` 3 → 3 — in the same run where the two alerts that were
actually *fixed* closed, 242 → 240. A comment left in the tree would suppress nothing
while reading as though it did. `paths-ignore` is no help either: it does not apply to a
language that is built, which is why the vendored directories the `analyze` target skips
are analyzed here.

That leaves two mechanisms, answering different questions. A query that is wrong on this
tree **by construction** goes in `query-filters` in the workflow, where the reason can
be written beside it; `cpp/unterminated-variadic-call` and `cpp/inconsistent-null-check`
are there, both defeated by glib's habit of counting variadic arguments and of returning
null as an ordinary value. A query that is right but **wrong at one site** is left armed
and the alert is dismissed in the Security tab: that is where the nine memory alerts
belong, all of them one blind spot — a struct field reassigned after `g_free()` keeps
its freed state, so every write through the new pointer reads as a use after free.

The first pass, for calibration, since the ratio is what decides whether the Security
tab is worth opening. 242 alerts. **Three were real** and are fixed: a null check that
could not fire, `localtime()` behind the file list's mtime column, and an implicit
double-to-int narrowing. Thirteen were the two filtered queries. Twelve were dismissed —
nine the memory blind spot, one a lambda invented by a macro expansion, two in vendored
code. **One is open on purpose**: `cpp/constant-comparison` at
`mootextview-input.c:1086`, where `order == 1` sits inside `if (!order …)`, so the guard
meant to reject a double-click just after a closing bracket has never run once. It
arrived with the root commit, so it is upstream's rather than a port regression, and
what to do with it is a decision about what double-click should select, not a cleanup.

The remaining 214 are 172 notes and 41 `cpp/poorly-documented-function`. Neither is
wrong, and neither is a finding: `cpp/fixme-comment` counts the 267 FIXME/TODO markers
and the `FIXME:` blocks that `grep` already indexes better. Read them as a map
of what is unfinished, not as a queue.

`.github/workflows/package.yml` builds the three packaging trees the way a distribution
would, also on every push:

| job | what it covers |
|---|---|
| `version` | the version in `CMakeLists.txt`, `NEWS`, `debian/changelog`, `packages/medit.spec`, `packages/medit.dsc`, `packages/PKGBUILD` and `README.md` — seven files, nothing deriving one from another |
| `deb` | `dpkg-buildpackage` on ubuntu 22.04 and debian 13, two compiles each, then installs the result and runs it |
| `rpm` | `rpmbuild` on fedora:44, then installs and runs |
| `arch` | `makepkg` on archlinux, then installs and runs |

Its build dependencies are deliberately *not* installed by name, unlike `build.yml`'s:
`mk-build-deps` reads `debian/control`, `dnf builddep` reads the spec, `makepkg -s`
reads the PKGBUILD. A dependency declared under a name the distribution has since
renamed is one of the things under test, and naming them in the workflow would hide it.

Each job then installs what it built and runs `medit --version`, which parses its
arguments before opening a display and is therefore the one thing a container can run.
That is what covers `${shlibs:Depends}` resolving to packages that exist, an `%files`
entry for a path cmake no longer installs, and a binary that links but does not start.

So a source change does not need a container to prove it compiles anywhere, or that it
still packages. What is still manual: the **apt scenarios**, which need a sequence of
installs rather than one — the fresh install, the switch to gtk2, the upgrade from the
old monolithic `medit`, and the system already on `medit-gtk2` that must stay there —
anything visual, and anything that has to run further than `--version`.

## The analyze target

The clang static analyzer over medit's own sources, the same command CI runs:

```bash
cmake -S . -B builda -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build builda --target analyze          # whole tree, about 2 minutes
clang-tidy -p builda src/mooutils/moopaned.c   # one file, a couple of seconds
```

It needs a **clang-configured build directory of its own**, a third beside `build2` and
`build3` — or, on a machine with no clang at all, the ui container of `doc/testing.md`, which has clang
and clang-tidy already and where `cmake --build <dir> --target analyze` is the same gate CI
runs. Worth doing before a push that touches C or C++: it is a gate, and it reports nothing
on this tree, so anything it does report is new. That is not taste: clang-tidy takes its flags from `compile_commands.json`,
`CompilerFlags.cmake` probes every flag against the compiler that configured the tree,
and a gcc tree therefore records gcc-only flags — `-fno-enforce-eh-specs` among them —
that clang rejects outright on every C++ file. In a gcc build directory the target says
so and stops rather than failing obscurely.

`cmake/Analyze.cmake` holds the checker list and the reasons for every exclusion. The
short version: the analyzer only, none of clang-tidy's lint families, and three of the
analyzer's own checkers off because on this code base they are pure noise —
`optin.core.EnumCastOutOfRange` fires once per cast to a GObject enum (502 times),
`security.insecureAPI.DeprecatedOrUnsafeBufferHandling` recommends MSVC's `memcpy_s`, and
`security.insecureAPI.strcpy` fires on the name of the function without looking at the
length check on the line above it.

**Most of what is left is false, and the reasons repeat.** The analyzer does not model
glib: `g_strfreev()` is not seen as freeing, so it reports a leak on the closing brace
one line after the call; reference counting is opaque to it, so any `..._unref()` looks
like it might free; ownership passing into a GObject setter looks like a leak; a field
set in `_init()` and never cleared still looks nullable; `g_strdupv()` returning NULL
only for NULL is not known. Read the trace to the "Memory is released" or "Assuming ...
is null" line before believing any of them.

## Two analyzers measured and not adopted

Both were run over the whole tree; neither earns a place beside `analyze`, and the
reasons are worth keeping so the question is not reopened from scratch.

**Semgrep** (`semgrep --config=p/c --config=p/default`, 6 s in the official container).
Its community ruleset is 1074 rules, of which **four** target C at all: `gets`, `scanf`
and `strtok` misuse plus a `/dev/random` fd check. medit calls none of the three, so C
findings are zero -- verified against a file that does call them, where all three fire,
because a check nobody has seen fail proves nothing. The seven findings it does report
are all in `.github/workflows/`, flagging `uses:` pinned to a mutable tag rather than a
commit sha. That is a real category, and deliberately not acted on: the two actions here
are GitHub's own, and the compromises the rule cites were third-party.

The disqualifying part is quieter. Semgrep has no preprocessor, and it **drops a file it
cannot parse without failing** -- the scan prints "Parsed lines: ~99.9%" and exits green.
Four files are dropped: `mootextview.c`, `mooiconview.c`, `moonotebook.c`, `moopaned.c`.
Those are exactly the top four by `GTK_CHECK_VERSION` count (58, 51, 34 and 25; the fifth
has 14 and parses), because they put `#if` inside argument lists. The blind spot is
precisely the code the GTK+3 port touched hardest. A custom rule written against the
"style calls silently dead on GTK+3" table finds one of its five call sites for that
reason -- and would in any case have flagged the three `gdk_window_set_background()`
calls that are correct, since they sit in the `#else` branch it cannot see.

**Coccinelle** with the kernel's 76 semantic patches (`scripts/coccinelle`, 82 s).
Nothing ships with the Debian/Ubuntu package -- the rules have to be fetched. 74 of the
76 are usable: `api/kmalloc_objs` is patch-only and rejects `-D report`, and
`hid/ff_race` does not compile under coccinelle 1.3. Two more (`null/badzero`,
`api/check_bq27xxx_data`) need an OCaml compiler; on Ubuntu 26.04 the package is `ocaml`,
`ocaml-nox` is gone.

It found four things, all cosmetic and all fixed: two doubled semicolons, one dead
`result` variable, one pointer compared against `0`. Most of the ruleset is about kernel
APIs that do not exist here. But it reads this code far better than Semgrep does: 61 of
76 files parse perfectly and 99.3% of lines are read, and it skips the region it cannot
handle rather than the file. A small header defining `G_STMT_START`/`G_STMT_END`,
`G_BEGIN_DECLS` and the `G_GNUC_*` attributes, passed as `--macro-file-builtins`, takes
that to 64 of 76. It never sees the 70 `.cpp` files at all; `--c++` is documented as "a
small attempt to parse C++ files".

So if a pattern checker is ever wanted for a rule specific to this tree -- a GTK+2 idiom
that must not appear on the GTK+3 side, say -- Coccinelle is the one that can actually
read the files where such a rule would matter.

## Code generation

Only three things are generated: `marshals.[ch]` (glib-genmarshal), `moo-pixbufs.h`
(gdk-pixbuf-csource) and `resources.c` (glib-compile-resources). Everything else that
used to be generated — interfaces, menu descriptions, the credits text — is a resource
now, listed in `src/resources.xml` and read at runtime. The build needs no python, as above.
Adding a source file means adding it to the `target_sources()` list in that directory's
`CMakeLists.txt`.

**A data file a feature cannot work without belongs in the bundle, not only in the
install.** `lsp.xml` is the defaults for the language server client; it was installed
into `MOO_DATA_DIR` and read from there, so a run from a build directory found no
configuration at all and started no server, and the menu item that hands the user a copy
of it wrote an empty stub. It is a resource now and the install is the second place
looked at, not the first. Two things to get right when adding one:

* **The dependency list is not a glob.** `file(GLOB_RECURSE MOO_UI_FILES … *.ui)` in
  `src/CMakeLists.txt` covers interfaces only; anything else has to be named in the
  `DEPENDS` of the `resources.c` command by hand, or editing it rebuilds nothing.
* **Do not `preprocess="xml-stripblanks"` a file the user is meant to read.** It is
  there to shrink interfaces. For a configuration file the formatting *is* the
  documentation, and the copy in the binary should be the same bytes as the copy on
  disk — `gresource extract <binary> /text/<name>` against `wc -c` says whether it is.

## The language corpus

`src/mooedit/langs/` is a **verbatim copy of gtksourceview's `data/language-specs/`**.
There is not one local modification in it, and there should not be: the way to update it
is to overwrite it from an upstream tag, which is what commit "Refresh the language
corpus from gtksourceview 5.20.0" did after nine years of drift. The format has not
moved since the 2.x fork — every file still declares `version="2.0"` and the
`language2.rng` they are validated against is, whitespace aside, the same file — so it
really is a `cp`.

Refreshing one:

```bash
curl -sSLO https://download.gnome.org/sources/gtksourceview/5.20/gtksourceview-5.20.0.tar.xz
# check it against the .sha256sum file next to it, then
cp <tarball>/data/language-specs/*.lang <tarball>/data/language-specs/language2.rng src/mooedit/langs/
rm src/mooedit/langs/testv1.lang     # upstream's fixture for the retired v1 format,
                                     # not hidden, so it shows up in the language menu
src/mooedit/langs/check.sh           # validates both schemas, .lang and .xml
```

Then regenerate the two lists that name the files one by one — the `install(FILES …)`
block in `src/mooedit/CMakeLists.txt` and `po-gsv/POTFILES.in` — from the directory
listing. `check.sh` and `styles.rng` stay out of the install list: only `language2.rng`
is read at run time. Packaging needs nothing: the specs take the whole data directory.

The style schemes in the same directory are **ours**, not upstream's, and must not be
overwritten with it. What they do have to keep up with is `def.lang`: a style id with no
`map-to` fallback that no scheme defines leaves the text unstyled, which is how the
markup group (`def:emphasis`, `def:heading`, `def:inline-code`, `def:link-*`, …) arrived
silently unpainted in Markdown and reStructuredText. After a refresh, resolve every
`style-ref="def:…"` in the corpus through `def.lang`'s `map-to` chains and check the
roots against each of the eight schemes.

This fork's `GtkSourceStyle` understands only `foreground background line-background
bold italic underline strikethrough`. `scale` does nothing (which is why `def:heading0`
… `def:heading6` are commented out), and `underline` goes through `get_bool()`, which
accepts `true`/`yes`/`1` and reads **everything else, upstream's `underline="single"`
included, as "underline off"**. Translate upstream's values, do not paste them.

## Dialogs

Interfaces live in `src/*/ui/*.ui` (GtkBuilder XML), are compiled into the binary by
`glib-compile-resources` through `src/resources.xml`, and are built by
`moo_builder_new ("/ui/<name>.ui")` + `moo_builder_get (builder, "<id>")`. Three things
to know when touching them:

* **A new .ui file needs three entries**: the file itself, a line in
  `src/resources.xml`, and a line in `po/POTFILES.in` — the latter prefixed with
  `[type: gettext/glade]`, because intltool goes by extension and does not know `.ui`.
  Forget the prefix and the dialog silently comes up untranslated.
* **A widget still belongs to its placeholder window.** Interfaces that describe a
  piece of a window keep it inside a `GtkWindow` or `GtkDialog`; use
  `moo_builder_reparent()` to move it where it belongs. Adding it directly leaves the
  target empty and, in the placeholder-dialog case, does not even warn.
* **A widget handed to other code needs `moo_builder_take()`**, not `moo_builder_get()`.
  Anything that will be packed by its receiver — a page returned to a factory, a custom
  widget given to GtkPrintOperation — must leave the placeholder first. Handing it over
  with a parent still attached makes `gtk_container_add()` refuse ("Can't set a parent
  on widget which has a parent"), puts the packing properties on the placeholder, and
  ends in an abort inside `gtk_container_propagate_expose` when the wrong container
  draws it.
* **Widget types must be registered** before GtkBuilder sees their name, or it fails
  with "Invalid object type". `moo_builder_new()` registers the mooutils widgets;
  widgets from elsewhere need a `g_type_ensure()` of their own.
* **Placeholder windows must not be `visible`**, or GtkBuilder shows them: empty windows
  appear beside the real dialog and get drawn after their content was moved out.
* **A `.ui` that has to load in the GTK+2 build cannot use `GtkBox` or `GtkGrid`.**
  `GtkBox` is abstract in GTK+2 and `GtkGrid` does not exist there, so every
  interface in the tree uses `GtkVBox`/`GtkHBox`/`GtkTable`, which still load in
  GTK+3. The terminal's is the only exception, and only because the terminal is a
  GTK+3-only feature. GtkBuilder reports the difference as "Invalid object type",
  at the moment the dialog is opened.
* **Do not describe a model or cell renderers** for a combo the code fills itself
  (`init_combo()` and friends). Two renderers draw the value twice — "Selected lines
  Selected lines" — and it looks like a theme glitch rather than a bug.

**Check that an edited .ui actually reached the binary**, before concluding anything from
a test run:

```bash
gresource extract build3/src/medit /ui/<name>.ui | head
```

The resource is baked in at build time, so a stale dependency means the running binary
still has the old interface while the file on disk looks right. That is exactly what
happened once: the glob feeding the dependency covered `src/*/ui/*.ui` only, and the
plugins keep theirs one level deeper, so two rounds of "fixes" changed nothing.

A missing id is only reported when the dialog is opened, and some dialogs are hard to
reach (the drop dialog needs a real drag and drop). That comparison — the ids each `.ui`
declares against what the code asks `moo_builder_get/take/reparent` for — is how the one
stale id left in mootextprint.c was found, and it is **a build step now**
(`cmake/CheckBuilderIds.cmake`), so it happens whether or not anyone remembers. It scopes
per source file, which is exact: every file that asks for an id also creates its own
builder.

Two other checks run with the build. `xml-stripblanks` in `src/resources.xml` puts 33
files through xmllint on the way into the bundle, which is what makes malformed markup
fail. `cmake/ValidateXml.cmake` covers what that misses: `lsp.xml`, which is bundled
without stripblanks on purpose because its formatting is documentation, and the usertools
descriptions, which are installed rather than bundled.

## Builtin plugins, and dependencies only one gtk version has

A builtin plugin is a directory under `src/plugins/`, a `target_sources()` list in its
own `CMakeLists.txt`, and one call in `moo_plugin_init()` (`plugins/mooplugin-builtin.cpp`).
`moofind.cpp` is the template: `MOO_PLUGIN_DEFINE_INFO` + `MOO_WIN_PLUGIN_DEFINE` +
`MOO_PLUGIN_DEFINE`, a pane added in the window plugin's `create` and removed in its
`destroy`, actions registered on `MOO_TYPE_EDIT_WINDOW` in `init` and removed in
`deinit`. Three things that are not obvious from it:

* **A pane needs no menu item.** `View → Panes` lists every pane by itself, and the
  paned strip gets a button; an action is only worth adding for the accelerator.
  `moo_big_paned_present_pane()` takes the pane **widget**, not the `MooPane*` that
  `moo_edit_window_add_pane()` returned — `moo_edit_window_show_pane (window, id)` is
  the call that does the lookup for you.
* **The focused widget sees a key before the accelerators.**
  `moo_window_key_press_event()` (`mooutils/moowindow.c:753`) calls
  `gtk_window_propagate_key_event()` *before* `gtk_window_activate_key()`, the inverse
  of GtkWindow's own order, deliberately. So a widget that wants raw keys — a terminal —
  really gets `Ctrl+F`, and in exchange **no** editor accelerator fires while it has the
  focus, `MOO_EDIT_ACCEL_FOCUS_DOC` included. Only accels the user marked global
  (`_moo_accel_prefs_get_global`) still run first, via `activate_global_accel()`. A pane
  that grabs the keyboard has to provide its own way back to the document; the terminal
  handles its own accelerator in `key-press-event` and uses it to toggle the focus.
* **A window plugin is attached before the window has a document.**
  `moo_edit_window_get_active_doc()` returns NULL in `create`, so anything that depends
  on the open file — a working directory, a path — has to wait. The terminal starts its
  shell from the pane's `::map` instead, which also means no shell is forked for a user
  who never opens the pane.

Three more things the LSP plugin ran into, all of which apply to any plugin:

* **The document context menu is not `GtkTextView::populate-popup`.**
  `_moo_edit_view_do_popup()` (`mooeditview.cpp:387`) builds it from
  `moo_editor_get_doc_ui_xml()` at the path `Editor/Popup`, out of *document*
  actions (`moo_edit_class_new_action` on `MOO_TYPE_EDIT`). A handler connected to
  the signal is simply never called; `MooTextView::populate_popup` prepends Undo
  and Redo because it is the vfunc, which does still run. Add entries at
  `Editor/Popup/PopupStart` or `PopupEnd`, the way usertools does.
* **`Plugins/<id>/enabled` is the framework's, not yours.** `moo_plugin_register()`
  registers that key and reads the plugin's enabled state from it, so a plugin whose id
  is `Lsp` must not define a preference called `Plugins/Lsp/enabled` of its own: it
  collides with the switch in Preferences → Plugins, and both registrations then argue
  over the default. A plugin that should not run until it is asked for passes
  `MooPluginParams { FALSE, TRUE }` — disabled, but listed — as ctags and the LSP client
  do; there is no need for a switch of its own, and the framework attaches and detaches
  window and document plugins on the change, including for documents that are already
  open.
* **A right click does not move the cursor.** GtkTextView leaves it where it was, so
  a context menu entry that goes by the cursor answers about wherever the cursor was
  last left. The entry then looks as though it needs the word selected first — selecting
  is simply what moves the cursor into it. Record the press and go by that; clear the
  record on any other button and on any key, since those move the cursor themselves and
  the cursor is then the truth.
* **A document just opened has no document-plugin state yet.** `notify::lang`
  arrives after the document is inserted into the window, so a plugin that
  attaches on the language — as the LSP client does — has attached nothing at the
  moment `moo_editor_open_path()` returns. Anything the caller needs about that
  document has to come from what it already knew, not from a lookup.
* **`moo_editor_open_file()` moves the cursor from an idle**, at
  `G_PRIORITY_HIGH_IDLE + 9`, and `do_move_cursor()` removes whatever id is in
  `view->priv->move_cursor_idle` when it runs. So a caller that wants a *column*
  after opening a file cannot ask for it with `moo_text_view_move_cursor(...,
  in_idle = TRUE)` — the earlier idle cancels it — and cannot ask immediately
  either, since the earlier idle then overwrites it. Use a plain
  `g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, ...)`, which runs after it and which
  nothing else touches.

Line marks, for a plugin that wants something in the left margin:
`MooLineMark:visible` defaults to **FALSE**, and `line_mark_added()` only makes a
mark drawable if it is visible when it arrives, so `g_object_new (MOO_TYPE_LINE_MARK,
"visible", TRUE, NULL)` is not optional — `MooEditBookmark` sets it in its own init,
which is why bookmarks appear without anyone asking. The margin itself is hidden
until a view is told `show-line-marks`, and that has to be set before the marks
arrive. `moo_line_mark_set_markup()` is **not** a tooltip: the markup is drawn in
the margin, in place of the icon.

A dependency that only one gtk version has follows `MOO_BUILD_CTAGS` / `MOO_BUILD_TERMINAL`:
a tri-state `ENABLE_<X>` cache variable (AUTO/ON/OFF), a `#cmakedefine` in
`cmake/config.h.in`, and `#ifdef` around the `add_subdirectory()`, the
`target_link_libraries()` and the one call in `moo_plugin_init()`. What that does *not*
cover is the plugin's own header: `mooplugin-builtin.cpp` includes it unconditionally,
so **it is compiled by the gtk2 build too**. Keep types the other toolkit lacks
(`GtkFontChooser`, `VteTerminal`, …) out of it — declare a `GtkWidget*` and cast inside
the `.cpp`. A green gtk3 build proves nothing here; only building gtk2 does, which is
how this one was caught, in a container, after the local gtk2 build had gone stale.
