# Ideas worth keeping

*For agents working in this tree. Nothing here is a promise; each entry is something the
code once meant to do, with what is in the tree today and what it would take.*

## What to suggest first

Asked what could be improved, or what to do next, answer in this order. A rung is only
reached when there is nothing left worth doing above it.

0. **Losing or corrupting what the user wrote.** Anything that can do that — a crash with
   unsaved documents, a save that writes the wrong bytes, a broken undo, an overwrite
   nobody was asked about — comes before everything else on this list, whatever else is
   open.
1. **Stability.** The program must not crash, and what it does must be predictable. In
   order — tests, UI and unit, both for coverage and for the edge cases; less code,
   because less of it is easier to test and to keep; less legacy, because that is what the
   next toolkit will cost; and safer practice where the code is being touched anyway —
   smart pointers, ownership that releases itself, guards. The build and CI belong here
   too: they are what keeps the rest of it true.
2. **Speed.** Lightness is what this editor is liked for. The hot paths of working with
   text first; then the user's own speed — fewer actions and less to think about for
   writing, editing, searching and replacing; then the extremes, a 100 MB json, a 100 MB
   single line, an expression that takes exponential time. Always with a measurement where
   one can be taken, and never by making the code harder to keep: stability outranks speed,
   so an optimisation that costs three hundred lines needs numbers before it is written,
   not after.
3. **New features.** Last, and only what the rungs above do not already argue against. New
   ones go into the GTK+3 build; GTK+2 is kept working, not extended, and a feature that
   would need a second implementation there simply does not get one.

Translations and documentation are not a rung: they are part of the change that made them
wrong, and are updated in the same commit. Packaging is last of all — CI builds the
packages on every push, and a broken one really matters only at a release, where there is
time to fix it.

When nothing on this list is open, the list is not the answer; the code is. The coverage
table, CodeQL's alerts, the warnings the strict build prints, the `XXX` and `FIXME` somebody
left behind, a function nobody calls any more — read those, and write what they say into this
file at the rung it belongs to.

This file used to open with the file view overwriting a dropped file without asking. That
entry is gone because the code is: `run_command_on_files()` in `moofileview.cpp` asks once
for every name already taken in the destination, and reads what the command it spawned did.
It came out of commented-out code that CodeQL's `cpp/commented-out-code` reported, and the
rest of those blocks described work that has since been done — `moo_editor_create_doc()`
makes a document outside any window today, and `moo_notebook_insert_page()` calls
`gtk_widget_set_can_focus()` a few lines below where the disabled `GTK_WIDGET_SET_FLAGS`
sat. `git log -S` on those names finds the removal and the original text.

Folding was here too, as 797 lines of fold tree that nothing ever put a fold into. The
View menu's *Toggle Fold* now makes one out of the lines indented deeper than the cursor
line, the margin appears with the first fold of a document rather than by a setting, and
a fold whose line is deleted takes itself out instead of leaving text invisible. What
indentation cannot know is where a function ends: `textDocument/foldingRange`, below, is
that answer.

---

*The next two are not leftovers of removed code. They are the places where the tree is
going to stop building, or is building on something nobody looks at, and each one is
cheaper to answer before it becomes a bug report.*

## Nothing builds the tree unless somebody pushes

`build.yml`, `ui.yml`, `codeql.yml` and `package.yml` all trigger on `push`,
`pull_request` and `workflow_dispatch`, and none of them has a `schedule:`. Everything the
tree builds against — GTK+, glib, vte, json-glib, the AT-SPI stack the UI tests drive, the
base images themselves — moves without medit moving, so a quiet week means the next commit
discovers a break that happened days earlier and gets blamed for it.

A nightly `schedule:` on `ui.yml` alone would separate "my change broke it" from "the world
moved", which is the only question that matters when a run goes red.

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

* `textDocument/foldingRange` — the fold tree is filled by indentation today, which folds
  a C block without its closing brace; this is what would replace that guess.
* `textDocument/prepareCallHierarchy` with `callHierarchy/incomingCalls` and
  `outgoingCalls`, and the same three for the type hierarchy. The references pane in
  `lsp-references.cpp` already shows a list of locations grouped by file; a hierarchy is
  that pane with a tree rather than a list.
* `textDocument/documentLink` — turn what the server says is a link into one. The hover
  and the diagnostics tooltip already render text; nothing currently follows a URI.

`selectionRange` was the cheapest of these and is done: Expand and Shrink Selection in
`lsp-navigate.cpp`. `documentLink` is what is left that needs no new pane.

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
