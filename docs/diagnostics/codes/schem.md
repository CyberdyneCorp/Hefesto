# Diagnostic codes: schem area

Codes `SCHEM-<TOPIC>-<NNN>` emitted by `src/core/src/schem/` (xschem-compatible
schematic capture: the `.sch`/`.sym` file format, symbols and libraries with
`tcleval`, SPICE netlisting, schematic editing, the `schematic` and `xschem`
commands and the schematic circuit source;
`openspec/changes/m8-schematic/design.md` D3). Conventions and the renderings
are described in [../codes.md](../codes.md).

The area's table is assembled by `src/core/src/schem/codes.cpp` (owner:
SCAFFOLD) from one fragment per subdirectory, each owned by its workstream.
The rows are sorted while compiling (`src/core/src/diag/sorted_codes.hpp`):

| Fragment | Topics | Owner |
|---|---|---|
| `src/core/src/schem/file/codes.inc` | `SCHEM-FILE-*` | FORMAT |
| `src/core/src/schem/sym/codes.inc` | `SCHEM-SYM-*` | SYMLIB |
| `src/core/src/schem/lib/codes.inc` | `SCHEM-LIB-*` | SYMLIB |
| `src/core/src/schem/tcl/codes.inc` | `SCHEM-TCL-*` | SYMLIB |
| `src/core/src/schem/net/codes.inc` | `SCHEM-NET-*` | NETLIST |
| `src/core/src/schem/edit/codes.inc` | `SCHEM-EDIT-*` | EDIT |
| `src/core/src/schem/cmd/codes.inc` | `SCHEM-CMD-*` | CMD |
| `src/core/src/schem/sim/codes.inc` | `SCHEM-SIM-*` | SIMINT |

While M8 is in progress, a function the scaffold declared before its
workstream implemented it reports the core code `CORE-NOTIMPL-007`
([../codes.md](../codes.md)) where it can carry a diagnostic.

## FORMAT

Codes of the file grammar, the reader and writer and the property-string
grammar (`src/core/src/schem/file/`; capability schematic-file-format; design
D6). Owner: FORMAT.

The reader never fails: each problem of "Malformed input" is a warning, the
record or field concerned is dropped and reading goes on. The texts of
`SCHEM-FILE-001` to `-003` are xschem's (the wording is informative).

| Code | When |
|---|---|
| `SCHEM-FILE-001` | `missing fields for <KIND> object, ignoring`: a numeric field of an `L`, `B`, `A`, `P`, `T`, `N` or `C` record is missing or not a number (KIND is `LINE`, `xRECT`, `ARC`, `POLYGON`, `TEXT`, `WIRE` or `INSTANCE`). The record is skipped together with the rest of the line where reading stopped, which may be the next line ("Malformed input"). A negative polygon point count and an integer field that does not fit an `int` count as missing. |
| `SCHEM-FILE-002` | `wrong or missing layer number for <KIND> object, ignoring`: the layer of an `L`, `B`, `A` or `P` record is missing or outside 0..21 ("Layers"); the record and the rest of its line are skipped. |
| `SCHEM-FILE-003` | `EOF reached, malformed {...} string input, missing close brace`: the file ends inside a brace string (or before its `{`); the field is read as absent ("Brace strings"). |
| `SCHEM-FILE-004` | A `[` embedded symbol block at the file's own level comes before any instance record; the block is skipped up to the first later line that starts with `]` ("Embedded symbols"). A block belongs to the last instance read before it, whatever records lie between. |
| `SCHEM-FILE-005` | The file ends inside an embedded symbol block (no `]`); the records read so far define the symbol. Reported once per file. |
| `SCHEM-FILE-006` | Error of `schem::save_document`: the file at the document's own path no longer has the bytes the document was loaded from or last saved as, so it is not overwritten until the user confirms ("Safe saving"). |
| `SCHEM-FILE-007` | `embedded symbol blocks nested deeper than 64 levels; block skipped`: a `[` would open a 65th nested block level; it is skipped up to the next line that starts with `]` (input limit Q-SCHEM-32, approved 2026-11-21, both modes of `xschem-compat`). |
| `SCHEM-FILE-008` | `Symbol recursively instantiating symbol: max depth reached, skipping`: inside one embedded block, a nested block would open symbol level 40 ("Embedded symbols", following the 40-level limit of schematic-symbols-and-libraries "Schematic used as symbol"); that block and the rest of the outer block are not read, at any level; the records read before it are kept. |

