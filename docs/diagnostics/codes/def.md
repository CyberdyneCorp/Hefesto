# Diagnostic codes: def area

Codes `DEF-<TOPIC>-<NNN>` emitted by `src/core/src/def/` (DEF read and DEF write;
`openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md` D1/D2). Conventions and the
renderings are described in [../codes.md](../codes.md).

The area's table is assembled by `src/core/src/def/codes.cpp` (owner:
INTEGRATION) from one fragment per workstream, each owned by that
workstream. The rows are sorted while compiling
(`src/core/src/diag/sorted_codes.hpp`):

| Fragment | Topics | Owner |
|---|---|---|
| `src/core/src/def/read/codes.inc` | `DEF-READ-*` | DEFR |
| `src/core/src/def/write/codes.inc` | `DEF-WRITE-*` | DEFW |

Every option and function the M6 scaffold declared is implemented; its stub
code `CORE-NOTIMPL-006` is retired ([../codes.md](../codes.md)).

## READ

Codes of the DEF reader (`src/core/src/def/read/`; capability def-read; design D8). Owner: DEFR.

Reader messages keep Magic's LefError form, `DEF read, Line <n> (Error|Warning|Message): <text>`, and are counted and capped by the shared counters (`LEF-LEX-001`, `LEF-LEX-002`). Messages of the shared LEF helpers (layer names, RECT parsing, END matching, cut growing) keep Magic's `LEF read` prefix in a DEF read; the via code's messages take their prefix from `lef::via_message_dialect`.

- `DEF-READ-001`, `DEF-READ-002`, `DEF-READ-003`: Magic's `LefError(DEF_ERROR | DEF_WARNING | DEF_INFO)` texts, for example `Unknown layer type "<t>" for NEW route`, `Number of nets read (2) does not match the number declared (3).` and `Unknown keyword "<t>" in DEF file; ignoring.`. With magic-compat OFF, `FILLS`, `SLOTS` and `STYLES` give one `DEF-READ-003` message, `<SECTION> section not supported; skipped.` (Q-LEFDEF-84).
- `DEF-READ-004`: the lines Magic prints with TxPrintf: `Reading DEF data from file <name>.`, the `  Processed N ... total.` line of each section, `DEF read: Processed <n> lines.` and `DEF read: Design has no contents.`. They are not counted. Magic's `This action cannot be undone.` is not printed: a DEF read is one undoable step (Q-LEFDEF-22).
- `DEF-READ-005`: `Cannot open input file <name> (<reason>).`; the command fails.
- `DEF-READ-006`: annotation (`-annotate`) found no paint under any route rectangle of a net: `Warning:  Label "<net>" did not land on any existing net geometry.` (TxError, not counted). The label goes on the last rectangle.
- `DEF-READ-007`, `DEF-READ-008`: Hefesto-only errors and warnings where Magic crashes, reads an undefined value or keeps no record of a fix: `No reference point for RECT` (Q-LEFDEF-73), `Pin specified without placement, was not placed.` (Q-LEFDEF-65 OFF), `Via "<name>": ROWCOL <r> <c> gives more than 1000000 cuts; the via is not generated.` (Q-LEFDEF-117, both modes), `Duplicate instance-id for cell <c> (<id>) will be renamed` (Q-LEFDEF-83), `Repeated UNITS statement; the new value replaces the previous one.` (Q-LEFDEF-79 OFF), `Unknown layer "<n>" in NONDEFAULTRULE definition.` (Q-LEFDEF-75) and `Blockage without a layer (a placement blockage) is not painted.` (Q-LEFDEF-76). They are counted with the reader's messages.

## WRITE

Codes of the DEF writer (`src/core/src/def/write/`; capability def-write; design D9). Owner: DEFW.

The writer keeps Magic's texts. Lines Magic prints with TxPrintf are `info`, those it prints with TxError are `warning` or `error`.

