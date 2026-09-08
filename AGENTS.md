# AGENTS.md — medit

Fork of medit (GTK+ text editor) **ported from GTK+2 to GTK+3**; both builds are kept
alive. Work branch: `main`.

The port was largely done by an AI and is buggy. It left 53 blocks marked
`/* FIXME: This code was written by AI and requires review */`, a marker that said
only who wrote the code, not what was wrong with it. All 53 have now been read
against their GTK+2 branch: 33 were faithful translations and lost the marker, and
the other 20 say what the review found instead of who to blame. **Grep for `FIXME:`
in a file you are about to change and read the ones that are there** — each is a
defect somebody has already located, with the measurement that located it. Nearly
every one of them is a case of §6's two mistakes; the survivors are indexed at the
end of §5.

One is still a blanket: `mootextview.c:21` warns about the whole file and has not
been gone through block by block.

**The GTK+2 branch of every `#if GTK_CHECK_VERSION(3,0,0)` is the specification.**
When GTK+3 misbehaves, read the `#else` branch first and ask what it achieved, then find
the GTK+3 way to achieve the same. Do not invent new behaviour.

---

## 1. Build

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

`./clean.sh` removes every build directory (anything holding a CMakeCache.txt) and
the leftovers of a package build under `debian/`; it touches nothing else.

Other options: `-DENABLE_NLS=OFF`, `-DENABLE_STRICT=ON` (all warnings and `-Werror`),
`-DCMAKE_BUILD_TYPE=Debug` (the default is RelWithDebInfo, i.e. `-g -O2`),
`-DENABLE_SANITIZERS=address,undefined`, `-DENABLE_UI_TESTS=ON` (§3),
`-DENABLE_UNIT_TESTS=ON` (§3, and it follows `ENABLE_UI_TESTS` unless set).

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
installed, not packaged, and nothing in `debian/`, `rpm/` or `arch/` mentions it;
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

### A/B comparison of one change

**Do not use a pre-session build as the "GTK+2 reference".** Since the translations fix
the UI language differs, so pixel comparisons against an old build are noise. Compare
*the same tree* with and without the one change, rebuilding in place:

```bash
cmake --build "$SRC/build3" -j8 && <screenshot>   # with
git stash push path/to/file
cmake --build "$SRC/build3" -j8 && <screenshot>   # without
git stash pop
```

### What CI already does, and what it does not

Every job that compiles anything does it with `-DENABLE_STRICT=ON`, so warnings are
errors everywhere — `build.yml` asks for it, and so do `debian/rules`, `rpm/medit.spec`
and `arch/PKGBUILD`. Which means the package builds are gates too: Fedora is where LTO
happens, and `-Wodr` has caught defects there that nothing else sees.

| job | what it covers |
|---|---|
| `deb` | ubuntu 22.04, both toolkits — the low end of everything: gtk 3.24.33, glib 2.72, gcc 11, cmake 3.22 |
| `langs` | `src/mooedit/langs/check.sh` over the 187 language definitions and schemes |

`.github/workflows/package.yml` is the other half of the compiling: the deb on Debian 12
and Ubuntu 26.04 (both toolkits each), the rpm on fedora:44 with LTO, the Arch package,
and a check that the version is the same in all six places it is written.

`.github/workflows/ui.yml` compiles with clang, runs the static analyzer, and is the only
job that runs the program rather than reading it. Its `harness` job goes first and takes
seconds: `flake8` over `tests/`, because a name that is not defined in a test file is a
failure the ui job reports half an hour later as "the dialog never opened". It builds debian:13 with
`-DENABLE_STRICT=ON -DENABLE_UI_TESTS=ON -DENABLE_SANITIZERS=address,undefined` for both
toolkits, runs `analyze`, then the `ui-test` target — a real X server, a real accessibility bus,
real clicks, sanitizers underneath. It is the only place a dialog that stopped opening
can fail anything. See §3 for what the tests are and how to write one; the evidence a
failure leaves is uploaded as an artifact, because a UI failure is close to
undiagnosable without it.

