# Ideas worth keeping

*For agents working in this tree. Nothing here is a promise; each entry is something the
code once meant to do, with what is in the tree today and what it would take.*

These came out of commented-out code that CodeQL's `cpp/commented-out-code` reported.
The blocks themselves are gone — each named functions, flags or dialogs that do not
exist, so none of them would have compiled and none could have been switched back on —
but two of them described something the program still does not do. `git log -S` on the
names below finds the removal and the original text. The rest turned out to describe
work that has since been done: `moo_editor_create_doc()` makes a document outside any
window today, and `moo_notebook_insert_page()` calls `gtk_widget_set_can_focus()` a few
lines below where the disabled `GTK_WIDGET_SET_FLAGS` sat.

## The file view overwrites without asking

`copy_files()` in `moofileview.cpp` spawns `cp -R --` and returns; `_moo_unix_spawn_async()`
keeps neither the exit status nor the standard error. Dropping a file onto a folder that
already holds a file of that name replaces it with no prompt and no undo, and dropping a
file onto the folder it is already in makes `cp` refuse with "are the same file" where
nobody sees it, so the drop reads as having done nothing.

Upstream had a disabled answer to the second half: a single-file drop whose destination
was the file's own directory opened a "copy file" dialog and ran `cp -R --` with the name
it returned. The dialog it called, `_moo_file_view_copy_file_dialog()`, was never written.

What this wants is one prompt shared by both cases — the destination already has this
name; replace, skip, or copy under a new name — rather than a special case for the
same-directory drop. Reporting what the spawned command did is the other half: a `cp`
that fails for any reason is silent today.

## `open_new_window` is a preference nothing reads

`MOO_EDIT_PREFS_OPEN_NEW_WINDOW` is registered in `mooeditprefs.cpp` with a default of
`FALSE`, and that is all that happens to it: no code reads it, and the preferences dialog
does not show it. The one place that did read it was `_moo_editor_open_uri()`, which
opened a file in a new window rather than in the active one when the preference was set
and no `MOO_OPEN_NEW_TAB` flag overrode it. That function and the whole `MOO_OPEN_*` flag
family are gone, and `moo_editor_open_files()` always uses the active window.

Either the preference gets its behaviour back — in `moo_editor_open_files()`, with a
checkbox in the dialog — or it should be dropped, so that nothing in the settings file
reads as configurable when it is not.
