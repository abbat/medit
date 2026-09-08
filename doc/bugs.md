# Bug patterns already found

*For agents working in this tree. Read `AGENTS.md` first, in particular the section
on the GTK+3 porting mistakes: most of what is here is an instance of one of them.*

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

## What the marker sweep found, and what it cost to fix

Reading all 53 `written by AI` blocks against their GTK+2 branch turned up 19 defects.
Every one is fixed, and each fix has a test written before it. Ordered by what a user
would have noticed:

| where | what was wrong | what asserts it |
|---|---|---|
| `mooiconview.c` | GTK+2's `set-scroll-adjustments` was dropped and GtkScrollable never implemented, so the file selector's icon view — its **default** view — was put in a GtkViewport and its scroll bar had no range at all: **`value=0 max=0` with 41 files in the directory, of which the first thirty could not be reached by any means** | `app/file_selector_browse` |
| `moopaned.c` `moo_paned_draw()` | mistake (a) in `AGENTS.md` exactly, so `draw_handle()` and `draw_border()` had not run since the port. `event_window` is one pointer shared by all four MooPaneds of a window | `app/pane_resize`, on pixels |
| `moopaned.c` ×4 | `gtk_style_context_get_border()` is **0** on this widget where `style->xthickness` was 1, so `border_size` and `shadow_size` were 0 and there was nothing to draw even once the dispatch was right. Floored at one pixel | the same |
| `moonotebook.c` drag snapshot | `gdk_pixbuf_new()` does not clear; the window was copied into a *second* pixbuf which was then unref'd, so a dragged tab painted uninitialised heap. `gdk_cairo_set_source_window()` was also given `+offset` where it wants `-offset`, and the failure branch stored nothing | `editor/tab_drag`, on pixels |
| `moobigpaned.c` ×2 | the drop indicator drew on `outer`'s `cr` instead of the shaped `drop_outline` window (mistake (a) again), and its mask unioned *filled* rectangles where GTK+2 drew outlines | `app/pane_move`, on pixels, and `/mooutils/paned/drop-mask` |
| `mooutils-misc.cpp` | `accel_label_set_string()` set accel 0/0 and stashed the text in object data it fed back to itself, so the second column of those menu items was empty | `/mooutils/accel/label` |
| `moopane.c` | the five state-tinted copies of a button icon were all made with the widget's *current* state, so they were identical | — |
| `moopaned.c` `draw_handle()` | `state \|= GTK_STATE_SELECTED` mixed a `GtkStateType` (3) into a `GtkStateFlags`, asking for ACTIVE\|PRELIGHT | — |
| `mooutils-treeview.cpp` | expander lines stroked at cairo's default width 2.0 on integer coordinates — grey and doubled where `gdk_draw_line()` was one pixel, which is mistake (c) | — |
| `moocommand-exe.cpp` | `DISPLAY` was set in *this* process around a `g_spawn_async()` given an explicit environment, so the child never read it | — |

Two things that came out of the sweep are worth keeping separately.

**`moofileentry.c` was marked and was not wrong.** The sweep reasoned by analogy with
MooPaned, where `gtk_style_context_get_border()` measures 0. A realized `GtkEntry`
answers 1 on every side, the same as GTK+2's `xthickness` — the difference is that an
entry has a CSS border of its own and a bare container does not. Measure the widget you
are about to change, not one that looks like it.

**What is left in `moonotebook.c` is one problem wearing three markers.** The current tab
has a line along its bottom closing it off from its page, where `gtk_paint_extension()`
left that side open. `gtk_render_extension()` with `GTK_POS_BOTTOM`,
`GTK_STYLE_CLASS_NOTEBOOK` and the states the right way round — GTK+2 drew the current
tab NORMAL and the rest ACTIVE, GTK+3's themes want the opposite — produces a
**byte-identical screenshot**. Since 3.20 a theme styles notebook parts through CSS
nodes, and a widget that is not a `GtkNotebook` has none of them whatever it passes to
the render calls. That wants a CSS name and node structure of its own, which is the
whole widget's drawing rather than a cleanup.

Known and deliberately left alone: `draw_entry()` in `mooiconview.c` still uses
`gdk_cairo_create()` per row (deprecated since 3.22, bypasses the clip, works).

Four `#if 0` blocks survive the dead-code cleanup on purpose, because each documents a
feature that is disabled rather than abandoned: the tree view's drag source in
`moofileview.c` (drag and drop works in icon view only), `_moo_edit_print_options_dialog()`
in `mootextprint.c` (`medit.xml` still lists a `PrintOptions` item with no action behind
it), and the overwrite-prompt code in `moofileview.c` (`copy_files()` runs `cp -R` with
no prompt at all). Leave them until the features are decided.

---
