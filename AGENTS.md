# AGENTS.md — medit

Fork of medit (GTK+ text editor) **ported from GTK+2 to GTK+3**; both builds are kept
alive. Work branch: `main`.

The port was largely done by an AI and is buggy. Most defects found so far sit inside
blocks marked `/* FIXME: This code was written by AI and requires review */`.
Run `grep -rc "written by AI" src` for the current count; they are concentrated in:

`moopaned.c`, `moonotebook.c` and `mooiconview.c` between them hold most of them.

The `_DEAD_CODE_*.md` files in the root, if they are still there, are **not reliable**:
their line numbers roughly hold but the descriptions are invented (they call a complete
function an "incomplete stub", name a macro that does not exist, and claim working file
copying is disabled). Re-read the code before acting on them.

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
reach (the drop dialog needs a real drag and drop). Cheap check for all of them at once:
collect the ids each `.ui` declares, collect what the code asks
`moo_builder_get/take/reparent` for, and compare. That is how the one stale id left in
mootextprint.c was found.

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

The package targets **Debian 11, Ubuntu 20.04 and 22.04** — much older toolchains than
this machine (gcc 12, cmake 3.25, glib 2.74+). The local build proves nothing about
them, so check anything that touches build files or headers in containers:

```bash
docker build -t medit-u2004 - <<'EOF'
FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq && apt-get install -y -qq build-essential debhelper cmake \
    pkg-config intltool libgtk2.0-dev libgtk-3-dev libxml2-dev libjpeg-dev
EOF
S=<scratch>                                                  # session scratch dir
git ls-files -z | tar --null -T - -czf $S/medit-src.tar.gz   # tracked files + local edits
docker run --rm -v $S:/w medit-u2004 bash -c 'set -o pipefail
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

The oldest cmake among the three is 3.16 (Ubuntu 20.04), which is what
`cmake_minimum_required` targets. Ubuntu 20.04 (gcc 9, glib 2.64) is the strictest
compiler of the three. Ubuntu images pull normally; **Debian 11 does not** — bullseye is
past EOL, its `main` moved to archive.debian.org while `bullseye-security` is gone from
deb.debian.org and not yet archived, so `apt-get install` dies with 404s. Use
snapshot.debian.org on the `debian/eol:bullseye` image:

```
deb http://snapshot.debian.org/archive/debian/20250601T000000Z bullseye main
deb http://snapshot.debian.org/archive/debian-security/20250601T000000Z bullseye-security main
```
plus `Acquire::Check-Valid-Until "false";`.

What actually broke there — none of it visible in a local build:

* **Unnamed parameters** in C function definitions (`static void f (Foo *x, gpointer)`) —
  legal in C++ and C23 only. gcc 12 accepts them *silently* even at `-std=gnu17`; gcc 9
  errors with "parameter name omitted". Write `G_GNUC_UNUSED gpointer data`.
* **Symbols newer than the oldest target glib**, e.g. `G_REGEX_DEFAULT` (2.74) on Ubuntu
  22.04's 2.72. Use `(GRegexCompileFlags) 0`, as the rest of the tree does.
* **`g_object_ref` in C++** returns `gpointer` on older glib (no `typeof` magic), so
  assigning it to a typed field needs an explicit cast.

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
| gcc 12 accepts C constructs that gcc 9/10 reject (unnamed parameters), so a clean local build says nothing about Debian 11 / Ubuntu 20.04 | see "Debian package build (old distros)" |
| `pkg_check_modules(GTK … ${GTK_PACKAGE})` defines `GTK_VERSION` as the version it found (`3.24.38`), shadowing the cache entry of the same name that selects the toolkit | anywhere below the Dependencies section of the top `CMakeLists.txt`, branch on `GTK_PACKAGE STREQUAL "gtk+-3.0"`, never on `GTK_VERSION` |
| A key name in an accelerator string is **case sensitive**: `"<Ctrl>Space"` does not parse and `"<Ctrl>space"` does. `_moo_accel_register()` drops an unparsable accelerator without a word, so the action simply has no key | test it: `gtk_accelerator_parse()` returns key 0. `MOO_EDIT_ACCEL_COMPLETE` carried this mistake unused since 1.2.92 |
| `_moo_get_accel()` and `_moo_get_default_accel()` read **different maps**: the first holds accelerators that were actually set, the second the defaults registered with the action. An accelerator that has only ever had its default reads as empty from the first | ask the first, fall back to the second — that is what a plugin matching its own accelerator by hand has to do |
| The focused widget sees a key before the accelerators (`moo_window_key_press_event`), so a plugin action whose key the text view consumes — `Ctrl+Space` — never fires | match the accelerator by hand in the view's `key-press-event`, as the terminal and the LSP completion do |
| `MooMarkup` turns a `<![CDATA[…]]>` section into a **comment node**, where `moo_markup_get_content()` cannot see it | put the text in as ordinary escaped element content; `GMarkup` unescapes it on the way in |
| `GMarkup` accepts a `--` **inside an XML comment**; expat and every other conforming parser reject it. A comment mentioning a command line like `clangd --background-index` therefore loads in medit and fails everywhere else | check any xml the user is meant to edit with a real parser: `python3 -c "import xml.dom.minidom as m; m.parse('f.xml')"` |
| A hover tooltip and a synthetic right click do not mix: with the pointer left resting on the target, the tooltip comes up and the context menu does not, and the run reads as a regression in whatever the menu was going to do (mechanism not established — the click may be swallowed, or the menu covered and dismissed) | move the pointer and click in the same breath, without a dwell, then screenshot and confirm the menu is up before clicking an item in it |
| A build-tree run also reads data from an **installed** medit package (`/usr/share/medit/`), so its stale `menu.xml` produces warnings about our tree | reproduce with `MOO_DATA_DIRS=<dir>` holding the tree's own xml — but note it *replaces* the whole search list, so style schemes and the file-selector plugin stop loading; use it to attribute a warning, not to test the UI |

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
  the change is visual, and state what was *not* verified.
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