- `DEF-WRITE-001`: `Generating DEF output <file> for cell <c>:`, the `Diagnostic:` progress lines (`Write DEF header for cell`, `Finding all components of cell`, `Finding all pins of cell`, `Finding all nets in cell`, `(This can take a while!)`) and the special-net notes of the Tcl build (`Node <n> is defined in the "globals" array`, `Node <n> matches VDD variable definition!`, `... GND ...`; Q-LEFDEF-102, from the project setting "global nets and power names").
- `DEF-WRITE-002`: `Cannot open output file <name> (<reason>).`; the command fails and the session is unchanged (no generated via key, no rule).
- `DEF-WRITE-003`: `Please name the cell before generating DEF.`
- `DEF-WRITE-004`: `Warning:  Circuit has no .ext file;  no nets written.` and `Run extract on this circuit if you want nets in the output.`. The netlist reader's own failure (`EXTFLAT-READ-001` `Cannot read extract file <name>.ext`) is Hefesto-only, since Magic leaves it in the Tcl result.
- `DEF-WRITE-005`: `<cell>.ext is out of date; nets may be wrong. Run extract.` (Q-LEFDEF-110, magic-compat OFF only).
- `DEF-WRITE-006`: `No LEF layer corresponding to layer <x> of pin "<y>".`, with Magic's swapped arguments (label text, then layer) under magic-compat ON and the layer, then the pin, OFF (Q-LEFDEF-114).
- `DEF-WRITE-007`: `Nothing of type <type> found at node <net> location (floating label?)!` (an electrically active type only).
- `DEF-WRITE-008`: `Cannot find via name <key> in table!`; the via is written as `<layer>_<w>_<h>`.
- `DEF-WRITE-009`: the `def write` argument notes: the `def` help (a misplaced word, an unknown option, a wrong count), `The "<opt>" option is only for lef write` and `The "-units" option requires an argument.` (also for a missing value, Q-LEFDEF-104).
- `DEF-WRITE-010`: `No cell selected`, and the Hefesto-only `No technology is loaded; nothing is written.`
- `DEF-WRITE-011`: `The "-units" value must be a positive integer; got <n>.` (Q-LEFDEF-104 fix: Magic writes every coordinate as 0 or negative).
- `DEF-WRITE-012`: Hefesto-only warnings where Magic 8.3.694 crashes and Hefesto writes defined output: a net that is neither a port nor special has blockage geometry (Q-LEFDEF-101; Magic's collector reads its data from the plane number, SIGSEGV), a non-default rule entry with no LEF record (null `lefInfo`), a via whose route residue has no LEF name (`strlen(NULL)`), and a `.ext` node layer that is not a type of the technology (`DBConnectTbl[-1]`).

## Table

The table below is generated from the fragments:

```sh
hefesto-cli --diagnostic-codes=def
```

The unit test `each area's code table matches its document and prefix`
fails when the two disagree.

<!-- BEGIN DIAGNOSTIC CODES (generated by `hefesto-cli --diagnostic-codes=def`) -->
| Code | Default severity | Meaning |
|---|---|---|
| `DEF-READ-001` | error | DEF read error (Magic's LefError DEF_ERROR text): unknown layer or via in a route, bad coordinates, unknown pin class, a section END missing. |
| `DEF-READ-002` | warning | DEF read warning (Magic's LefError DEF_WARNING text): section count mismatch, technology name mismatch, converted instance name, unplaced component. |
| `DEF-READ-003` | info | DEF read message (Magic's LefError DEF_INFO text): unknown keyword or property ignored. |
| `DEF-READ-004` | info | DEF read report line: `Reading DEF data from file ...`, `Processed N ... total.`, `DEF read: Processed N lines.`, `DEF read: Design has no contents.`. |
| `DEF-READ-005` | error | The DEF file cannot be opened: `Cannot open input file <name> (<reason>).`. |
| `DEF-READ-006` | warning | Annotation: `Label "<net>" did not land on any existing net geometry.`; the label goes on the last route rectangle. |
| `DEF-READ-007` | error | Hefesto-only DEF read error where Magic crashes or reads an undefined value: RECT without a reference point, pin without placement, a generated via above 1000000 cuts. |
| `DEF-READ-008` | warning | Hefesto-only DEF read warning: duplicate instance renamed, repeated UNITS replaced, unknown NONDEFAULTRULE layer, placement blockage not painted. |
| `DEF-WRITE-001` | info | DEF write report line: `Generating DEF output <file> for cell <c>:`, the `Diagnostic:` progress lines and the special-net notes (`Node <n> matches VDD variable definition!`). |
| `DEF-WRITE-002` | error | DEF write: the output file cannot be opened (`Cannot open output file <name> (<reason>).`); nothing is written. |
| `DEF-WRITE-003` | error | DEF write: the cell is unnamed (`Please name the cell before generating DEF.`); nothing is written. |
| `DEF-WRITE-004` | warning | DEF write: the cell's `.ext` netlist cannot be read; no SPECIALNETS or NETS are written. |
| `DEF-WRITE-005` | warning | DEF write: a `.ext` file is older than its cell (Q-LEFDEF-110, magic-compat OFF); the nets may be wrong. |
| `DEF-WRITE-006` | error | DEF write: a pin's layer has no LEF name (Q-LEFDEF-114); its LAYER line is not written. |
| `DEF-WRITE-007` | warning | DEF write: nothing of a node's type at its location (`Nothing of type <t> found at node <n> location (floating label?)!`). |
| `DEF-WRITE-008` | error | DEF write: a contact region has no via in the position index (`Cannot find via name <key> in table!`). |
| `DEF-WRITE-009` | info | def write arguments: the help, `The "<opt>" option is only for lef write`, `The "-units" option requires an argument.`. |
| `DEF-WRITE-010` | error | def write: no cell is selected, or no technology is loaded; nothing is written. |
| `DEF-WRITE-011` | error | def write: the `-units` value is not positive (Q-LEFDEF-104 fix); nothing is written. |
| `DEF-WRITE-012` | warning | Hefesto-only: an input on which Magic 8.3.694 crashes in def write (Q-LEFDEF-101 blockage nets, a rule or via layer with no LEF record, a `.ext` layer the technology lacks); Hefesto writes defined output. |
<!-- END DIAGNOSTIC CODES -->
