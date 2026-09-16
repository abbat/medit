# Ideas worth keeping

*For agents working in this tree. Nothing here is a promise; each entry is something the
code once meant to do, with what is in the tree today and what it would take.*

The first entry came out of commented-out code that CodeQL's `cpp/commented-out-code`
reported. The blocks themselves are gone — each named functions, flags or dialogs that
do not exist, so none of them would have compiled and none could have been switched back
on — but one of them described something the program still does not do. `git log -S` on
the names below finds the removal and the original text. The rest turned out to describe
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

## Folding is drawn, toggled and never created

`moofold.cpp` is 797 lines of fold tree, `MooTextBuffer` keeps one for every buffer,
`mootextview.cpp` draws the expander in the line margin and paints the collapsed lines,
and `mootextview-input.cpp` turns a click on the expander into a toggle. What no code
does is make a fold: `_moo_fold_tree_add()` has no caller but `mooedit-tests.cpp`,
`mootextbuffer.h` exports `get_fold_at_line`, `toggle_fold` and `toggle_folds` and no
way to add one, and `enable-folding` is never set, so `MooTextView::enable_folding`
stays `FALSE` for the life of the process and none of the drawing ever runs.

The missing half is whatever decides where a fold starts and ends — the indentation is
the version that needs no server, `textDocument/foldingRange` is the version that knows
what a function is — plus an action to fold and unfold and the property set to `TRUE`
when it is on. The other answer is to delete `moofold.cpp`, its test, the three signals
and the margin drawing, and to start over the day someone wants folding.

---

*The next four are not leftovers of removed code. They are the places where the tree is
going to stop building, or is building on something nobody looks at, and each one is
cheaper to answer before it becomes a bug report.*

## vte is taking the window title away

`terminal-plugin.cpp` names the terminal pane after whatever the shell puts in the window
title: it connects to `window-title-changed` and reads `vte_terminal_get_window_title()`.
vte deprecated both in 0.78, and the replacement is the termprop API —
`vte_terminal_get_termprop_string (term, VTE_TERMPROP_XTERM_TITLE, NULL)` with the
`termprop-changed` signal. The build asks pkg-config for `vte-2.91` with no version bound,
so the day a distribution ships a vte that has dropped the old names the terminal pane
stops compiling, and `-Wno-error=deprecated-declarations` means the strict build will not
have warned about it first.

The whole of the change is a `#if VTE_CHECK_VERSION (0, 78, 0)` around two lines: the pane
wants one string and both APIs return one. Doing it now costs an hour; doing it after the
release that breaks costs a hotfix.

## Nothing builds the tree unless somebody pushes

`build.yml`, `ui.yml`, `codeql.yml` and `package.yml` all trigger on `push`,
`pull_request` and `workflow_dispatch`, and none of them has a `schedule:`. Everything the
tree builds against — GTK+, glib, vte, json-glib, the AT-SPI stack the UI tests drive, the
base images themselves — moves without medit moving, so a quiet week means the next commit
discovers a break that happened days earlier and gets blamed for it.

A nightly `schedule:` on `ui.yml` alone would separate "my change broke it" from "the world
moved", which is the only question that matters when a run goes red.

## `src/medit-app` is the least covered thing in the tree

The table in `doc/testing-panes.md` puts `src/medit-app` at 47.6% of lines, the lowest
module in a tree whose total is 74.9%. What lives there is the startup path: the command
line, the single-instance handshake, the session file, the save on a crash. It is the code
that runs before there is anything for a UI test to click on, and the part of the program
where a mistake means the editor does not come up at all.

The harness starts the real binary, so the missing tests are not hard to write — a run with
two files named on the command line, a run with `--new-app`, a second instance handing its
arguments to the first. They are tests nobody wrote because the harness was built to click
on panes, not to start the program in more than one way.

## GTK+2 stays until something real needs it gone

Both toolkits are built and tested on every push, which doubles every CI run, and GTK+2 is
why the terminal pane exists in one build and not the other, why `MOO_GTK3` guards exist in
the tests, and why a good deal of `mooutils` has two branches. GTK+2 has had no release
since 2020 and Debian 13 still ships it.

The condition for dropping it is not a date: it is the first distribution medit is packaged
for dropping the runtime, or the first feature that cannot be written twice. Until one of
those happens the double build is the price of a fork that still runs where the original
ran, and this entry exists so that the next person to ask is told what the answer depends
on rather than told no.

---

*The rest of this file is about the language server plugin. Nothing here is a leftover of
removed code: these are the parts of the protocol the plugin knows about and has not asked
for. Each entry says what is in the tree today, because in several cases the answer is
"the capability is announced and the request is never sent".*

## `textDocument/declaration` is announced and never asked