The same build is instrumented for coverage (`-DENABLE_COVERAGE=ON`, clang's own), so what
the clicks and the unit tests reached is measured without a second compile of anything. A
third job merges the two toolkits' reports, writes the table into the run's summary, and
compares the total with `tests/coverage.floor` — a percentage kept in the tree, raised by
hand with the change that earned it. §3 has the commands and what the number does not
mean.

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
| `version` | the version in `CMakeLists.txt`, `NEWS`, `debian/changelog`, `rpm/medit.spec`, `arch/PKGBUILD` and `README.md` — six files, nothing deriving one from another |
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

### The analyze target

The clang static analyzer over medit's own sources, the same command CI runs:

```bash
cmake -S . -B builda -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build builda --target analyze          # whole tree, about 2 minutes
clang-tidy -p builda src/mooutils/moopaned.c   # one file, a couple of seconds
```

It needs a **clang-configured build directory of its own**, a third beside `build2` and
`build3` — or, on a machine with no clang at all, the ui container of §3, which has clang
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

### Two analyzers measured and not adopted

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

### Code generation

Only three things are generated: `marshals.[ch]` (glib-genmarshal), `moo-pixbufs.h`
(gdk-pixbuf-csource) and `resources.c` (glib-compile-resources). Everything else that
used to be generated — interfaces, menu descriptions, the credits text — is a resource
now, listed in `src/resources.xml` and read at runtime. The build needs no python (§1).
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

### The language corpus

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

### Dialogs

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

### Builtin plugins, and dependencies only one gtk version has

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

### Debian package build (old distros)

The package targets **Debian 12 and 13, Ubuntu 22.04, 24.04 and 26.04** — Debian 11 and
Ubuntu 20.04 were dropped when their support ended. Each is covered once, and by the job
that adds the most:

| target | compiled by | why there |
|---|---|---|
| Ubuntu 22.04 | `build.yml` | the oldest gtk, glib, gcc and cmake of the five |
| Ubuntu 26.04 | `package.yml` | the newest of all four, and the packaging of the LTS most users are on |
| Debian 12 | `package.yml` | the oldest packaging; `debian/rules` is a gate there |
| Debian 13 | `ui.yml` | where the UI tests run anyway |
| Ubuntu 24.04 | nothing, on a push | between two ends that are both built; the release builds it by hand on OBS |

A package build costs two compiles, one per toolkit, which is why `package.yml` carries
two targets rather than five. `debian/rules`, `rpm/medit.spec` and `arch/PKGBUILD` all ask
for `ENABLE_STRICT`, so every one of those builds is a gate rather than a smoke test. What is worth doing by hand is the faster loop while *writing* a
packaging change, and the apt scenarios below, which CI does not reach:

```bash
docker build -t medit-deb - <<'EOF'
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq && apt-get install -y -qq build-essential debhelper cmake \
    pkg-config intltool libgtk2.0-dev libgtk-3-dev libxml2-dev libxml2-utils \
    libjson-glib-dev libvte-2.91-dev libjpeg-dev
EOF
S=<scratch>                                                  # session scratch dir
git ls-files -z | tar --null -T - -czf $S/medit-src.tar.gz   # tracked files + local edits
docker run --rm -v $S:/w medit-deb bash -c 'set -o pipefail
  mkdir /build && cd /build && tar xzf /w/medit-src.tar.gz
  dpkg-buildpackage -us -uc -b -j8 2>&1 | tail -25'
```

The package builds medit **twice**, once per gtk version: `medit-gtk2` and
`medit-gtk3` carry the two builds and conflict with each other, and `medit` is an
arch-all metapackage depending on `medit-gtk3 | medit-gtk2`. So `debian/rules` runs
`dh_auto_configure`/`dh_auto_build`/`dh_auto_install` once per `--builddirectory`, and a
package build takes twice as long as a plain one. When changing the packaging, check
both the fresh install (`apt install medit` must pull gtk3, the first alternative), the
switch (`apt install medit-gtk2` must remove gtk3), and the upgrade from the old monolithic
`medit` (its `/usr/bin/medit` has to move to `medit-gtk3` without a file conflict —
that is what the `Breaks`/`Replaces: medit (<< 1.3.1)` are for). A fourth case is worth
one more run: a system already on `medit-gtk2` must **stay** there, because the installed
package still satisfies the alternative and apt does not reconsider the order.

Cache the image once (`docker build -t medit-u2004`); each fresh `apt-get install` costs
a few minutes. To collect **every** error in one pass instead of one per run, replace
`dpkg-buildpackage` with `cmake -S . -B b && cmake --build b -j8 -- -k 2>&1 | grep
error: | sort -u` — `-k` keeps make going after the first failing file.

The span the matrix covers is gcc 11 to gcc 15 and cmake 3.22 to cmake 4.2. The oldest
is Ubuntu 22.04; `cmake_minimum_required` still asks for 3.16, which is lower than
anything now tested and deliberately so — cmake 4 is the version that stops accepting
compatibility with anything before 3.5, and 3.16 is above that line.

What has actually broken on an old toolchain, none of it visible in a local build:

* **Symbols newer than the oldest target glib**, e.g. `G_REGEX_DEFAULT` (2.74) on Ubuntu
  22.04's 2.72. Use `(GRegexCompileFlags) 0`, as the rest of the tree does.
* **`g_object_ref` in C++** returns `gpointer` on older glib (no `typeof` magic), so
  assigning it to a typed field needs an explicit cast.
* **Unnamed parameters** in C function definitions (`static void f (Foo *x, gpointer)`) —
  legal in C++ and C23 only. gcc 9 errored with "parameter name omitted" while gcc 12
  accepted them silently at `-std=gnu17`. No compiler in the matrix rejects them any
  more, so this one is history rather than a live trap; write
  `G_GNUC_UNUSED gpointer data` anyway.

### Fedora and Arch packages

Three packaging trees live side by side: `debian/` (three packages, both gtk versions),
`rpm/medit.spec` (Fedora 44, gtk-3 only) and `arch/PKGBUILD` (one `medit`, gtk-3 only).
The last two build from the GitHub tag tarball, so their `sha256sums`/`Source0` follow
the release, not the working tree. To test a spec against uncommitted work, tar the
worktree with a `medit-<version>/` prefix into `~/rpmbuild/SOURCES` instead.

```bash
docker build -t medit-f44 - <<'EOF'
FROM fedora:44
RUN dnf -y --setopt=install_weak_deps=False --disablerepo=fedora-cisco-openh264 install \
        rpm-build rpmdevtools cmake gcc gcc-c++ gtk3-devel glib2-devel libxml2-devel \
        gdk-pixbuf2-devel libICE-devel libSM-devel intltool gettext desktop-file-utils
EOF
```

`--disablerepo=fedora-cisco-openh264` is not optional: that repository is frequently
unreachable and a weak dependency drags it in, failing the image build.

Fedora compiles with **LTO and gcc 15**, which see things the Debian build cannot — in
`package.yml`'s rpm job, the only place in CI that builds this way:

* **`-Wodr`** catches two file-local structs sharing a name across translation units
  with different fields. They are only file-local by convention — C gives them external
  linkage — so LTO merges them. `RegexActionInfo`/`RegexFilterInfo` were renamed for
  this. An anonymous namespace would be the C++ answer, but it trades the warning for
  `-Wsubobject-linkage` as soon as an externally visible struct has such a member.
* **`-Wc++20-compat`** catches identifiers that became keywords: a variable named
  `requires` would stop compiling the day the project moves to C++20.

Two spec details that are easy to get wrong: `-DENABLE_INSTALL_HOOKS=OFF`, or
`gtk-update-icon-cache` runs inside `%{buildroot}` and ships a stale `icon-theme.cache`;
and `--no-warn-unused-cli`, which silences CMake's notice about the `*_RELEASE` and
Fortran flags `%cmake` passes unconditionally.

**CentOS is not a target and cannot be one.** CentOS Linux 8 died in 2021 and Stream 8
in 2024, their repositories survive only on vault.centos.org, and what is there is glib
2.56 / gtk 3.22 — below the floor this code needs. Stream 9 (gtk 3.24.31) and Stream 10
(3.24.43) would work if anyone asks.

### Cutting a release

**Before anything else, check that the distributions are still the right ones.** They
age between releases and nothing notices on its own. Compare what is claimed against
what is supported *today*, and fix both directions — drop what has reached end of life,
add what has been released since:

* `README.md` — the "DEB packages for …" line under **download**.
* `.github/workflows/build.yml` — the `deb` job's `image:`, which is the oldest target
  and only that, so an aged image there loses the low end of the range rather than one
  point of it.
* `.github/workflows/codeql.yml` — the runner and its dependency list.
* `.github/workflows/package.yml` — the `deb` matrix, which carries the newest target and
  the oldest packaging, and the Fedora release in the `rpm` job. **Build the targets no
  workflow covers by hand at release time**, on OBS: Ubuntu 24.04 is compiled nowhere on a
  push, and Ubuntu 22.04 and Debian 13 are compiled but not packaged.
* `AGENTS.md` — "Debian package build (old distros)", which names the targets and the
  compiler span they cover.
* `debian/control`, `rpm/medit.spec`, `arch/PKGBUILD` — dependency names occasionally
  move between packages across releases.

The same check applies to the actions the workflows pin. GitHub retires the Node
runtime under them on its own schedule, and the first sign is a warning in a green
job rather than a failure: `actions/checkout@v4` targets Node 20, which was
deprecated in September 2025, and jobs kept passing while being force-run on Node 24.
Read `uses:` in both workflows against the current major of each action
(`actions/checkout`, `github/codeql-action`); a bump costs nothing when the tree is
quiet and is a surprise when the runtime is finally withdrawn.

A dropped distribution usually takes a workaround with it: retiring Debian 11 removed
the whole `snapshot.debian.org` recipe its dead archive needed. A new one is worth a
container run before it goes in the matrix — Ubuntu 26.04 arrived with gcc 15 and cmake
4.2, two and three major versions ahead of anything the tree had been built with.

The version itself lives in six places and they all have to move together. The `version`
job in `package.yml` compares all six and fails if one is left behind, so this is a list
to work through rather than a thing to remember. `1.3.4` was cut like this:

1. `CMakeLists.txt` — `MOO_MICRO_VERSION`. The comment above it says "keep in sync with
   debian/changelog", and that is the whole of the coupling: nothing derives one from
   the other.
2. `NEWS` — a dated `* === Released 1.3.4 ===` block at the **top**, prose, wrapped the
   way the file already is.
3. `debian/changelog` — a `medit (1.3.4) unstable; urgency=low` stanza at the top.
   `dch` is not used; the stanzas are written by hand, so mind the two-space indent,
   the blank line before the signature and the RFC 2822 date (`date -R`).
4. `rpm/medit.spec` — `Version:` and a `%changelog` entry, newest first, dated
   `Day Mon DD YYYY`.
5. `arch/PKGBUILD` — `pkgver`.
6. `README.md` — "current release of this fork", and the two tag examples in the
   paragraph about `git checkout`.

Then commit, merge to `main`, push, and tag:

```bash
git tag -a v1.3.4 -m "medit 1.3.4"
git push origin main v1.3.4
```

**The Arch checksum can only be filled in after the tag is pushed**, and it therefore
lands in a commit of its own, after the tag — the tarball GitHub generates for a tag
contains the PKGBUILD that would have to carry its own hash. Put
`sha256sums=('0000…')` in the release commit rather than `SKIP`, so that forgetting it
fails the build loudly. `git archive` does **not** reproduce GitHub's tarball (checked:
v1.3.2 hashes differently), so fetch the real one:

```bash
curl -sSL https://github.com/abbat/medit/archive/refs/tags/v1.3.4.tar.gz | sha256sum
```

---

## 2. Running and verifying

### The exit-code rule (most important)

medit **exits in ~0.15s when it crashes**, and a crash produces no stderr. Grepping
stderr for criticals therefore reports success on a segfaulting binary. This already
caused one broken commit to be pushed. Always:

```bash
timeout 15 ./src/medit --new-app FILE >log 2>&1; echo "exit=$?"
# 124 = survived the full 15s = good.   139 = SIGSEGV.   0 = exited early, investigate.
```

`--new-app` is mandatory — medit is single-instance and will otherwise hand the file to
a running copy and exit.

### A fixture that props the feature up hides the bug in it

Every test of the language server client began by copying `lsp.xml` into the sandbox's
data directory, because without it nothing started. That is precisely the bug: the
defaults were only read from the install, so a build-tree run had none, and the menu
item meant to hand the user a copy of them wrote an empty file. Every green run said
nothing about it, because every one of them had quietly supplied by hand the thing that
was missing.

When a test needs a step to make the feature work at all, ask what a user's first run
does instead of that step. Set the sandbox up the way an untouched machine is — empty
`XDG_DATA_HOME`, nothing installed — and see what happens before adding anything to it.

