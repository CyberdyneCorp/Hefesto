# Extract Substrate

## Purpose

Defines how the extractor represents the substrate: the global substrate node of each cell, isolated substrate regions (e.g., p-wells inside deep n-wells), substrate shielding types, naming of the substrate node, the `substrate` record, device substrate terminals, relocation of coupling to the substrate, and the merging of subcell substrates into the parent's substrate. The `substrate` tech keyword is parsed by `extract-tech-section`.

Let SUB = substrate types, P_sub = substrate plane, D = default substrate type (lowest user type of SUB on P_sub, if any), SH = substrate shield types, NAME = optional global substrate node name.

## Requirements

### Requirement: Backward-compatible mode without a substrate declaration

When the extract style has no `substrate` line, the extractor SHALL create no substrate node: no `substrate` record, no substrate merges with subcells, and device substrate terminals SHALL be the names given in the device records.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:5118-5150, extract/ExtHier.c:228-229, extract/ExtCell.c:283-286)

#### Scenario: Old SCMOS style
- **GIVEN** a style with `fet pfet pdiff 2 pfet Vdd! 0 0` and no `substrate` line
- **WHEN** a cell is extracted
- **THEN** the pfet substrate field is `"Vdd!"` and no `substrate` record appears

### Requirement: Isolated substrate regions

When P_sub ≥ 0 and D exists, for the duration of extracting a cell, the cell's P_sub plane SHALL be replaced by a copy in which every area of P_sub that is space in the flattened hierarchy (cell plus all subcells) and is enclosed by shield types or by non-substrate types of P_sub—i.e., an *isolated* substrate area not connected to the outer (unbounded) substrate—is painted with D. Areas reachable from the unbounded outside SHALL remain space. The original plane SHALL be restored after the cell (and, for `extract all`, after the whole run) is extracted. Under full R-C extraction the entire substrate area of the cell SHALL instead be painted with D (owned by capability `extresist-command`).
Compatibility: exact

(Magic ref: extract/ExtCell.c:283-330, extract/ExtCell.c:410-450, database/DBcellcopy.c:DBCellGenerateSubstrate)

#### Scenario: P-well island in deep n-well
- **GIVEN** `substrate *psd,space,pwell well -dnwell` with D = pwell, a dnwell ring enclosing empty well-plane area containing an nfet
- **WHEN** the cell is extracted
- **THEN** the enclosed area is treated as pwell paint and forms its own node, distinct from the global substrate node

### Requirement: Global substrate node discovery

The substrate node of a cell SHALL be formed before all other nodes as follows. Let SUBNS = SUB minus space minus D (D is kept only under full R-C extraction). On P_sub, every tile of a SUBNS type not covered (in its area) by any SH type on any plane SHALL be collected. On every other plane, tiles of SUBNS types SHALL be collected if not covered by SH, and—only when space ∈ SUB—only if the area under them on P_sub is entirely space. All collected tiles, and everything connected to them, SHALL form one single node (the substrate node), even if the collected tiles are mutually disconnected. If nothing is collected and P_sub ≥ 0, a substrate node with no tiles SHALL be created with reference point (−1073741817, −1073741817) (i.e., −(2³⁰−4)+3), plane P_sub, type space and no labels.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:5085-5170, extract/ExtBasic.c:5211-5339, tiles/tile.h:164)

#### Scenario: Tap diffusion in the substrate
- **GIVEN** `substrate *psd,space well` (space ∈ SUB, no D) and two separate `psd` taps over empty well-plane area
- **WHEN** the cell is extracted
- **THEN** both taps and everything connected to them are one substrate node

#### Scenario: Cell with no substrate material
- **GIVEN** `substrate space well VSUB` and a cell containing only metal1
- **WHEN** the cell is extracted
- **THEN** the record `substrate "VSUB" 0 0 -1073741817 -1073741817 space ...` is written

### Requirement: Substrate node naming