`client_capabilities()` in `lsp-server.cpp` announces `linkSupport` for `definition`,
`typeDefinition`, `implementation` **and** `declaration`, but `declaration` is the one of
the four with no action behind it: `lsp-plugin.cpp` registers Go to Definition, Go to Type
Definition and Go to Implementation, and nothing sends `textDocument/declaration`. The
machinery is already general — `lsp_goto_location (window, view, method)` in
`lsp-navigate.cpp` takes the method name, and `lsp_can_ask()` derives the provider name
from it — so this is an action, a menu item and a UI test, not new plumbing.

The alternative is to drop `declaration` from `link_methods[]`, so that nothing is claimed
that cannot be used. For C and C++ the distinction between a declaration and a definition
is the one that matters most, which argues for adding it rather than removing it.

## Nothing is read-only: no code lens, inlay hints or semantic tokens

The three features that decorate a document without being asked are all missing, and all
three want the same thing the plugin does not have — a way to put text or marks into a
`MooTextView` that is not part of the buffer:

* **code lens** (`textDocument/codeLens`, `codeLens/resolve`) puts a clickable line above
  a function. The commands it carries would run through the `workspace/executeCommand`
  path `lsp-actions.cpp` already has.
* **inlay hints** (`textDocument/inlayHint`) put parameter names and inferred types
  between the characters of a line.
* **semantic tokens** (`textDocument/semanticTokens/full`, `/delta`, `/range`) are the
  server's own highlighting, layered over the language definition medit highlights with.
  `workspace/semanticTokens/refresh` is already answered with an empty result, which is
  the correct answer while nothing is displayed.

Each needs a decision about the text view before it needs any LSP code. Semantic tokens
also need to agree with `MooLangMgr` about which wins where, which is the hard half.

## Folding, selection ranges and the hierarchies

These ask the server about the shape of the code rather than about a name:

* `textDocument/foldingRange` — the fold tree is there and nothing fills it, so this is
  a source for the entry above rather than a feature of its own.
* `textDocument/selectionRange` — grow and shrink the selection by syntax. This one needs
  nothing new in the view: it is a pair of actions over a stack of ranges.
* `textDocument/prepareCallHierarchy` with `callHierarchy/incomingCalls` and
  `outgoingCalls`, and the same three for the type hierarchy. The references pane in
  `lsp-references.cpp` already shows a list of locations grouped by file; a hierarchy is
  that pane with a tree rather than a list.
* `textDocument/documentLink` — turn what the server says is a link into one. The hover
  and the diagnostics tooltip already render text; nothing currently follows a URI.

`selectionRange` is the cheapest of these and the one with no prerequisites.

## `workspace/symbol` — the symbols pane stops at the file

`lsp-symbols.cpp` asks for `textDocument/documentSymbol` and shows what one file holds.
`workspace/symbol` is the same question asked of the project, and it is the request that
makes a language server better than a grep. It wants a search entry rather than a pane
that follows the current document, so it is closer to the Find in Files plugin in shape
than to the symbols pane, even though the results are the symbols pane's rows.

## The requests that are sent, but not in every form

* **Range and on-type formatting.** `lsp-plugin.cpp` sends `textDocument/formatting` for
  the whole document. `textDocument/rangeFormatting` over the selection is the obvious
  companion; `textDocument/onTypeFormatting` would have to be driven from the view's
  key handling, the way completion triggers are.
* **`textDocument/prepareRename`.** `rename` is announced with `prepareSupport: false`,
  so the rename dialog in `lsp-edits.cpp` offers to rename whatever is under the cursor
  and finds out from the server's error that it cannot be renamed. `prepareRename` asks
  first, and also returns the range of the name, which is what the dialog should be
  showing as the old name instead of the word the view guessed.
* **`codeAction/resolve`.** `lsp-server.cpp` announces `dataSupport: false` and no
  `resolveSupport`, and `code_action_new()` in `lsp-actions.cpp` drops an action that
  carries neither an edit nor a command for exactly that reason. Servers that compute
  their edits lazily — and several large ones do — therefore offer fewer actions to medit
  than to an editor that resolves. Adding it means keeping `data` on `LspCodeAction` and
  asking again when an action without an edit is chosen.
* **Snippets in completion.** `completionItem.snippetSupport` is `false`, so a server
  that would have inserted `foo(${1:bar})` inserts `foo()` or the plain label. A snippet
  needs tab stops in the view, which is the same missing piece as the read-only
  decorations above.
* **`documentChanges` in a WorkspaceEdit.** Announced as `false` on purpose: the
  `documentChanges` form also carries creating, renaming and deleting files, which is not
  something an editor should do because a server asked. `lsp_workspace_edit_parse()`
  reads the form anyway when a server sends it regardless, but only the edits.

## What a slow server cannot say

`publishDiagnostics` is likewise announced without `relatedInformation` or
`codeDescriptionSupport`: a diagnostic that points at a second location ("first declared
here") arrives without it, and the code that would have a documentation URL does not carry
one. Both need somewhere to show a secondary location, which the diagnostics pane could
grow as child rows.