### Translations

The binary's compiled-in `MOO_LOCALE_DIR` points at the install prefix, so a build tree
run used to come up with an untranslated UI. `cmake/Gettext.cmake` also lays the
catalogs out as `<builddir>/locale/<lang>/LC_MESSAGES/<domain>.mo`, and
`moo_get_locale_dir()` falls back to that tree when the configured directory has
no catalog. `MOO_LOCALE_DIR` in the environment still overrides both.

If the UI comes up in English, check `find locale -type f | wc -l` in the build
directory — if it is empty, rebuild: the catalogs are a build target
(`cmake --build build3`).

**Nothing regenerates the .pot any more.** intltool went with autotools, and the tree
carries no template — `po/POTFILES.in` is only a list. To find out what a catalog is
missing, build one by hand and merge:

```bash
sed -e 's/^\[type: gettext\/glade\][[:space:]]*//' -e '/^#/d' po/POTFILES.in > files.txt
xgettext --directory=. --files-from=files.txt --from-code=UTF-8 \
    --keyword=_ --keyword=N_ --keyword=Q_ --keyword=C_:1c,2 --keyword=NC_:1c,2 \
    --add-comments -o medit.pot
msgmerge --no-fuzzy-matching po/ru.po medit.pot -o /tmp/ru.po
msgfmt --statistics -o /dev/null /tmp/ru.po
```

This is an approximation — xgettext treats `.xml` as C and does not understand
`.desktop.in`, both of which intltool handled — so trust it for "which msgid is
missing", not for the absolute counts.

**A translation can be present and still not appear.** The glade era left msgids that
no longer match the code: dialog titles were extracted as `"Dialog title|About"` (the
intltool "strip everything before the bar" idiom) and the Russian file additionally
carried `msgctxt "yes"` on them. When the About and Credits dialogs became plain C
calling `_("About")`, the lookups quietly missed and the dialogs came up in English
while `msgfmt --statistics` reported the catalog as fully translated. If a string looks
translated but shows in English, compare the msgid in the .po with the literal in the
source before anything else.

**The catalogs still carry the msgids of features that were removed.** ru.po kept
everything the python plugins had translated, so reinstating a feature in C gets its
translations back for free in every language — provided the literal matches the old one
exactly, typographic quotes included (`"“cd” to current file directory"`). Grep the .po
before inventing a wording; that is why the terminal's context menu came up in Russian
with only nine new strings to write. For strings gtk itself carries, `D_(str, "gtk30")`
borrows gtk's catalog the same way (`"Pick a Font"`); the python plugin used `"gtk20"`.

Catalog state, measured with the command above: `ru` is complete (626 strings) and is the
one to check first; `fr` and `es` are one string short, `pl` is complete but carries nine
fuzzy entries, `de` 20 short, `fi` 24, `ja` 29; `cs` (190 short) and `zh_CN` (215) are
still half empty, and `nl` 119. Every string of the terminal and of the LSP client is
translated in all ten, which is the one part of the tree where the newer catalogs are not
behind.

**A string can be live, translated, and still English on screen.** Two ways, both found by
auditing rather than by looking: a file that marks strings for translation and is not in
`POTFILES.in` (its msgids go obsolete in every catalog, and the translations sit there
behind `#~` while the program shows English — `moofontsel.c` and its "Show only fixed width
fonts" spent years like that), and a literal that was never marked at all (the heading the
editor's commands appear under in Configure Shortcuts was `"Editor"`, the window's display
name, passed as a bare string). Both checks are worth repeating after adding a file:
compare the set of files containing `_(`, `N_(`, `C_(` against `POTFILES.in`, and look for
what the catalogs have obsolete that the source still contains.

Note when translating a display name that the *id* beside it is not one: accelerator paths
and the `Shortcuts/` preference keys are built from the id, so translating that would make
a user's key bindings locale-dependent.

`ja.po` and `pl.po` still fail `msgfmt --check` on plural forms, which is pre-existing and
about the header rather than any one string; `--check-format` is clean everywhere, the one
Japanese entry that had lost a `%s` having been fixed. `fi.po` had a Spanish string in one
obsolete entry, which is what reviving one blindly can cost. Two things to keep true when adding
to a catalog: a translated string with a mnemonic keeps the underscore (on a letter of the
translation, not of the English), and `msgctxt` entries have to be appended with their
context or the lookup misses -- `C_("symbol kind", "class")` is not the same msgid as a
bare "class", and a catalog that has one still shows the other in English.

### Isolate config

medit writes `~/.local/share/medit/{prefs,file-list-config}.xml` and
`~/.cache/medit/{state,recent-files-editor}.xml` — the user's real settings. Always run
tests with:

```bash
env XDG_DATA_HOME=$S/xdg/data XDG_CACHE_HOME=$S/xdg/cache XDG_CONFIG_HOME=$S/xdg/config ./src/medit ...
```