## SYMLIB

Codes of the symbol model and pin order (`schem/sym/`), symbol references,
the library search path, `xschemrc` as data and the symbol cache
(`schem/lib/`), and `tcleval` values with the script permission model
(`schem/tcl/`) (capability schematic-symbols-and-libraries; design D7).
Owner: SYMLIB.

- `SCHEM-SYM-001` (warning): every pin of a symbol has `sim_pinnumber`, so
  the pins are sorted by it, and two or more pins share one value. The pins
  that share it keep their file order (a stable sort); the warning names the
  symbol, the value and how many pins share it. Given in both modes of
  `xschem-compat` (Q-SCHEM-5, `Resolution: fix`), once per duplicated value.
- `SCHEM-SYM-002` (warning): `Token <token> is not defined, but <prefix> is:
  a token ends only before a blank, @ or %`. A displayed-text token (for
  example `@name=` in `@name=@value`) is undefined while its longest
  identifier prefix names a defined key or a predefined token; the token
  gives the empty text (Q-SCHEM-26, `Resolution: replicate`).
- `SCHEM-LIB-001` (warning): `Relative symbol reference <reference> resolved
  against the schematic folder <folder>; xschem resolves it against its
  start folder`. A `./` or `../` reference, or a reference found through the
  `.` element of the search path, or through another relative element such
  as `lib` or `../lib`, was resolved against the folder of the schematic
  that contains the instance, in both modes (Q-SCHEM-30, `Resolution: fix`;
  Q-SCHEM-38 option (a), approved 2026-11-22). Once per reference and folder.
- `SCHEM-LIB-002` (warning): an `http://` or `https://` reference is kept as
  written and never fetched; the instance shows the missing-symbol
  placeholder (Q-SCHEM-9, `Resolution: platform`). Once per reference.
- `SCHEM-LIB-003` (warning): `Symbol not found: <reference>`. The instance
  shows the placeholder, keeps its reference and properties and netlists as
  nothing; a later change of the search path re-resolves it. Once per
  reference.
- `SCHEM-LIB-004` (warning): the symbol file exists but could not be read or
  parsed (the reader's error follows); the instance shows the placeholder.
  Once per file.
- `SCHEM-LIB-005` (warning): `Symbol recursively instantiating symbol: max
  depth reached, skipping`. An instance inside a schematic used as a symbol
  would open level 40 (a schematic that places itself nests until then; no
  cycle is detected, as in xschem); the rest of the symbol is not read at
  any level, and what was read before is kept. Once per file.
- `SCHEM-LIB-006` (warning): an `xschemrc` raised a Tcl error (a sandbox
  denial included); what it set before the error is kept, the rest of the
  file is not run. The search path is built from what was set. An unknown
  command is such an error (`invalid command name "<cmd>"`), except the
  GUI-only commands the three PDK `xschemrc` files call (`button`,
  `destroy`, `focus`, `label`, `menu`, `pack`, `radiobutton`, `raise`,
  `toplevel`, `winfo`, `wm`), which do nothing while the files run
  (Q-SCHEM-38 option (a), approved 2026-11-22, both modes).
- `SCHEM-LIB-007` (info): an `xschemrc` set `xschem_execute_scripts`; the
  value is read and ignored, because `tcleval` values always run in the
  sandbox and launcher scripts are confirmed once per document (Q-SCHEM-25,
  `Resolution: platform`).
- `SCHEM-LIB-008` (info): a line an `xschemrc` printed with `puts`; rc files
  never write to Hefesto's standard streams.
- `SCHEM-LIB-009` (warning): `Symbol <reference>: more than 1000000 records
  when flattened; the rest is not loaded`, or `... more than 268435456 bytes
  when flattened; ...`. A schematic used as a symbol would hold more than
  1,000,000 records (input limit approved 2026-11-21, Q-SCHEM-32) or more
  than 256 MiB (16 bytes per point plus every string a record carries;
  Q-SCHEM-40, M8-D2, approved 2026-11-22) over all levels; the records added
  so far are kept and reading stops, in both modes. The message names the
  limit reached. Once per file.
- `SCHEM-LIB-010` (warning): `xschemrc <file>: evaluation stopped after 10 s`.
  The `xschemrc` files of one document ran for 10 s in total; the stop is
  handled like a Tcl error in `<file>` (earlier settings kept, later ones
  and later files lost), in both modes (input limit approved 2026-11-21,
  Q-SCHEM-32).
- `SCHEM-LIB-011` (warning): `Symbol <reference>: symbol generators that
  run external programs are not supported`. A generator reference
  `name(args)` names a program that is not a Tcl script (a name ending in
  `.tcl`, or a first `#!` line naming `tclsh` or `wish`); it is not run and
  the instance shows the placeholder (Q-SCHEM-11). Once per program.
- `SCHEM-LIB-012` (warning): `Symbol generator <file> failed: <why>`. A Tcl
  generator raised an error, ran past its 10 s limit, printed more than
  256 MiB or its script is larger than 256 MiB; the instance shows the
  placeholder. Once per reference until the cache is cleared or the
  generator is forgotten. Also `Symbol <reference>: symbol generators ran
  for their total of 60000 ms; no further generator runs until the symbols
  are reloaded`, once per cache, when the generators of one symbol cache
  have spent their shared 60 s (M8-D5, approved 2026-11-23); no generator
  file is read after that. Also `Symbol <reference>: the generator <file> is
  larger than 268435456 bytes; it is neither read nor run`, once per file,
  for a generator file over 256 MiB with or without `.tcl` (M8-D6, approved
  2026-11-23; before, a file without `.tcl` gave `SCHEM-LIB-011`). Both modes.
  The sandbox's denials during a generator run (`SCRIPT-SANDBOX-*`, also those
  the generator catches) are reported next to it (security review, task 12.1);
  when an uncaught denial stopped the run, `<why>` is `the script sandbox
  denied an operation (<code>)` and the denial's text is in its own
  diagnostic only. A script that grows past 256 MiB after its size was
  checked is not read past that size and fails the same way.
