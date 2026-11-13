# netlist-spice-options

## Purpose

This capability defines the SPICE netlist generator (Magic's `ext2spice` / `exttospice` command): its options, defaults and run flags, the output file, and the structure of a flat netlist:
- header, `.option scale`, the top-level `.subckt` wrapper and port order
- node naming per dialect and the HSPICE name dictionary
- numeric formatting
- parasitic coupling, resistor and node-capacitance lines
- subcircuit instance lines

Device lines belong to capability `netlist-spice-devices`. Hierarchical output (`hierarchy on`) belongs to capability `netlist-spice-hierarchy`. Reading and flattening belong to the `ext-flattening-*` capabilities. The goal is byte-identical output for identical `.ext` input and options.

## Requirements

### Requirement: Command options
The command SHALL take an optional first word. It is matched case-insensitively, with unique-prefix matching, against `default format rthresh cthresh merge extresist resistor scale short subcircuits hierarchy blackbox renumber global lvs help`. If the first word is missing, all words SHALL be treated as run arguments. A first word that is `run`, ambiguous, or not an option prefix depends on the `magic-compat` setting (Q-NETLIST-13, resolution: compat): with it ON (the default) all words, that word included, SHALL be treated as run arguments, as in Magic, so `run` or the ambiguous word becomes the cell name; with it OFF `run` is a keyword and the remaining words are the run arguments, an ambiguous prefix prints `Ambiguous option: <w>` and does nothing, and a word that is not an option prefix is still taken, with the rest, as run arguments (the `ext2spice <cell>` form). The settings below SHALL persist across invocations within a session. On/off values SHALL accept `yes true on` as on and `no false off` as off, case-insensitively with prefix matching. Giving an option without a value SHALL report the current setting (informative text).

| Option | Effect | Default |
|---|---|---|
| `format spice2\|spice3\|hspice\|ngspice\|cdl` | Selects the dialect. Choosing `hspice` also turns `scale` on. | spice3 |
| `cthresh <fF>\|inf…` | Node and coupling capacitance threshold. A float; any word starting with `inf` means infinite. | 2 |
| `rthresh <Ω>\|inf…` | Integer threshold. It has no effect on SPICE output (Q-NETLIST-3). | infinite |
| `merge none\|conservative\|aggressive` | Parallel-device merging (see capability `netlist-spice-devices`). | none |
| `extresist on\|off` | Read `.res.ext` overlays. | off |
| `resistor tee [on\|off]` | Split resistors into two halves. | off |
| `scale on\|off` | On: emit `.option scale` and dimensionless geometry. Off: emit SI-scaled geometry. | on |
| `short none\|resistor\|voltage` | Shorted-port policy (see capability `ext-flattening-read`). | none |
| `subcircuits on\|off` | Same as `subcircuits descend on\|off`. | on |
| `subcircuits top on\|off\|auto` | Top-level `.subckt` wrapper. | auto |
| `subcircuits descend on\|off` | Emit cells with ports as subcircuit calls. Giving `auto` here depends on the `magic-compat` setting (Q-NETLIST-41, resolution: compat): with it ON (the default) it sets *top* to auto, as in Magic; with it OFF it prints the usage message and changes nothing. | on |
| `hierarchy on\|off` | Hierarchical output. | off |
| `blackbox on\|off` | Emit abstract views as stub subcircuits. | off |
| `renumber on\|off` | Number instances `X0, X1, …` instead of using their ids. | off |
| `global on\|off` | Merge unconnected nets that have the same global name. | on |

`default` SHALL reset `cthresh` to 2 and `rthresh` to infinite, clear the name-conversion flags, set convert-comma and convert-equal, reset the scale to unset, and clear the `-T` technology and the `-p` search path. It SHALL NOT reset format, merge, hierarchy, scale mode or other switches.

`lvs` SHALL set: hierarchy on, format ngspice, cthresh infinite, rthresh infinite, renumber off, scale off, blackbox on, global off, subcircuits top auto.

A bad format SHALL print `Bad format type.  Formats are:spice2, spice3, hspice, ngspice, and cdl.` and leave the format unchanged.
Compatibility: behavioral
(Magic ref: ext2spice/ext2spice.c:47-60, ext2spice/ext2spice.c:260-820, ext2spice/ext2spice.c:629-646)

#### Scenario: LVS preset
- **GIVEN** a fresh session
- **WHEN** `ext2spice lvs` is issued
- **THEN** a later run writes hierarchical ngspice output with no `.option scale`, no parasitic capacitors, and instance names taken from use ids

#### Scenario: HSPICE forces scale
- **GIVEN** `ext2spice scale off`
- **WHEN** `ext2spice format hspice` is issued
- **THEN** scale mode is on again

#### Scenario: Run keyword with magic-compat ON
- **GIVEN** a cell `foo` and `magic-compat` ON
- **WHEN** `ext2spice run foo` is issued
- **THEN** the run arguments are `run foo`, so `run` is taken as the cell name, as in Magic

#### Scenario: Run keyword with magic-compat OFF
- **GIVEN** a cell `foo` and `magic-compat` OFF
- **WHEN** `ext2spice run foo` is issued
- **THEN** `foo` is netlisted to `foo.spice`

#### Scenario: Descend auto
- **GIVEN** `subcircuits top on`
- **WHEN** `ext2spice subcircuits descend auto` is issued
- **THEN** with `magic-compat` ON *top* becomes auto
- **AND** with `magic-compat` OFF the usage message is printed and *top* stays on

### Requirement: Run arguments and output file
A run SHALL accept the common arguments of capability `ext-flattening-hierarchy` plus these flags:

| Flag | Effect |
|---|---|
| `-d` | Distributed junction areas |
| `-M` | Aggressive merge |
| `-m` | Conservative merge |
| `-B` | No attribute comments |
| `-F` | Output only nodes connected to devices; applies to this run only |
| `-o <file>` | Output file |
| `-f <format>` | Format, case-insensitive. `hspice` turns scale on. |
| `-J hier\|flat` | Hierarchical source/drain area/perimeter default |
| `-y <n>` | Deprecated. Prints `The cap accuracy flag 'y' is deprecated.` |
| `-h` | Usage |

An unknown flag SHALL fail the run with `Unrecognized flag: <f>`. Without a cell argument, the run SHALL use the cell in the current layout window. The output file SHALL be `<cell>.cdl` in CDL format and `<cell>.spice` otherwise, in the current directory, unless `-o` is given for that run (see Q-NETLIST-42). The thresholds set by options SHALL be copied into the run before arguments are parsed. All device, capacitor, resistor, diode, voltage-source, subcircuit and node counters SHALL be reset at the start of each run:
- devices start at 1000
- SPICE2 node numbers start at 10
- all other counters start at 0

On completion, the informative message `exttospice finished.` SHALL be printed.
Compatibility: behavioral
(Magic ref: ext2spice/ext2spice.c:824-880, ext2spice/ext2spice.c:1416-1508)

#### Scenario: CDL file name
- **GIVEN** format cdl and cell `amp`
- **WHEN** the netlister runs without `-o`
- **THEN** it writes `amp.cdl`

### Requirement: File header and scale card
The first output line SHALL be `* <FMT> file created from <cell>.ext - technology: <tech>` followed by one empty line. `<FMT>` is one of `SPICE2 SPICE3 HSPICE NGSPICE CDL`. If scale mode is on, the next lines SHALL be `.option scale=<v>` followed by an empty line. Here `<v>` is the SI form (see Requirement "SI value formatting") of `1e-6 × S_out / 100`, where `S_out` is the global output scale of capability `ext-flattening-read`. If scale mode is off, the output length unit SHALL be `S_out / 100` µm per internal unit. If the active extraction style does not match the file, the run SHALL warn `Warning:  Current extraction style does not match .ext file!` and continue.
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:900-915, ext2spice/ext2spice.c:1063-1075)

