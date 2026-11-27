# Diagnostic codes: lvs area

Codes `LVS-<TOPIC>-<NNN>` emitted by `src/core/src/lvs/` (layout versus
schematic: the netlist readers, setup files, the comparison, reduction,
properties, hierarchy, the reports and the `lvs` command;
`openspec/changes/m9-lvs-simulation/design.md` D3). Conventions and the
renderings are described in [../codes.md](../codes.md).

The area's table is assembled by `src/core/src/lvs/codes.cpp` (owner:
SCAFFOLD) from one fragment per subdirectory, each owned by its workstream.
The rows are sorted while compiling (`src/core/src/diag/sorted_codes.hpp`):

| Fragment | Topics | Owner |
|---|---|---|
| `src/core/src/lvs/net/codes.inc` | `LVS-NET-*` | LVSREAD |
| `src/core/src/lvs/read/codes.inc` | `LVS-READ-*` | LVSREAD |
| `src/core/src/lvs/setup/codes.inc` | `LVS-SETUP-*` | LVSSETUP |
| `src/core/src/lvs/cmp/codes.inc` | `LVS-CMP-*` | LVSALG |
| `src/core/src/lvs/reduce/codes.inc` | `LVS-REDUCE-*` | LVSRED |
| `src/core/src/lvs/prop/codes.inc` | `LVS-PROP-*` | LVSPROP |
| `src/core/src/lvs/hier/codes.inc` | `LVS-HIER-*` | LVSHIER |
| `src/core/src/lvs/report/codes.inc` | `LVS-REPORT-*` | LVSREP |
| `src/core/src/lvs/cmd/codes.inc` | `LVS-CMD-*` | LVSCMD |

The M9 scaffold (tasks.md 4.1, 4.2) declared no code. While M9 is in
progress, a command or function the scaffold declared before its workstream
implemented it fails with the core code `CORE-NOTIMPL-009`
([../codes.md](../codes.md)); INTEGRATION retires it at M9 exit.

Each resource cap of design D18 (include depth, logical line, name length,
hierarchy depth, devices per run, setup evaluation) gets its own code from
the workstream that enforces it.

## LVSREAD

Codes of the netlist database and the readers (`lvs/net/`, `lvs/read/`;
capability lvs-netlist-input). M9 round 1 implements SPICE and CDL; the
Verilog, `.sim` and `.ext` readers (tasks.md 6.3) still fail with
`CORE-NOTIMPL-009`. Constants: `hefesto/lvs/net/codes.hpp`,
`hefesto/lvs/read/codes.hpp`.

- **Errors stop the read** and are the error of `lvs::read_netlist` (the
  run is an input error): a missing file (`LVS-READ-001`), an undetectable
  or unsupported format (`-002`, `-003`; netgen's `ntk`, `prm`, `netgen`,
  `actel` and `xilinx` formats are not read), a bad file number (`-004`),
  binary input (`-007`), a subcircuit calling itself (`-014`), a path that
  cannot be read (`-024`: no file service, or a `~user` include on iPadOS,
  Q-LVS-14) and the limits.
- **Limits** (Q-LVS-36, design D18, decision LVS-READ-025; both
  `netgen-compat` modes): include depth 64 (`-019`, counted in files open at
  once with the top file included: depth 64 is the top file plus 63 nested
  includes), logical line 16 MiB after continuation lines are joined
  (`-020`), name 4 KiB (`-021`; tokens without `=`), 50 million devices
  (`LVS-NET-001`, counted per netlist database: the instances of every read
  into one database, so a second read into the same database counts on from
  the first). Each is a `lvs::ReadLimits` field that may be lowered and
  never raised past twice its default (`-022`). Each diagnostic names the
  limit, the file and the line. `-025` (decision LVS-READ-025, approved
  under the standing rule for crafted-input limits): a read stops at its
  65537th `.INCLUDE` read (65536 are allowed; a file tree in which each file
  includes the next one twice would otherwise read 2^depth files), or once
  its included files hold more than 64 times the logical-line limit in bytes
  (1 GiB by default; exactly 64 times is allowed; a large file included many
  times would otherwise be lexed for hours).
- **Warnings** carry netgen's message text and are counted per file: after
  100 the reader prints `-006` once and reports no more, and a file read
  with warnings ends with the summary `-008`. They cover skipped cards
  (`-005`), definitions (`-009` to `-011`), malformed cards (`-012`), call
  parameters and placeholders (`-013`, `-016`, `-017`), case-sensitive SPICE
  matching (`-018`) and unreadable includes (`-023`). `-015` (info) reports a
  call dropped by the setup file's `ignore` list.

## LVSSETUP

Codes of setup-file evaluation (`lvs/setup/`; capability lvs-setup-file,
`evaluate_setup` in `hefesto/lvs/setup/setup.hpp`).

