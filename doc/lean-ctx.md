# lean-ctx in this tree

Read this only when the `lean-ctx` MCP server is installed — its tools are named
`mcp__lean-ctx__ctx_*`. Without it, everything here falls back to the shell and the
native Read/Edit/Grep, and nothing else in `doc/` depends on it.

When it is there, its tools come first. They compress what comes back, and since every
later call re-reads the whole context, a 30k dump saved once is saved on every call
after it. The shell is where compression pays most and where it bites hardest; the
traps below are the ones that cost real time here.

The share of calls in a measured stretch of sessions, which is also the order of this
file: `ctx_shell` 39%, `ctx_read` 28%, `ctx_search` 19%, `ctx_patch` 7%, then
`ctx_edit`, `ctx_glob`, `ctx_tree`, `ctx_smells`, `ctx_outline`, `ctx_expand`,
`ctx_git_read`, `ctx_callgraph`, `ctx_compose` in single digits.

## ctx_shell

- **Compression keeps "safety-relevant" lines** — anything with `fail` or `error` in
  it, including inside a commit message or a test name — and drops others. A
  compressed `git log` or ctest summary can therefore read as a failure that is not
  there, or hide the line that settles it. For output a decision rests on, pass
  `raw=true` when it is short, or send it to a file and grep the file.
- **A redirect into the project tree is refused** (`> build.log`, `| tee x`): the
  compressed bytes would land in the file. Redirect into the scratchpad or `/tmp`
  with an absolute path — `make -C build2 -j8 >$S/b2.log 2>&1; echo "exit=$?"` — which
  keeps the output out of the channel altogether.
- **The full output of a compressed call is kept** in a log whose path the reply
  names; `ctx_expand id=<that path> search=<text>` slices it instead of re-running.
- **An allowlist blocks binaries not named in it** (`shell_allowlist` in
  `~/.config/lean-ctx/config.toml`). It is per machine and the user changes it, so
  this file names no command as blocked or allowed: the refusal itself says which.
  Wrapping a command in `timeout` does not help, and the hook guards the native Bash
  too.
- **When a command is refused, ask the user** — do not run `lean-ctx allow`, edit the
  config, or reroute through `ctx_execute language=shell` on your own. Offer three
  answers: add the binary to the allowlist, authorize a workaround this once (the
  native Bash tool, a script file, another tool that gives the same result), or say
  what to do instead. The one exception is `bash` itself: never ask for it; drop the
  `bash -c` wrapper and run the command directly. A subagent cannot ask, so it
  reports the refused command back and stops that step.
- **A config that fails to parse silently drops back to the built-in allowlist**, so
  an entry just added "does not work". The refusal names the TOML error;
  `lean-ctx doctor 2>&1 | grep "parse error"` checks it.
- **Python goes through `ctx_execute language=python`**; for a script kept in the
  scratchpad, `ctx_execute action=file path=…`.
- **A command past ~110s detaches** into a `shell_*` job (`timeout_ms` sets its
  lifetime, `background_action=status job_id=…` polls it). A full build fits; a CI
  wait loop does not — it stays in Bash.
- `cwd` persists between calls; still prefer absolute paths.

## ctx_read

- **A re-read of a file you edited returns a diff**, not the lines you asked for
  (`[delta-explicit] … served as a diff`). That is the point — but when you need the
  text itself (the lines around an edit, a macro definition), pass `fresh=true` or use
  `sed -n` through `ctx_shell raw=true`.
- **Pick the mode by intent**: `lines:N-M` for the function you will touch,
  `signatures`/`map` for orientation, `full` only to edit a whole file, `anchored`
  only when the next call is `ctx_patch`.
- **`signatures` and `ctx_outline` miss GNU-style definitions** whose return type
  ends in `*` on a line of its own — `moo_edit_get_view`, `color_parse`,
  `generate_css_style` all vanish — and mark `static` functions "pub". In this tree
  the reliable table of contents is a search, since GNU style puts the name at
  column 0: `ctx_search pattern="^[a-z_]+ \(" path=<file>` lists every definition with
  its line.
- `paths=[…]` batches several reads into one call.

## ctx_search

