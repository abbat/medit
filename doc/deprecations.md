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
  | `VteTerminal::window-title-changed` | 163 | the same VteTerminalAccessible (`vteaccess.cc` connects it even in VTE 0.80); our terminal plugin uses `termprop-changed` from VTE 0.78 on. Not ours to remove | — |
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
  the same as a `GtkBox`. **Not the same: `expand`.** `GtkVBox`/`GtkHBox` call
  `_gtk_box_set_old_defaults()`, so `gtk_container_add()` and GtkBuilder packed their
  children with `expand` TRUE; a plain `GtkBox` packs with FALSE. Every child without
  an `expand` in its `<packing>` collapsed (`lsp.plugin_toggle` found it: the plugin
  list had no extent). Fixed by giving those children `expand` True in the `.ui` files,
  and in C++ by packing with `gtk_box_pack_start (…, TRUE, TRUE, 0)` where a former
  VBox gets a child through `gtk_container_add()`. Internal children (a dialog's
  `vbox`) were never old-default boxes and are left alone.
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

- [x] 3.1 Dialog and button text: `gtk_dialog_add_button`/`gtk_info_bar_add_button`
  given a `GTK_STOCK_*` → our own `_("_Cancel")`-style string. Also
  `moofiledialog.cpp`'s `get_string_maybe_stock` goes; callers pass plain titles. Done: about 40 buttons and 2 titles across 12 files now take our own `_("_Cancel")`-style strings; `get_string_maybe_stock` is gone and the translations were copied from gtk30 into our catalogs.
- [x] 3.2 Menu items: `_moo_menu_item_new_from_stock()` (`mooutils-misc.cpp`) →
  a label + icon-name constructor at every caller, then delete it. Done: every caller passes `_("_Undo")`-style label + icon name to `_moo_menu_item_new()`; ids with no freedesktop icon (cancel, font, colour, edit, move, link) pass no image. The function survives only as a static in `moouixml.cpp`, fed by the xml stock id, and goes with 3.4.
- [x] 3.3 Images: `gtk_image_*_from_stock` → `_from_icon_name`;
  `gtk_widget_render_icon` → `gtk_icon_theme_load_icon` (`moolinemark.cpp`,
  `moofileicon.cpp`); `_moo_window_set_icon_from_stock` →
  `gtk_window_set_icon_name`; cell renderers' `stock-id` → `icon-name`. Removes
  `GtkCellRendererPixbuf:stock-id` from `KNOWN_DEPRECATED`. Partly done: the sites
  whose id is a literal GTK one are converted (`moodialogs.cpp`, `moofileselector.cpp`,
  `moofileicon.cpp` except its `MOO_STOCK_FILE` fallback, the `moofilelist.cpp` and
  `mooeditprefspage.cpp` cell renderers). What remains takes its id from data or from
  a pixmap of ours: `moolinemark.cpp` render_icon, `moopane.cpp` and
  `_moo_window_set_icon_from_stock` (pane label, 3.4); `moouixml.cpp` (3.4);
  `mooprefsdialog.cpp` and `moobookmarkmgr.cpp`/`moobookmarkview.cpp` renderers
  (3.4/3.5, so `GtkCellRendererPixbuf:stock-id` stays in `KNOWN_DEPRECATED`);
  `get_stock_icon (MOO_STOCK_FILE)` in `moofileicon.cpp` (3.7).
  Done with 3.4: the pane label, `_moo_window_set_icon_from_stock` (deleted),
  `moolinemark.cpp`, `moouixml.cpp` and the prefs dialog renderer (`icon-name`) are
  converted. Done with 3.5: the bookmark renderers too, so
  `GtkCellRendererPixbuf:stock-id` is out of `KNOWN_DEPRECATED`. Left:
  `get_stock_icon (MOO_STOCK_FILE)` (3.7).
- [x] 3.4 Data fields: `MooPaneLabel.icon_stock_id`, MooPrefsPage `icon-stock-id`,
  MooLineMark `stock-id`, MooUiXml `stock-id`/`icon-stock-id`/`stock-label` →
  icon names and labels, with every in-tree caller and the two `.xml` files.
  Done: `MooPaneLabel.icon_name`, MooPrefsPage `icon-name`, MooLineMark `icon-name`
  (`moo_line_mark_set_icon_name`), MooUiXml `icon-name`; the unused `stock-id`
  attribute and `_moo_menu_item_new_from_stock` are gone, `stock-label="gtk-edit"`/
  `"gtk-help"` became `_label="_Edit"`/`"_Help"` (`_Edit` translated from gtk30).
  Ids with no freedesktop icon got a substitute: preferences → `preferences-system`
  (also the default for a page without one), edit → `accessories-text-editor`,
  index → `view-list`, the Shortcuts page → `preferences-desktop-keyboard-shortcuts`,
  Plugins → `application-x-addon`.
  Our own pixmaps are named `medit-terminal`, `medit-file-selector`, `medit-file`;
  they are an icon theme in the GResource (3.7), laid out as `/medit/icons/<size>x<size>/apps/`
  and registered with `gtk_icon_theme_add_resource_path`. Nothing here is read from user config, so no id mapping
  on load (bookmarks are 3.5).
