# AGENTS.md — medit

Fork of medit (GTK+ text editor) **ported from GTK+2 to GTK+3**; both builds are kept
alive. Work branch: `main`.

The port was largely done by an AI and is buggy. Most defects found so far sit inside
blocks marked `/* FIXME: This code was written by AI and requires review */`.
Run `grep -rc "written by AI" src` for the current count; they are concentrated in:

`moopaned.c`, `moonotebook.c` and `mooiconview.c` between them hold most of them.

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
`-DCMAKE_BUILD_TYPE=Debug` (the default is RelWithDebInfo, i.e. `-g -O2`).

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

`.github/workflows/build.yml` runs on every push, all of it with
`-DENABLE_STRICT=ON`, so warnings are errors:

| job | what it covers |
|---|---|
| `deb` | ubuntu 26.04, 24.04, 22.04, debian 13, 12 — each for both toolkits, ten in all |
| `clang` | clang on debian:trixie, both toolkits, plus the `analyze` target |
| `fedora` | fedora:44, gtk-3, with LTO, which is the only place `-Wodr` has anything to see |
| `langs` | `src/mooedit/langs/check.sh` over the 187 language definitions and schemes |

`.github/workflows/codeql.yml` runs CodeQL over the gtk-3 build on pushes to `main`, on
pull requests, and weekly. It judges a pull request on the alerts it *introduces*, which
is why it can be a gate while the analyzer's existing findings are not zero.

So a source change does not need a container to prove it compiles anywhere. What is
still manual, and still worth a container: the **package** builds (`dpkg-buildpackage`,
`rpmbuild`, `makepkg` — CI compiles but does not package), anything visual, and anything
that has to actually run.

### The analyze target

The clang static analyzer over medit's own sources, the same command CI runs:

```bash
cmake -S . -B builda -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build builda --target analyze          # whole tree, about 2 minutes
clang-tidy -p builda src/mooutils/moopaned.c   # one file, a couple of seconds
```

It needs a **clang-configured build directory of its own**, a third beside `build2` and
`build3`. That is not taste: clang-tidy takes its flags from `compile_commands.json`,
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

### Code generation

Only three things are generated: `marshals.[ch]` (glib-genmarshal), `moo-pixbufs.h`
(gdk-pixbuf-csource) and `resources.c` (glib-compile-resources). Everything else that
used to be generated — interfaces, menu descriptions, the credits text — is a resource
now, listed in `src/resources.xml` and read at runtime. The build needs no python.
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
Ubuntu 20.04 were dropped when their support ended. `.github/workflows/build.yml`
compiles all five for both toolkits on every push, so ordinary source changes need no
container. What CI does *not* do is build the package, so check anything that touches
`debian/` by hand:

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

Fedora compiles with **LTO and gcc 14**, which see things the Debian build cannot:

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
* `.github/workflows/build.yml` — the `deb` job's `image:` matrix, and the Fedora
  release in the `fedora` job.
* `.github/workflows/codeql.yml` — the runner and its dependency list.
* `AGENTS.md` — "Debian package build (old distros)", which names the targets and the
  compiler span they cover.
* `debian/control`, `rpm/medit.spec`, `arch/PKGBUILD` — dependency names occasionally
  move between packages across releases.

A dropped distribution usually takes a workaround with it: retiring Debian 11 removed
the whole `snapshot.debian.org` recipe its dead archive needed. A new one is worth a
container run before it goes in the matrix — Ubuntu 26.04 arrived with gcc 15 and cmake
4.2, two and three major versions ahead of anything the tree had been built with.

The version itself lives in six places and they all have to move together. `1.3.4` was
cut like this:

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

Catalog state, for reference: `ru` is complete and is the one to check first; `es`, `fr`,
`pl`, `ja`, `fi`, `de` are 90%+; `cs`, `nl` and `zh_CN` are half empty, and adding a
stray translated string to their untouched sections is worse than leaving the gap. Also
pre-existing: `ja.po` and `pl.po` fail `msgfmt --check` on plural forms.

### Isolate config

medit writes `~/.local/share/medit/{prefs,file-list-config}.xml` and
`~/.cache/medit/{state,recent-files-editor}.xml` — the user's real settings. Always run
tests with:

```bash
env XDG_DATA_HOME=$S/xdg/data XDG_CACHE_HOME=$S/xdg/cache XDG_CONFIG_HOME=$S/xdg/config ./src/medit ...
```

### Two runtime checks worth turning on, and one not turned on yet

Both need medit to actually run, which is why neither is in CI: there is no test that
drives the program. Whoever writes that test should wire both into it.

**`G_ENABLE_DIAGNOSTIC=1`** costs nothing and can be used today. It is an environment
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

**Sanitizers are ready but deliberately not enabled.** Measured, so that nobody has to
measure again:

```bash
cmake -S . -B <dir> -DGTK_VERSION=3 \
    -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
```

* **ASan and UBSan: clean.** The tree builds with both, and a run that opens the file
  selector, the bookmark editor, the find dialog and the menus produces **zero** reports
  from either. They could be a gate from day one, unlike the static analyzer.
* **LSan: do not.** `detect_leaks=1` reports 633 records, 121 KB at a clean exit. 344 are
  purely library, and the 289 that name our code do not mean what they look like: the
  largest, 101 records from `mootextview.c:3554`, is `update_tab_width()`, which frees
  all three of the things it allocates. What is retained is pango's font and shaping
  cache, attributed to the nearest frame that is not a library. Run with
  `detect_leaks=0`.
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

## 3. UI sandbox (headless X + screenshots + synthetic input)

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
| `pkg_check_modules(GTK … ${GTK_PACKAGE})` defines `GTK_VERSION` as the version it found (`3.24.38`), shadowing the cache entry of the same name that selects the toolkit | anywhere below the Dependencies section of the top `CMakeLists.txt`, branch on `GTK_PACKAGE STREQUAL "gtk+-3.0"`, never on `GTK_VERSION` |
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
  about it at any level, which is the argument for the clang job in CI.
  → *When a port adds a guard, check that everything depending on the guarded call moved
  inside it.*

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
- Remove the `/* FIXME: This code was written by AI */` marker on any block you fix.
- Verify before claiming: build both, run both with the exit-code rule, screenshot when
  the change is visual, and state what was *not* verified. CI covers five distributions
  and two compilers on push, so what is worth doing by hand is what CI cannot: running
  the program, looking at it, and building the packages.
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