The substrate node SHALL be named like any node (first name-role label, see `extract-nodes-and-labels`), except that a node whose reference x is ≤ −1073741817 (the tile-less substrate node) SHALL be named NAME when the style gives one; when NAME starts with a single `$`, the value of the host variable named by the rest SHALL be used if defined, else NAME without the `$` (see Q-EXTRACT-9 [platform]). Labels of a SUB type on P_sub that do not attach to any tile and whose probe points found no connecting type SHALL be attached to the substrate node.
Compatibility: exact (names); behavioral (variable substitution)

(Magic ref: extract/ExtBasic.c:886-940, extract/ExtRegion.c:270-305)

#### Scenario: Labeled substrate
- **GIVEN** `substrate *pwell,space well VSUB`, a cell with no well-plane paint, and a label `gnd` of type `pwell` placed over empty well-plane area
- **WHEN** the cell is extracted
- **THEN** the substrate node carries the label `gnd` and is named `gnd`

### Requirement: Substrate record

The substrate node SHALL be written as `substrate "<name>" <R> 0 <x> <y> <type> <a0> <p0> ...` (capacitance field always `0`) in place of a `node` record, unless the cell has the `LEFview` property, in which case the cell has no substrate node designation and the node SHALL be written as an ordinary `node` record.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:304-307, extract/ExtBasic.c:733-751)

#### Scenario: Abstract view
- **GIVEN** a cell with property `LEFview` = true
- **WHEN** it is extracted
- **THEN** the file contains `abstract` and no `substrate` record

### Requirement: Device substrate terminals

A device's substrate field SHALL be the node found under the device on a type of its record's substrate types (see `extract-devices`), the cell's substrate node when the record's substrate types include space and none is found, and otherwise the record's substrate name (with `$`-variable substitution only in backward-compatible mode, informative). Types in device records' substrate masks that are also substrate types SHALL mark those device types as "substrate-connected"; together with substrate types not on P_sub and not shields, they SHALL be the types used to decide whether a subcell's substrate is shielded (Requirement: Subcell substrate connections).
Compatibility: exact

(Magic ref: extract/ExtBasic.c:3577-3640, extract/ExtBasic.c:2612-2630, extract/ExtTech.c:3633-3650)

#### Scenario: NFET in isolated well
- **GIVEN** the deep-n-well scenario of Requirement: Isolated substrate regions with an nfet record whose substrate types are `pwell,space`
- **WHEN** the cell is extracted
- **THEN** the nfet's substrate field names the isolated pwell node, not the global substrate

### Requirement: Subcell substrate connections

For each subcell use (once per use; each element of an array), unless the use's cell is "don't use" or the parent has no substrate node, the child's substrate node SHALL be computed (Requirement: Global substrate node discovery on the child) and, unless shielded, unified with the parent's substrate node in the hierarchical name table (producing a `merge`; see `extract-hierarchy`). The child substrate's name SHALL be `<useid>/<name>`, `<useid>[<i>]/<name>` for 1-D arrays, or `<useid>[<y>,<x>]/<name>` for 2-D arrays. The child substrate SHALL be considered shielded, and not unified, when within the 1×1 square at the child's substrate reference point (transformed to the parent) — or the child's whole bounding box when that point lies outside it — the parent has SH material under which the child subtree contains substrate-connected device types.
Compatibility: exact

(Magic ref: extract/ExtHier.c:208-395, extract/ExtSubtree.c:670-700, extract/ExtSubtree.c:720-760)

#### Scenario: Standard cell substrate merge
- **GIVEN** `substrate space well VSUB`, a top cell with substrate node `VSUB` and an instance `x1` of a cell whose substrate node is also `VSUB`
- **WHEN** the top is extracted
- **THEN** the top `.ext` contains `merge "x1/VSUB" "VSUB" ...` (or the reverse order per unification rules)

### Requirement: Substrate coupling relocation

When P_sub ≥ 0 and coupling is on, coupling between any node and the substrate node SHALL be converted into substrate capacitance of the other node (see `extract-capacitance`).
Compatibility: exact

(Magic ref: extract/ExtCouple.c:232-280)

#### Scenario: No cap record to substrate
- **GIVEN** any coupling rule whose target material belongs to the substrate node
- **WHEN** the cell is extracted
- **THEN** no `cap` record names the substrate node
