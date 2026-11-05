# Netlist Generation (ext2spice, ext2sim, extcheck)

## Purpose
The extflat library reads a tree of `.ext` files, flattens it or walks it hierarchically, merges nodes and global names, and writes netlists:
- SPICE through `ext2spice`: SPICE2, SPICE3, HSPICE, ngspice and CDL dialects
- `.sim` through `ext2sim`

Both are magic commands; in the Tcl build they can also be loaded as modules. `extcheck` is a standalone statistics checker.

## Requirements

### Requirement: ext2spice options and defaults
`ext2spice` SHALL accept these options (src: ext2spice/ext2spice.c:260-808, extflat/EFargs.c:186-268):
- `format spice2|spice3|hspice|ngspice|cdl`
- `cthresh <fF>|infinite`, `rthresh <ohms>|infinite`
- `merge none|conservative|aggressive`
- `extresist on|off`, `resistor tee`
- `scale`, `short none|resistor|voltage`
- `subcircuits [top|descend] on|off|auto`
- `hierarchy`, `blackbox`, `renumber`, `global`
- `lvs`, `default`
- command-line flags

An unknown first argument SHALL be treated as the name of the cell to run on.

Defaults SHALL be:

| Setting | Default |
|---|---|
| format | SPICE3 |
| cthresh | 2 fF |
| rthresh | infinite |
| scale | on |
| hierarchy | off |
| blackbox | off |
| subcircuit descend | on |
| subcircuit top | auto |
| merge | none |

#### Scenario: LVS preset
- **WHEN** `ext2spice lvs` is run
- **THEN** it SHALL set hierarchy on, format ngspice, both thresholds infinite, renumber off, scale off, blackbox on, global off, and subcircuit top auto

#### Scenario: Bad format
- **WHEN** `ext2spice format foo` is run
- **THEN** the system SHALL report "Bad format type.  Formats are:spice2, spice3, hspice, ngspice, and cdl."

### Requirement: SPICE output
The netlist SHALL be written to `<cell>.spice`, or `<cell>.cdl` for the CDL format. Its header SHALL be "* <cell> file created from <cell>.ext - technology: <tech>". Device prefixes SHALL be:

| Device | Prefix |
|---|---|
| MOS | M |
| BJT | Q |
| diode | D |
| resistor | R |
| capacitor | C |
| veriloga | N |
| subcircuit classes | X |

Trailing `!` on global names SHALL be removed, except in CDL. Hierarchical output SHALL write `.global` for the substrate names. Resistor body capacitance SHALL be split between the two terminals (pi model). That split SHALL apply only to 3-terminal `rsubcircuit` devices. (src: ext2spice/ext2spice.c:879-1113,2813-3914; ext2spice/ext2hier.c:936-951)

#### Scenario: Flat netlist
- **WHEN** `extract all` and then `ext2spice` are run on cell `top`
- **THEN** `top.spice` SHALL be written and "exttospice finished." printed

### Requirement: Shorted ports
When two ports of a cell are the same net, the behavior SHALL depend on the `short` mode:
- `short resistor` or `short voltage`: a 0-ohm resistor or a 0 V source SHALL be inserted.
- otherwise: the ports SHALL be merged, with the warning "Ports \"a\" and \"b\" are electrically shorted.". There SHALL be no warning when the two names differ only by case or by a `_uqN` suffix.

With `extresist on`, the ports SHALL be kept separate. (src: extflat/EFbuild.c:594-674)

#### Scenario: Short with voltage source
- **GIVEN** `ext2spice short voltage`
- **WHEN** ports A and B are shorted
- **THEN** a 0 V source between A and B SHALL be written

### Requirement: Preferred node names
When nodes are merged, the name SHALL be chosen by these preferences, in order (src: extflat/EFname.c:510-569):
1. a label over a generated `#` name
2. fewer hierarchical path components
3. a name other than `0`
4. the shorter name
5. the lexically later name

In Tcl builds, a name SHALL be global only when it is in the Tcl `globals` array. In non-Tcl builds, a name SHALL be global when it ends in `!`. (src: extflat/EFname.c:101-112)

#### Scenario: Label wins
- **WHEN** a node has the names `a/b/#123` and `out`
- **THEN** the netlist SHALL use `out`

### Requirement: ext2sim
`ext2sim` SHALL write `<cell>.sim`, plus `.al` (aliases) and `.nodes` (labels) when enabled. Its defaults SHALL be: format MIT, cthresh 2 fF, rthresh 10 Ω. Its options SHALL be `alias`, `labels`, `default`, `format MIT|LBL|SU`, `rthresh`, `cthresh`, `merge` and `extresist`. (src: ext2sim/ext2sim.c:255-709)

#### Scenario: SU format
- **WHEN** `ext2sim format SU` is run
- **THEN** the `.sim` header SHALL report `format: SU`

### Requirement: extcheck
The standalone `extcheck` program SHALL flatten a `.ext` tree and print counts of devices, nodes, capacitors and resistors. It SHALL NOT be built by default. (src: extcheck/extcheck.c:93-102, Makefile.in:128-132)

#### Scenario: Statistics
- **WHEN** `extcheck top` is run
- **THEN** it SHALL print lines such as "N devices" and "M nodes (G global, L local)"
