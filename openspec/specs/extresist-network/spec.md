# extresist-network

## Purpose

This capability defines how `extresist-command` builds a resistor network for one net from layout geometry, and how it reduces that network: path resistance, capacitance distribution, Elmore delay, pruning and simplification. The rules for which net is selected belong to capability `extresist-command`. Node naming and file output belong to capability `extresist-output`. Resistances are single-precision floats in mΩ. Sheet resistances come from the extraction style (mΩ/square) and via resistances in mΩ per via. Tile geometry, CIF contact sizes and connectivity come from capabilities owned elsewhere (tech/extract).

## Requirements

### Requirement: Net geometry collection
For a selected net, the extractor SHALL build a private copy of the geometry as follows:
1. Collect all paint electrically connected to the net. Start from the net's recorded location and layer, or from the first drive point if the location is undefined. Connectivity is per layer, using the per-type copy masks, which the `skip` option can reduce.
2. Add the full area of each device that has a terminal on the net.
3. Erase the upper layers of stacked contacts so that each contact is processed once, then record each contact's position, size and per-plane tiles.
4. Dissolve the contacts into their residue layers.
5. Re-fracture every plane so that maximal horizontal strips are split at concave corners in the direction with the least same-type material. This makes current paths follow wires.

The detailed fracture rule is captured in Q-NETLIST-38.
Compatibility: behavioral
(Magic ref: resis/ResMain.c:1243-1580, resis/ResFract.c:43-160, resis/ResFract.c:164-490)

#### Scenario: Skipped layer not followed
- **GIVEN** `extresist skip poly` and a net that continues from metal1 through a contact onto poly
- **WHEN** the geometry is collected
- **THEN** the poly portion is not part of the copied net

### Requirement: Network nodes and breakpoints
The network SHALL contain these nodes:
- one **origin** node at the driver location, with path resistance 0
- one node per **device terminal** contact with the net
- **contact** nodes, per Requirement "Contact resistors"
- **port, drive and sink** nodes, at the net's drive and sink rectangles and at port labels
- **junction** nodes where current crosses between abutting tiles of the net

Each node SHALL be registered as a breakpoint at its location on every tile it touches. Breakpoints of different nodes at the same coordinate along a tile's resistor axis SHALL be merged into one node.
Compatibility: behavioral
(Magic ref: resis/ResBasic.c:47-409, resis/ResJunct.c:51-234, resis/ResMain.c:229-590, resis/ResRex.c:2316-2334)

#### Scenario: Origin
- **GIVEN** a net driven by a MOSFET at (10,20)
- **WHEN** the network is built
- **THEN** a node with path resistance 0 exists at the MOSFET's terminal on the net

### Requirement: Tile resistors
For each tile, the breakpoints SHALL be turned into resistors as follows:
- **No breakpoints.** The tile produces nothing.
- **One breakpoint.** Its node receives the whole tile area. No resistor is made.
- **A device breakpoint present.** The near-device rule is used. It cuts current paths toward the device terminals; see Q-NETLIST-38 for the unverified details.
- **Otherwise.** If the breakpoints' y-extent is greater than their x-extent, the tile is cut north-south; otherwise east-west.

East-west cutting SHALL work as follows:
1. Sort the breakpoints by x. The merge sort keeps equal keys in input order.
2. Merge breakpoints with equal x into one node.
3. For each pair of consecutive distinct positions x1 < x2, create a resistor `R = sheet[type] × (x2 − x1) / H`, where H is the tile height. Add `((x2 − x1) × H) / 2` (integer division) to the area of both end nodes.
4. Add `H × (x1 − left)` to the first node's area and `H × (right − x_last)` to the last node's area.

North-south cutting is the same with x and y swapped and the tile width in place of H. For a diagonally split tile, the type is the left type, or the right type when the left side is empty space. The resistor is marked diagonal. New resistors are prepended to the network's resistor list.
Compatibility: exact
(Magic ref: resis/ResMakeRes.c:55-120, resis/ResMakeRes.c:122-545, resis/ResMakeRes.c:1013-1230)

#### Scenario: Straight wire segment
- **GIVEN** a metal1 tile 100 wide and 4 high with sheet resistance 80 mΩ/sq, and breakpoints at x = 0 and x = 100 on its left and right edges
- **WHEN** it is cut
- **THEN** one resistor of 2000 mΩ is made, and each end node gains an area of 200

### Requirement: Contact resistors
For a contact of type t, the following SHALL apply:
- **No resistance.** If the via resistance of t is 0 or the CIF via size is 0, one node at the contact center SHALL join all of its plane tiles, with no resistor.
- **Via count.** Otherwise, let `c` be the CIF scale factor and `minside`, `via` and `spacing` the CIF contact dimensions.
  - If `width·c < minside` or `height·c < minside`, then `nx = ny = 1`, and a one-time warning `<type> at <x> <y> smaller than extract section allows` is printed.
  - Otherwise `nx = (int)((width·c − minside)/(via + spacing)) + 1` and `ny = (int)((height·c − minside)/(via + spacing)) + 1`.
