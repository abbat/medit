# Removing deprecated API

The goal: no deprecated GTK+/GLib API anywhere it can be removed, including the
places that were exempted long ago. An exemption is not a reason to keep it.

This file is the plan and the progress record. A session that picks the work up
reads it top to bottom, takes the first unchecked step, and ticks it off in the
same commit that does it. Each step names how to check it and who can do it.

**Rules for every step**

- Strict build with gcc (`build-warn`) and clang (`build-clang`), unit tests
  (`ctest -L unit` through the `suite-run` agent), then commit.
- One commit per symbol or group. Push gtk3-only after each batch and wait for CI
  (`ci-triage` agent).
- Visual changes need a screenshot. Anything a test does not cover goes into the
  commit message as "not verified".
- Builds, test runs and CI waits go to Haiku. Mechanical edits with a clear recipe
  go to Sonnet. Design and review stay with the main session.

## Inventory (2026-10-04, `src/` without `src/vendor/`)

| what | size |
|---|---|
| GtkAction family (MooAction derives from GtkAction) | 278 lines, 35 files |
| `GTK_STOCK_*` | 257 lines, 33 files |
| `gtk_stock_*`, `*_from_stock`, `render_icon` | ~100 lines |
| GtkIconFactory/IconSet/IconSource (`moostock.cpp`) | 27 lines |
| `.ui`: `GtkVBox` 77, `GtkHBox` 34, `GtkHButtonBox` 15, `GtkAlignment` 41, `GtkVSeparator` 1 | ~27 files |
| `.ui`: `use_stock` 23, `stock` 19 | 11 files |
| focus chain, composite child, `get_background_color`, `GdkColor`, `GtkTable`, `GtkMisc`, `GtkImageMenuItem` | ~40 lines |
| `G_GNUC_BEGIN_IGNORE_DEPRECATIONS` blocks | 114 in 53 files |

At runtime, `G_ENABLE_DIAGNOSTIC=1` reports five names over the UI suite; 0.2 lists
them.

## Stage 0 — measuring

- [x] 0.1 `-DMOO_SHOW_DEPRECATIONS=ON` makes `G_GNUC_BEGIN/END_IGNORE_DEPRECATIONS`
  expand to nothing (`src/sysheaders.h`), so a non-strict build lists every remaining
  site. Measure it with clang, which also reports `GTK_STOCK_*` and deprecated parent
  types:
  ```
  cmake -S . -B build-deps -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
        -DENABLE_STRICT=OFF -DMOO_SHOW_DEPRECATIONS=ON -DENABLE_UNIT_TESTS=ON
  make -C build-deps -j8 -k >deps.log 2>&1
  grep "warning:.*Wdeprecated-declarations" deps.log | grep -v /vendor/ \
      | grep -oE "^[^ ]+:[0-9]+:[0-9]+" | sort -u | wc -l
  ```
  | date | sites | breakdown |
  |---|---|---|
  | 2026-10-04 | 384 | `GtkStock` 255; GtkAction family ~75; icon factory/set/source and `gtk_stock_*` ~30; `render_icon`/`*_from_stock` 12; `gtk_vbox_get_type` 4, `gtk_alignment_get_type` 1; composite child 6; `get_background_color` 3; `gdk_color_get_type` 2; focus chain 1 |

  `gtk_misc_set_alignment`, `GtkTable` and `GtkImageMenuItem` in the inventory above
  were grep hits in comments; there is no code to change for them.
- [x] 0.2 Runtime inventory: the runner sets `G_ENABLE_DIAGNOSTIC=1` and prints a
  `deprecated:` line per test. Over the 162 UI tests, 2026-10-04:
  | name | tests | source | stage |
  |---|---|---|---|
  | `GtkCellRendererPixbuf:stock-id` | 162 | stock icons in tree views | 3 |
  | `GtkSettings:gtk-menu-images` | 162 | GtkImageMenuItem made by `gtk_action_create_menu_item` | 4.1 |
  | `GtkWidget::visibility-notify-event` | 162 | libvte's VteTerminalAccessible, when atk-bridge asks for it (gdb: `g_signal_connect_data` ← libvte ← libatk-bridge); absent without a11y. Not ours to remove | — |
  | `GtkSettings:gtk-button-images` | 61 | stock buttons, presumably; not traced | 3 |
  | `GtkButton:use-stock` | 60 | stock buttons in `.ui` and code | 3 |

  Fixed while taking it: `gtk-toolbar-style` (01587e0), `rules-hint` (2149438),
  GtkMisc paddings (b46849f), `gtk-show-unicode-menu` (ddddd17). The GtkAlignment
  paddings, `GtkImage:stock` and `xscale`/`yscale` of an earlier inventory are gone
  with stage 2.

