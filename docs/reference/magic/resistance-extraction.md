# Resistance Extraction (extresist)

## Purpose
`extresist` performs full R-C extraction. It reads a cell's `.ext` file and selects nets whose estimated R·C delay exceeds a threshold. For each selected net it breaks the net's geometry into a resistor network between device terminals and drive points, simplifies the network, and writes `<cell>.res.ext`. `ext2spice` and `ext2sim` can merge that file in place of the lumped nodes.

## Requirements

### Requirement: extresist command options
`extresist` SHALL accept these options:
- `all`, `cell`, `box <type>`
- `threshold [mΩ]`, default 10000
- `minresist [mΩ]`, default 1000
- `mindelay [ps]`, default 1 ps; 0 means no cutoff
- `simplify`, `extout`, `lumped`, `silent`, `debug`, `stats`, `blackbox`, each `on|off`
- `skip <type>`, `force <name>|none`, `ignore <name>|none`, `include <name>|all`
- `fasthenry [freq]`, `geometry`, `help`

`tolerance` SHALL be accepted, but it is deprecated and has no effect. Settings SHALL persist between runs. (src: resis/ResRex.c:201-785)

#### Scenario: Deprecated option
- **WHEN** `extresist tolerance 5` is run
- **THEN** the system SHALL print "Note:  This option has been deprecated and is unused."

### Requirement: Net selection
For each node of the cell, extresist SHALL decide as follows (src: resis/ResRex.c:1066-1304, resis/ResReadExt.c:536-545):
- A node with no `node` record, such as a port whose name differs from its node, SHALL be skipped.
- The node's capacitance SHALL be taken from its `node` record.
- The driver SHALL be the largest W/L transistor. If there is none, it SHALL be a drive point; if there is none of those either, a sink point.
- The node SHALL be extracted when it is forced, or when R·C passes the cutoff. A zero threshold or zero mindelay disables the cutoff. C = 0 skips the delay test.

#### Scenario: No driver
- **WHEN** a forced node has no drive point or driving device
- **THEN** the system SHALL report "Node %s has force label but no drive point or driving device"

### Requirement: Network simplification preserves ports
Series merging, network scrunching and tree pruning SHALL NOT remove:
- port nodes
- the origin node
- sink nodes
- a resistor directly between two ports

(src: resis/ResSimple.c:233-236,530-546,914-917)

#### Scenario: Port-to-port wire
- **GIVEN** two ports connected only by a wire with no devices
- **WHEN** extresist runs
- **THEN** the output SHALL contain a resistor between the two port nodes, and each port SHALL appear exactly once

### Requirement: .res.ext output
The output file `<cell>.res.ext` SHALL be written in the extract path if one is set. It SHALL contain:
- a `scale` line
- for each extracted net:
  - `killnode` for the lumped node
  - `rnode "<net>.nK" 0 C x y 0` for each subnode
  - `resist "a" "b" R` for each resistor
  - device records whose terminals are renamed to subnodes

When the original node is kept, its capacitance SHALL be subtracted from the matching subnode, so that capacitance is not counted twice. At the end, extresist SHALL print totals: "Total Nets / Nets extracted / Nets output". (src: resis/ResPrint.c:87-326, resis/ResRex.c:1333-1384)

#### Scenario: Capacitance conservation
- **WHEN** a net is split into subnodes
- **THEN** the sum of the subnode capacitances plus the retained node capacitance SHALL equal the original node capacitance

### Requirement: Integration with extraction and netlisting
The integration SHALL work as follows (src: extract/ExtMain.c:825-838,1070-1105; extflat/EFread.c:66-78,398-406,628-715):
- `extract do extresist` SHALL run resistance extraction after normal extraction, for each cell.
- `ext2spice extresist on` and `ext2sim extresist on` SHALL read `.res.ext` after `.ext`.
- `equiv` records SHALL be deferred while reading, and SHALL be applied only when neither name is described in the `.res.ext`.

#### Scenario: Full RC netlist
- **WHEN** a user runs `extract do extresist`, `extract all`, `ext2spice extresist on` and `ext2spice`
- **THEN** the SPICE netlist SHALL contain the distributed resistors and subnode capacitances in place of the lumped nodes
