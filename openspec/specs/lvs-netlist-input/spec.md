# LVS Netlist Input

## Purpose
Defines how the built-in layout-versus-schematic (LVS) engine reads the two netlists it compares. Decision P-5 requires the results to be compatible with netgen. This covers:
- the netlist database
- format selection
- the SPICE/CDL reader, including `.subckt`, `.include`, `.param`, `.global` and `.model`
- structural Verilog
- the `.sim` reader
- netgen's legacy `.ext` reader
- library (black-box) reads
- name matching rules
- SI value parsing

**Compatibility adaptation.** This capability is about netgen, not Magic. The compatibility classes in `docs/spec-conventions.md` therefore apply relative to **netgen** (`RTimothyEdwards/netgen` @ `93dcda14d36349d0b0ca63168d3cbbab20fe2ece`, version 1.5.327):
- `exact`: identical to netgen's data model and accepted input
- `behavioral`: same comparison semantics
- `informative`: netgen detail that Hefesto MAY differ from
- `native`: Hefesto-only, and cites `(Hefesto decision: docs/decisions/product.md#P-5)`

Traces use `(netgen ref: path/file.c:line)`. They exist for traceability only. Implementers SHALL work from this spec and SHALL NOT consult the GPL sources (capability `hefesto-licensing`).

Other capabilities own related parts:
- How a layout netlist is produced from extraction: capabilities `extract-commands`, `netlist-spice-options`, `netlist-spice-hierarchy` and `netlist-spice-devices`, and the LVS entry point in capability `lvs-command`.
- Setup-file commands: capability `lvs-setup-file`.

Open questions are in `docs/decisions/open-questions/LVS.md` (Q-LVS-n).

## Requirements

### Requirement: Netlist database model
The LVS engine SHALL keep a single database of cell definitions. Each cell SHALL be identified by the pair (name, file number). Each netlist read SHALL get its own file number: an integer ≥ 0, assigned in increasing order of reading. A cell SHALL hold:
- an ordered list of ports
- an ordered list of internal nets
- an ordered list of instances
- a device class
- a property-definition table, also called the "properties of interest" (see capability `lvs-property-matching`)
- a list of permutable pin pairs
- flags: top, placeholder, case-insensitive, duplicate, matched, verilog, series-combinable, no-parallel

Each instance SHALL record:
- its class (cell) name
- its instance name
- one connection per port of the class, in port order
- zero or more property records, each an ordered list of key/value pairs

Nets SHALL be identified by positive integers. An unconnected port SHALL have no net. The cell that represents a whole file SHALL carry the file's name and the top flag.
Compatibility: behavioral
(netgen ref: base/objlist.h:126-245, base/objlist.c:386)

#### Scenario: Two files get distinct numbers
- **GIVEN** an empty database
- **WHEN** `layout.spice` and then `schem.spice` are read
- **THEN** they SHALL receive file numbers 0 and 1, and a cell `inv` defined in both SHALL exist as the two distinct cells (`inv`, 0) and (`inv`, 1)

### Requirement: Device classes
Every cell SHALL have exactly one class from this set:
- undefined
- nmos and pmos: 3 pins (drain, gate, source)
- nmos4 and pmos4: 4 pins
- fet: 3 or 4 pins
- bjt, npn and pnp: 3 pins
- resistor: 2 pins, or res3 with 3 pins
- capacitor: 2 pins, cap3 with 3 pins, or ecap with 3 pins
- diode: 2 pins
- inductor: 2 pins
- transmission line: 4 pins
- voltage source and current source
- module (black box)
- subcircuit

The primitive classes SHALL be every class except subcircuit and module. Only subcircuit cells SHALL be flattenable and descended into during hierarchical comparison. Module cells SHALL be compared as opaque devices with pin checking (see capability `lvs-hierarchy`).
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:1563-1705, base/netcmp.c:3637-3733)

