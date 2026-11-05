# Layout Connectivity

## Purpose

Specifies the electrical connectivity relation of the layout database as observable semantics: which pieces of paint are connected within a plane, across planes through contacts, and across the cell hierarchy, and how the flat and hierarchical connectivity searches used by net selection, net highlighting, DEF/netlist tools and resistance extraction enumerate a net. Extraction's own node-building algorithm is specified in `extract-nodes-and-labels`; the technology `connect` section syntax in `tech-*`.

## Requirements

### Requirement: Connect table

The system SHALL derive from the technology a symmetric "connects-to" relation over tile types, including the rules for contacts and generated stacked contacts given below.
Compatibility: exact

- Every type connects to itself; the technology `connect` section adds pairs (see `tech-*`).
- A contact connects to each of its residues and to everything its residues connect to as declared by the technology.
- A generated stacked type connects to its residue contacts, to every type those contacts connect to, and to every other stacked type with which it shares a residue contact.
- The relation is made symmetric after the above.
- Only contact (and stacked) types link planes: the "connect planes" of a type are its own planes if it is a contact, otherwise none.
- Space connects to nothing.

(Magic ref: database/DBtech.c:272-300,352-470,500-510)

#### Scenario: Stacked type connects to both metals
- **GIVEN** stackable `via1 (metal1,metal2)`, `via2 (metal2,metal3)` and generated `via1+via2`
- **WHEN** the connect relation is queried
- **THEN** `via1+via2` SHALL connect to `via1`, `via2`, `metal1`, `metal2` and `metal3`

### Requirement: Connectivity within one cell

Two pieces of paint in the same cell SHALL be connected when they are on the same plane, their types connect, and they share a boundary segment of positive length (corner-only contact does not connect); or when one is a contact whose image on another plane overlaps or abuts the other piece on that plane; connectivity SHALL be the transitive closure of these relations.
Compatibility: behavioral

- Split (triangle) tiles connect only across their two legs and through their hypotenuse to the other half when the types connect; a triangle half does not connect through the corner opposite its right angle.
- Within the flat search the starting tile is the first tile (in plane order, then tile enumeration order) of a requested type overlapping the start area; if no such tile exists the net is empty. If several distinct nets lie under the start area, only the first is returned.
- A search may be bounded by an area: tiles not overlapping it are neither reported nor traversed.

(Magic ref: database/DBconnect.c:171-247,394-668)

#### Scenario: Corner touch does not connect
- **GIVEN** `metal1 (0,0)-(10,10)` and `metal1 (10,10)-(20,20)`
- **WHEN** the net containing `(1,1)` is traced
- **THEN** it SHALL contain only `(0,0)-(10,10)`

#### Scenario: Through a via
- **GIVEN** `metal1 (0,0)-(10,10)`, `via (4,4)-(6,6)`, `metal2 (4,4)-(30,6)`
- **WHEN** the net containing `metal1` at `(1,1)` is traced
- **THEN** it SHALL contain the metal1 area, the via on both planes and `metal2 (6,4)-(30,6)`

### Requirement: Flat search cross-plane quirk

The flat (single-cell) connectivity search SHALL, from a contact tile, look on each other plane of the contact for one tile of a connecting type overlapping the contact tile's rectangle expanded by one unit, and continue the search from that single tile.
Compatibility: informative

Consequences of Magic's implementation: material on the other plane that only touches the contact diagonally may be picked as the continuation, and the contact's own image on that plane may then not be visited if it is not adjacent to the picked tile. Hefesto SHALL implement the relation of requirement "Connectivity within one cell"; whether to reproduce this quirk is open (Q-MODEL-17).

(Magic ref: database/DBconnect.c:640-668,348-361)

#### Scenario: Diagonal metal2 next to a via (verify against Magic)
- **GIVEN** `metal1 (0,0)-(8,8)`, `via (5,5)-(8,8)` inside it, and `metal2 (8,8)-(12,12)` touching the via only at its corner
- **WHEN** the flat net containing `(1,1)` is traced
- **THEN** Magic may include `metal2 (8,8)-(12,12)` although it only touches the via at a corner

### Requirement: Hierarchical connectivity

Connectivity across the hierarchy SHALL treat the paint of all descended cells, transformed into root coordinates, as if flattened: pieces in different cells connect under the same rules as within one cell (overlap, or abutment along a positive-length segment, of connecting types on the same plane), and in addition the material under port labels with equal index in the same cell definition SHALL be connected.
Compatibility: behavioral

Hierarchical net copy (used by net selection and net highlighting):
1. Find the starting tiles of the requested types under the start area in all descended cells.
2. For each tile found, clip its root-coordinate rectangle to the bounding area. A tile is ignored when it lies within 1 unit of the search area's boundary in both x and y (`tile.xbot >= S.xtop-1` or `tile.xtop <= S.xbot+1`, and `tile.ybot >= S.ytop-1` or `tile.ytop <= S.ybot+1`), i.e. it only touches the previously found tile diagonally, unless the search area is exactly 1 unit wide or tall.
3. If the destination already contains connected material over the whole clipped rectangle on that plane, it is not processed again; otherwise it is painted into the destination (using the paint table of its type on that plane) and its rectangle expanded by 1 unit (for a triangle only on its two leg sides) is queued with the connect mask of its type.
4. Process queued areas until none remain; for each, search all descended cells for tiles of the connect mask, and (if labels are requested) copy labels attached to that area: labels from the root cell keep their text; labels from subcells get the hierarchical path prefix (or only the root name in "simple" mode); a label is not copied twice; for a port label, the area under one other port label with the same index (and non-space type) in the same cell is queued.
5. Finally recompute the destination bounding box.

When several distinct nets lie under the start area only one is copied. The destination is not cleared first (successive calls accumulate).

(Magic ref: database/DBconnect.c:700-1227, database/DBcellsrch.c:547-700)

#### Scenario: Net through a subcell
- **GIVEN** parent with `metal1 (0,0)-(10,2)` and an instance of a child whose `metal1 (10,0)-(20,2)` abuts it in parent coordinates
- **WHEN** the net is selected starting at `(1,1)` in the parent
- **THEN** the selection SHALL contain `metal1 (0,0)-(20,2)`

#### Scenario: Port equivalence in hierarchy
- **GIVEN** child `pad` with port labels `VDD` index 1 on `metal1 (0,0)-(5,5)` and on `metal1 (50,0)-(55,5)`, unconnected by paint
- **WHEN** a net is selected in the parent starting on the first rectangle
- **THEN** the selection SHALL also contain the second rectangle

### Requirement: Connectivity is computed on demand

Connectivity SHALL be derived from the current geometry at the time of the query; the system SHALL NOT persist net membership in the layout database or files.
Compatibility: informative

Magic marks visited tiles temporarily during a search and clears the marks afterwards (two-pass algorithm); searches must not modify paint while running. Hefesto MAY cache connectivity if it invalidates the cache on every edit.

(Magic ref: database/DBconnect.c:120-135,218-246)

#### Scenario: Edit invalidates connectivity
- **GIVEN** two metal1 rectangles joined by a third
- **WHEN** the joining rectangle is erased and the net is traced again
- **THEN** the result SHALL contain only one of the two rectangles
