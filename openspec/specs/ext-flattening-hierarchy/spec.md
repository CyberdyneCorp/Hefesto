# ext-flattening-hierarchy

## Purpose

This capability defines how the hierarchical circuit model read by capability `ext-flattening-read` is flattened, fully or one level at a time, into flat nodes, devices, couplings and resistors. It also defines the exact order in which netlisters visit these items. Byte-identical netlists depend on this order. The capability covers:
- the common command-line arguments shared by all netlisters
- node merging and name selection on merge
- connections across arrays
- kills and global-name merging
- the lumped resistance estimate
- device length/width derivation
- the container iteration order that Magic's output order inherits

Hierarchical name formats belong to capability `ext-flattening-names`.

## Requirements

### Requirement: Container iteration order
Several output orders are defined by iterating unordered keyed tables. Hefesto SHALL reproduce those orders exactly by emulating each table as a Magic string hash table, as defined in capability `hefesto-output-determinism` (requirement "Magic string hash table"), which owns the hash, bucket, growth and iteration rules. Under decision X-1 these orders depend only on names and insertion history and SHALL be byte-identical to Magic's. The affected tables are:
- device table per definition (string keys `"<x>x<y>_<t>"`), requested size 32
- use table per definition (string keys = use ids), requested size 32
- node-name table per definition (string keys), requested size 32
- global-name table during flattening, requested size 1024

Tables keyed by hierarchical names use the client hash of capability `ext-flattening-names` (requirement "Component hash"), sign-extended as the owning requirement states. A table's iteration order is therefore fully determined by its key insertion history. See Q-NETLIST-2.
Compatibility: exact
(Magic ref: extflat/EFdef.c:36-40, extflat/EFdef.c:256-272, extflat/EFflat.c:41)

#### Scenario: Use iteration order
- **GIVEN** a definition with uses inserted in the order `u3`, `u1`, `u2` into a 32-bucket table
- **WHEN** the uses are iterated
- **THEN** the hashes are 7675132, 7675133 and 7675134, the buckets are 3, 4 and 5, and the order is `u1`, `u2`, `u3`

#### Scenario: Device iteration order
- **GIVEN** devices keyed `10x20_7` (bucket 11) and `30x20_7` (bucket 9) in a 32-bucket table
- **WHEN** devices are visited
- **THEN** `30x20_7` is visited before `10x20_7`

#### Scenario: Same bucket
- **GIVEN** keys `a` then `b`, both of which hash to bucket 22 of 32
- **WHEN** the table is iterated
- **THEN** `b` is visited before `a`

### Requirement: Common netlister arguments
All netlisters SHALL accept these arguments, which may be mixed with tool-specific flags:

| Argument | Effect |
|---|---|
| `-c <fF>` | Capacitance threshold, a float |
| `-C` | Infinite capacitance threshold |
| `-r <Ω>` | Resistance threshold, an integer |
| `-R` | Infinite resistance threshold |
| `-p <path>` | `.ext` search path |
| `-s sym=value` | Defines a symbol for `ext:l=`/`ext:w=` attributes |
| `-S <file>` | Symbol file |
| `-t <chars>` | Output-name conversions (see below) |
| `-T <tech>` | Technology (informative: accepted only by Magic's standalone netlister programs) |
| `-v` | Warnings |
| `-n <name>`, `-N <file>` | Watch nodes; debug |
| `-z` | Name statistics |
| `-h` | Help |

For `-t`, each character in `<chars>` turns on one conversion:
- `!` trims a trailing `!`.
- `#` trims a trailing `#`.
- `,` converts `,` to `|`.
- `=` converts `=` to `:`.
- `[` or `]` converts brackets to `_`.

A non-flag argument SHALL be taken as the root cell name. A later one replaces an earlier one, with a warning. The default thresholds are 2 fF and 10 Ω. Infinite means a capacitance of 1.0E38 and a resistance of 2^31 − 1. An unrecognized flag that the tool does not accept SHALL print the usage text and fail the command.
Compatibility: behavioral
(Magic ref: extflat/EFargs.c:46-58, extflat/EFargs.c:100-345, extflat/EFtypes.h:308-310)

#### Scenario: Threshold override
- **GIVEN** the command `ext2spice -c 0.5 top`
- **WHEN** the arguments are parsed
- **THEN** the capacitance threshold for this run is 0.5 fF and the root cell is `top`

### Requirement: Flat node construction
Full flattening SHALL visit the hierarchy depth-first. Children are visited before the parent's own items, and uses are visited in use-table order. For an array use, x is the outer loop and y the inner loop, each running from the lower index to the higher. Element (x, y) SHALL be translated by `(xsep·(x−xlo), ysep·(y−ylo))`; a reversed range negates the separation.

For each definition instance, each local node SHALL become a flat node. The new node is pushed onto the head of the global flat node list, so global node order is the reverse of creation order. The flat node SHALL copy the local node's capacitance, area/perimeter, flags and attributes. Each of its names SHALL be the instance path joined with the local name. A node flagged as device-terminal-only (an implicit substrate) SHALL instead use the bare local name, so that instances of it share one flat node. The first name keeps its position, and later names are inserted after it.

Then each `merge` connection of the definition SHALL be applied. The delta capacitance and area/perimeter are added to the first node, and if a second name is given, the two nodes are merged. Array ranges in connection names SHALL be expanded with matched offsets (see capability `ext-flattening-names` for name formatting). Names that do not resolve SHALL be ignored, with a warning in flat mode.
Compatibility: exact
(Magic ref: extflat/EFflat.c:104-150, extflat/EFflat.c:320-345, extflat/EFflat.c:462-728, extflat/EFhier.c:72-232)

#### Scenario: Array connection expansion
- **GIVEN** a connection `merge "u[0:1]/a" "u[1:2]/b" 0`
- **WHEN** it is flattened
- **THEN** `u[0]/a` is merged with `u[1]/b` and `u[1]/a` is merged with `u[2]/b`

### Requirement: Node merge semantics
Merging two distinct nodes SHALL work as follows:
- **Kept node.** The node with the larger member count is kept. On a tie, the first operand is kept.
- **Accumulation.** Capacitance, area and perimeter are summed, and member counts are added.
- **Flags.** The port, top-port, substrate-port, device-substrate and global-substrate flags are OR-ed. The device-terminal-only flag survives only if both nodes had it.
- **Attributes.** The attribute lists are concatenated, with the removed node's list first.
- **Canonical name.** Rules are applied in order:
  1. If exactly one node has a name, use it.
  2. If exactly one is a top-level port, use the port's name.
  3. Otherwise apply the preference order of capability `ext-flattening-names` to the two canonical names.
- **Name list.** If the removed node's name wins, the result is the removed node's names followed by the kept node's names. Its location and layer are also taken if its layer index is > 0. Otherwise the result is the kept node's canonical name, then the removed node's names, then the kept node's remaining names.
- **Unlinking.** The removed node is unlinked from the node list.

When two local nodes of one definition are merged by `equiv`, device terminals that referenced the removed node SHALL be redirected to the kept node.
Compatibility: exact
(Magic ref: extflat/EFbuild.c:2185-2410, extflat/EFbuild.c:2119-2183, extflat/EFbuild.c:663-705)

#### Scenario: Port name wins
- **GIVEN** a top-level port node named `OUT` and a non-port node named `a`, and `a`'s node has more members
- **WHEN** they merge
- **THEN** the kept node's canonical name is `OUT`

### Requirement: Kills
After nodes are flattened, every `killnode` of every instance SHALL mark the flat node named instance-path + kill name as killed. Killed nodes SHALL be skipped by node visits. Couplings and resistors that touch a killed node SHALL be dropped. In full-flat mode, devices that have any terminal on a killed node SHALL be skipped. In one-level hierarchical mode such devices SHALL be reported with `Error:  Device at (<x> <y>) is connected to one or more eliminated nodes.` but still emitted.
Compatibility: exact
(Magic ref: extflat/EFflat.c:934-975, extflat/EFvisit.c:330-380, extflat/EFhier.c:388-445, extflat/EFhier.c:471-500)

#### Scenario: Killed node removes devices
- **GIVEN** a flat netlist run where `.res.ext` kills node `n1` and a device has a terminal on `n1` that was not rewritten
- **WHEN** devices are visited
- **THEN** that device is not output

### Requirement: Global name merging
Unless name merging is disabled (the SPICE netlister option `global off`, or `lvs`), the flattener SHALL walk the flat node list after kills. Each node whose canonical name's last component is global (see capability `ext-flattening-names`) SHALL be recorded under that last component. A later node with the same global leaf SHALL be merged into the first. If neither node is device-terminal-only, the message `*** Global name <g> not fully connected:` SHALL be printed first, listing up to 10 names of each part (informative). Then, iterating the global-leaf table in container order, the bare leaf name SHALL be added as a new name of its node. It is prepended, so it becomes the canonical name, unless that bare name already exists as a flat name.
Compatibility: exact
(Magic ref: extflat/EFflat.c:730-905)

#### Scenario: Disconnected vdd! merged
- **GIVEN** flat nodes `u1/vdd!` and `u2/vdd!` that are not connected in layout
- **WHEN** global merging runs
- **THEN** one node remains, its canonical name is `vdd!`, and a "not fully connected" warning is printed

### Requirement: Coupling capacitance flattening
When couplings are requested (the capacitance threshold is finite), each `cap` record of each instance SHALL be resolved to flat nodes. Unresolvable names are ignored silently when |C|/1000 < threshold, and with a warning otherwise. The coupling SHALL be dropped if either node is killed or both are the same node. If either node is the global substrate, C SHALL be added to the other node's capacitance to ground. Otherwise C SHALL be accumulated on the unordered node pair. Iterating the accumulated pairs yields Magic's memory-address-dependent order, and terminal order within a pair also depends on memory addresses. Hefesto SHALL emit pairs in the deterministic order of capability `hefesto-output-determinism` (decision X-1); see Q-NETLIST-1.
Compatibility: behavioral
(Magic ref: extflat/EFflat.c:1043-1157, extflat/EFvisit.c:629-650)

#### Scenario: Coupling to substrate folds into ground cap
- **GIVEN** the global substrate `VSUB` and the record `cap a VSUB 3000`
- **WHEN** it is flattened
- **THEN** node `a`'s capacitance increases by 3000 aF and no coupling pair is produced

### Requirement: Visit orders for output
The flattener SHALL offer these visits:
- **Devices.** Recursively visit children first, in use order with array expansion as in Requirement "Flat node construction", then the definition's own devices in device-table order. Definitions flagged as subcircuits are not descended into.
- **Resistors.** Children first, then the definition's own `resist` records in reverse file order, since they are prepended. Pairs on the same or killed nodes are skipped.
- **Couplings.** As in Requirement "Coupling capacitance flattening".
- **Nodes.** Flat node list order. Killed nodes are skipped. The capacitance passed is 0 for the global substrate node. In substrate compatibility mode (no `substrate` record was read), it is 0 for a root-level node named `GND!` (or the value of the Tcl variable `GND`).
- **Subcircuits.** First the root, if it is a subcircuit. Then, for each child in use order: call it if it is a subcircuit, otherwise recurse into it.

Each device visit SHALL pass the definition's scale (see Requirement "Scale record" of capability `ext-flattening-read`) if the scale-changed flag is set and that scale ≠ 1, else 1.0.
Compatibility: exact
(Magic ref: extflat/EFvisit.c:93-170, extflat/EFvisit.c:280-330, extflat/EFvisit.c:489-627, extflat/EFvisit.c:685-780)

#### Scenario: Resistor order
- **GIVEN** a cell whose file lists `resist a b 100` and then `resist b c 200`
- **WHEN** resistors are visited
- **THEN** `b c` is visited before `a b`

### Requirement: Lumped node resistance
The lumped resistance of a node SHALL be computed in mΩ as follows. For each resistance class n with area A > 0 and perimeter P > 0:
- `s = sqrt(P² − 16A)`, or 0 if `P² − 16A` is negative.
- `r_n = (P + s) / (P − s) × R_n`, where `R_n` is the class sheet resistance.

The result is the integer sum of the `r_n`, accumulated by truncating conversion and saturating at 2^31 − 1.
Compatibility: exact
(Magic ref: extflat/EFvisit.c:783-830)

#### Scenario: Square wire
- **GIVEN** one class with R = 100 mΩ/sq, area 100 and perimeter 40
- **WHEN** the lumped resistance is computed
- **THEN** s = 0 and the result is 100 mΩ

### Requirement: Device length and width
For classes other than the legacy `fet`, L and W SHALL be the stored length and width. For a legacy `fet` with 2 terminals:
- `L = (P − (int)sqrt(P² − 16A)) >> 2`
- `W = A / L`, using integer division.

For a legacy `fet` with more terminals, L = gate terminal length / 2 and W = (source length + drain length) / 2, both by integer division. Gate attributes `ext:l=<v>` or `ext:w=<v>` (comma-separated, v an integer or a symbol defined by `-s`/`-S`) SHALL override L or W for legacy fets.
Compatibility: exact
(Magic ref: extflat/EFvisit.c:175-278, extflat/EFvisit.c:402-487)

#### Scenario: Legacy fet geometry
- **GIVEN** a 3-terminal `fet` with gate length 4, source length 6 and drain length 6
- **WHEN** L and W are derived
- **THEN** L = 2 and W = 6

### Requirement: One-level flattening for hierarchical output
For hierarchical netlists, each definition SHALL be flattened on its own:
- Its local nodes are flattened with their own capacitance and area/perimeter.
- Nodes of child instances are added with zero capacitance and zero area/perimeter, so that child parasitics are not counted twice.
- Child definitions that contain no devices and no uses are absorbed: their ports, connections and couplings are flattened into the parent, and they are flagged as device-less.
- A definition whose uses are all absorbed and which has no devices of its own is itself flagged as device-less.

Kills and global merging SHALL apply as in full flattening.
Compatibility: exact
(Magic ref: extflat/EFflat.c:183-262, extflat/EFflat.c:405-460, extflat/EFflat.c:979-1040)

#### Scenario: Wire-only child absorbed
- **GIVEN** a child cell `wire` containing only metal and ports
- **WHEN** its parent is flattened one level
- **THEN** `wire` is flagged device-less and its nets merge into the parent's nets
