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

At runtime, `G_ENABLE_DIAGNOSTIC=1` reports `GtkSettings:gtk-toolbar-style`,
`GtkSettings:gtk-menu-images` and `GtkAlignment:left-padding`/`right-padding` on a
bare start. Dialogs report more.

## Stage 0 — measuring

- [ ] 0.1 Replace `G_GNUC_BEGIN/END_IGNORE_DEPRECATIONS` outside `src/vendor/` with
  `MOO_BEGIN/END_IGNORE_DEPRECATIONS` from `mooutils/mooutils-macros.h`. A CMake option
  `-DMOO_SHOW_DEPRECATIONS=ON` makes them expand to nothing, so a non-strict build
  lists every remaining site. Record the count here.
- [ ] 0.2 Runtime inventory: run the whole UI suite with `G_ENABLE_DIAGNOSTIC=1`
  and collect every distinct "is deprecated" message from the logs, with the test
  that produced it. Find who reads `gtk-toolbar-style` and `gtk-menu-images`, our
  code or GTK+ itself (`break g_logv`). Record the list here.

## Stage 1 — single-site replacements

- [ ] 1.1 `gtk_misc_set_alignment` (`lsp-edits.cpp`) → `gtk_label_set_xalign`/`yalign`
  or `halign`/`valign`. `GtkTable` → `GtkGrid`.
- [ ] 1.2 `GdkColor` in `mooedit-tests.cpp` and `mooedit-perf.cpp` → `GdkRGBA`.
- [ ] 1.3 `gtk_style_context_get_background_color` (`mooiconview.cpp`,
  `mootextview.cpp`, `terminal-colors.cpp`) → `gtk_style_context_get (ctx, state,
  GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &rgba, NULL)`, then free the result.
- [ ] 1.4 `moofontsel.cpp`: drop `gtk_widget_push/pop_composite_child`. Replace
  `gtk_container_set_focus_chain` with child order or a `::focus` override. Check the
  Tab order by hand.
- [ ] 1.5 `GtkImageMenuItem` → `GtkMenuItem` through `_moo_menu_item_new`.
- [ ] 1.6 Push and CI.

## Stage 2 — `.ui` files and container classes

- [ ] 2.1 In `.ui`: `GtkVBox`/`GtkHBox` → `GtkBox` + `orientation`, `GtkHButtonBox` →
  `GtkButtonBox`, `GtkVSeparator` → `GtkSeparator` + `orientation`. A script, one commit
  per type. Check the defaults: a `GtkVBox` had `homogeneous` FALSE and `spacing` 0,
  the same as a `GtkBox`.
- [ ] 2.2 `GtkAlignment` in `.ui` → margins and `halign`/`valign` on the child.
  `left_padding` maps to `margin-start`, and so on. Also check `xalign`/`xpad` on
  anything that is not a `GtkLabel`: those are `GtkMisc` properties.
- [ ] 2.3 The C++ classes deriving `GtkVBox`/`GtkHBox`/`GtkAlignment`
  (`mooeditprogress`, `mooedittab`, `moofileview`, `mooplugin`, `mooprefspage`, and any
  that 0.1 turns up) → `GtkBox` with the orientation set in `_init`. Screenshot each.
- [ ] 2.4 `use_stock`/`stock` buttons in `.ui` → `label` with a mnemonic and a `GtkImage`
  with `icon-name`. **Translation trap:** GTK+'s own domain translated the stock labels.
  The new strings are ours and must reach `po/`, or the buttons turn English under
  a Russian locale.
- [ ] 2.5 Gate: `tests/lib/runner.py` fails a test on any "is deprecated" diagnostic.
  Push and CI.

## Stage 3 — stock to named icons

- [ ] 3.1 `moostock.cpp`: ship our own icons as an icon theme in the GResource
  (`gtk_icon_theme_add_resource_path`) and drop the GtkIconFactory/IconSet code and
  `gtk_icon_theme_add_builtin_icon`.
- [ ] 3.2 `gtk_image_new_from_stock`, `gtk_image_set_from_stock`,
  `gtk_widget_render_icon`, `gtk_style_context_lookup_icon_set` →
  `gtk_image_new_from_icon_name` and `gtk_icon_theme_load_icon`.
- [ ] 3.3 `GTK_STOCK_*` used as an icon → a freedesktop icon name.
- [ ] 3.4 `GTK_STOCK_*` used as a button or menu label → our own translatable string
  (the 2.4 trap applies here too).
- [ ] 3.5 MooAction's `stock-id` → `icon-name` + `label`. `gtk_stock_lookup` supplied a
  default label and accelerator; set both explicitly. Dump every action's accelerator
  before and after and diff the two: a lost default accelerator fails silently.
- [ ] 3.6 Remove `moostock.h` names that are left unused. Push and CI.

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