- `SCHEM-TCL-001` (error): a procedure provided to `tcleval` values
  (`abs_sym_path`, `write_data`) was called with the wrong arguments, or
  `write_data` was refused outside the sandbox's writable folders; the
  enclosing value gives `?` LF. A refused `write_data` is also reported with
  the sandbox's `SCRIPT-SANDBOX-004` (security review, task 12.1).
- `SCHEM-TCL-002` (info): a launcher's `tclcommand` did not run because the
  user did not confirm it for this document (Q-SCHEM-25); the next Ctrl+click
  asks again.
- `SCHEM-TCL-003` (warning): `tcleval value stopped after 10 s (tcleval of
  <owner>)`: one `tcleval` value ran for 10 s (`kTclevalTimeLimit`); or
  `tcleval values ran for their total of 60 s; no further value is evaluated
  in this run`, once per netlist run, when the values of one `TclEnvironment`
  spent their shared 60 s (`kTclevalTotalTimeLimit`). The value gives `?` LF.
  Both modes of `xschem-compat` (a hang, fixed by the security review of task
  12.1; xschem has no limit). The sandbox's denials during a value
  (`SCRIPT-SANDBOX-*`, also caught ones) go to the same sink with
  ` (tcleval of <owner>)` appended, `<owner>` being what the value belongs to
  (`TclContext::owner`, else the symbol, else the instance).

## NETLIST

Codes of connectivity, net naming, format substitution, `expr`,
post-processing and the netlist writer (`src/core/src/schem/net/`; capability
schematic-netlisting; design D9). Owner: NETLIST. The constants are in
`src/core/include/hefesto/schem/net/codes.hpp`.

- `SCHEM-NET-001` (error): a short between two differently named nets
  ("Net naming order"). The text is `<tag> shorted: <a> - <b>`, where `<a>`
  is the name kept and `<b>` the one that lost, and `<tag>` is `Net` (two
  labels or ports), `Instance pin` or `Bus tap` (a bus tap would rename a
  net that already has a name). Every short is reported, including top-level
  shorts (Q-SCHEM-16), and the run fails.
- `SCHEM-NET-002` (warning): `syntax error in <string>`, a name that is not
  valid bus notation; it is used unchanged with unknown width.
- `SCHEM-NET-003` (warning): `unconnected pin,  Inst idx: <i>, Pin idx: <j>
  Inst:<name>`, a pin without a net, written `__UNCONNECTED_PIN__<k>`.