## Stage 1 — single-site replacements

- [x] 1.1 Nothing to do (see 0.1).
- [x] 1.2 `GdkColor` in `mooedit-tests.cpp` and `mooedit-perf.cpp` → `GdkRGBA`.
- [x] 1.3 `gtk_style_context_get_background_color` (`mooiconview.cpp`,
  `mootextview.cpp`, `terminal-colors.cpp`) → `gtk_style_context_get (ctx, state,
  GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &rgba, NULL)`, then free the result.
- [x] 1.4 `moofontsel.cpp`: drop `gtk_widget_push/pop_composite_child`. Replace
  `gtk_container_set_focus_chain` with child order or a `::focus` override. Check the
  Tab order by hand.
- [x] 1.5 Nothing to do (see 0.1).
- [ ] 1.6 Push and CI.

## Stage 2 — `.ui` files and container classes

- [x] 2.1 In `.ui`: `GtkVBox`/`GtkHBox` → `GtkBox` + `orientation`, `GtkHButtonBox` →
  `GtkButtonBox`, `GtkVSeparator` → `GtkSeparator` + `orientation`. A script, one commit
  per type. Check the defaults: a `GtkVBox` had `homogeneous` FALSE and `spacing` 0,
  the same as a `GtkBox`.
- [x] 2.2 `GtkAlignment` in `.ui` → margins and `halign`/`valign` on the child.
  `left_padding` maps to `margin-start`, and so on. Also check `xalign`/`xpad` on
  anything that is not a `GtkLabel`: those are `GtkMisc` properties.
- [x] 2.3 The C++ classes deriving `GtkVBox`/`GtkHBox`/`GtkAlignment`
  (`mooeditprogress`, `mooedittab`, `moofileview`, `mooplugin`, `mooprefspage`, and any
  that 0.1 turns up) → `GtkBox` with the orientation set in `_init`. Screenshot each.
- [x] 2.4 `use_stock`/`stock` buttons in `.ui` → a translatable `label` with `use_underline`
  (no image: GTK+3 hides button images by default, so the stock buttons showed none);
  `GtkImage` `stock` → `icon_name` (each pair probed pixel-identical).
  **Translation trap:** GTK+'s own domain translated the stock labels. The new strings
  are ours and carry gtk30's "Stock label" translations, so the buttons read as before.
- [x] 2.5 Gate: `tests/lib/runner.py` fails a test on any deprecated name outside
  `KNOWN_DEPRECATED`, which holds the five of 0.2. Each stage takes its names out of
  that list in the commit that stops using them. Checked against an empty list: 158
  of 162 UI tests failed with `FAIL: deprecated API in use: …`.

## Stage 3 — stock to named icons

The callers move first and the factory goes last: `moostock.cpp` registers the
labels and the icon sets every other step still looks up, so removing it first
would blank icons and labels all over the UI.

Facts the survey of 2026-10-04 established, so they need not be found again:

- **Accelerators are not at risk.** GtkAction takes a stock item's accelerator
  only in `gtk_action_group_add_action_with_accel()`; medit adds actions with
  plain `gtk_action_group_add_action()` and sets its own `default-accel`. What a
  stock id supplies is the label (`gtkaction.c`, `set_stock_id`) and the image.
  The before/after accelerator diff in 3.6 is still the check.
- **One persisted path: bookmarks.** `moobookmarkmgr.cpp` writes
  `<bookmark icon="…">` under `FileSelector/bookmarks`, with any id from
  `gtk_stock_list_ids()`. Everything else (pane labels, line marks, prefs pages,
  usertools, LSP, file selector) is a literal in the tree.
- **No `.ui` file uses `use_stock` or `stock`.** `mooaccelbutton.ui` names
  `gtk-cancel`/`gtk-ok` as `icon_name`; `medit.xml` and `moofileview.xml` carry
  `stock-label`/`icon-stock-id` attributes.
- **Some `gtk-*` icons exist only in GTK's own factory** — apply, cancel, ok,
  yes, no, edit, index, preferences, select-color, select-font. They need a
  freedesktop substitute or no icon, never the user's theme by luck.
- **Moving labels off GTK's catalog loses GTK's translations** of them; take the
  existing `gtk30` "Stock label" translations into our `.po` files, as in 2.4.