#### Scenario: Module is not flattened
- **GIVEN** a cell `sram` of class module instanced in both circuits
- **WHEN** hierarchical comparison runs
- **THEN** `sram` SHALL be treated as a device whose pins are compared, and its instances SHALL never be flattened

### Requirement: Format selection
Reading a netlist SHALL accept an optional format and an optional file number: `readnet [format] file [filenum]`.

**Formats.** The format keyword SHALL be matched with unique-prefix matching against `automatic ext extflat sim prm ntk spice verilog netgen actel xilinx`. If the first argument of a 2-argument form is not a format, it SHALL be taken as the file name with format `automatic`.

**Automatic detection.** Detection SHALL use these steps in order:
1. **Known suffix.** If the file name ends with a known suffix, the matching reader SHALL be used. The suffixes are tested in this order: `.ntk`, `.ext`, `.sim`, `.prm`, `.spice`, `.spc`, `.sp`, `.spi`, `.fspc`, `.cir`, `.ckt`, `.v`, `.sv`, `.ntg`. The first five SPICE suffixes and `.cir`/`.ckt` select SPICE; `.v`/`.sv` select Verilog.
2. **Appended suffix.** Otherwise each suffix SHALL be appended in that same order, and the first resulting file that exists SHALL be read.
3. **First character.** Otherwise, if the file exists, its first character SHALL decide: `*` selects SPICE and `|` selects `.sim`. Any other first character SHALL fail with `ReadNetlist: don't know type of file '<f>'`.

A missing file SHALL fail with `ReadNetlist: unable to find file '<f>'`.

**Re-reads and file numbers.** If a cell with the file's name already exists, the file SHALL NOT be read again. Its existing file number SHALL be returned. If a different explicit file number was given, the read SHALL fail with `File is already loaded as a different file number.` A negative explicit file number SHALL be rejected. An explicit file number SHALL make the new cells belong to that netlist.

On success the command SHALL return the file number.
Compatibility: exact
(netgen ref: base/netfile.c:910-1002, base/netfile.h:4-22, tcltk/tclnetgen.c:709-847)

#### Scenario: Suffix detection
- **WHEN** `readnet top.cir` is issued
- **THEN** the SPICE reader SHALL be used

#### Scenario: First-character detection
- **GIVEN** a file `netlist` whose first line is `* layout`
- **WHEN** `readnet netlist` is issued and none of `netlist.ntk` … `netlist.ntg` exists
- **THEN** the file SHALL be read as SPICE

#### Scenario: Re-read returns same number
- **GIVEN** `a.spice` was read as file 0
- **WHEN** `readnet a.spice` is issued again
- **THEN** the result SHALL be 0 and nothing SHALL be re-parsed

### Requirement: Name matching and case sensitivity
Reading SPICE SHALL make name matching case-insensitive. The only exception: if a case-sensitive format was read earlier in the session, SPICE matching SHALL stay case-sensitive and SHALL print `Warning:  A case-sensitive file has been read and so the SPICE netlist must be treated case-sensitive to match.` The `.sim`, `.ext` and Verilog readers SHALL set case-sensitive matching.

When two cells are compared, matching SHALL be case-insensitive only if both compared top cells were read case-insensitively. Otherwise it SHALL be case-sensitive.

Case-insensitive name equality SHALL hold when both of these are true:
1. The strings are equal after mapping each byte through a fold table. The table maps `a`–`z` to `A`–`Z`. It maps `[`, `{` and `<` to `<`, and `]`, `}` and `>` to `>`. So `a[3]`, `A<3>` and `a{3}` are equal.
2. A leading backslash on exactly one of the names is ignored, together with one trailing space after the compared text (Verilog escaped names).

A null name SHALL never be equal to anything.

Case-sensitive equality SHALL be byte equality. Hash keys used to find cells by name SHALL fold only letters, not brackets (see Q-LVS-3).
Compatibility: exact
(netgen ref: base/objlist.c:181-282, base/hash.c:116-174, base/spice.c:2025-2035, base/netcmp.c:3921-3933)