- `SCHEM-NET-004` (warning): `expr: the number '<run>' is read differently
  with xschem-compat on and off`, a number run with a `-` that is not an
  exponent sign (Q-SCHEM-12); reported in both modes.
- `SCHEM-NET-005` (warning): `token '<token>' is undefined while '<prefix>'
  is defined`, the Q-SCHEM-26 prefix warning (for example `@W/` and `W`).
- `SCHEM-NET-006` (warning): a netlisting input limit (Q-SCHEM-32, approved
  2026-11-21, both modes): `bus expansion of <string> exceeds a limit (1048576
  names, 67108864 bytes or 256 nested parentheses); used unchanged`
  (`schem::kMaxBusBits`, `kMaxBusBytes`, `kMaxBusParenDepth`; the string cut
  to 80 bytes and `...`), `expr nested deeper than 200 levels; text copied
  without evaluation` (`schem::kMaxExprDepth`), or a vector element whose
  lines would pass `kMaxBusBytes`, written unexpanded. Past the total of
  expanded bus text one netlist run or one editing session may produce
  (Q-SCHEM-41, user decision M8-D3 approved 2026-11-22, both modes:
  `schem::kMaxBusTotalBytes` = 64 x `kMaxBusBytes` = 4294967296 bytes, kept
  in a `schem::BusBudget`), a name is used unchanged with `bus expansion of
  <string> exceeds the total of 4294967296 bytes of expanded bus text per
  netlist run or session; used unchanged`, and a vector element is written
  unexpanded with `vector element <text> exceeds the total of 4294967296
  bytes of expanded bus text per netlist run when expanded; written
  unexpanded`.
- `SCHEM-NET-007` (warning): top-level ports share a `sim_pinnumber`; the
  stable sort keeps their instance order (Q-SCHEM-5).
- `SCHEM-NET-008` (error): the pins of a symbol and the ports of its
  schematic differ in names or directions (`ipin` in, `opin` out, `iopin`
  inout); the netlist is written and the run fails.
- `SCHEM-NET-009` (warning): two subcircuit symbols from different files
  have one cell name; xschem-compat ON writes the first only, OFF writes the
  later one as `<cell>_<k>` (Q-SCHEM-14).
- `SCHEM-NET-010` (warning): a template key occurs in `extra` only inside a
  word (Q-SCHEM-19).
- `SCHEM-NET-011` (warning): a subcircuit template spans several lines
  (Q-SCHEM-21).
- `SCHEM-NET-012` (warning): `load_schematic(): unable to open file:
  <path>`, a subcircuit schematic that does not exist; its definition has an
  empty body and the run does not fail (Q-SCHEM-27).
- `SCHEM-NET-013` (warning): `instance: <index> (<symbol>): no name attribute
  set`.
- `SCHEM-NET-014` (warning): `Symbol: <symbol>: no type attribute set`.
- `SCHEM-NET-015` (warning): every pin of an instance is unconnected.
- `SCHEM-NET-016` (warning): two identical instances of one symbol overlap.
- `SCHEM-NET-017` (warning): the flat netlist stopped (Q-SCHEM-42, user
  decision M8-D1 approved 2026-11-22, both modes): `flat netlist stopped:
  depth limit of 64 levels` (`schem::kMaxFlatDepth`; the instance below the
  limit is kept as an `X` line and flattening goes on with the next line)
  or `flat netlist stopped: output passed 268435456 bytes`
  (`schem::kMaxFlatBytes`; each line counts with its LF and each expanded
  instance counts its own line; nothing after the line that would pass the
  limit is written). What was written so far is kept and the run does not
  fail.
- `SCHEM-NET-018` (error): `netlist run interrupted`: the caller's interrupt
  check (`NetlistEnvironment::interrupted`, polled once per schematic level
  placed) fired; the run returns no text and no computed labels. `schematic
  netlist` and `check` report it as SCHEM-CMD-011.
- `SCHEM-NET-019` (error): `undriven node: <net>`: the net driver check
  ("Errors and warnings", both modes) found a net with no `out` and no
  `inout` pin; the netlist is written and the run fails (exit status 10),
  but a simulation deck is still built when the driver check is the run's
  only error (Q-SCHEM-53, approved 2026-11-27; `NetlistResult::
  driver_check_only`). xschem prints `Error: undriven node: <net>`.