#### Scenario: Default header
- **GIVEN** cell `inv`, technology `scmos`, output scale `S_out` 5, and default options
- **WHEN** the netlist is written
- **THEN** it begins `* SPICE3 file created from inv.ext - technology: scmos`, an empty line, `.option scale=50n`, an empty line

### Requirement: SI value formatting
Numeric values written in SI form SHALL be formatted from a single-precision value v as follows:
1. Let a = |v|.
2. Choose the suffix:
   - if a < 1e-18: no suffix, value unchanged
   - else if a < 0.9999e-13: `f`, ×1e15
   - else if a < 1.0001e-10: `p`, ×1e12
   - else if a < 1.0001e-7: `n`, ×1e9
   - else if a < 1.0001e-4: `u`, ×1e6
   - else if a < 1.0001e-2: `m`, ×1e3
   - else if a > 0.9999e9: `G`, ÷1e9
   - else if a > 0.9999e3: `k`, ÷1e3
   - otherwise no suffix
3. The scaled value is single-precision.
4. Print it with exactly 5 decimals (`%.5f`).
5. Remove trailing zeros, and then a trailing `.`.
6. Append the suffix.

The `a` (atto) suffix SHALL never be used.
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:3485-3656)

#### Scenario: Micron value
- **GIVEN** v = 1.8e-7
- **WHEN** it is formatted
- **THEN** the text is `0.18u`

