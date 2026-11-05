# netlist-spice-hierarchy

## Purpose

This capability defines SPICE output with `hierarchy on`: one subcircuit per cell definition, the pre-pass that promotes nets to ports, which cells are omitted, stubbed or absorbed, the order of definitions and sections, subcircuit calls, and how hierarchical device and parasitic output differs from flat output. Line formats that are the same as in flat mode are owned by capabilities `netlist-spice-options` and `netlist-spice-devices`. One-level flattening is owned by capability `ext-flattening-hierarchy`.

## Requirements

### Requirement: Definition traversal order
Hierarchical output SHALL make two post-order passes over the definition tree, each visiting every definition exactly once. Children are visited before the parent, and a definition's uses are taken in use-table order (see capability `ext-flattening-hierarchy`):
1. The port-promotion pass (see Requirement "Port promotion").
2. The output pass, which writes one section per definition. The root definition is written last.

When the files are read for hierarchical output, `port` records SHALL mark cells as subcircuits.
Compatibility: exact
(Magic ref: ext2spice/ext2hier.c:71-105, extflat/EFhier.c:237-290, ext2spice/ext2spice.c:880-886)

#### Scenario: Leaf cells first
- **GIVEN** `top` uses `mid`, and `mid` uses `inv`
- **WHEN** the hierarchical netlist is written
- **THEN** the sections appear in the order `inv`, `mid`, `top`

### Requirement: Port promotion
For each definition that has uses, every `merge` connection in which one name is hierarchical (contains `/`) SHALL be traced down through the use path. Array subscripts are ignored when the full bracketed name is not itself a use. In each child definition reached, the named net SHALL be created if missing and marked as a port with no index, so that an index is assigned later. A net that is substrate or global substrate, is not a substrate port, and is reached while cthresh is infinite SHALL NOT be promoted.

The substrate-port flag SHALL propagate from a child net to the parent net it connects to. Coupling capacitor records whose |C|/1000 ≥ cthresh SHALL promote child nets the same way. The bracketed array form `[a:b:c][d:e:f]` before the `/` is recognized there.
Compatibility: exact
(Magic ref: ext2spice/ext2hier.c:1995-2298)

#### Scenario: Internal net becomes port
- **GIVEN** `top` contains `merge u1/n5 a` and cell `inv` has no port on `n5`
- **WHEN** port promotion runs
- **THEN** `n5` is a port of `inv` and appears in `.subckt inv`

#### Scenario: Coupling below threshold
- **GIVEN** cthresh infinite and `cap u1/n5 b 4000`
- **WHEN** port promotion runs
- **THEN** `n5` is not promoted by this record

### Requirement: Omitted, stubbed and absorbed definitions
For each definition in the output pass, the following rules SHALL apply in order:
1. A definition flagged primitive SHALL produce no output.
2. Consider a non-root definition with no devices and no uses. If subcircuits top is `auto`, it is kept only if it has a port or substrate port. If it is not kept (or top is `off`), all its port flags SHALL be cleared and it SHALL produce no output.
3. After one-level flattening, a non-root definition flagged device-less SHALL produce no output, unless it is abstract and blackbox is on.
4. An abstract definition with blackbox on SHALL be written as a stub. The line `* Black-box entry subcircuit for <cell> abstract view` is followed by its `.subckt` line, with ports named by their own port names, and `.ends`. No contents are written.
Compatibility: exact
(Magic ref: ext2spice/ext2hier.c:2300-2380)

#### Scenario: Black-box stub
- **GIVEN** blackbox on and an abstract cell `sram` with ports `A` (0) and `D` (1)
- **WHEN** its section is written
- **THEN** it reads `* Black-box entry subcircuit for sram abstract view`, `.subckt sram A D`, `.ends`, and an empty line