- `LVS-SETUP-001` (warning) is one failed setup-file command. netgen's line
  `Error <setupfile>:<line> (ignoring), <message>` goes to the setup log's
  stderr; evaluation goes on, and `Warning:  There were errors reading the
  setup file` follows at the end. `<line>` is the last line read for a
  command that spans lines.
- `LVS-SETUP-002` and `-003` are the setup-file name errors of "Setup file
  evaluation" (missing, unreadable); a missing name that contains `nosetup`
  is no error.
- `LVS-SETUP-004` ends evaluation when the step budget (Tcl commands counted
  by the interpreter, default 50 million) or the wall-time backstop (default
  60 s, for a compiled loop such as `while {1} {}` that runs no command) is
  exhausted; `-005` ends it when the run is cancelled (polled before each
  command, inside every setup command and from the interpreter's progress
  hook). Both apply in the two `netgen-compat` modes ("Setup evaluation is
  bounded and cancellable").
- `LVS-SETUP-006` is the Tcl error a setup command raises (`No such cell!`,
  `Cells to equate are in the same netlist.`, ...); evaluate_setup reports it
  as `-001`.
- `LVS-SETUP-007` refuses a setup file larger than 16 MiB, a crafted-input
  limit in both modes.
- `LVS-SETUP-008` refuses a context without both circuits or with a step or
  time budget of zero or above twice its default.

## LVSALG

Codes of the comparison (`lvs/cmp/`; capability lvs-comparison-algorithm).
Constants: `hefesto/lvs/cmp/codes.hpp`. The comparison's netgen messages
(`Netlists match uniquely.`, the `verify` lines, `equate` answers) are log
lines of the result (`lvs::LogLine`), not diagnostics; the codes cover the
conditions that stop or cut short a comparison:

- `LVS-CMP-001` (error): a circuit has no devices or no nets; the pair is a
  mismatch and its log carries netgen's `Circuit <c> contains no ...` line.
- `LVS-CMP-002` (warning): the run was cancelled between refinement rounds
  ("Long-running comparisons"); the partition stays consistent and the
  result is marked cancelled.
- `LVS-CMP-003` (warning): a Hefesto guard against a cycling partition:
  refinement stops after twice the vertex count of rounds.

## LVSRED

Codes of device reduction (`lvs/reduce/`; capability lvs-device-reduction).
Constants: `hefesto/lvs/reduce/codes.hpp`. The merge and removal messages
(`Class <c> (<f>):  Merged <n> parallel devices.`, ...) are log lines of
`lvs::ReductionSummary`.

- `LVS-REDUCE-001` (warning): a permutable pair names a pin the class does
  not have (`Class <c> does not have pin <p>.`); no permutation of that
  class is applied (Q-LVS-9 tier 1, both modes).

## LVSPROP

Codes of property matching (`lvs/prop/`; capability lvs-property-matching).
None: mismatches are report lines and `lvs::PropertyError` entries, and a
check that cannot compare carries the detail `property-check-failed`
(Q-LVS-10) instead of a code.

## LVSHIER

Codes of the compare queue and flattening (`lvs/hier/`; capability
lvs-hierarchy). None yet.

## LVSREP

Codes of the reports (`lvs/report/`; capability lvs-reports). None yet.

## LVSCMD

Codes of the `lvs` command, its sources and cross-probing (`lvs/cmd/`;
capability lvs-command). None yet.

## Table

The table below is generated from the fragments:

```sh
hefesto-cli --diagnostic-codes=lvs
```

The unit test `each area's code table matches its document and prefix`
fails when the two disagree.

<!-- BEGIN DIAGNOSTIC CODES (generated by `hefesto-cli --diagnostic-codes=lvs`) -->
| Code | Default severity | Meaning |
|---|---|---|
| `LVS-CMP-001` | error | A compared circuit has no devices or no nets (`Circuit <c> contains no devices.`, `... no nets.`); the comparison stops. |
| `LVS-CMP-002` | warning | The comparison was cancelled between refinement rounds; the partition is kept, so reports can still be written, and the result is marked cancelled. |
| `LVS-CMP-003` | warning | Refinement stopped at its round limit (twice the vertex count) without converging; the partition is kept (a Hefesto guard against a cycling partition). |
| `LVS-NET-001` | error | A read would make its netlist database hold more devices than the device limit (default 50 million, Q-LVS-36; counted per netlist database, decision LVS-READ-025); the read stops and the run is an input error. |
| `LVS-READ-001` | error | The netlist file does not exist (`ReadNetlist: unable to find file '<f>'`). |
| `LVS-READ-002` | error | The netlist format cannot be detected from the suffix or the first character (`ReadNetlist: don't know type of file '<f>'`). |
| `LVS-READ-003` | error | The format keyword is unknown or ambiguous, or names a netgen format Hefesto does not read (ntk, prm, netgen, actel, xilinx). |
| `LVS-READ-004` | error | The explicit file number is negative or differs from the number the file was loaded with (`File is already loaded as a different file number.`). |
| `LVS-READ-005` | warning | An unrecognized card is skipped (`Ignoring line starting with token: <tok>`). |
| `LVS-READ-006` | warning | More than 100 warnings in one file (`Too many warnings. . . will not report any more.`); later warnings are counted but not reported. |
| `LVS-READ-007` | error | A token is mostly non-ASCII bytes (`Input file "<f>" appears to be binary. . . bailing out`); reading stops. |
| `LVS-READ-008` | info | The summary of a file read with warnings (`File <f> read with <n> warning[s].`). |
| `LVS-READ-009` | warning | A `.SUBCKT` inside an unclosed definition, or the end of the file inside one (`Missing .ENDS statement on subcircuit.`). |
| `LVS-READ-010` | warning | `.ENDS` outside any definition (`.ENDS occurred outside of a subcircuit!`). |
| `LVS-READ-011` | warning | A subcircuit is defined twice in one file; the earlier one is renamed `<name>[[k]]` and flagged duplicate. |
| `LVS-READ-012` | warning | An element card is malformed or its model has the wrong number of ports (`Badly formed line in input.`); the card is skipped. |
| `LVS-READ-013` | warning | A token after the parameters of a subcircuit call has no `=` (`Token "<t>" is not a parameter!`); it is ignored. |
| `LVS-READ-014` | error | A subcircuit calls itself (`Fatal: Class "<c>" is instanced inside of itself!`); reading stops. |
| `LVS-READ-015` | info | A call is dropped because its class is ignored, or is ignored when shorted and all its connections are one net. |
| `LVS-READ-016` | warning | A call to an undefined subcircuit creates a placeholder module (`Call to undefined subcircuit <c>`). |
| `LVS-READ-017` | warning | A call has more or fewer connections than its class has ports (`Parameter list mismatch in <inst>: ...`). |
| `LVS-READ-018` | warning | A SPICE netlist is matched case-sensitively because a case-sensitive format was read earlier. |
| `LVS-READ-019` | error | The include depth limit (default 64, Q-LVS-36) is exceeded; the read stops. |
| `LVS-READ-020` | error | A logical line, after continuation lines are joined, exceeds the line limit (default 16 MiB, Q-LVS-36); the read stops. |
| `LVS-READ-021` | error | A name exceeds the name-length limit (default 4 KiB, Q-LVS-36); the read stops. |
| `LVS-READ-022` | error | A read limit setting is zero or above twice its default (Q-LVS-36). |
| `LVS-READ-023` | warning | An `.INCLUDE` file cannot be read; the line is skipped. |
| `LVS-READ-024` | error | A path cannot be read: no file service was given, or a `~user` prefix cannot be expanded on this platform (Q-LVS-14). |
| `LVS-READ-025` | error | One read performs more than 65536 `.INCLUDE` reads, or its includes hold more than 64 times the logical-line limit in bytes (an include tree that multiplies, below the depth limit); the read stops. |
| `LVS-REDUCE-001` | warning | A permutable pair of a class names a pin the class does not have (`Class <c> does not have pin <p>.`); no permutation of that class is applied. |
| `LVS-SETUP-001` | warning | A setup-file command failed (`Error <setupfile>:<line> (ignoring), <message>`); evaluation continues with the next command, and `Warning:  There were errors reading the setup file` follows at the end. |
| `LVS-SETUP-002` | error | The setup file does not exist and its name does not contain `nosetup` (`Error:  Setup file <f> does not exist.`); the `lvs` command ends. |
| `LVS-SETUP-003` | error | The setup file exists but cannot be read (`Error:  Cannot read the setup file <f>`). |
| `LVS-SETUP-004` | error | Setup evaluation ran out of its step budget (default 50 million Tcl commands) or its wall-time backstop (default 60 s, for compiled loops that run no command); evaluation stops and the run is an error. |
| `LVS-SETUP-005` | warning | The run was cancelled during setup evaluation; evaluation stops and the run is cancelled. |
| `LVS-SETUP-006` | error | A setup command raised an error (the netgen message, e.g. `No such cell!`); inside a setup file it is reported as LVS-SETUP-001. |
| `LVS-SETUP-007` | error | The setup file is larger than the setup-file limit (16 MiB, a crafted-input limit in both modes); nothing is evaluated. |
| `LVS-SETUP-008` | error | The setup context is incomplete (a circuit is missing), or its step or time budget is zero or above twice the default. |
<!-- END DIAGNOSTIC CODES -->
