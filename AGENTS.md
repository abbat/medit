# AGENTS.md — medit

Fork of medit (GTK+ text editor) being **ported from GTK+2 to GTK+3**. Work branch: `clean`.

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

The tree is configured **in-tree for GTK+3** (`./configure GTK_VERSION=3`). Just:

```bash
cd /home/abbat/my/medit && make -C src        # ~10s for one .o + link
```

Do **not** run `configure` here for GTK+2 — it aborts with "source directory already
configured" and fixing that would need `make distclean`, destroying the working build.

### GTK+2 reference build (for A/B comparison)

`configure` defaults to `GTK_VERSION=2`. Build a throwaway copy:

```bash
M=<scratch>/m2
rsync -a --exclude='.git' --exclude='*.o' --exclude='locale/' --exclude='src/medit' \
      /home/abbat/my/medit/ $M/
(cd $M && make distclean >/dev/null 2>&1; ./configure GTK_VERSION=2 && make -C src)
```

rsync of the working tree beats `git archive` plus hand-copied files: it picks up
uncommitted work and file deletions for free, and one stale copy is enough to make an
A/B comparison lie. Full build ≈ 3 min; incremental ≈ 10s. Both GTK+2 2.24.33 and
GTK+3 3.24.38 dev packages are installed. **Every fix must build clean and behave
correctly on both.**

**Do not use a pre-session build as the "GTK+2 reference".** Since the translations
fix the UI language differs, so pixel comparisons against an old build are noise. To
check a change for regressions, A/B *the same tree* with and without that one change:

```bash
git show HEAD:path/to/file > $M/path/to/file   # without
(cd $M && make -C src) && <screenshot>          # ...
cp path/to/file $M/path/to/file                 # with
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

### Translations

The catalogs are not installed unless `make install` is run, and the binary's
compiled-in `MOO_LOCALE_DIR` points at `/usr/local/share/locale`, so a build tree
run used to come up with an untranslated UI. `po/Makefile.am` now also lays the
catalogs out as `<builddir>/locale/<lang>/LC_MESSAGES/<domain>.mo`, and
`moo_get_locale_dir()` falls back to that tree when the configured directory has
no catalog. `MOO_LOCALE_DIR` in the environment still overrides both.

If the UI comes up in English, check `find locale -type f | wc -l` in the build
directory — if it is empty, run `make -C po && make -C po-gsv`.

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

## 5. Bug patterns already found (all from AI-ported blocks)

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