### Two runtime checks, now wired into the UI tests

Both need medit to actually run. That is now what `tests/` does, and both are set for
every test in `tests/lib/runner.py` — so the numbers below are printed by every run,
and the sanitizer half of them is a gate.

**`G_ENABLE_DIAGNOSTIC=1`** costs nothing. It is an environment
variable read by libgobject, not a build flag — there is nothing to enable in
`CMakeLists.txt`. With it set, GObject warns when a **deprecated property or signal** is
used, which is a class the compiler cannot see at all: `-Wdeprecated-declarations`
catches deprecated *functions*, while these are named by string, through `g_object_set()`
or from a `.ui` file. A bare startup produces four:

```
The property GtkSettings:gtk-toolbar-style is deprecated …
The property GtkSettings:gtk-menu-images   is deprecated …
The property GtkAlignment:left-padding     is deprecated …
The property GtkAlignment:right-padding    is deprecated …
```

All four are things GTK+4 removes outright, so this is the cheapest survey of that work
there is. Opening dialogs finds more.

**Sanitizers are a build option now**, `-DENABLE_SANITIZERS=address,undefined`, which
puts the flag on the compile and the link together. Measured, so that nobody has to
measure again:

* **ASan and UBSan: clean, and therefore a gate.** The tree builds with both, and a run
  that opens the file selector, the bookmark editor, the find dialog and the menus
  produces **zero** reports from either — as does the whole About dialog test. Unlike the
  static analyzer they could be a gate from day one, and they are: `tests/lib/sanitizer.py`
  parses the logs and fails the test on any finding.
* **LSan: opt-in, `UI_TEST_LEAK_CHECK=1`, and off by default.** `detect_leaks=1` reports
  633 records, 121 KB at a clean exit. 344 are purely library, and the 289 that name our
  code do not mean what they look like: the largest, 101 records from `mootextview.c:3554`,
  is `update_tab_width()`, which frees all three of the things it allocates. What is
  retained is pango's font and shaping cache, attributed to the nearest frame that is not
  a library. That is also why `tests/lsan.supp` is nearly empty — suppressing by the name
  of the function a leak is blamed on would suppress the next real leak in that same
  function. Note that leak checking is *on* by default in the runtime, so a sanitized
  binary run without `ASAN_OPTIONS` exits non-zero over fontconfig's caches; the runner
  sets the options whether or not it was told the build is sanitized, for exactly that
  reason.
* **clang's UBSan checks two things gcc's does not**, and both showed up the first time
  the tree was built with it. `function` — a call through a pointer of another type —
  reports sixteen places, all of them how a GObject callback is called: `g_signal_connect`
  takes a `G_CALLBACK` and the marshaller casts it back. That is the idiom, not sixteen
  defects, so `CompilerFlags.cmake` turns that one check off when the compiler has it.
  `enum` found a real one, and in the flags helpers themselves: an unscoped enum holds
  only the values its enumerators span, so `~flag` is not one of them, and
  `MOO_DEFINE_FLAGS`'s `operator~` was casting it back to the enum — undefined, and so is
  every load after it. `operator~` returns an `int` now (`mooutils-cpp.h`), which a
  following `&` brings back into range. **C++ only** — C gives an enum the range of its
  underlying type, so `moofile.c`'s `flags &= ~MOO_FILE_HAS_STAT` is fine as it stands.
* **TSan: pointless.** Nothing in our code creates a thread — no `g_thread_new`, no
  `pthread_create`.
* **MSan: impossible** without an instrumented glib, gtk and pango.

Cost: the binary goes from 9 MB to 32 MB and startup is visibly slower, but well within
what a test run can take.

### Backtraces

```bash
G_DEBUG=fatal-criticals gdb -batch -ex run -ex "bt 25" --args ./src/medit --new-app FILE
```

---

## 3. Driving the program

### The unit tests

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
`operator~` note in §2 turns on. `(int) type < N_TOOLS` is what the rest of the tree
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
where they were. The first is new, and is why the file exists: see §1 on `file.c:42`.

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