#### Scenario: Zero and kilo
- **GIVEN** v = 0, and separately v = 1500
- **WHEN** they are formatted
- **THEN** the texts are `0` and `1.5k`

#### Scenario: Boundary value
- **GIVEN** v = 0.01
- **WHEN** it is formatted
- **THEN** the text is `10m`

### Requirement: Node names per dialect
Each flat node SHALL get its SPICE name once, the first time it is referenced. Later references reuse it.
- **SPICE2**: the name is the next node number, starting at 10.
- **Other dialects**: the name is the canonical name converted as in capability `ext-flattening-names`. The flags set for every run are trim-global (except CDL), convert-equal and convert-comma, plus trim-local in HSPICE.
- **HSPICE**: the converted text is split at its last `/` into prefix and leaf.
  - A prefix seen for the first time is assigned the next subcircuit number s (shared with the `X` counter) and recorded.
  - The name becomes `x<s>/<leaf>`. A name without `/` is kept.
  - If the result is longer than 15 characters, it becomes `z@<n>`, where n is the next node number starting at 10.
  - At the end of a flat HSPICE netlist, the lines `** hspice subcircuit dictionary` (preceded by an empty line) and then one `* x<s>\t<prefix>` per recorded prefix SHALL be written, in assignment order.

Unresolvable names SHALL be written as `errGnd!`.
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:917-930, ext2spice/ext2spice.c:4249-4306, ext2spice/ext2spice.c:4398-4512)

#### Scenario: HSPICE prefix mapping
- **GIVEN** HSPICE format and the nodes `top/u1/a` then `top/u1/b`
- **WHEN** they are named
- **THEN** both are named with the same prefix, `x0/a` and `x0/b`, and the dictionary contains `* x0	top/u1`

#### Scenario: SPICE2 numbering
- **GIVEN** SPICE2 format and the first three nodes referenced, `in`, `out`, `vdd`
- **WHEN** the netlist is written
- **THEN** they are named `10`, `11`, `12`

### Requirement: Flat netlist structure
With `hierarchy off`, the body SHALL be written in this order:
1. The `.subckt` header line (see Requirement "Subcircuit header line"), when the wrapper is written. That depends on the `magic-compat` setting (Q-NETLIST-37, resolution: compat): with it ON (the default) only `subcircuits top on` writes a wrapper, because flat mode reads without subcircuit marking, as in Magic 8.3.694; with it OFF `top on` writes one, and so does `top auto` when the root `.ext` has `port` records.
2. All device lines, in device visit order (capability `ext-flattening-hierarchy`).
3. If cthresh is finite: coupling capacitor lines.
4. Resistor lines.
5. Subcircuit instance lines.
6. Node lines.
7. `.ends` if a wrapper was written.
8. The HSPICE dictionary, in HSPICE format.