- [x] 3.5 Bookmarks: map stock ids to icon names on load (unknown → `folder`),
  write icon names, build the icon combo from icon names. Test the round trip with
  an rc file written by the old code. Done: `MooBookmark.icon_name`;
  `_moo_bookmark_icon_name_for_stock()` maps `gtk-*`/`moo-*` ids and keeps anything
  else; `_moo_bookmark_mgr_load_node()` is the load without the prefs global, so
  `unit.moofileview.bookmark.load-old-rc` feeds it an rc as the old code wrote it.
  The editor's combo is a fixed list of eleven icon names.
- [x] 3.6 MooAction `stock-id` → `icon-name` + an explicit `label`, then drop
  `gtk_stock_lookup` from `mooactionbase.cpp`. Four actions have no label of their
  own: GoToCurrentDocDir (`moofileselector.cpp`), Cut/Copy/Paste
  (`mooeditaction-factory.cpp`). Dump every action's accelerator before and after
  and diff the two.
  Done: every action sets `icon-name` and carries its own `_()` label, and the
  `display-name`/`tooltip` that were stock ids are plain `_()` strings (new msgids,
  translated from gtk30). `moo_action_base_set_{display_name,label,tooltip}` no
  longer call `gtk_stock_lookup`, and `strip_underscore` is gone. The 109 action
  accelerators dumped before and after are identical. Our own Keyboard id maps to
  `preferences-desktop-keyboard-shortcuts`, as on the Shortcuts page.
- [x] 3.7 `moostock.cpp`: the pixmaps become an icon theme in the GResource
  (`gtk_icon_theme_add_resource_path`); the factory, the aliases,
  `gtk_stock_add_static` and `_moo_stock_init` go. Fix `mooaccelbutton.ui`.
  Removes `GtkButton:use-stock` and `GtkSettings:gtk-button-images` from
  `KNOWN_DEPRECATED`, if stock buttons were their source.
  Done: `moostock.cpp` and `moostock.h` are both gone (nothing but `MOO_STOCK_FILE` was
  still read, and that is now `medit-file` by name in `moofileicon.cpp`). The pixmaps
  are in `resources.xml` under `/medit/icons/<size>x<size>/apps/` (`medit-file`,
  `medit-file-selector`, `medit-terminal`, `medit`), added once in the `MooEditor`
  constructor with `gtk_icon_theme_add_resource_path (…, "/medit/icons")`. The prefix
  must not contain the theme name: with `/medit/icons/hicolor` the names did not
  resolve and the pane labels showed the missing-image icon. `mooaccelbutton.ui` lost
  its two `gtk-cancel`/`gtk-ok` images.
  A local UI run with `ctest -V` (the lines are not in the CI log) then found
  `use-stock` in no test and `gtk-button-images` in two. `GtkButton` reads the setting
  in `show_image()` unless `always-show-image` is already set, so
  `gtk_button_set_image()` before `gtk_button_set_always_show_image()` reads it once:
  the About dialog's two buttons and the Credits close button now set the flag first.
  The other was the overwrite dialog's `_Replace` button, whose `document-save-as`
  image GTK+3 never showed (the setting defaults to FALSE); it is a plain dialog button
  now. Both names are out of `KNOWN_DEPRECATED`.
- [x] 3.8 Remove what is left unused in `moostock.h`, or the header (gone with 3.7). Build with
  clang too — it flags every `GTK_STOCK_*` macro gcc lets through. Push and CI.

## Stage 4 — GtkAction

Design first, written into this file and agreed with the user before any code.
The working proposal: MooAction becomes a plain GObject carrying only what the tree
uses (name, label, tooltip, icon, sensitive/visible, accel path, toggle/radio, proxy
creation and sync). MooUiXml and the plugin API stay. The alternative,
GAction/GMenu/GtkApplication, rewrites menus, toolbars, user tools and the
accelerator editor for the same result.