- `SCHEM-NET-020` (warning, an error when the `xschemrc` sets
  `erc_open_net_is_error` true): `open net: <net>`, a net with exactly one
  pin. With xschem-compat ON it is shown only in the report of a failed
  run (`driver_report_text`), as xschem's `Warning: open net: <net>`.
- `SCHEM-NET-021` (warning, an error when the `xschemrc` sets
  `erc_shorted_output_is_error` true): `shorted output node: <net>`, a net
  with two or more `out` pins; shown as SCHEM-NET-020 is.

## EDIT

Codes of the document model, the editing operations and net highlighting
(`src/core/src/schem/edit/`; capability schematic-editing; design D11).
Owner: EDIT.

The errors raise a script error and leave the document as it was
(schematic-command "Return values and errors in scripts").

- `SCHEM-EDIT-001`: `schematic: <file> is read-only (PDK store or system
  library); use "schematic save <file>" to save it in a user folder`, from
  every option that changes a document whose file is under `PDK_ROOT`, under
  `<CAD_ROOT>/assets/schematic` or under a read-only root of the project
  sandbox ("Read-only libraries"). Viewing, selecting, highlighting and
  descending stay allowed.
- `SCHEM-EDIT-002`: `schematic: cannot descend into <symbol>: its type is not
  subcircuit or primitive` ("Hierarchy navigation"; `descend -symbol` opens
  the symbol of any instance).
- `SCHEM-EDIT-003`: `schematic: descending is limited to 40 levels`.
- `SCHEM-EDIT-004`: `schematic: already at the top of the hierarchy`, from
  `ascend` at the top (or after another document was made active, which
  becomes the new top).
- `SCHEM-EDIT-005` (warning): `schematic: bus width mismatch at pin <pin> of
  <instance>: net <net> has <n> bits, the pin <p> and the instance <w>`, when
  the net width is neither the pin width nor the pin width times the
  instance width ("Bus editing"). Reported after an edit, once per mismatch:
  an edit reports only mismatches the previous edit of the document did not
  have.
- `SCHEM-EDIT-006`: `schematic: no schematic <file> for <symbol>` or
  `schematic: cannot open <file>`, from `descend` when the schematic (or,
  with `-symbol`, the symbol file) does not exist or cannot be read.
- `SCHEM-EDIT-007`: `schematic: <file> exists; use "schematic symbol make
  -force"`, from `symbol make` without `-force` and `symbol toschematic` when
  the file to write exists ("Labels and ports": no overwrite without
  confirmation).
- `SCHEM-EDIT-008`: `schematic: the clipboard holds no schematic objects`,
  from `paste` when the clipboard is empty or its text has no object records
  ("Clipboard").

## CMD

Codes of the `schematic` command and the `xschem` Tcl command
(`src/core/src/schem/cmd/`; capability schematic-command; design D12).
Owner: CMD.