#### Scenario: Bus delimiter equivalence
- **GIVEN** both netlists are SPICE
- **WHEN** layout net `data[3]` is compared with schematic net `DATA<3>`
- **THEN** the names SHALL be equal

#### Scenario: Mixed sensitivity
- **GIVEN** circuit 1 is Verilog and circuit 2 is SPICE
- **WHEN** they are compared
- **THEN** `Vdd` and `VDD` SHALL be different names

### Requirement: SPICE lexical structure
The SPICE reader SHALL apply these lexical rules.

**Tokens.** Tokens SHALL be separated by space, tab, CR and LF.

**Line joining.**
- A line beginning with `+` SHALL continue the previous logical line.
- A full comment line (first character `*`) that appears between continuation lines SHALL be skipped without ending the logical line.
- A backslash immediately before the end of a line SHALL join it with the next physical line.

**Comments.** A logical line whose first token begins with `*` SHALL be a comment. The exception is `*.GLOBAL` (case-insensitive), which SHALL be handled like `.GLOBAL`.

**Quotes.** Inside single quotes or double quotes, spaces SHALL be removed so that a quoted expression forms one token. A single quote directly after a digit SHALL NOT open a quoted string.

**Ignored blocks.** Lines from `.CONTROL` through `.ENDC` SHALL be ignored.

**Card names.** Card names SHALL be matched case-insensitively. Element letters SHALL be matched case-insensitively.

**Unknown cards.** An unrecognized card SHALL be skipped with `Ignoring line starting with token: <tok>`. After 100 such warnings, this message SHALL be printed once and no more SHALL follow: `Too many warnings. . . will not report any more.`

**Binary input.** If a token has non-ASCII bytes and its length is less than 4 × their count, reading SHALL stop with `Input file "<f>" appears to be binary. . . bailing out`.

**Other cards.** `.END` SHALL be accepted silently. At the end of the file, a final summary SHALL be printed when there were warnings: `File <f> read with <n> warning[s].`
Compatibility: exact
(netgen ref: base/netfile.c:207-420, base/netfile.c:603-700, base/spice.c:519-560, base/spice.c:855-866, base/spice.c:1942-1993)

#### Scenario: Continuation with interleaved comment
- **GIVEN** these three lines:
  ```
  M1 d g s b nfet
  * note
  + W=1u L=0.15u
  ```
- **WHEN** the file is read
- **THEN** M1 SHALL have the properties `W=1u` and `L=0.15u`

#### Scenario: CDL global comment
- **WHEN** the line `*.GLOBAL VDD VSS` is read
- **THEN** `VDD` and `VSS` SHALL become global nets, as for `.GLOBAL VDD VSS`

### Requirement: SPICE subcircuit definitions
The reader SHALL handle subcircuit definitions as follows.

**Header.** `.SUBCKT name t1 t2 …` SHALL open the definition of cell `name` in the current file, with class subcircuit. Each remaining token SHALL be one of:
- the keyword `PARAMS:` (any case), which SHALL be ignored
- `key=value`, which SHALL declare a string-typed property of interest `key`, with default `value` and tolerance 0
- anything else, which SHALL declare a port, in order

A definition with no ports SHALL receive a single unnamed proxy port. Global nets of the enclosing cell SHALL be copied into the new cell.

**Nesting.** Definitions MAY nest. `.ENDS` SHALL close the innermost definition and resume the enclosing one. A `.SUBCKT` inside an unclosed definition SHALL print `Missing .ENDS statement on subcircuit.` `.ENDS` outside any definition SHALL print `.ENDS occurred outside of a subcircuit!` End of file inside a definition SHALL print `Missing .ENDS statement on subcircuit.` and close the definition.

**Port cleanup at `.ENDS`.** When a definition closes:
- A port with no net SHALL take the net of another port with the same name, if one exists.
- A proxy port SHALL receive a fresh net.
- Unconnected globals SHALL be removed.

