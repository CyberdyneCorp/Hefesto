# Extract Capacitance

## Purpose

Defines the exact numeric models the extractor uses to compute parasitic capacitance within one flattened area of layout: node capacitance to substrate (area, perimeter and corner terms), overlap coupling with shielding, sidewall coupling between facing edges, sidewall-overlap (fringe) coupling under the legacy model and under the distributed "fringe halo" model, corner fringe coupling, the relocation of coupling to the substrate node, and the output of `cap` records. The same models are applied by `extract-hierarchy` to clipped interaction areas.

All rule tables (coefficients, multipliers, shield sets, plane order) come from capability `extract-tech-section`, already scaled to internal units. Unless stated, arithmetic is IEEE double precision; coordinates, lengths and areas are integers in internal units; capacitances are aF. A *contact residue* is the non-contact type a contact represents on a given plane; whenever a contact type appears as the inside, outside or target type of a rule lookup, its residue on the plane of that tile SHALL be used.

Let `F_m(d) = 0.6366 · atan(m · d)` (the constant is exactly 0.6366, not 2/π), where `m` is a fringe multiplier from the tech tables: `m(a,0)` for type a against substrate (space) and `m(a,b)` for type a against type b.

## Requirements

### Requirement: Capacitance option gating

When option `capacitance` is off, every node's substrate capacitance SHALL be forced to 0 while nodes are built (later subtractions may make it negative; output clamps to 0). When option `coupling` is off, no overlap, sidewall, sideoverlap or corner computation SHALL be performed: node capacitance then contains only area, perimeter and (if enabled) corner substrate terms, and no `cap` record is written.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:5869-5871, extract/ExtBasic.c:338-351)

#### Scenario: Coupling disabled
- **GIVEN** `extract no coupling` and a metal2 plate fully over a metal1 plate with an `overlap metal2 metal1` rule
- **WHEN** the cell is extracted
- **THEN** no `cap` record is written and metal2's node capacitance still includes its full area capacitance

### Requirement: Area capacitance to substrate