### Requirement: Section layout
Each written definition SHALL be laid out as follows:
- **Opening.** A non-root definition, a root flagged as subcircuit, or a root under subcircuits top `on`, SHALL open with the `.subckt` header of capability `netlist-spice-options`. So SHALL a root under `auto` that has any port or substrate port. Any other root SHALL open with an empty line, `* Top level circuit <cell>`, and an empty line.
- **Contents**, unless the definition is a stub:
  1. subcircuit calls
  2. device lines, after the merge pre-pass if merging is on
  3. resistor lines
  4. coupling capacitor lines
  5. node lines, written only if a `substrate` record was read (not in compatibility mode)
- **Closing.** The section SHALL close with `.ends` and an empty line, or with `.end` and an empty line for a root that was not wrapped.
- **Counters.** After each section, the counters for capacitors, devices (back to 1000), resistors, diodes, subcircuits and SPICE2 nodes (back to 10) and the HSPICE prefix table SHALL be reset. The voltage-source counter is not reset (Q-NETLIST-35).
Compatibility: exact
(Magic ref: ext2spice/ext2hier.c:2380-2473)

#### Scenario: Unwrapped top level
- **GIVEN** a root `chip` with no ports and subcircuits top `auto`
- **WHEN** its section is written
- **THEN** it starts with an empty line, then `* Top level circuit chip`, then an empty line, and ends with `.end`

#### Scenario: Per-cell numbering
- **GIVEN** two cells that each contain one MOSFET
- **WHEN** both sections are written
- **THEN** both MOSFETs are named `M1000`

### Requirement: Subcircuit calls in hierarchical mode
Each use of the definition SHALL produce a call, one per element for arrays, in use order with x outer and y inner, in these cases:
- The used cell has at least one port with an index, or a substrate port.
- The used cell is not device-less.
- The used cell is a stub.

A device-less used cell that is not a stub SHALL produce no call. The line format and port order SHALL be those of capability `netlist-spice-options`. The instance name is the converted instance path, which for one level is the use id with an array suffix such as `u1[2]`. The path is converted with only convert-comma active. Under `renumber on` or SPICE2, the name is `X<n>` instead.
Compatibility: exact
(Magic ref: ext2spice/ext2hier.c:591-660, extflat/EFhier.c:324-354)

#### Scenario: Array instance call
- **GIVEN** a use `b[0:1]` of cell `bit` with port `q` and renumber off
- **WHEN** calls are written
- **THEN** the lines `Xb[0] … bit` and `Xb[1] … bit` are written

### Requirement: Hierarchical device and parasitic differences
Hierarchical output SHALL follow the flat formats of capability `netlist-spice-devices`, with these exact differences:
- **Skipped devices.** Devices with at most one terminal are skipped.
- **Names.** A device named by its gate attribute does not get the instance path.
- **Tee resistors.** The second `B` line of an `rsubckt` tee is written.
- **Modeled resistor geometry.** In scale mode it is written as integers (`w=%d l=%d` of the truncated values).
- **Unnamed `c` parameter.** It writes the capacitance.
- **Perimeter parameter.** With scale off and no `k` scale, it uses `1e-6`.
- **Coupling and node capacitors.** These are compared using |C|. Node connectivity is "any terminal or resistor touched".
- **Killed nodes.** Devices on killed nodes are emitted with an error message.
- **Merging.** It compares the raw substrate nodes and does not check area/perimeter attributes.
- **Area/perimeter marks.** The "already written" marks for terminal area/perimeter are kept per node across the definition.

See Q-NETLIST-19.
Compatibility: exact
(Magic ref: ext2spice/ext2hier.c:681-1360, ext2spice/ext2hier.c:1486-1717)

#### Scenario: Hierarchical tee rsubckt
- **GIVEN** resistor tee on and an `rsubckt` device `rhigh` with ends `a`, `b` and body `m`
- **WHEN** it is written in hierarchical mode
- **THEN** the lines `X0A a m rhigh …` and `X0B m b rhigh …` are written
