## MODIFIED Requirements

### Requirement: Net name map
Every DUT generation SHALL also produce a net-name map with one entry per SPICE node of the DUT netlist. Each entry has four fields:
- `layoutName`: the node's hierarchical layout name as computed by capability `irsim-interface`, Requirement "Hierarchical node names", relative to the DUT cell. Only that naming rule is used; it applies on every platform, including iPadOS, where the IRSIM interface itself is unavailable (capability `hefesto-ipad-feature-scope`)
- `spiceName`: the node name as written in the netlist
- `simName`: the ngspice vector name of that node once it is instantiated as `Xdut`
- `point`: a hierarchical use path and a point on the node in the layout, from the netlister's name map

`layoutName` SHALL be computed by applying the naming rule at `point`, so that `layoutName` and `spiceName` always describe the same node, even when the two names differ. The map SHALL be searchable by `layoutName` and by `spiceName`.

`simName` SHALL be `xdut.` followed by the instance path and the local node name. The instance path is the sequence of subcircuit instance names, lowercased and each followed by `.`. All letters are lowercased, because ngspice lowercases names. A DUT port maps to the testbench node it is bound to, not to an `xdut.` name.

For each resistor network node created by an `rnode` record in mode `rc`, the map SHALL record the parent net's `layoutName` together with the rnode name, so that probes can reach individual network points.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-6, Q-SIM-24, Q-LVS-33.)

#### Scenario: Internal node of a subcell
- **GIVEN** DUT cell `buf` containing instance `inv_0` of cell `inv`, whose local net `mid` is not a port
- **WHEN** the map is built
- **THEN** the entry for layout name `inv_0/mid` SHALL have `simName` `xdut.xinv_0.mid`

#### Scenario: Port net
- **GIVEN** DUT port `A` bound to testbench node `in`
- **WHEN** the map is built
- **THEN** layout name `A` SHALL map to `simName` `in`

#### Scenario: Renamed node keeps its layout name
- **GIVEN** DUT cell `top` with distinct nets labelled `OUT` and `out`
- **WHEN** the map is built
- **THEN** the entry with `spiceName` `out__c1` SHALL have `layoutName` `out` and a point on the net labelled `out`

## ADDED Requirements

### Requirement: sg13g2 mismatch flag on extracted devices
When the DUT is a layout of an IHP sg13g2 design and the job's Monte Carlo mode includes mismatch (capability `simulation-monte-carlo`), deck preparation SHALL add `mm_ok=1` to every sg13g2 MOS instance of the DUT netlist that does not already set `mm_ok`, inside the prepared deck only. The SPICE that `ext2spice` writes, and the files in the project, SHALL NOT change. Without a mismatch mode, the instances SHALL be passed unchanged.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-5, D-23.)

#### Scenario: Mismatch on an extracted sg13g2 pair
- **GIVEN** an extracted sg13g2 differential pair with no `mm_ok` parameter on its devices
- **WHEN** a Monte Carlo run in mode `mismatch` with 50 samples is prepared and run
- **THEN** every MOS instance line of the prepared deck SHALL carry `mm_ok=1`, the input offset SHALL vary between samples, and the project's `.spice` file SHALL be unchanged

### Requirement: Deck limits
Deck preparation SHALL enforce these limits:
- a prepared deck of at most 256 MiB;
- include depth 32.

Each limit SHALL be a setting that can be lowered. Exceeding one SHALL stop preparation with a diagnostic that names the limit and the file, before anything reaches the engine.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-25.)

#### Scenario: Include loop in a user deck
- **GIVEN** a raw deck whose `.include` files include each other
- **WHEN** it is submitted
- **THEN** preparation SHALL fail with the include-depth diagnostic, and no job SHALL be started in the engine