Every one is an error that raises a script error (Requirement "Return
values and errors in scripts").

- `SCHEM-CMD-001`: `schematic: ambiguous option "<w>"` or `schematic:
  unknown option "<w>". Use "schematic help".` for the option word and for
  keyword arguments (`new schematic|symbol`, `-window`, `path list|set|prepend`,
  `getprop instance|symbol|wire|schematic`, the `K`..`E` tags).
- `SCHEM-CMD-002`: `Usage: schematic <syntax>` with the option's line of
  `schematic help` (missing or extra arguments, an unknown or incomplete
  flag, two of `-hier`/`-top`/`-flat`, a `-buschar` that is not two
  characters); for `xschem`, `xschem <subcommand>: wrong number of arguments`,
  and `xschem get_tok: bad argument "<n>"` for a third argument that is not
  an integer (Q-SCHEM-62).
- `SCHEM-CMD-003`: `schematic: no schematic is open`, for every option
  except `open`, `new`, `reload_symbols`, `path`, `resolve`, `pdk`, `help`
  and `clipboard` (but `clipboard copy` needs a document).
- `SCHEM-CMD-004`: `schematic: unsaved changes`, from `close` without
  `-discard` and `open -window current` over a modified document, and from
  `xschem clear` without `force` over a modified document with
  `xschem-compat` OFF ("clear without force").
- `SCHEM-CMD-005`: the netlist run of `netlist` or `check` reported errors;
  the message is the list of error texts, one per line (the diagnostics
  themselves are reported first). `netlist <file> -force` writes the file
  before raising.
- `SCHEM-CMD-006`: `schematic: no instance "<name>"` (also `no symbol`,
  `no wire`, `no box <n> on layer <l>`), `xschem getprop: instance not
  found:<inst>` (a missing name or an index past the last instance),
  `xschem getprop rect: no such box`, `xschem translate: instance not found`,
  and the `xschem descend` / `change_sch_path` errors for a missing instance
  or vector element.
- `SCHEM-CMD-007`: `schematic: PDK <name> has no xschem data (<file>)`, from
  `schematic pdk <name>` when `<PDK root>/<name>/libs.tech/xschem/xschemrc`
  does not exist or no PDK store is known.
- `SCHEM-CMD-008`: `xschem command '<cmd>' is not supported`, also
  `'get <key>'` and `'set <var>'` (capability schematic-command
  "xschem compatibility command").
- `SCHEM-CMD-009`: `xschem set netlist_type: only spice is supported`.
- `SCHEM-CMD-010`: `xschem command '<cmd>' needs the editor`, from
  `place_symbol`, `paste` without a point and `saveas` without a file.
- `SCHEM-CMD-011`: `schematic: the netlist run was interrupted; no file is
  written`, from `netlist` and `check` when the front end's Interrupt
  (Ctrl-C, the Interrupt card) was pending before the run or was made during
  it; no file is written and no computed label is stored ("Subcommand
  table"). The dispatcher adds `[Interrupted]` after the command.

## SIMINT

Codes of the schematic circuit source and deck building, M8 part
(`src/core/src/schem/sim/`; capability schematic-simulation-integration;
design D13). Owner: SIMINT.

- `SCHEM-SIM-001` (error): `Control command '<cmd>' is not allowed in
  simulation decks` ("Control block translation"): a `.control` command of a
  schematic-source deck is `shell`, `cd`, `load`, `source`, `codemodel`,
  `osdi` or `pre_osdi`, or reaches one at run time (a `$variable` or
  `!history` command word, an `alias` or backquoted command naming a refused
  one; `<cmd>` is that word). No deck is built.
- `SCHEM-SIM-002` (warning): ``Simulation deck: cannot find "<path>" of
  `<line>`; kept as written`` ("Testbench code blocks"): a `.include` or
  `.lib <file> <section>` path resolves to no file in the netlist folder, the
  schematic's folder or the PDK store. ngspice may still find it at run time
  (its `sourcepath`).
- `SCHEM-SIM-003` (warning): `Launcher command skipped: <message>; the
  launcher went on` ("Simulation launchers"): a confirmed launcher's `xschem`
  command is not supported (`xschem command '<cmd>' is not supported`), or
  `simulate`, `xschem raw_read` or `xschem annotate_op` needs simulation
  results (`not implemented yet: ...`, until M9).
- `SCHEM-SIM-004` (error): `Simulation deck not built: <first netlist
  error>` ("Schematic circuit source"): the testbench failed to netlist
  (for example a short, SCHEM-NET-001).

## Table

The table below is generated from the fragments:

```sh
hefesto-cli --diagnostic-codes=schem
```

The unit test `each area's code table matches its document and prefix`
fails when the two disagree.

<!-- BEGIN DIAGNOSTIC CODES (generated by `hefesto-cli --diagnostic-codes=schem`) -->
| Code | Default severity | Meaning |
|---|---|---|
| `SCHEM-CMD-001` | error | Unknown or ambiguous `schematic` option or keyword argument. |
| `SCHEM-CMD-002` | error | Wrong arguments: the usage line of the `schematic` option, or of the `xschem` subcommand. |
| `SCHEM-CMD-003` | error | The `schematic` option needs an open schematic document and none is open. |
| `SCHEM-CMD-004` | error | The active document has unsaved changes; it is not closed or replaced. |
| `SCHEM-CMD-005` | error | The netlist run reported errors; the message lists them. |
| `SCHEM-CMD-006` | error | No instance, symbol, wire or box with that name or index in the active document. |
| `SCHEM-CMD-007` | error | The PDK named by `schematic pdk` has no xschem data (`libs.tech/xschem/xschemrc`). |
| `SCHEM-CMD-008` | error | `xschem`: unsupported subcommand, `get` key or `set` variable. |
| `SCHEM-CMD-009` | error | `xschem set netlist_type`: only `spice` is supported. |
| `SCHEM-CMD-010` | error | `xschem place_symbol`, `paste` without a point or `saveas` without a file needs the interactive editor. |
| `SCHEM-CMD-011` | error | `schematic netlist` or `check` was interrupted; no file is written. |
| `SCHEM-EDIT-001` | error | The active document is in the PDK store or the system library and is read-only; save it in a user folder to edit it. |
| `SCHEM-EDIT-002` | error | The instance cannot be descended into: its symbol is not a `subcircuit` or `primitive`. |
| `SCHEM-EDIT-003` | error | Descending would pass the hierarchy depth limit of 40 levels. |
| `SCHEM-EDIT-004` | error | `schematic ascend` at the top of the hierarchy. |
| `SCHEM-EDIT-005` | warning | Bus width mismatch: the net on a pin is neither the pin width nor the pin width times the instance width. |
| `SCHEM-EDIT-006` | error | The schematic or symbol file to descend into does not exist or cannot be read. |
| `SCHEM-EDIT-007` | error | `schematic symbol make` or `toschematic` would overwrite an existing file (`make -force` overwrites). |
| `SCHEM-EDIT-008` | error | `schematic paste`: the clipboard holds no schematic objects. |
| `SCHEM-FILE-001` | warning | A record has a missing or non-numeric field and is skipped. |
| `SCHEM-FILE-002` | warning | A graphic record has a missing or out-of-range layer and is skipped. |
| `SCHEM-FILE-003` | warning | The file ends inside a brace string; the field is read as absent. |
| `SCHEM-FILE-004` | warning | An embedded symbol block comes before any instance and is skipped. |
| `SCHEM-FILE-005` | warning | The file ends inside an embedded symbol block; the block is kept. |
| `SCHEM-FILE-006` | error | The file changed on disk since it was loaded; it is not overwritten. |
| `SCHEM-FILE-007` | warning | Embedded symbol blocks are nested deeper than 64 levels; the block is skipped. |
| `SCHEM-FILE-008` | warning | An embedded symbol nests symbols 40 levels deep; the rest of its block is not read. |
| `SCHEM-LIB-001` | warning | A `./` or `../` reference, or one found through the `.` search-path element, was resolved against the schematic folder, where xschem uses its start folder (Q-SCHEM-30). |
| `SCHEM-LIB-002` | warning | An `http://` or `https://` symbol reference is not fetched; the instance is shown as a missing symbol (Q-SCHEM-9). |
| `SCHEM-LIB-003` | warning | Symbol not found: the reference resolves to no file; the instance is shown as a missing symbol. |
| `SCHEM-LIB-004` | warning | A symbol file exists but could not be read or parsed; the instance is shown as a missing symbol. |
| `SCHEM-LIB-005` | warning | Symbol recursively instantiating symbol: an instance inside a schematic used as a symbol would open level 40; the rest of the symbol is not read. |
| `SCHEM-LIB-006` | warning | An `xschemrc` file raised a Tcl error; the settings it would have made after the error are lost. |
| `SCHEM-LIB-007` | info | `xschem_execute_scripts` in an `xschemrc` is ignored: `tcleval` values always run in the sandbox and launchers are confirmed (Q-SCHEM-25). |
| `SCHEM-LIB-008` | info | A line an `xschemrc` file printed with `puts`. |
| `SCHEM-LIB-009` | warning | A schematic used as a symbol holds more than 1,000,000 records or 256 MiB when flattened; the rest is not loaded (Q-SCHEM-32, Q-SCHEM-40). |
| `SCHEM-LIB-010` | warning | Evaluating the `xschemrc` files of a document stopped at the 10 s limit; later settings are lost (Q-SCHEM-32). |
| `SCHEM-LIB-011` | warning | A symbol generator that is not a Tcl script is not run (symbol generators that run external programs are not supported); the instance is shown as a missing symbol (Q-SCHEM-11). |
| `SCHEM-LIB-012` | warning | A Tcl symbol generator failed, ran past its 10 s limit, printed more than 256 MiB, its script is larger than 256 MiB, or the generators of the symbol cache spent their shared 60 s; the instance is shown as a missing symbol. |
| `SCHEM-NET-001` | error | Two differently named nets are shorted (`Error: <tag> shorted: <a> - <b>`); the first name is kept and the netlist is still written. |
| `SCHEM-NET-002` | warning | A net, pin or instance name is not valid bus notation (`syntax error in <string>`); it is used unchanged with unknown width. |
| `SCHEM-NET-003` | warning | A pin has no net and is written as `__UNCONNECTED_PIN__<k>`. |
| `SCHEM-NET-004` | warning | An `expr` number contains a `-` that xschem-compat ON and OFF read differently (Q-SCHEM-12). |
| `SCHEM-NET-005` | warning | A format token is undefined while a shorter identifier prefix of it is defined, for example `@W/` and `W` (Q-SCHEM-26). |
| `SCHEM-NET-006` | warning | A bus expansion, `expr` nesting or vector element exceeds a netlisting size limit; the text is used unchanged. |
| `SCHEM-NET-007` | warning | Top-level ports share a `sim_pinnumber`; they keep their instance order (Q-SCHEM-5). |
| `SCHEM-NET-008` | error | The pins of a symbol and the ports of its schematic differ; the netlist is written and the run fails. |
| `SCHEM-NET-009` | warning | Two subcircuit symbols have one cell name; xschem-compat ON writes the first only, OFF renames the later one (Q-SCHEM-14). |
| `SCHEM-NET-010` | warning | A template key occurs in `extra` only inside a word (Q-SCHEM-19). |
| `SCHEM-NET-011` | warning | A subcircuit template spans several lines (Q-SCHEM-21). |
| `SCHEM-NET-012` | warning | The schematic of a subcircuit does not exist; its definition has an empty body (Q-SCHEM-27). |
| `SCHEM-NET-013` | warning | An instance has no `name` attribute and its template has none either. |
| `SCHEM-NET-014` | warning | A symbol has no `type` attribute. |
| `SCHEM-NET-015` | warning | Every pin of an instance is unconnected. |
| `SCHEM-NET-016` | warning | Two identical instances of one symbol overlap. |
| `SCHEM-NET-017` | warning | The flat netlist stopped at the depth limit of 64 levels or after 268435456 bytes of output (Q-SCHEM-42); what was written so far is kept and the run does not fail. |
| `SCHEM-NET-018` | error | The netlist run was interrupted (the environment's `interrupted` callback returned true); no netlist text is returned. |
| `SCHEM-NET-019` | error | Net driver check: a net has no `out` and no `inout` pin (`Error: undriven node: <net>`); the netlist is written and the run fails, but a simulation deck is still built (Q-SCHEM-53). |
| `SCHEM-NET-020` | warning | Net driver check: a net carries exactly one pin (`Warning: open net: <net>`); an error when the xschemrc sets `erc_open_net_is_error`. |
| `SCHEM-NET-021` | warning | Net driver check: a net carries two or more `out` pins (`Warning: shorted output node: <net>`); an error when the xschemrc sets `erc_shorted_output_is_error`. |
| `SCHEM-SIM-001` | error | A `.control` line of a schematic-source deck names a command that is not allowed in simulation decks (`shell`, `cd`, `load`, `source`, `codemodel`, `osdi`, `pre_osdi`, or one named only at run time). |
| `SCHEM-SIM-002` | warning | A `.include` or `.lib` path of a schematic-source deck names no file in the netlist folder, the schematic's folder or the PDK store; it is kept as written. |
| `SCHEM-SIM-003` | warning | A launcher's `xschem` command (or `simulate`) is not supported or not available until M9; it was skipped and the rest of the launcher ran. |
| `SCHEM-SIM-004` | error | The testbench of a schematic circuit source could not be netlisted, so no simulation deck was built. |
| `SCHEM-SYM-001` | warning | Pins of a symbol share a `sim_pinnumber`; they keep their file order (Q-SCHEM-5). |
| `SCHEM-SYM-002` | warning | A displayed-text token is undefined while an identifier prefix of it is defined: a token ends only before a blank, `@` or `%` (Q-SCHEM-26). |
| `SCHEM-TCL-001` | error | A procedure provided to `tcleval` values (`abs_sym_path`, `write_data`, ...) was called with the wrong arguments or refused. |
| `SCHEM-TCL-002` | info | A launcher's `tclcommand` did not run: the user did not confirm it for this document. |
| `SCHEM-TCL-003` | warning | A `tcleval` value stopped at its 10 s limit, or the `tcleval` values of one netlist run spent their shared 60 s; the value is `?`. |
<!-- END DIAGNOSTIC CODES -->