**Duplicate definitions.** A second definition of a name that is already defined, and is not a module placeholder, SHALL rename the earlier cell and all its instances to `name[[k]]`. k is the smallest integer ≥ 0 that is not in use; numbering continues after an existing `[[n]]` suffix. The earlier cell SHALL be flagged duplicate. The reader SHALL print `Duplicate cell <name> in file` and `Renaming original cell to <name[[k]]>`, then define the new cell under `name`.

**Replacing a placeholder.** If `name` was created earlier as a placeholder for an undefined subcircuit, the placeholder SHALL be replaced:
- If its ports are named `1`, `2`, …, it SHALL be deleted and redefined.
- Otherwise (a Verilog placeholder) the SPICE definition SHALL be read into a temporary cell. After `.ENDS`, the pins of the placeholder and all its instances SHALL be reordered to the SPICE order using the pin-matching procedure of capability `lvs-hierarchy`. The reader SHALL print `Verilog placeholder <name> replaced by SPICE definition`.
Compatibility: exact
(netgen ref: base/spice.c:436-486, base/spice.c:560-760)

#### Scenario: Parameters on the header
- **WHEN** `.subckt res2 a b PARAMS: w=1 l=2` is read
- **THEN** `res2` SHALL have ports `a`, `b`, and string properties of interest `w` (default `1`) and `l` (default `2`)

#### Scenario: Redefinition
- **GIVEN** `.subckt buf a y` … `.ends` appears twice in one file
- **WHEN** the file is read
- **THEN** the first definition SHALL be renamed `buf[[0]]` and flagged duplicate, and the second SHALL be `buf`

### Requirement: SPICE primitive device cards
Each element card SHALL create an instance whose instance name is `<model>:<name>`, where `<name>` is the card name without its leading letter. The first time a device model is used in a file, the reader SHALL create a model cell with the ports, class and properties of interest listed in this table:

| Card | Terminals read | Model cell ports | Class | Properties of interest (type, tolerance, default) |
|---|---|---|---|---|
| `M` | d g s b, then model | drain gate source bulk | fet (4 pins) | `L` double 0.01 0; `W` double 0.01 0; `M` integer 0 1 |
| `Q` | c b e, then model | collector base emitter | bjt | `M` integer 0 1 |
| `D` | a k, then model | anode cathode | diode | `M` integer 0 1 |
| `C` | top bottom [value] [model] | top bottom | capacitor | `value` value-type 0.01 0; `M` integer 0 1 |
| `R` | end_a end_b [value] [model] | end_a end_b | resistor | `value` value-type 0.01 0; `M` integer 0 1 |
| `L` | end_a end_b [value] [model] | end_a end_b | inductor | `M` integer 0 1 |
| `T` | n1 n2 n3 n4 [model] | node1 node2 node3 node4 | transmission line | `M` integer 0 1 |
| `V` / `I` | pos neg [value] [model] | pos neg | voltage / current source | `M` integer 0 1 |
| `E` | pos neg ctrlp ctrln [value] [model] | pos neg ctrlp ctrln | module | `M` integer 0 1 |

**Instance properties.** Every later token of the form `key=value` SHALL be stored in the instance's property record as an unconverted string.

**Value tokens.** On `C`, `R`, `L`, `V` and `I` cards, a token that parses as a number or an expression (see Requirement "SI value parsing") SHALL be stored as property `value`.

**Models.**
- A non-`=` token after the value SHALL be the model name. A CDL token `$[name]` SHALL also set the model.
- A `C` card with no model SHALL use the built-in primitive capacitor `c`. An `R` card with no model SHALL use the built-in primitive resistor `r` (ports end_a end_b, `value` double tolerance 0.01).
- An existing model cell with the wrong port count SHALL produce `Device "<model>" has wrong number of ports for a <kind>.` and the card SHALL be skipped as `Badly formed line in input.`

**`.MODEL` cards.** `.MODEL name type` SHALL reclassify an existing model cell when the type (case-insensitive) is one of these:

| Type | Class |
|---|---|
| NMOS | nmos |
| PMOS | pmos |
| PNP | pnp |
| NPN | npn |
| D | diode |
| R | resistor |
| C | capacitor |
| L | inductor |
| V | voltage source |
| I | current source |

**Ignoring R and C.** When the ignore-RC option is set, `C` and `R` cards SHALL be skipped.
Compatibility: exact
(netgen ref: base/spice.c:860-1700, base/netgen.c:1907-1990)

#### Scenario: MOSFET model cell
- **WHEN** `M1 y a vss vss sky130_fd_pr__nfet_01v8 W=1 L=0.15` is the first use of that model
- **THEN** cell `sky130_fd_pr__nfet_01v8` SHALL be created with class fet and ports drain gate source bulk, and the instance SHALL be named `sky130_fd_pr__nfet_01v8:1`

#### Scenario: Plain resistor
- **WHEN** `R5 a b 1k` is read
- **THEN** an instance `r:5` of primitive `r` SHALL be created, with property `value` = `1k`

### Requirement: SPICE subcircuit calls and placeholders
An `X` card SHALL be read as `X<name> n1 … nk cell [key=value …]`. The parsing rules are:
- A token `/`, or a leading `/` on the cell token, SHALL be ignored (CDL syntax).
- `PARAMS:` SHALL be ignored.
- A token containing `=` SHALL start the parameter list, unless a cell with that exact name exists.
- A non-`=` token after a parameter SHALL print `Token "<t>" is not a parameter!`.
- The last positional token SHALL be the cell name. The ones before it SHALL be the connections, in port order.

**Instance name.** The instance name SHALL be `<cell>:<name>`, unless `<name>` already starts with `<cell>`, in which case it SHALL be `/<name>`.

**Rejected calls.**
- If the cell is the one being defined, the reader SHALL print `Fatal: Class "<c>" is instanced inside of itself!` and stop reading the file.
- If the cell is in the ignore list with type class, the call SHALL be dropped with `Class '<c>' instanced in input but is being ignored.`
- If the cell is in the ignore list with type shorted and all its connections are the same net (case-insensitive), the call SHALL be dropped with `Instance of '<c>' is shorted, ignoring.`

**Undefined cells.** A call to an undefined cell SHALL print `Call to undefined subcircuit <c>` and `Creating placeholder cell definition.` It SHALL then create a placeholder cell with:
- class module
- the placeholder flag
- ports named `1` … `k` (or one proxy port if k = 0)
- property of interest `M` (integer, tolerance 0, default 1)

**Connection count.** A connection count different from the port count SHALL print `Parameter list mismatch in <inst>: ` followed by `Too many parameters in call!` or `Not enough parameters in call!`.

**Pin names on calls.** At the end of the file, the pin names of instances whose class was a placeholder SHALL be rewritten to the class's final port names.
Compatibility: exact
(netgen ref: base/spice.c:1703-1940, base/spice.c:394-430, base/spice.c:1985)

#### Scenario: Placeholder for a PDK device
- **GIVEN** the layout netlist calls `XM1 d g s b sky130_fd_pr__nfet_01v8 w=1 l=0.15` and has no `.subckt` for it
- **WHEN** the file is read
- **THEN** `sky130_fd_pr__nfet_01v8` SHALL be a placeholder module with ports `1 2 3 4`, and the instance SHALL be named `sky130_fd_pr__nfet_01v8:M1`

#### Scenario: Instance name already prefixed
- **WHEN** `Xinv_1 a y inv` is read
- **THEN** the instance SHALL be named `/inv_1`

### Requirement: Globals, includes and parameters
**Globals.** `.GLOBAL n1 n2 …` (or `*.GLOBAL`) SHALL make each named net global:
- If nets of that name already exist in the current cell, they SHALL be converted to global scope.
- Otherwise a global SHALL be declared in the current cell. If there is no current cell, a file-level cell named after the file SHALL be opened first.