### The UI tests

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
```

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
and a medit that will not quit fails the test. This is the exit-code rule of §2 as a
mechanism rather than a habit.

### What the terminal tests know

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

### What the LSP tests know

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

### What the shortcuts know

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

### Coverage

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

### The ad-hoc sandbox (headless X + screenshots + synthetic input)

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

### Letting the user watch/drive

```bash
x11vnc -display :99 -localhost -nopw -forever -shared -repeat -rfbport 5999 &
```
They connect with `vncviewer localhost::5999` (needs `sudo apt install tigervnc-viewer`;
no viewer is installed on their side). Alternative with zero install: run the session on
`Xephyr` instead of `Xvfb` — it appears as a window on their desktop.

### Click coordinates

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

### Comparing renders

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

### Teardown

**Kill by PID from `$S/pids`, never by name.** `pkill -x xfwm4` once killed the user's
desktop window manager. `pkill -f "src/medit --new-app"` matches the agent's own shell
command line and kills the shell (exit 144). `pkill -x medit` is safe.

---

## 4. Environment traps that cost tokens

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
| A per-line `grep` over a `-j8` build log miscounts: two compilers writing at once interleave mid-line, so one warning's text lands inside another's and a filter like `grep warning: \| grep -v deprecated` reports a warning that does not exist | check the surrounding lines before believing a count of one |
| `gtk-builder-tool validate` stops at the **first** error, and 13 of our 30 `.ui` files fail immediately on `Invalid object type 'MooEntry'` and friends, because the standalone tool does not know the Moo widgets. Everything after that line in those files goes unchecked | it is still worth running on the 17 it can read; a full check needs a validator that registers the types first |
| gcc 12 accepts C constructs that gcc 9/10 reject (unnamed parameters), so a clean local build says nothing about the oldest target | no compiler in the current matrix rejects them; see "Debian package build (old distros)" |
| `pkg_check_modules(<prefix> …)` writes `<prefix>_VERSION` into the **cache**, so a prefix of `GTK` overwrote the `GTK_VERSION` entry that selects the toolkit — the first configure worked, the second failed with "Unsupported GTK version: 3.24.38" | fixed: the prefix is `GTKPKG`, and `GTK_VERSION` is again the toolkit choice and safe to branch on. Never give `pkg_check_modules` a prefix that names an option |
| A key name in an accelerator string is **case sensitive**: `"<Ctrl>Space"` does not parse and `"<Ctrl>space"` does. `_moo_accel_register()` drops an unparsable accelerator without a word, so the action simply has no key | test it: `gtk_accelerator_parse()` returns key 0. `MOO_EDIT_ACCEL_COMPLETE` carried this mistake unused since 1.2.92 |
| `_moo_get_accel()` and `_moo_get_default_accel()` read **different maps**: the first holds accelerators that were actually set, the second the defaults registered with the action. An accelerator that has only ever had its default reads as empty from the first | ask the first, fall back to the second — that is what a plugin matching its own accelerator by hand has to do |
| The focused widget sees a key before the accelerators (`moo_window_key_press_event`), so a plugin action whose key the text view consumes — `Ctrl+Space` — never fires | match the accelerator by hand in the view's `key-press-event`, as the terminal and the LSP completion do |
| `MooMarkup` turns a `<![CDATA[…]]>` section into a **comment node**, where `moo_markup_get_content()` cannot see it | put the text in as ordinary escaped element content; `GMarkup` unescapes it on the way in |
| `GMarkup` accepts a `--` **inside an XML comment**; expat and every other conforming parser reject it. A comment mentioning a command line like `clangd --background-index` therefore loads in medit and fails everywhere else | check any xml the user is meant to edit with a real parser: `python3 -c "import xml.dom.minidom as m; m.parse('f.xml')"` |
| GTK+3 hides images in menus unless `gtk-menu-images` is on. It is off in a bare sandbox and commonly on in a real desktop, so a screenshot from the sandbox showing no icon says nothing about what the user sees, and GTK+2 shows them always | to check an icon on GTK+3, write `[Settings]\ngtk-menu-images=1` into `$XDG_CONFIG_HOME/gtk-3.0/settings.ini` for the run |
| A hover tooltip and a synthetic right click do not mix: with the pointer left resting on the target, the tooltip comes up and the context menu does not, and the run reads as a regression in whatever the menu was going to do (mechanism not established — the click may be swallowed, or the menu covered and dismissed) | move the pointer and click in the same breath, without a dwell, then screenshot and confirm the menu is up before clicking an item in it |
| A build-tree run also reads data from an **installed** medit package (`/usr/share/medit/`), so its stale `menu.xml` produces warnings about our tree | reproduce with `MOO_DATA_DIRS=<dir>` holding the tree's own xml — but note it *replaces* the whole search list, so style schemes and the file-selector plugin stop loading; use it to attribute a warning, not to test the UI |
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

## 5. Bug patterns already found (mostly, not all, from AI-ported blocks)

Reading these first will usually identify the next one:

- **`ab681d7`** `moo_paned_add` called `gtk_widget_set_parent()` instead of chaining to
  `GtkBin::add`, so `gtk_bin_get_child()` returned NULL forever (GTK+2 poked
  `bin->child` directly; that field is private in GTK+3).
  → *Chain up to the parent class instead of reimplementing it.*
- **`5b0a655`** `get_preferred_width/height` **vfuncs** called directly with `NULL` for
  the natural size. The public `gtk_widget_get_preferred_*()` wrappers tolerate NULL;
  the vfuncs dereference both out-params. Segfault.
  → *Vfuncs are not the public API.*
- **`1f1ef72`** `GtkStatusbar:has-resize-grip` does not exist in GTK+3 (grip moved to
  `GtkWindow` in 3.0, removed in 3.14).
- **`37f5564`** `gtk_widget_set_allocation()` called from `init()`; it asserts
  `visible || toplevel` and is a `g_return_if_fail`, so it logged a critical *and did
  nothing*. GTK+2 wrote `widget->allocation` directly as a "not yet allocated" sentinel.
  → *Keep such state in the private struct.*
- **`66e9338`** `gdk_drawing_context_get_clip()` is in the **frame's toplevel**
  coordinate space, not the widget's; intersecting it with widget-space rectangles
  clipped away the top rows of the list. GTK+2 used `event->region` (widget space).
  → *Use `gdk_cairo_get_clip_rectangle (cr, …)`.* Same file also created a fresh
  `gdk_cairo_create()` on that toplevel and drew with widget coordinates — draw on the
  `cr` you are handed, inside `cairo_save`/`cairo_restore`.
- **`d98a8ba`** The file list border came from a tabless `GtkNotebook` painting a frame
  around its page (GTK+2 behaviour). GTK+3 themes do not, so the scrolled windows have
  to ask for `GTK_SHADOW_IN`.
  → *A missing visual may come from a container two levels up, not the widget itself.*

- **`d842683`, and the Window menu** Two cases where GTK+3 changed *when*
  something happens rather than what an API does. Whitespace markers: see (c)
  above. The Window menu: it was filled from `::select` on the menu item, which
  on GTK+3 arrives when the submenu is already `visible` and `mapped` — fine on a
  click, but when the pointer slides over from a neighbouring menu the submenu
  keeps the size it had without the document items and shows a scroll arrow. The
  fix was to stop filling it lazily and keep it up to date from
  `moo_edit_window_update_doc_list()`. `gtk_widget_queue_resize()` +
  `gtk_menu_reposition()` on the already-placed menu does **not** rescue it, and
  the submenu's `::show` is never emitted at all — both were tried.

- **`5dd83ef` + `166576e`** `MooEditWindow` cached the active tab in a plain
  `priv->active_tab` pointer that nothing owned. It was cleared when a tab moved
  between notebooks but not when one was destroyed, so closing the last document left
  it dangling and every later lookup ran on freed memory. Fixed by holding it with
  `g_object_add_weak_pointer()`.
  → *Two lessons. Any cached widget pointer with no reference wants a weak pointer.
  And when you introduce a setter, grep for **every** direct assignment to the field:
  two were missed here, which desynchronised add/remove and produced
  `g_object_weak_unref: couldn't find weak ref`.*
  → *The build defines `-DG_DISABLE_CAST_CHECKS`, so `MOO_EDIT_TAB (x)` is a plain
  cast that validates nothing. Where a pointer comes from outside, check it with
  `MOO_IS_…` explicitly.*