Each tile of a node SHALL contribute `A · areacap(r)` where r is the tile type (residue on the tile's plane for contacts) and A is the tile area, clipped to the extraction clip area when one is in effect (hierarchical extraction); for a split (diagonal) tile A SHALL be the clipped rectangle area divided by 2 using integer division.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:5560-5582)

#### Scenario: Rectangle area cap
- **GIVEN** `areacap metal1 30`, `perimc metal1 space 20`, `cscale 1`, a 1:1 grid and a lone metal1 rectangle (0,0)-(10,4)
- **WHEN** the cell is extracted
- **THEN** the node capacitance is 40·30 + 28·20 = 1760 and the node record shows `1760`

#### Scenario: Default cscale
- **GIVEN** the same layout with default `cscale 1000`
- **WHEN** the cell is extracted
- **THEN** the node record shows `1.76`

### Requirement: Perimeter capacitance to substrate

For each tile of a node, every side SHALL be walked against each neighboring tile; for each shared segment of length `len` (clipped to the clip area when in effect, 0 if the clipped segment is empty) with neighbor type t (residue on the tile's plane if t is a contact), the node SHALL gain `len · perimc(r, t)`. For a split tile only the two orthogonal sides belonging to the node's half SHALL be walked, and the diagonal SHALL contribute `L · perimc(r, t')` with `L = (int)sqrt(w² + h²)` (w, h tile width and height, unclipped) and t' the type of the other half.
Compatibility: exact

Because perimeter rules exist only for edges whose outside type is declared, edges between connected materials of the same node normally contribute 0.

(Magic ref: extract/ExtBasic.c:5584-5868)

#### Scenario: Diagonal tile
- **GIVEN** `areacap metal1 30`, `perimc metal1 space 20` and a metal1 right triangle filling the lower-left half of a 4×4 split tile surrounded by space
- **WHEN** the cell is extracted
- **THEN** the capacitance is (16/2)·30 + (5 + 4 + 4)·20 = 500

### Requirement: Corner fringe capacitance to substrate

When options `corners`, `fringehalo` are both on and the side halo is > 0, then for every horizontal segment of a node tile (top or bottom side) that has non-zero perimeter capacitance `c` against its neighbor, both endpoints SHALL be classified, and the node SHALL gain `+c·(k/m)` for each convex endpoint and `−c·(k/m)` for each concave endpoint, where `m = m(r,0) > 0` (no corner term when `m <= 0`) and `k` is the corner factor (default 1.0, set by `extract do corners <k>`). An endpoint at x = cx (cx = max of the two left edges, or min of the two right edges) on the segment line is *convex* when the tile continuing the inside side beyond cx and the tile diagonally beyond cx on the outside are both space, and *concave* when both have types that connect to the node's type; otherwise no correction. Split tiles adjacent to the endpoint SHALL disable the correction. With a clip area, only endpoints with `xbot <= cx < xtop` and `ybot <= cy < ytop` of the clip area SHALL be considered.
Compatibility: exact

See Q-EXTRACT-24.

(Magic ref: extract/ExtBasic.c:5341-5441, extract/ExtBasic.c:5466-5468)

#### Scenario: Isolated rectangle corners
- **GIVEN** corners, fringehalo and sidehalo > 0, `areacap metal1 30` (so m = 0.6/unit), `perimc metal1 space 20`, k = 1, and a lone metal1 rectangle
- **WHEN** the cell is extracted
- **THEN** the four convex corners add 4 · 20 · (1/0.6) ≈ 133.333 aF to the node capacitance (verify against Magic)

### Requirement: Overlap coupling with shielding

For each plane p with overlap rules, for each tile T (type a, residue on p) whose type has overlap targets, and for each other plane q in a's overlap-target planes, for each tile U on q of a target type b overlapping T (clipped to the search clip area), with both tiles belonging to nodes: let R = T ∩ U (clipped) and A = area(R). If the pair (a,b) has shield planes, A SHALL be reduced by the area of R covered by shield-type material on any shield plane, computed by visiting shield planes in increasing plane number: material of a shield type on the first plane covers its area; other material on that plane defers to the next shield plane for its sub-rectangle; split tiles count half their clipped area. If the remaining `O = A > 0`:
Compatibility: exact

1. if `planeorder(p) > planeorder(q)`, node(T) SHALL lose `areacap(a) · O`;
2. if node(T) ≠ node(U), the coupling between node(T) and node(U) SHALL gain `overlapcap(a,b) · O`.

(Magic ref: extract/ExtCouple.c:190-215, extract/ExtCouple.c:336-584, extract/ExtCouple.c:586-670)

#### Scenario: Plate over plate
- **GIVEN** plane order metal1 < metal2, `areacap metal2 10`, `overlap metal2 metal1 20`, metal2 (0,0)-(10,10) over metal1 (0,0)-(10,10) on different nodes
- **WHEN** the cell is extracted
- **THEN** metal2's area contribution 1000 is cancelled (1000 − 10·100 = 0, perimeter terms remain) and the pair coupling is 20·100 = 2000

#### Scenario: Partially shielded overlap
- **GIVEN** plane order metal1 < metal2 < metal3, `overlap metal3 metal1 8 metal2`, metal3 (0,0)-(10,10), metal1 (0,0)-(10,10) and metal2 (0,0)-(10,4)
- **WHEN** the cell is extracted
- **THEN** the metal3–metal1 coupling is 8 · (100 − 40) = 480 and metal3 loses areacap(metal3)·60 for this pair

### Requirement: Sidewall edge enumeration

For each plane with side rules and each tile of a type with side rules, every Manhattan boundary segment whose outside type is in the tile type's side-edge set SHALL be considered (non-Manhattan diagonals are skipped). The edge SHALL be skipped when its sidewall coupling-target set is empty and `fringehalo` is on and the side halo ≠ 0. With inside type a and outside type o (residues), it SHALL be skipped when edge (a|o) has neither sidewall nor sideoverlap rules. With a clip area, segments not overlapping it SHALL be skipped and others clipped to it.
Compatibility: exact

For the edge, the *walk area* SHALL be the segment extended outward (away from the inside) by the side halo h, and the *fringe distance* SHALL be `h` when `fringehalo` is on and h > 0, else 1. The *fringe-halo model* applies iff `fringehalo` is on and h > 0; otherwise the *legacy model* applies.

(Magic ref: extract/ExtCouple.c:830-866, extract/ExtCouple.c:1105-1245)

#### Scenario: Halo zero forces legacy model
- **GIVEN** `fringehalo` on and a style with `sidehalo 0`
- **WHEN** a cell is extracted
- **THEN** sideoverlap coupling uses the legacy model

### Requirement: Nearest-neighbor shadowing along an edge

Within the walk area, the segment SHALL be partitioned by shadowing: scanning outward from the edge, the nearest tile whose type is in the edge's coupling-target set (far types) and that overlaps the walk area SHALL be selected; the part of the segment facing it (its extent along the edge, clipped to the segment) couples to it, and the parts of the segment to either side SHALL be processed recursively with the same rule on their own sub-areas; material behind a selected tile is shadowed. Ties at equal distance are resolved by the tile-plane scan order (rows nearest first; along a row, from the end of the segment opposite to the start corner of the walk: right-to-left for top edges, left-to-right for bottom edges, top-to-bottom for right edges, bottom-to-top for left edges).
Compatibility: exact

For each selected far tile and its facing sub-segment: sidewall coupling SHALL be computed (Requirement: Sidewall coupling value); the fringe (sideoverlap) search SHALL be performed over the gap between the sub-segment and the far tile (Requirement: Legacy sideoverlap / Requirement: Fringe-halo sideoverlap); and, in the fringe-halo model only, the substrate fringe blocked by the far tile SHALL be removed: node(inside) loses `perimc(a,o) · (1 − F_m(d)) · len`, with `m = m(a,0)`, d the gap distance (≥ 0) and len the sub-segment length, using the boundary's stored inside/outside types. Sub-segments with no far tile within the halo SHALL get the fringe search over their full walk sub-area.

(Magic ref: extract/ExtCouple.c:2449-2885, extract/ExtCouple.c:1256-1311)

#### Scenario: Shadowed third wire
- **GIVEN** three parallel metal1 wires A, B, C of equal length with A–B spacing 2 and B–C spacing 2 and side halo 10
- **WHEN** the cell is extracted
- **THEN** A couples to B only; no A–C sidewall coupling is produced

### Requirement: Sidewall coupling value

For a selected far tile F (type f) at separation `sep` (integer distance between the edge and F's facing side), each tile N adjacent to F on the side facing the edge contributes over its overlap `ov` with the sub-segment (only positive overlaps) and with near type n: for every sidewall rule of edge (a|o) whose near set contains n and far set contains f, the coupling between node(inside) and node(F) SHALL gain `C_half · ov / (sep + offset)`, where `C_half` is the halved coefficient and `sep + offset` is evaluated in single-precision float (offset is a float in internal units). No coupling SHALL be added when node(F) is the inside node or F belongs to no node.
Compatibility: exact

Because the same facing pair is also visited from the other edge, the total coupling between two facing edges of common length L at separation s is `C · L / (s + offset)`.

See Q-EXTRACT-20.

(Magic ref: extract/ExtCouple.c:2886-3134)

#### Scenario: Two parallel wires, legacy
- **GIVEN** `extract no fringehalo`, `sidehalo 4`, `sidewall metal1 space space metal1 40`, `cscale 1` and two metal1 wires (0,0)-(10,2) and (0,4)-(10,6)
- **WHEN** the cell is extracted
- **THEN** the record `cap "<w1>" "<w2>" 200` is written (two visits of 20·10/2)

#### Scenario: Sidewall with offset
- **GIVEN** the same layout with `sidewall metal1 space space metal1 40 0.5`
- **WHEN** the cell is extracted
- **THEN** the coupling is 2 · 20·10/2.5 = 160

### Requirement: Legacy sideoverlap (fringe directly below the edge)

In the legacy model, for each (sub-)segment the search strip SHALL be the segment extended outward by exactly 1 unit, restricted along the edge to the sub-segment. On each target plane of edge (a|o), for each tile U of a target type b ≠ space overlapping the strip, belonging to node(U): let `len` = overlap of U with the *whole* boundary segment along the edge, `S` = area of U ∩ strip. For each sideoverlap rule of edge (a|o) whose plane mask includes U's plane and whose target set contains b: let `O = S` minus the area shielded on the pair's shield planes by the rule's shield types (computed as in overlap shielding); if node(U) ≠ node(inside) the pair coupling gains `cap_rule · O`; and `acc += O`. Then if `planeorder(edge plane) > planeorder(U plane)`, node(inside) SHALL lose `perimc(a, o') · min(acc, len)` (o' the residue of the boundary's outside type) and a resulting node value with |value| < 0.001 SHALL be set to 0. Finally if the nodes differ and both exist, the coupling SHALL be updated.
Compatibility: exact

See Q-EXTRACT-18.

(Magic ref: extract/ExtCouple.c:1330-1418, extract/ExtCouple.c:1651-1836)

#### Scenario: Edge over a plate, legacy
- **GIVEN** legacy model, `sideoverlap metal2 space metal1 6`, `perimc metal2 space 9`, plane order metal1 < metal2, metal2 (0,0)-(10,10) over a large metal1 plate on another node
- **WHEN** the cell is extracted
- **THEN** each of the 4 edges adds 6·10 = 60 to the metal2–metal1 coupling (240 total) and removes 9·10 = 90 from metal2's substrate capacitance

### Requirement: Fringe-halo sideoverlap

In the fringe-halo model, for each (sub-)segment and its gap area G (walk sub-area up to the selected far tile or the full halo), on each target plane, for each tile U of target type b ≠ space overlapping G: let R = U ∩ G, `len` = extent of R along the edge, `dnear`, `dfar` = distances from the edge line to R's near and far sides (dnear clamped to ≥ 0). With `mc = m(a,b)` and `ms = m(a,0)`: `cfrac = F_mc(dfar) − F_mc(dnear)` and `sfrac = F_ms(dfar) − F_ms(dnear)`. For each sideoverlap rule of edge (a|o) for U's plane whose targets contain b: compute shield fractions by visiting the shield planes of (a,b) as for overlap shielding, where each shielding piece P ⊂ R with distances `dn`, `df` and length `lp` adds `(F_mc(df) − F_mc(dn))·lp/len` to `coupfrac` and `(F_ms'(df) − F_ms'(dn))·lp/len` to `subfrac_rule` (here ms' = m(a',0) with a' the boundary's stored inside type); if node(U) ≠ node(inside): coupling gains `cap_rule · (cfrac − coupfrac) · len` and `subfrac += subfrac_rule`. Then if `planeorder(edge plane) > planeorder(U plane)`, node(inside) SHALL lose `perimc(a, space) · (sfrac − subfrac) · len`, with |value| < 0.001 set to 0. The coupling entry SHALL be updated when nodes differ and both exist.
Compatibility: exact

See Q-EXTRACT-17.

(Magic ref: extract/ExtCouple.c:1420-1649, extract/ExtCouple.c:672-828)

#### Scenario: Edge over a plate, fringe halo (verify against Magic)
- **GIVEN** fringe-halo model with `sidehalo 10`, `overlap metal2 metal1 20` (so m(metal2,metal1) = 0.4), `sideoverlap metal2 space metal1 6`, and metal2 (0,0)-(10,10) over a metal1 plate extending 20 beyond every edge
- **WHEN** the cell is extracted
- **THEN** each edge adds 6 · 0.6366·atan(4) · 10 ≈ 50.64 to the coupling (≈ 202.56 total)

### Requirement: Corner fringe coupling

When options `corners` and the fringe-halo model apply and edge (a|o) has sideoverlap rules, each horizontal boundary SHALL have its two endpoints classified with respect to the boundary's inside node (convex: the inside-continuation tile and the diagonal tile are both space; concave: both belong to the inside node and the boundary's outside tile is space; split tiles disable). The quadrant SHALL be the h×h square at the corner: for convex corners diagonally outward, for concave corners on the outside of the boundary on the segment side. For each target tile U (type b ≠ space) on the edge's target planes overlapping the quadrant, with R = U ∩ quadrant non-empty, and each matching rule with `mc = m(a,b) > 0`: `cfrac = Fx·Fy` with `Fx = 0.6366(atan(mc·dxfar) − atan(mc·dxnear))` and `Fy` likewise (distances from the corner, near distances clamped ≥ 0), minus the same product summed over shielding pieces (clamped ≥ 0); if nodes differ the corner cap gains `cap_rule · (k/mc) · cfrac` and the shielded substrate fraction is accumulated. If `planeorder(edge plane) > planeorder(U plane)` and `ms = m(a,0) > 0`, node(inside) SHALL lose `sign · perimc(a,0) · (k/ms) · max(0, Fx·Fy|ms − subfrac)` (sign = +1 convex, −1 concave), |value| < 0.001 set to 0. A non-zero corner cap times sign SHALL be added to the pair coupling when nodes differ.
Compatibility: exact

(Magic ref: extract/ExtCouple.c:1838-2447)

#### Scenario: Convex corner over a plate (verify against Magic)
- **GIVEN** `extract do corners`, fringe-halo model, and a metal2 rectangle over a metal1 plate covering all four corner quadrants
- **WHEN** the cell is extracted
- **THEN** the metal2–metal1 coupling exceeds the no-corners value by four convex corner terms

### Requirement: Coupling to the substrate node

When the style declares a substrate plane and coupling is on, after all coupling is computed, every non-zero pair entry that involves the cell's substrate node SHALL be removed and its value added to the other node's substrate capacitance.
Compatibility: exact

(Magic ref: extract/ExtCouple.c:232-280, extract/ExtBasic.c:347-350)

#### Scenario: Overlap to substrate well becomes node cap
- **GIVEN** `defaultareacap metal1 metal1 well 30` with `pwell` a substrate type and a metal1 shape over a pwell that is part of the substrate node
- **WHEN** the cell is extracted
- **THEN** no `cap` record names the substrate node; the overlap value appears in metal1's node capacitance

### Requirement: Coupling record output

Each pair entry with value V ≠ 0 after dividing by `cscale` SHALL be written as `cap "<name1>" "<name2>" <V>` with V in C `%lg` format (6 significant digits). Coupling records SHALL NOT be written for a cell carrying the `device` property. Pair entries are unordered: in Magic the order of the two names within a record and the order of records depend on hash and memory-address order (see Q-EXTRACT-7); Hefesto SHALL emit them in the deterministic order of capability `hefesto-output-determinism` (decision X-1).
Compatibility: exact (set of unordered pairs and values); behavioral (order)

The node capacitance written in a `node` record SHALL be `max(0, C_node / cscale)` in `%lg` format.

(Magic ref: extract/ExtCouple.c:289-322, extract/ExtBasic.c:745-749, extract/ExtBasic.c:399-401)

#### Scenario: Value formatting
- **GIVEN** a coupling of 1234567 aF and `cscale 1000`
- **WHEN** the record is written
- **THEN** the value field is `1234.57`