`subcircuits descend` SHALL have no effect in flat mode, in both `magic-compat` settings: child cells are always fully flattened (Q-NETLIST-37).
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:1086-1180, ext2spice/ext2spice.c:888, ext2spice/ext2spice.c:1123-1145, extflat/EFread.c:469-472)

#### Scenario: Flat inverter order
- **GIVEN** an inverter with two MOSFETs and node `out` at 5 fF
- **WHEN** a default flat netlist is written
- **THEN** the two `M` lines come before the `C… out 0 5f` line, and no `.subckt` line is written

#### Scenario: Flat top auto with ports and magic-compat ON
- **GIVEN** a root cell `inv` whose `.ext` file has `port` records for `in` (index 0) and `out` (index 1), and `subcircuits top auto`
- **WHEN** a flat netlist is written with `magic-compat` ON
- **THEN** no `.subckt` or `.ends` line is written

#### Scenario: Flat top auto with ports and magic-compat OFF
- **GIVEN** the same cell and options
- **WHEN** a flat netlist is written with `magic-compat` OFF
- **THEN** the body is wrapped in `.subckt inv in out` … `.ends`, and child cells are still flattened

### Requirement: Subcircuit header line
A `.subckt` line SHALL be written as `.subckt <name>` followed by the ports in ascending port index, each preceded by one space. `<name>` is the cell name, prefixed with `x` if its first character is not a letter; the cell's name is permanently renamed this way.
- **Port index.** A port node without an explicit index on any of its names SHALL receive one past the current maximum, in node-table iteration order. That order is the Magic string hash table order of capability `ext-flattening-hierarchy` (requirement "Container iteration order"), which Hefesto SHALL reproduce exactly because it decides the port numbering (decision X-1, capability `hefesto-output-determinism`). Abstract cells in black-box mode are the exception. Indices need not be contiguous; gaps are skipped.
- **Port name.** The name is the node's SPICE name. For abstract cells it is the converted port name itself.
- **Duplicate names.** A port name already written SHALL NOT be written again; the duplicate node's index is set to the first one's.
- **Optimized-out ports.** A port whose name no longer resolves to a flat node SHALL be dropped. The note `Note:  Port <p> was optimized out of <cell>` is informative.
- **Line continuation.** Before each port, if the running line length exceeds 80, a newline and `+` are written. The count starts at 8 + the name length, and a `+` line starts at 1.
- **Cell parameters.** Cell parameters declared with `parameters :<cell> …` SHALL follow, verbatim, with the same continuation rule.
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:1857-2110)

#### Scenario: Numeric cell name
- **GIVEN** a cell named `7seg` with ports `a` (index 0) and `b` (index 1)
- **WHEN** its header is written
- **THEN** the line is `.subckt x7seg a b`

### Requirement: Subcircuit instance lines
A subcircuit call SHALL be written as follows.
- **Instance name.** `X<n>` (next subcircuit number) if renumbering is on, the format is SPICE2, or the use has no id. Otherwise `X` followed by the full instance path, converted with only convert-comma active.
- **Nodes.** One space plus the SPICE name of each port in ascending port index.
  - Without a declared port order, ports follow the subcell's node-table iteration order, reproduced exactly as for the port index.
  - With one, each index takes the subcell's canonical port name, or the most preferred alias that carries that index.
- **Model name.** In CDL, ` /` is written before the cell name. Then a space and the cell name follow, with the `x`-prefix rule.
- **Parameters.** Cell instance parameters SHALL be appended.

The same 80-column continuation rule SHALL apply.
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:1600-1790)

#### Scenario: Instance by id
- **GIVEN** a use `u1` of cell `inv` with ports `A`, `Y` connected to `n1`, `n2`, renumber off, and SPICE3 format
- **WHEN** the call is written
- **THEN** the line is `Xu1 n1 n2 inv`

#### Scenario: CDL instance
- **GIVEN** the same use in CDL format
- **WHEN** the call is written
- **THEN** the line is `Xu1 n1 n2 / inv`