- **Unused `moostock.h` ids:** BUILD, CLOSE_PROJECT, COMPILE, DOC_DELETED,
  DOC_MODIFIED, DOC_MODIFIED_ON_DISK, EXECUTE, MENU, NEW_PROJECT, OPEN_PROJECT,
  PROJECT_OPTIONS.

- [ ] 3.1 Dialog and button text: `gtk_dialog_add_button`/`gtk_info_bar_add_button`
  given a `GTK_STOCK_*` → our own `_("_Cancel")`-style string. Also
  `moofiledialog.cpp`'s `get_string_maybe_stock` goes; callers pass plain titles.
- [ ] 3.2 Menu items: `_moo_menu_item_new_from_stock()` (`mooutils-misc.cpp`) →
  a label + icon-name constructor at every caller, then delete it.
- [ ] 3.3 Images: `gtk_image_*_from_stock` → `_from_icon_name`;
  `gtk_widget_render_icon` → `gtk_icon_theme_load_icon` (`moolinemark.cpp`,
  `moofileicon.cpp`); `_moo_window_set_icon_from_stock` →
  `gtk_window_set_icon_name`; cell renderers' `stock-id` → `icon-name`. Removes
  `GtkCellRendererPixbuf:stock-id` from `KNOWN_DEPRECATED`.
- [ ] 3.4 Data fields: `MooPaneLabel.icon_stock_id`, MooPrefsPage `icon-stock-id`,
  MooLineMark `stock-id`, MooUiXml `stock-id`/`icon-stock-id`/`stock-label` →
  icon names and labels, with every in-tree caller and the two `.xml` files.
- [ ] 3.5 Bookmarks: map stock ids to icon names on load (unknown → `folder`),
  write icon names, build the icon combo from icon names. Test the round trip with
  an rc file written by the old code.
- [ ] 3.6 MooAction `stock-id` → `icon-name` + an explicit `label`, then drop
  `gtk_stock_lookup` from `mooactionbase.cpp`. Four actions have no label of their
  own: GoToCurrentDocDir (`moofileselector.cpp`), Cut/Copy/Paste
  (`mooeditaction-factory.cpp`). Dump every action's accelerator before and after
  and diff the two.
- [ ] 3.7 `moostock.cpp`: the pixmaps become an icon theme in the GResource
  (`gtk_icon_theme_add_resource_path`); the factory, the aliases,
  `gtk_stock_add_static` and `_moo_stock_init` go. Fix `mooaccelbutton.ui`.
  Removes `GtkButton:use-stock` and `GtkSettings:gtk-button-images` from
  `KNOWN_DEPRECATED`, if stock buttons were their source.
- [ ] 3.8 Remove what is left unused in `moostock.h`, or the header. Build with
  clang too — it flags every `GTK_STOCK_*` macro gcc lets through. Push and CI.

## Stage 4 — GtkAction

Design first, written into this file and agreed with the user before any code.
The working proposal: MooAction becomes a plain GObject carrying only what the tree
uses (name, label, tooltip, icon, sensitive/visible, accel path, toggle/radio, proxy
creation and sync). MooUiXml and the plugin API stay. The alternative,
GAction/GMenu/GtkApplication, rewrites menus, toolbars, user tools and the
accelerator editor for the same result.

- [ ] 4.0 Design: list every GtkAction feature actually used (properties, signals,
  proxies, Activatable sync), and the order of migration.
- [ ] 4.1 Our own proxy creation and sync in `moouixml.cpp` instead of
  `gtk_action_create_menu_item`/`create_tool_item` and GtkActivatable.
- [ ] 4.2 MooActionGroup/collection without GtkActionGroup.
- [ ] 4.3 MooAction, MooToggleAction and MooRadioAction rebased on GObject.
- [ ] 4.4 The remaining `gtk_action_*` callers: accel prefs, bookmarks,
  fileview tools, LSP, spell, user tools.
- [ ] 4.5 Hand check: menus, toolbar, accelerators, toggle/radio, user tools,
  plugin actions. Push and CI.

## Stage 5 — vendored gtksourceview (needs the user's decision)

- [ ] 5.1 Measure: build `src/vendor` without `-Wno-deprecated-declarations` and count.
- [ ] 5.2 The user decides whether to fix it in place (upstream gtksourceview 3 is
  finished, so "belongs upstream" no longer applies) or to keep the exemption.

## Stage 6 — locking it in

- [ ] 6.1 Delete `MOO_BEGIN/END_IGNORE_DEPRECATIONS` and the CMake option.
- [ ] 6.2 Drop the exemptions from `AGENTS.md`, and update `doc/running.md` (the
  list of four runtime warnings).
- [ ] 6.3 Delete this file, or reduce it to what it taught.