**Includes.** `.INCLUDE file` SHALL read another file into the same netlist, with the same file number and the same black-box mode. Quote characters `'`, `"` and `` ` `` around the name SHALL be stripped. A relative path SHALL be resolved against the directory of the including file. `~/` and `~user/` prefixes SHALL be expanded on desktop platforms; see Q-LVS-14 for iPadOS.

**Parameters.** `.PARAM k=v …` SHALL record file-level parameters that are used to evaluate expressions in property values.

**Expressions.** A property value that is an expression SHALL be reduced to a number when its operands resolve:
- through `.PARAM` values
- through the parent cell's property defaults
- through instance properties

An unresolved expression SHALL remain an expression-typed value. An expression-typed value never matches in property comparison (capability `lvs-property-matching`).
Compatibility: behavioral
(netgen ref: base/spice.c:775-850, base/spice.c:2087-2140, base/netgen.c:868-900)

#### Scenario: Relative include
- **GIVEN** `/work/lvs/top.spice` contains `.include "cells/inv.spice"`
- **WHEN** it is read
- **THEN** `/work/lvs/cells/inv.spice` SHALL be read into the same file number

### Requirement: SI value parsing
A numeric string SHALL be parsed in four steps.

1. **Mantissa.** Parse the longest prefix that forms a floating-point number (C `strtod` syntax). Skip any spaces after it.
2. **Scale suffix.** Apply at most one case-insensitive scale suffix:

   | Suffix | Multiplier |
   |---|---|
   | `g` | 1e9 |
   | `k` | 1e3 |
   | `meg` | 1e6 |
   | `m` (not followed by `eg`) | 1e-3 |
   | `c` | 1e-2 |
   | `u` | 1e-6 |
   | `n` | 1e-9 |
   | `p` | 1e-12 |
   | `f` | 1e-15 |
   | `a` | 1e-18 |

3. **Unit word.** The remaining text MAY be one unit, case-insensitive, given either as its first letter alone or as the full word, singular or plural: `f`/`farad`, `m`/`meter`, `h`/`henry`, `s`/`second`, `o`/`ohm`, `v`/`volt`, `a`/`amp`. Henry also accepts `henries`. Any other trailing text SHALL make the conversion fail. A failed conversion leaves the value a string.
4. **Precision.** Compute in at least 64-bit extended precision, then round to a double.

**Number test.** For the purpose of recognizing a value token, a string SHALL count as a number when the mantissa parses and the next non-space character is one of `g k c m u n p f a` (any case) or the end of the string.
Compatibility: exact
(netgen ref: base/netgen.c:2585-2745)

#### Scenario: Mega versus milli
- **WHEN** `1meg` and `1m` are converted
- **THEN** the results SHALL be 1e6 and 1e-3

#### Scenario: Unit word
- **WHEN** `10pF` and `2kohms` are converted
- **THEN** the results SHALL be 1e-11 and 2000

#### Scenario: Unknown unit
- **WHEN** `3x` is converted
- **THEN** conversion SHALL fail and the value SHALL stay the string `3x`

### Requirement: Library and black-box reads
`readlib spice <file>` SHALL read a SPICE file in black-box mode:
- each `.SUBCKT` header SHALL define a cell of class module with its ports and parameters
- its contents up to `.ENDS` SHALL be skipped
- element cards outside definitions SHALL be ignored

After any SPICE read, if no cell named after the file exists, an empty cell with that name SHALL be created and flagged top. This is how a library-only file is still registered as a file.

When automatic black-boxing is on (`model blackbox on`, or the `-blackbox` option of `lvs`), a subcircuit that is closed with no instances SHALL get class module.
Compatibility: exact
(netgen ref: base/spice.c:2078-2085, base/spice.c:2058-2068, base/spice.c:481-485, tcltk/tclnetgen.c:855-930)

#### Scenario: Empty subcircuit as black box
- **GIVEN** `model blackbox on`
- **WHEN** `.subckt ip a b` followed directly by `.ends` is read
- **THEN** cell `ip` SHALL have class module

### Requirement: Structural Verilog input
The Verilog reader SHALL accept the structural subset used for gate-level and analog-top netlists:
- `module`/`endmodule` with ANSI or non-ANSI port lists and `input`/`output`/`inout` declarations, including bus ranges `[msb:lsb]`
- `wire`/`logic`/`real` declarations
- `assign a = b;` net aliasing, including concatenations `{…}`
- module instances with positional or named (`.pin(net)`) connections, bus bit selects, part selects and concatenations
- `parameter`/`localparam` and instance parameter overrides `#(…)`, kept as instance properties
- `` `define ``, `` `undef ``, `` `include `` and macro substitution
- `generate`/`for` loops with integer bounds

`primitive`…`endprimitive`, `reg`, `always`, `initial`, `specify` blocks and other backtick directives SHALL be skipped.

Bus bits SHALL be named `name[i]`. Matching SHALL be case-sensitive. A module instanced before it is defined SHALL become a placeholder with the verilog flag. An escaped identifier `\name ` SHALL keep its backslash and trailing space, and SHALL match the unescaped name under the rule in Requirement "Name matching and case sensitivity".
Compatibility: behavioral
(netgen ref: base/verilog.c:1092-2300, base/verilog.c:3004-3110, base/netfile.c:740-760)

#### Scenario: Named connections
- **GIVEN** `inv u1 (.A(n1), .Y(n2));` and `module inv(input A, output Y);`
- **WHEN** the file is read
- **THEN** instance `u1` SHALL connect port A to `n1` and port Y to `n2`, whatever the order of the named connections

### Requirement: .sim input
The `.sim` reader SHALL create one cell named after the file, with case-sensitive matching.

**Units.** A line `| units: s …` SHALL set the scale s. Length and width SHALL be converted to metres as value × s × 1e-8.

**Records.**

| Record | Fields | Device created |
|---|---|---|
| `n g d s [l w [x y]]` | gate, drain, source, length, width, position | primitive `n` (ports drain gate source; `length`, `width` double tolerance 0.01) |
| `p …` | as `n` | primitive `p` (same ports and properties) |
| `e top bottom_a bottom_b …` | | `e` (class ecap) |
| `b base emitter collector …` | | `b` (class npn; ports collector base emitter) |
| `c a b [C]` | C in fF | `c`; `value` = C × 1e-15 F |
| `r a b [R]` | R in Ω | `r` |
| `z …` | | 3-terminal resistor |

When the position fields are present, the instance name SHALL be `<type>@<x>,<y>`. Lines starting with `|` are otherwise comments.
Compatibility: exact
(netgen ref: base/ext.c:924-1240, base/netgen.c:1787-1870)

#### Scenario: Sim scaling
- **GIVEN** `| units: 100 tech: scmos`
- **WHEN** the record `n a y gnd 2 4 10 20` is read
- **THEN** an `n` device SHALL be created with `length` = 2e-6 and `width` = 4e-6, named `n@10,20`

### Requirement: Legacy .ext input
netgen's own `.ext` reader recognizes `node`, `equiv`, `fet`, `cap`, `use` and `merge` records and skips header records. `fet` records with types `nfet`, `pfet`, `ecap`, `bnpn` and `zpolyResistor` are mapped to primitives. Modern `device` records are parsed but **produce no device**. `extflat` flattens each `use` as it is read. Hefesto SHALL NOT rely on this path for layout netlists. Layout netlists for LVS SHALL come from extraction through SPICE netlisting (capability `lvs-command`). Hefesto MAY implement this reader for script compatibility; see Q-LVS-1.
Compatibility: informative
(netgen ref: base/ext.c:165-448)

#### Scenario: Device records ignored
- **GIVEN** a Magic `.ext` file containing only `device mosfet nfet …` records
- **WHEN** netgen reads it with `readnet ext`
- **THEN** the resulting cell has no devices
