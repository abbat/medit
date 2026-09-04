# AGENTS.md — medit

Fork of medit (GTK+ text editor) being **ported from GTK+2 to GTK+3**. Work branch: `clean`.

The port was largely done by an AI and is buggy. Most defects found so far sit inside
blocks marked `/* FIXME: This code was written by AI and requires review */`.
**64 such markers remain** (`grep -rc "written by AI" src`), concentrated in:

| file | markers |
|---|---|
| `src/mooutils/moopaned.c` | 23 |
| `src/mooutils/moonotebook.c` | 14 |
| `src/moofileview/mooiconview.c` | 10 |
| `src/mooutils/moopane.c` | 3 |

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
mkdir -p $M && (cd /home/abbat/my/medit && git archive HEAD | tar -x -C $M)
cp <your modified files> $M/<same paths>          # keep in sync by hand
(cd $M && ./configure GTK_VERSION=2 && make -C src)
```

Full build ≈ 3 min; incremental ≈ 10s. Both GTK+2 2.24.33 and GTK+3 3.24.38 dev
packages are installed. **Every fix must build clean and behave correctly on both.**

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

Known and deliberately left alone: `draw_entry()` in `mooiconview.c` still uses
`gdk_cairo_create()` per row (deprecated since 3.22, bypasses the clip, works).

---

## 6. Conventions

- Fix both GTK versions in one change where the API allows it, and **delete the
  `#if GTK_CHECK_VERSION` split** when one code path is correct for both.
- Remove the `/* FIXME: This code was written by AI */` marker on any block you fix.
- Verify before claiming: build both, run both with the exit-code rule, screenshot when
  the change is visual, and state what was *not* verified.
- One logical fix per commit. Commit message: what the GTK+2 code did, why the GTK+3
  port broke it, the evidence (verbatim critical text / gdb output), and the fix.
- Commit only source files — the tree is full of untracked build artifacts.
- Trailer:
  ```
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
  ```
- Do not push unless asked.