### Requirement: Parasitic and node lines
The parasitic and node lines SHALL be written as follows.

**Coupling lines.** A coupling capacitor of C aF SHALL be written as `C<k> <n1> <n2> <SI(C×1e-18)>` only if C/1000 > cthresh. `k` is the shared capacitor counter.

**Resistor lines.** A resistor of R mΩ SHALL be written as `R<k> <n1> <n2> <%g of R/1000>`. Both nodes count as connected.

**Node lines.** For each flat node in node visit order:
1. In SPICE2, or in HSPICE when the node's name starts with `z@`: write `** <full converted name> == <spice name>`.
2. If its ground capacitance C satisfies C/1000 > cthresh: write `C<k> <name> <gnd> <SI(C×1e-18)>`. `<gnd>` is the SPICE name of the global substrate node, or `0` if there is none.
   - If the node is not connected to any device terminal or resistor and is not a port, append ` **FLOATING`, written as ` ; **FLOATING` in ngspice.
   - With `-F`, unconnected nodes SHALL be skipped entirely.
3. If the node has attributes and `-B` is not set: write `**nodeattr <name> : <a1>,<a2>,…`, prefixed with ` ; ` in ngspice.

Capacitor numbering SHALL continue from the device capacitors.
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:3995-4207)

#### Scenario: Node capacitance
- **GIVEN** node `out` with 5000 aF, a device on it, no substrate record, and cthresh 2
- **WHEN** node lines are written
- **THEN** the line is `C0 out 0 5f` if no capacitor devices were written before it

#### Scenario: Floating node in ngspice
- **GIVEN** a node `n7` with 3000 aF that touches no device, and ngspice format
- **WHEN** node lines are written
- **THEN** the line is `C0 n7 0 3f ; **FLOATING`

#### Scenario: Below threshold
- **GIVEN** a node with 2000 aF and cthresh 2
- **WHEN** node lines are written
- **THEN** no capacitor line is written for it, because 2 is not greater than 2

### Requirement: Global card in hierarchical mode
In hierarchical mode, before the body, `.global` SHALL be written followed by each distinct default device-substrate name of the extraction style, each name followed by one space, and then an empty line. Names are taken in reverse order of the device list, and any trailing `!` is removed except in CDL. Names starting with `err`, containing `error`/`Error`/`ERROR`, or equal to `None` ignoring case SHALL be excluded. No `.global` line SHALL be written if the list is empty. With CDL format, the `sub`, `vdd` and `gnd` fields of the project setting "global nets and power names" (capability `hefesto-settings`; Magic's `$SUB`, `$VDD` and `$GND`) SHALL also be added, each only if it is set and ends in `!`, as in Magic's Tcl build; this trailing-`!` test is an explicit rule of this requirement. They are checked in the order `sub`, `vdd`, `gnd`, and each one added is placed before all names already in the list, so the written order is `gnd`, `vdd`, `sub`, then the device-substrate names. They are not checked for duplicates or for the excluded names. A default device-substrate name of the form `$<name>` resolves `$SUB`, `$VDD` and `$GND` from the same setting (see Q-NETLIST-33, decided by D-14).
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:965-1045, ext2spice/ext2spice.c:1086-1120)

#### Scenario: Two substrate names
- **GIVEN** an extraction style whose device list has substrate names `VSUBS!` (first) and `VDD!` (second), and hierarchical SPICE3 output
- **WHEN** the global card is written
- **THEN** the line is `.global VDD VSUBS ` followed by an empty line

#### Scenario: CDL adds bang-terminated power names (verify against Magic)
- **GIVEN** an extraction style whose only default substrate name is `VSUBS`, CDL hierarchical output, and the setting with `sub` = `VSUBS`, `vdd` = `vdd!` and `gnd` = `gnd!`
- **WHEN** the global card is written
- **THEN** the line is `.global gnd! vdd! VSUBS ` followed by an empty line; `sub` adds nothing because `VSUBS` does not end in `!`
