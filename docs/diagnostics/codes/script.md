# Diagnostic codes: script area

Codes `SCRIPT-<TOPIC>-<NNN>` emitted by `src/core/src/script/` (the
embedded Tcl, its sandbox, the `magic::` command binding, PDK startup and
consent; `openspec/changes/m4-scripting/design.md` D1/D2). Conventions and
the renderings are described in [../codes.md](../codes.md).

The area's table is assembled by `src/core/src/script/codes.cpp` (owner:
INTEGRATION) from one fragment per workstream, each owned by that
workstream. The topics of the fragments interleave, so the rows are sorted
while compiling (`src/core/src/diag/sorted_codes.hpp`):

| Fragment | Topics | Owner |
|---|---|---|
| `src/core/src/script/tcl/codes.inc` | `SCRIPT-TCL-*`, `SCRIPT-SANDBOX-*` | TCL |
| `src/core/src/script/binding/codes.inc` | `SCRIPT-BIND-*` | BINDING |
| `src/core/src/script/pdk/codes.inc` | `SCRIPT-PDK-*`, `SCRIPT-TRUST-*` | PDKINT |

## TCL

Codes of the embedded Tcl and its sandbox (`src/core/src/script/tcl/`;
design D5, capabilities `hefesto-scripting` "Embedded sandboxed Tcl on all
platforms" and `hefesto-security` "Script sandbox enforcement").

Evaluation results (`script::Interpreter::evaluate` and `source`):

- `SCRIPT-TCL-001` carries the message of a Tcl error, located at Tcl's
  error line and, for `source`, the file.
- `SCRIPT-TCL-002`: the evaluation was stopped by `request_interrupt` (the
  console's and the canvas card's Interrupt, Ctrl-C). `catch` cannot absorb
  it. The console reports it with Tcl's own text: `eval unwound ·
  <file>:<line> · interrupted by the user (N edits kept; undo reverts them
  as one step)`.
- `SCRIPT-TCL-003`: the evaluation ran more commands or longer than the
  project's `SandboxLimits` allow (Tcl's resource limits); `catch` cannot
  absorb it. The limits apply again from zero to the next evaluation.
- `SCRIPT-TCL-004`: the desktop drew a frame from a running script's
  progress report and the user clicked, pressed a key macro or chose a menu
  item in it: nothing runs (the layout takes no input until the script
  ends; the Interrupt card and Ctrl-C still work, and typed console and
  command-bar lines are queued instead).

Sandbox denials. Each raises a Tcl error whose message starts with the
operation (`exec:`, `open:`, `cd:`, `file delete:`, ...), which `catch` can
absorb as Magic's PDK code expects; the diagnostic is reported either way,
with the script file and line (Tcl's `info frame`), together with a
`script::DeniedOperation` event (`ScriptEvents::publish_denied`). The events
are published when the top-level evaluation ends: a denial the script
absorbed and went on from has `caught` set and its diagnostic text ends with
` · caught by the script's catch` (board M4-Script-Consent tile 6):

- `SCRIPT-SANDBOX-001`: `exec`, or `open` of a command pipeline
  (`open |cmd`): process execution is not permitted. Also the engine
  command `shell` from every origin, typed console, command-bar and
  hefesto-cli lines included (`shell: process execution is not permitted
  (Hefesto runs no external programs)`; user decision 2026-11-11), and the
  engine commands that download when a script, a key macro or a plugin
  runs them: `pdk install`, `remote`, `updates` and `import` (security
  finding S-01).
- `SCRIPT-SANDBOX-002`: `socket` (client or server).
- `SCRIPT-SANDBOX-003`: `load` or `unload` of a native extension.
- `SCRIPT-SANDBOX-004`: a file access outside the granted roots (the project
  folder, the PDK store, the session's temporary area, opened documents):
  `open`, `source`, `glob`, `cd`, the `file` subcommands that read or change
  the filesystem, `file tempfile` without a granted directory, `file link`
  to a target outside, or any other path Tcl resolves through the
  filesystem layer. Paths are compared after `.`/`..`, `~` and symbolic
  links are resolved (dangling links too, from the link's directory).
  Writes under a read-only root (the PDK store, the install root) are
  refused the same way ("is read-only in the script sandbox"). Engine
  commands run from a script (`save`, `load`, `feedback save`, `readspice`,
  ...) read and write through the same check (security finding S-02).
  Tcl's own library code probing host paths (for example `clock`'s
  time-zone directories) is denied silently instead.
- `SCRIPT-SANDBOX-005`: `interp create` without `-safe`, and `interp
  invokehidden`, `expose` or `marktrusted` (also through a child
  interpreter's command), and a recursion limit above Tcl's default of 1000
  (security finding S-07).
- `SCRIPT-SANDBOX-006`: `package require` of a package that Tcl's embedded
  library does not provide and no script has provided (`package provide`).
- `SCRIPT-SANDBOX-007`: another removed command, such as `exit` (scripts end
  the program with `quit`), and `encoding system` or `encoding dirs` with an
  argument, which change process-wide state (security finding S-08).
- `SCRIPT-SANDBOX-008`: a command that changes which PDK scripts are
  trusted or what the PDK store holds, run by a script, a key macro or a
  plugin: `pdk trust`, `untrust`, `install`, `uninstall`, `enable`, `pin`,
  `remove-library`, `import` and `export` (security finding S-03).

## BINDING

The command binding (`script::bind_registry`, tasks.md section 5) reports
through the dispatcher's sinks; Tcl errors of bound commands carry
`SCRIPT-TCL-001` (an empty message for Magic's quiet dispatch failures).

- `SCRIPT-BIND-001` (warning, native only): a `$NAME` path variable's
  value changed after its first expansion (scripting-api "Script variables
  read by the engine", Q-SCRIPT-5). Printed once per name; with
  magic-compat ON the first value is still used, with it OFF the new one.
- `SCRIPT-BIND-002` (error): retired in M5. It was a `cif` option other
  than `scale`, `lambda`, `istyle`, `ostyle` and `help`; the M5 scaffold
  moved `cif` to `src/core/src/cif/command/`, where an option not yet
  implemented answers `CORE-NOTIMPL-005`.
- `SCRIPT-BIND-003`, `SCRIPT-BIND-004`, `SCRIPT-BIND-005` (info): the
  startup helpers `netgen`, `xcircuit` and `irsim` (decision D-34): Hefesto
  sources no package; `netgen` names the built-in `lvs` command.

The cross-PDK redefinition warning of a `magic::` procedure (Q-SCRIPT-41)
is PDKINT's `SCRIPT-PDK-003`; the binding installs its hook and reports it.

## PDKINT

PDK startup, consent and the PDK menu interception (`src/core/src/script/pdk/`,
design D7; tasks.md section 6).

- `SCRIPT-PDK-001` is Magic's text for an error in a startup file
  (`Error parsing pre-startup file "<path>": <message>`, `Error parsing
  system startup file ...`, `Error parsing user "<rc name>": ...`, `Error
  parsing "<rc name>": ...`, `Error parsing local ".magic": ...`); the rest
  of that file is skipped and startup continues with the next file. A local
  file adds `SCRIPT-PDK-002` (`Bad local startup file "<rc name>",
  continuing without.`).
- `SCRIPT-PDK-003` warns when a `magic::` procedure defined by one PDK's
  script is redefined by a script of another PDK (or PDK version) in the
  same project interpreter (Q-SCRIPT-41); the new definition takes effect.
- `SCRIPT-PDK-004` reports one skipped Tk widget statement of a PDK menu
  callback, with the script file and line (Q-SCRIPT-42, D-32).
- `SCRIPT-PDK-005` and `SCRIPT-PDK-009` are the curated-pack policy
  (Q-SCRIPT-46, D-30): a PDK without bundled scripts is refused; a curated
  PDK uses its bundled scripts, never an imported pack's.
- `SCRIPT-PDK-006` and `SCRIPT-PDK-007` are the file-argument lines. In
  wrapper mode they are the script host's (`Handling file entry <f>
  extension <ext>`, `Handling compressed entry <f> extension <ext>`, `don't
  know how to load file <f>`, `only compressed .mag and .gds are
  handled.`). In NULL graphics only `SCRIPT-PDK-006` is used, with Magic's C
  startup text `Loading "<f>" from command line.` (utils/main.c
  mainInitFinal); no `handling file entry` line is printed.
- `SCRIPT-PDK-008`: a startup file or `<tech>.magicrc` could not run
  because the project has no interpreter.
- `SCRIPT-TRUST-001` is printed when a PDK script was not run for lack of
  consent (the user declined, or batch mode without `--trust-pdk`). The
  layout then opens read-only with the `minimum` technology. A declined
  version is not asked again in the session (the prompt is shown once per
  PDK version); every later script of it, and every cell whose technology
  is that PDK variant, is declined with this code until `pdk trust`,
  `pdk untrust` or "Review scripts..." forgets the decline.
  `SCRIPT-TRUST-002` is printed when the user trusted a PDK version (the
  prompt's "Trust this version", or `pdk trust <variant> <version>`);
  `SCRIPT-TRUST-004` after `pdk untrust <variant> <version>`;
  `SCRIPT-TRUST-003` when either could not be saved in the PDK store's
  provenance record (the session keeps the change).
- `SCRIPT-TRUST-005`: a cell opened read-only because its PDK scripts were
  declined is never written: `save`, `save <other name>`, `writeall` and a
  `save` after `cellname writeable <cell> true` are refused (the cell holds
  only what `minimum` could read, so writing it would drop the PDK's
  layers). Such a cell is also read-only when it is read with the
  technology ignored (`load -force`, subcell expansion, `flush`).
- `SCRIPT-TRUST-006`: "Review scripts..." got a yes and the PDK's script
  ran, but its technology could not be loaded (neither by the script nor
  by technology auto-discovery); the decline is restored and the cells
  stay read-only.

The M4 boards carry these codes (until round 3 they showed placeholders
such as `SCRIPT-SANDBOX-…`); board text → code:

| Board | Row | Code |
|---|---|---|
| `M4-Menu-Bar` | Feedback row "skipped widget statement" | `SCRIPT-PDK-004` |
| `M4-Script-Consent` | Diagnostics row "socket is not permitted" | `SCRIPT-SANDBOX-002` |
| `M4-Script-Consent` | Diagnostics row "process execution (exec) is not permitted" | `SCRIPT-SANDBOX-001` |
| `M4-Console` | `exec ls` → "process execution (exec) is not permitted" | `SCRIPT-SANDBOX-001` |
| `M4-Script-Consent` | declined banner and read-only chip | `SCRIPT-TRUST-001` |

The other sandbox denials a diagnostics row can show are `SCRIPT-SANDBOX-003`
(`load`/`unload`), `-004` (a file outside the granted roots), `-005`
(`interp create` without `-safe` and the hidden-command operations), `-006`
(`package require` outside Tcl's embedded library) and `-007` (`exit` and
other removed commands); see the TCL section.

## Table

Generated from the fragments:

```sh
hefesto-cli --diagnostic-codes=script
```

The unit test `each area's code table matches its document and prefix`
fails when the two disagree.

<!-- BEGIN DIAGNOSTIC CODES (generated by `hefesto-cli --diagnostic-codes=script`) -->
| Code | Default severity | Meaning |
|---|---|---|
| `SCRIPT-BIND-001` | warning | A path variable's value changed after its first use; the expansion keeps the first value (magic-compat ON) or uses the new one (OFF). |
| `SCRIPT-BIND-002` | error | Retired in M5, never reused. Was: a `cif` option that needed the CIF reader and writer (the M5 scaffold moved `cif` to cif/command/; unimplemented options answer CORE-NOTIMPL-005). |
| `SCRIPT-BIND-003` | info | `netgen` is not available; Hefesto's built-in `lvs` command compares netlists. |
| `SCRIPT-BIND-004` | info | `xcircuit` is not available in Hefesto. |
| `SCRIPT-BIND-005` | info | `irsim` is not available on this host or in this milestone. |
| `SCRIPT-PDK-001` | error | A startup file raised a Tcl error; Magic's `Error parsing ...` text, the rest of that file is skipped. |
| `SCRIPT-PDK-002` | info | A local startup file failed; startup continues without it (Magic's `Bad local startup file`). |
| `SCRIPT-PDK-003` | warning | A `magic::` procedure defined by one PDK's script was redefined by another PDK's script. |
| `SCRIPT-PDK-004` | warning | A Tk widget statement in a PDK menu callback was skipped; evaluation continues. |
| `SCRIPT-PDK-005` | error | Curated-pack mode refused a PDK without bundled scripts. |
| `SCRIPT-PDK-006` | info | A command-line file argument being loaded (`Handling file entry ...` in wrapper mode, `Loading "F" from command line.` in NULL graphics). |
| `SCRIPT-PDK-007` | error | A command-line file argument wrapper mode cannot load (`don't know how to load file ...`). |
| `SCRIPT-PDK-008` | error | A PDK script could not run: the project has no interpreter. |
| `SCRIPT-PDK-009` | info | Curated-pack mode uses the bundled curated scripts of a PDK; the imported pack's own scripts are not executed. |
| `SCRIPT-SANDBOX-001` | error | The script sandbox refused to run a process (exec, open of a command pipeline, or shell from any origin). |
| `SCRIPT-SANDBOX-002` | error | The script sandbox refused to open a network socket. |
| `SCRIPT-SANDBOX-003` | error | The script sandbox refused to load or unload a native extension. |
| `SCRIPT-SANDBOX-004` | error | The script sandbox refused a file access outside the granted roots. |
| `SCRIPT-SANDBOX-005` | error | The script sandbox refused an unsafe interpreter operation. |
| `SCRIPT-SANDBOX-006` | error | The script sandbox refused a package that Tcl's embedded library does not provide. |
| `SCRIPT-SANDBOX-007` | error | The script sandbox refused a removed command. |
| `SCRIPT-SANDBOX-008` | error | The script sandbox refused a command that changes PDK trust or the PDK store. |
| `SCRIPT-TCL-001` | error | A Tcl script raised an error; the text is Tcl's error message. |
| `SCRIPT-TCL-002` | error | A script evaluation was interrupted. |
| `SCRIPT-TCL-003` | error | A script evaluation exceeded its command count or time limit. |
| `SCRIPT-TCL-004` | warning | Input that would run a command was ignored because a script evaluation is running. |
| `SCRIPT-TRUST-001` | warning | A PDK script was declined; the layout opens read-only with the `minimum` technology. |
| `SCRIPT-TRUST-002` | info | A PDK version is now trusted; its scripts run without asking. |
| `SCRIPT-TRUST-003` | warning | The trust record of a PDK version could not be saved in the PDK store. |
| `SCRIPT-TRUST-004` | info | A PDK version is no longer trusted (`pdk untrust`); its scripts ask before running. |
| `SCRIPT-TRUST-005` | error | A cell opened read-only because its PDK scripts were declined cannot be written. |
| `SCRIPT-TRUST-006` | error | After Review scripts, the PDK's technology could not be loaded; the declined cells stay read-only. |
<!-- END DIAGNOSTIC CODES -->
