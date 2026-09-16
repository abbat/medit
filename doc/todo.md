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

This file used to open with the file view overwriting a dropped file without asking. That
entry is gone because the code is: `run_command_on_files()` in `moofileview.cpp` asks once
for every name already taken in the destination, and reads what the command it spawned did.
It came out of commented-out code that CodeQL's `cpp/commented-out-code` reported, and the
rest of those blocks described work that has since been done — `moo_editor_create_doc()`
makes a document outside any window today, and `moo_notebook_insert_page()` calls
`gtk_widget_set_can_focus()` a few lines below where the disabled `GTK_WIDGET_SET_FLAGS`
sat. `git log -S` on those names finds the removal and the original text.

## A user tool that cannot start says so to nobody

`moocommand-exe.cpp` runs a tool three ways, and two of them lose the failure. `run_sync()`
and the async launch both end in the same line — `g_message ("%s: could not run command: %s
(command line was '%s')")` — which is stderr, and a medit started from a desktop file has no
terminal for anyone to read it in. A tool whose command line has an unpaired quote, or which
names a program that is not installed, is a menu item that does nothing and explains nothing.
`moo_error_dialog()` does not appear anywhere in `src/plugins/usertools`.

The third way is not affected: a tool that runs in the output pane says what it said on
screen, because that is what the pane is for.

There is a second half, and it is the one the file view had. `run_command()` calls
`run_sync()` with `NULL` for both `exit_status` and `output_err`, so a tool that starts and
then fails is silent too — the exit status is not asked for and the standard error is thrown
away. What this wants is the dialog the rest of the program puts an error in, and the
`WIFEXITED`/`WEXITSTATUS` pair that `rm_fr()` in `mooutils-fs.cpp` has always had.

A tool whose command does not exist is the test, and there is room for it:
`src/plugins/usertools` is at 64.8%, the second lowest module in the tree.

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

*The next five are not leftovers of removed code. They are the places where the tree is
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

## `MooFileSystem` is an interface with one implementation

`moofilesystem.cpp` is a `GObject` whose class struct is a vtable, and its `class_init()`
fills every slot of it with a function in the same file whose name ends in `_unix`. Nothing
derives from the class, `_moo_file_system_create()` hands out a single instance and keeps a
weak reference to it so that the next caller gets the same one, and the ten public functions
are a `g_return_val_if_fail` followed by `MOO_FILE_SYSTEM_GET_CLASS(fs)->something (fs, ...)`.
Half the implementations mark `fs` itself `G_GNUC_UNUSED`.

It was the shape a Windows port would have needed, and there is no Windows port: the fork
builds and is packaged for Linux only, and `/* TODO windows */` in `moofileview.cpp` is the
only other trace of one.

Collapsing it is a deletion rather than a rewrite — the `_unix` functions stay and become
the functions, the vtable and the `GET_CLASS` hops go, and the `fs` argument comes off a
dozen signatures. What is worth keeping is the singleton: the folder cache lives on the
instance, and two file views must share it.

The notes that accumulate around the indirection go with it. `/* XXX must set error */` and
`/* XXX check the caller */` sit over `parse_path_unix()` and its neighbours, and they are
accurate — several of those paths return `FALSE` with the `GError` untouched. Nothing
crashes on it, because `moo_error_message()` answers "Unknown error" for a null error, which
is also exactly what the user is told.

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