- Each hit shows its enclosing symbol, which often saves the follow-up read.
- `queries=[…]` batches regexes; each entry takes `pattern`, `include`, `ext`,
  `exclude`, `exclude_pattern`, `max_results` — **not `path`**, which is an error.
  Scope the path at the top level.
- It honours `.gitignore`; `src/vendor/` is searched when named as `path`.
- `action=symbol` misses symbols (`_gtk_source_style_scheme_apply`). A regex on the
  name is the dependable route.
- A pattern starting with `-` is fine here (the shell's `grep` is ugrep and is not).

## Editing: ctx_patch, ctx_edit

- `ctx_patch op=replace_unique path old_text new_text` and `ctx_edit` are exact
  unique replacements without a prior read — the cheap edit for a known line.
- `ctx_patch` with anchors (`set_line`, `replace_lines`, `insert_after`) needs a
  `ctx_read mode=anchored` first; a `CONFLICT` means the file moved, re-read.
- `replace_symbol` reports `NO_SYMBOL` for C functions the outline misses (above) and
  leaves stray blank lines where it works. For C/C++ the native Read + Edit is cleaner.
- Batch edits to several files in one `ctx_patch ops=[…]`.

## Finding files: ctx_glob, ctx_tree

- `ctx_glob pattern="src/**/moo*paned*.cpp"` is the way to find files.
- `ctx_tree` is for shape only: at `depth=1` a directory holding only subdirectories
  reads as `(0)`, and a large tree is archived, to be read with `ctx_expand id=…`.

## ctx_expand

Retrieves anything archived or tee'd: `id=<archive id | log path | shell_* job>` with
`search=`, `head=`, `tail=` or `start_line`/`end_line`. Use it before re-running a
command whose output was compressed away.

## Upstream code: ctx_git_read

Reads a remote repository through a cached shallow clone — the cheapest way to hold
`src/vendor/` against upstream:

```
ctx_git_read url=https://gitlab.gnome.org/GNOME/gtksourceview mode=grep
             ref=3.24.11 query=xmlReaderForFd
ctx_git_read url=… mode=read ref=3.24.11 path=gtksourceview/gtksourcestylescheme.c
```

`ref=master` tells whether upstream still has a construct; `3.24.11` is the GTK+3
release the style scheme port came from.

## Graph tools: ctx_callgraph, ctx_impact, ctx_compose

- `ctx_callgraph callers` finds **none** for functions plainly called
  (`moo_edit_get_view`, `_gtk_source_style_scheme_apply`). Do not trust an empty
  answer; `ctx_search` for the name is the dependable route.
- `ctx_impact action=analyze path=<header>` lists the files that include it, which is
  right and cheap: what a header change recompiles and what to test.
- `ctx_compose` and `ctx_explore` rank by text and pull in `.git` hooks, `NEWS` and
  `po/` files. A targeted search is cheaper here.

## Code health: ctx_smells, ctx_quality, ctx_review

- `ctx_smells`: `duplicate_definitions` is noise (`run` in every `tests/*/test.py`),
  `long_function` is mostly `*_class_init`. `dead_code` is worth a look, cross-checked
  against lcov before deleting anything.
- `ctx_quality` does not read `.cpp`; its report covers only the Python and C it can
  parse, so its score says nothing about `src/`.
- `ctx_review` finds no tests, since every test here is a UI script under `tests/`.

## Other tools worth knowing

Non-core tools are reached through `ctx_call name=<tool>`;
`ctx_call name=ctx_discover_tools` lists all of them.

- `ctx_crush` shrinks JSON and logs while keeping anomalies — for a
  `gh run view --json jobs` or `gh api` answer that must be read, not grepped.
- `ctx_retrieve path=…` gets the verbatim text of a file a compressed read summarised.
- `ctx_delta path=…` is the explicit form of the diff a re-read gives.

## Subagents

A subagent starts without this file. Its prompt must say to use lean-ctx and name the
calls (`mcp__lean-ctx__ctx_shell` with the output in the scratchpad, `ctx_search` on
the log), and the agent definition must list the tools. `.claude/agents/suite-run.md`
and `ci-triage.md` do both.
