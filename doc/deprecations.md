# What removing deprecated API taught

Deprecated GTK+/GLib API is gone from the tree, `src/vendor/` included: a strict build
turns `-Wdeprecated-declarations` into an error everywhere, and nothing is wrapped in
`G_GNUC_BEGIN_IGNORE_DEPRECATIONS`. The removal ran in six stages; the plan and its
step-by-step record are in `git log -- doc/deprecations.md`. The one thing left needs a
person — a hand check of the action rewrite — and is in `doc/todo.md`.

What it took to get there, and what the next such job should know first.

## Finding what is deprecated

- **clang sees more than gcc.** Every `GTK_STOCK_*` macro casts to the deprecated
  `GtkStock` typedef, and a deprecated parent type in `G_DEFINE_TYPE` (`GTK_TYPE_VBOX`,
  `GTK_TYPE_ALIGNMENT`) is a use of it; gcc reports neither. 217 sites went unseen until
  the clang job. Build strict with both.
- **Properties and signals are invisible to the compiler**: they are named by string.
  `G_ENABLE_DIAGNOSTIC=1` makes GObject warn on them at run time, and the UI runner sets
  it and fails a test on any name outside `KNOWN_DEPRECATED` (`tests/lib/runner.py`).
  The two names left there are libvte's accessible, not ours. `doc/running.md` has the
  rest.

## GtkAction → MooAction

- The action family is a plain GObject now (`MooAction`, `MooToggleAction`).
  In a factory that builds widgets from actions, test for the toggle
  type **before** `MooAction`, or a toggle becomes a plain item.
- Accelerator closures are connected by path before the menu items exist, and the first
  handler wins: `window.tab_navigation` caught an accelerator taken twice.

## GtkStock

- **The labels were gtk's translations.** A stock item brought its label from the
  `gtk30` domain; its replacement is a literal of ours and a new msgid, untranslated in
  every catalog. Copy gtk30's "Stock label" translations into our `.po` files, and after
  such a change look for empty msgstrs, not only for a changed template.
- **Some `gtk-*` icon names exist only in GTK's own icon factory**, which goes with
  GtkStock. They need a freedesktop name: preferences → `preferences-system`, edit →
  `accessories-text-editor`, index → `view-list`, keyboard →
  `preferences-desktop-keyboard-shortcuts`, plugins → `application-x-addon`.
- **The icon resource path is `/medit/icons`**, not `/medit/icons/hicolor`; with the
  theme directory in the path the names do not resolve.
- **Bookmarks are the one place a stock id was persisted.**
  `_moo_bookmark_icon_name_for_stock()` maps the old ids when a user's bookmarks load.
- `gtk_button_set_image()` called before `gtk_button_set_always_show_image()` reads
  `gtk-button-images` and hides the image; set the flag first.

## Containers and the rest

- `GtkHBox`/`GtkVBox` → `GtkBox` changes the default `expand` of children; the trap is
  in `AGENTS.md`.
- The style scheme's `GtkRcStyle` code was replaced by gtksourceview 3.24.11's own port:
  a `GtkCssProvider` at priority `GTK_STYLE_PROVIDER_PRIORITY_APPLICATION - 2`.
- libxml2's deprecated global parser defaults became options on `xmlReaderForFd()`.