- **`4c98d11`** `moo_notebook_size_allocate()` allocated only the current page. The
  others stay visible widgets and `forall()` hands them all to GTK, so a page that was
  never allocated sits at GTK's default 1x1 — smaller than the borders of the scrolled
  window inside it, giving `Negative content width -1`.
  → *If GTK complains about a nonsensical allocation, look for a container that
  allocates some of its children and not others.*
- **`0b21a59`** GTK+2's `gtk_combo_box_entry_new()` is a combo **with an entry** and a
  model of your choosing. Its GTK+3 spelling is `gtk_combo_box_new_with_entry()`, not
  `gtk_combo_box_text_new()` — the latter has no entry (`gtk_bin_get_child()` returns a
  `GtkCellView`, which has no `::activate` and is not a `GtkEntry`) and owns its own
  model. Symptom: the filter field in the file dialog stayed empty.

- **`3e59d41`** `_moo_edit_window_remove_doc()` ended with
  `edit_changed (window, nullptr)`, which looks like "refresh the window for
  whatever is active now". `edit_changed()` does its work only when its argument
  **is** the active document, and NULL is the active document only when the last
  one has just been closed — so closing one of several left the title, the status
  bar, the language menu and the encoding item all describing the document that
  had just gone. Passing `ACTIVE_DOC (window)` says what was meant. Not a porting
  bug; it predates the fork.
  → *A guard of the form `if (doc == ACTIVE_DOC (window))` turns a NULL argument
  into "only when there is no document", which is rarely what a caller passing
  NULL intends.*

- **Notebook frame gap** GTK+2 drew the page frame with `gtk_paint_box_gap (…,
  GTK_POS_TOP, gap_x, gap_width)` — a frame with a hole where the active tab meets it.
  The GTK+3 branch called `gtk_render_frame()` and explained in a comment that "themes
  can create gaps by omitting borders via CSS". They cannot. The tell was three dead
  stores: `moo_notebook_draw()` computes `gap_x` and `gap_width` over twenty-five lines
  and then reads them nowhere. `gtk_render_frame_gap()` is the replacement, and it takes
  the two **edges** of the gap, not an offset and a width.
  → *A value computed carefully and never used means the call that consumed it was lost
  in the port. The analyzer's dead-store reports are worth following for that reason
  alone.*
- **`moolineview.cpp` uninitialized read** The GTK+2 branch asked the parent for
  `scrollbar-spacing` unconditionally. The GTK+3 branch added a NULL check around the
  *call* and left the *read* of the value outside it, so a NULL parent put stack garbage
  into the calculation. Found by clang's `-Wsometimes-uninitialized`; gcc says nothing
  about it at any level, which is why the UI job compiles with clang.
  → *When a port adds a guard, check that everything depending on the guarded call moved
  inside it.*

### What the marker sweep found and did not fix

Reading all 53 `written by AI` blocks against their GTK+2 branch turned up 19 defects,
each of which is now a `FIXME:` naming itself. None is fixed. Ordered by what a user
would notice:

| where | what |
|---|---|
| `moopaned.c` `moo_paned_draw()` | §6a exactly: `draw_handle()` and `draw_border()` are never called. **Measured** — `event_window` is one pointer shared by all four MooPaneds of a window, while `gtk_cairo_should_draw_window()` answers TRUE for both child windows |
| `moopaned.c` ×4 | `gtk_style_context_get_border()` is **0** where `style->xthickness` was 1, so `border_size` and the handle's `shadow_size` are 0 and there is nothing to draw even once the above is fixed |
| `moonotebook.c` drag snapshot | `gdk_pixbuf_new()` does not clear; the window is copied into a *second* pixbuf which is then unref'd, so a dragged tab paints uninitialised heap. Its failure branch stores nothing and leaks, and `snapshot_pixmap` is a `cairo_surface_t*` that `drag_end()` frees with `g_object_unref()` |
| `mooiconview.c` | `set_scroll_adjustments` was dropped and GtkScrollable not implemented, so the file selector's icon view — its **default** view — never gets an adjustment and its scrollbar moves nothing |
| `mooutils-misc.cpp` | `accel_label_set_string()` sets accel 0/0 and stores the text in object data it feeds back to itself: every menu item going through `_moo_menu_item_set_accel_label()` shows an empty shortcut column |
| `moonotebook.c` tabs | `gtk_render_background()`+`gtk_render_frame()` with no style class where `gtk_paint_extension()` drew a tab, so each tab has a line between it and its own page |
| `moobigpaned.c` ×2 | the drop indicator draws on `outer`'s `cr` instead of the shaped `drop_outline` window (§6a again), and its mask unions *filled* rectangles where GTK+2 drew outlines |
| `moopane.c` | the five state-tinted copies of a button icon are all made with the widget's *current* state, so they are identical |
| `moopaned.c` `draw_handle()` | `state \|= GTK_STATE_SELECTED` mixes a `GtkStateType` (3) into a `GtkStateFlags`, asking for ACTIVE\|PRELIGHT. The PRELIGHT line beside it is right only because both spellings are 2 |
| `mooutils-treeview.cpp` | expander lines stroked at cairo's default width 2.0 on integer coordinates — grey and doubled where `gdk_draw_line()` was one pixel (§6c) |
| `moofileentry.c` | entry borders from `gtk_style_context_get_border()` alone, 0 on the themes measured; the completion popup is positioned with them |
| `moocommand-exe.cpp` | the `DISPLAY` set around `g_spawn_async()` is never read — the child is given an explicit `real_env` — while the process-wide environment is modified anyway |

Known and deliberately left alone: `draw_entry()` in `mooiconview.c` still uses
`gdk_cairo_create()` per row (deprecated since 3.22, bypasses the clip, works).

Four `#if 0` blocks survive the dead-code cleanup on purpose, because each documents a
feature that is disabled rather than abandoned: the tree view's drag source in
`moofileview.c` (drag and drop works in icon view only), `_moo_edit_print_options_dialog()`
in `mootextprint.c` (`medit.xml` still lists a `PrintOptions` item with no action behind
it), and the overwrite-prompt code in `moofileview.c` (`copy_files()` runs `cp -R` with
no prompt at all). Leave them until the features are decided.

---

## 6. The two GTK+3 porting mistakes that account for most bugs

Nearly every visual bug found so far is one of these two. Check for them first.

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

`gdk_cairo_create()` on a window inside `::draw` does still paint — the text
view's whitespace markers and the icon view's cells reach the screen that way —
but it is deprecated and bypasses the clip, so prefer transforming the context
you were handed. Where it appeared not to work, the real cause was ordering:
`GtkTextView` fills the border windows in its own `::draw` and wiped out what had
been painted before the chain-up.

Ordering matters too: `GtkTextView` fills its windows' backgrounds in its own
`::draw`, so anything painted *before* chaining up is wiped out. Line numbers are
painted after the chain-up; line backgrounds that must sit under the text are
painted after it with `CAIRO_OPERATOR_MULTIPLY`.

### c) Translating GDK drawing primitives to cairo one call at a time

`gdk_draw_polygon (…, FALSE, points, 3)` over three points a pixel apart draws
three pixels. The same path stroked with cairo gets a 1px antialiased line on
*either side* of every edge — the whitespace markers turned from neat dots into
blurry triangles. When the GTK+2 original addressed individual pixels, fill
1x1 rectangles with `CAIRO_ANTIALIAS_NONE` rather than stroking a path.

### b) Style calls that are silently dead on GTK+3

These compile, run, and do nothing — no warning:

| call | status |
|---|---|
| `gtk_style_context_set_background()` | no-op since 3.18 |
| `gdk_window_set_background[_rgba]()` | no-op; GDK does not paint window backgrounds |
| `gtk_style_context_add_region()` | no-op since 3.14 |
| `gtk_style_context_get_background_color()` | returns **fully transparent** on a bare widget context |
| `gtk_style_context_get_border()` | returns **0** on a bare widget context, where GTK+2's `style->xthickness`/`ythickness` were 1 — measured on `MooPaned`, and the reason four separate thickness translations in it draw nothing |

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

`ui/stest.c`-style throwaway probes are cheap: a 20-line GTK+3 program that
prints what these functions return settles such questions in one build.

## 7. Conventions

- Fix both GTK versions in one change where the API allows it, and **delete the
  `#if GTK_CHECK_VERSION` split** when one code path is correct for both.
- Remove the `FIXME:` on any block you fix, and if you review one and find nothing,
  remove it too — a marker that survives a reading it passed costs the next reader the
  same reading. If you find something and are not fixing it now, replace the marker
  with what you found and how you found it.
- Verify before claiming: build both, run both with the exit-code rule, screenshot when
  the change is visual, and state what was *not* verified. On a push CI compiles every
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
over the 344 own sources (150959 lines, the vendored directories excluded the way
`--target analyze` excludes them): 88570 indented lines use spaces against 205 with a
tab, calls are written `foo (x)` in 38891 places against 1180 without the space, 9462
braces sit on a line of their own against 576 trailing a statement, and `char *p`
outnumbers `char* p` 6139 to 97. Whitespace hygiene is at the same level — eleven lines
in nine files carry trailing whitespace, one file ends without a newline, nothing is
CRLF.

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