- **Resistors.** One node SHALL be created per plane tile of the contact. Consecutive tiles SHALL be joined by a resistor of value `viaResist[t] / (nx·ny)`, with `rr_cl = ny` and `rr_width = nx`.
Compatibility: exact
(Magic ref: resis/ResMakeRes.c:856-1011)

#### Scenario: Two-via contact
- **GIVEN** a via resistance of 10000 mΩ and a contact large enough for nx = 2, ny = 1, spanning two planes
- **WHEN** it is processed
- **THEN** one resistor of 5000 mΩ joins the two plane nodes

### Requirement: Path resistance and capacitance distribution
After the network is built, the path resistance of every node SHALL be computed from the origin with Dijkstra's algorithm. The priority keys are `(int)(noderes + R)`. Ties are resolved by the heap order (Q-NETLIST-25). Resistors that close a loop between two finished nodes SHALL be marked as ignored for delay. If no node has resistance 0, the source terminal of the driver device becomes the origin. `maxres` SHALL be the largest path resistance.

The net capacitance C SHALL then be distributed: each node's capacitance is `area_node × C / Σarea`. If Σarea = 0, the message `Error: Node with no area.` SHALL be printed and the capacitance left undistributed.
Compatibility: exact
(Magic ref: resis/ResSimple.c:942-960, resis/ResSimple.c:1083-1211, resis/ResSimple.c:716-757)

#### Scenario: Two-segment path
- **GIVEN** an origin connected to A by 2000 mΩ, and A connected to B by 3000 mΩ
- **WHEN** path resistance is computed
- **THEN** A has 2000, B has 5000, and maxres = 5000

### Requirement: Elmore delay and pruning
When mindelay > 0, the downstream capacitance of each node SHALL be computed. It is the node's own capacitance, plus `L·W·gateCap + (W+W)·sdCap` for each device whose gate is this node (Q-NETLIST-24), plus the downstream capacitance of all children through non-ignored resistors. A loop SHALL make the delay undefined (Tdi = −1). Otherwise:
- `Tdi(origin) = 0`
- `Tdi(child) = Cdown(child) × R + Tdi(parent)`
- the net's Tdi is the maximum over all nodes

When simplification is on and Tdi is defined, every leaf SHALL be pruned, working bottom-up. A leaf here is a node with exactly one resistor, directed into it, that is not an origin, port or sink node, and whose Tdi < mindelay. Pruning merges the leaf into its upstream node.
Compatibility: exact
(Magic ref: resis/ResSimple.c:759-940, resis/ResSimple.c:960-1000)

#### Scenario: Single RC delay
- **GIVEN** origin → A through 1000 mΩ, A with 2000 aF, and no devices
- **WHEN** Tdi is computed
- **THEN** Tdi(A) = 2e6 zs

### Requirement: Network simplification
When simplification is on, nodes SHALL be processed from the origin outward with the tolerance `minres`. Origin and sink nodes (ports, drive points, the driver) SHALL never be eliminated. The following rules apply:
- **Series.** A node with exactly two resistors, both below the tolerance, SHALL be removed. Its resistors are combined into one of value R1 + R2. The removed node's area is split: R1·A/(R1+R2) goes to the far end of R2, and R2·A/(R1+R2) goes to the far end of R1. Its devices move to the end chosen by the dead-end and smaller-resistance rules.
- **Dead end.** A dead-end node reached by one resistor below the tolerance SHALL be merged into its upstream node. This is done only when the upstream node has more than one driver or other large or dead-end receivers. Its resistor's area is added to the upstream node.
- **Final pass.** After propagation, resistors SHALL be sorted by ascending value. With fewer than 10 resistors, a stable insertion sort is used; otherwise an unstable sort (Q-NETLIST-26). While the smallest resistor is below the tolerance and more than one resistor remains:
  - A resistor whose two ends are both protected is skipped.
  - Otherwise, the smallest other resistor adjacent to either end is found. If both ends have other resistors, the small value is added to that resistor; otherwise the small resistor's area is added to its end node.
  - The resistor is removed, and its two nodes are merged into the protected one when one is protected.
  - Simplification is rerun.
Compatibility: exact
(Magic ref: resis/ResSimple.c:66-376, resis/ResSimple.c:448-714, resis/ResMerge.c:45-1290)

#### Scenario: Series reduction
- **GIVEN** minres 1000 and a chain origin → X → port with 400 mΩ and 300 mΩ, where X is not protected
- **WHEN** the network is simplified
- **THEN** X is removed and one 700 mΩ resistor joins the origin to the port

#### Scenario: Port-to-port resistor kept
- **GIVEN** a 10 mΩ resistor whose ends are two port nodes
- **WHEN** the final pass runs
- **THEN** the resistor remains in the network
