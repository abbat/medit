# Running medit and verifying it works

*For agents working in this tree. Read `AGENTS.md` first.*



## The exit-code rule (most important)

medit **exits in ~0.15s when it crashes**, and a crash produces no stderr. Grepping
stderr for criticals therefore reports success on a segfaulting binary. This already
caused one broken commit to be pushed. Always:

```bash
timeout 15 ./src/medit --new-app FILE >log 2>&1; echo "exit=$?"
# 124 = survived the full 15s = good.   139 = SIGSEGV.   0 = exited early, investigate.
```

`--new-app` is mandatory — medit is single-instance and will otherwise hand the file to
a running copy and exit.

## A fixture that props the feature up hides the bug in it

Every test of the language server client began by copying `lsp.xml` into the sandbox's
data directory, because without it nothing started. That is precisely the bug: the
defaults were only read from the install, so a build-tree run had none, and the menu
item meant to hand the user a copy of them wrote an empty file. Every green run said
nothing about it, because every one of them had quietly supplied by hand the thing that
was missing.

When a test needs a step to make the feature work at all, ask what a user's first run
does instead of that step. Set the sandbox up the way an untouched machine is — empty
`XDG_DATA_HOME`, nothing installed — and see what happens before adding anything to it.

## Translations

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

## Isolate config

medit writes `~/.local/share/medit/{prefs,file-list-config}.xml` and
`~/.cache/medit/{state,recent-files-editor}.xml` — the user's real settings. Always run
tests with:

```bash
env XDG_DATA_HOME=$S/xdg/data XDG_CACHE_HOME=$S/xdg/cache XDG_CONFIG_HOME=$S/xdg/config ./src/medit ...
```

## Two runtime checks, now wired into the UI tests

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

## Backtraces

```bash
G_DEBUG=fatal-criticals gdb -batch -ex run -ex "bt 25" --args ./src/medit --new-app FILE
```

---