- [x] 4.0 Design: list every GtkAction feature actually used (properties, signals,
  proxies, Activatable sync), and the order of migration.
  Inventory (2026-10-05): 299 lines in 35 files name the family, about 47 of them
  real `gtk_action_*`/`gtk_toggle_action_*` calls; the rest are `GtkAction *` types
  and casts. GtkUIManager and radio actions are not used at all, so only MooAction
  and MooToggleAction need a new base. No binding (.defs, gir, lua, python) exposes
  GtkAction.
  - Used from GtkAction: `name`, `label`, `tooltip`, `icon-name`, `sensitive`,
    `visible`, `accel-path`, `activate` (signal and vfunc), and `active`/`toggled`
    on toggles. Never used: `stock-id`, `gicon`, `short-label`, `is-important`,
    `hide-if-empty`, `visible-horizontal/vertical`, `accel-closure`, everything
    radio.
  - Everything else (`display-name`, `default-accel`, `no-accel`, `dead`,
    `has-submenu`, `use-underline`, the closure properties) is already Moo's own,
    in `mooactionbase.cpp` and `mooaction.cpp`.
  - Proxies: `moouixml.cpp` creates menu items (`gtk_action_create_menu_item`) and
    tool items (`gtk_action_create_tool_item`, or `gtk_activatable_set_related_action`
    for a split button); five classes override `create_menu_item` (MooMenuAction,
    the encodings menu, special characters, open recent, go-to-bookmark). GtkActivatable
    does all syncing; the one manual `sync_proxies()` is in `mooeditwindow.cpp`.
  - Accelerators: `moo_window_add_action()` gives each action an accel path and the
    window's accel group; GtkAction connects the closure and the menu item shows the
    label. Without it: `gtk_accel_group_connect_by_path()` and
    `gtk_menu_item_set_accel_path()` ourselves. GtkAccelMap itself is not deprecated
    and stays.
  - Order, each step building and working:
    a. `moo_action_*`/`moo_action_group_*` accessors that still delegate to GtkAction;
       move the ~47 call sites onto them, so later steps touch one file.
       Done: step a. The group's add is `moo_action_group_insert_action`, since
       `moo_action_group_add_action` is already the action factory; the MooAction
       `activate` vfunc became `moo_action_activate_real` for the same reason.
    b. (4.1) Proxy creation behind our own helpers and a MooAction `create_menu_item`
       slot; our own proxy sync from `notify::` (label, tooltip, icon, sensitive,
       visible, active). Risk: toggle feedback loops, menu icons, accel labels.
       Done: step b. `moo_action_create_menu_item`/`_tool_item`/`_sync_proxies` make plain
       GtkMenuItem/GtkCheckMenuItem and GtkToolButton/GtkToggleToolButton/GtkMenuToolButton and
       follow `notify::` and `toggled`; overriders fill `MooActionClass.create_menu_item`.
       `connect-proxy`/`disconnect-proxy` had no listeners and are gone; `gtk-menu-images` left
       `KNOWN_DEPRECATED`.
    c. (4.2) MooActionGroup on GObject: a name and a hash of actions.
       Done: step c. `MooActionGroup` is a plain GObject: a name, a display name and a hash
       of its actions, which it owns. An action finds its group through a pointer on itself
       that insert sets and remove or the group's finalize clears; the `action-group` property
       of GtkAction is no longer read. Nothing in `src/` set a group insensitive or invisible,
       or listened to `pre-activate`/`post-activate`, so the group sensitivity/visibility
       GtkAction folded into `is_sensitive` is dropped. `moo_action_collection_*` and every
       caller take `MooActionGroup *`. Accelerators are untouched: GtkAction connects them
       through its own accel group and accel path, neither of which involves the group.
    d. Accelerators connected by path ourselves. Risk: user-set shortcuts and
       runtime changes; the shortcut tests in `doc/testing-panes.md` cover them.
    e. (4.3) MooAction and MooToggleAction on GObject, with their own properties
       and `activate`/`toggled` signals.
    f. (4.4) The public headers take `MooAction *` instead of `GtkAction *`; the
       ignore-deprecation wrappers and `GtkSettings:gtk-menu-images` in
       `KNOWN_DEPRECATED` go.
    g. (4.5) Hand check, then CI.
- [x] 4.1 Our own proxy creation and sync in `moouixml.cpp` instead of
  `gtk_action_create_menu_item`/`create_tool_item` and GtkActivatable.
- [x] 4.2 MooActionGroup/collection without GtkActionGroup.
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
