# Extract Hierarchy

## Purpose

Defines hierarchical circuit extraction: the order in which cells are extracted, how interactions between a cell's own paint and its subcells (and between subcells, and between elements of arrays) are found in step-size squares, how connectivity across the hierarchy is recorded as `merge` records, how capacitance, area and perimeter that would be double counted are corrected (pass 2 adjustments), how coupling between hierarchy levels is written as `cap` records, and how hierarchical node names are formed. The flat models used inside each interaction area are those of `extract-nodes-and-labels`, `extract-capacitance` and `extract-resistance-lumped`; substrate merging is in `extract-substrate`; command entry points in `extract-commands`.

## Requirements

### Requirement: Bottom-up extraction order

Extracting a cell tree SHALL extract each reachable cell definition exactly once, children before parents, in depth-first post-order of the subcell enumeration starting at the root. Internal (yank-buffer) cells and cells marked "don't use" SHALL be skipped. A cell whose children are all skipped and that contains no paint other than space SHALL be marked "don't use" and skipped (it produces no `.ext` file and no `use` record in its parents). If nothing remains, `Warning:  There is nothing here to extract.` SHALL be reported.
Compatibility: behavioral (order); exact (which cells are written and `use` records omitted)

(Magic ref: extract/ExtMain.c:185-249, extract/ExtMain.c:300-345, extract/ExtCell.c:670-675)

#### Scenario: Empty child
- **GIVEN** a top cell with two uses of a cell `empty` that contains only labels and no paint
- **WHEN** `extract all` runs
- **THEN** no `empty.ext` is written and the top `.ext` has no `use empty ...` record

### Requirement: Interaction halo and step squares

For each extracted cell, the hierarchical halo SHALL be `h = sidehalo + 1` when options `coupling` and `adjust` are both on, else `h = 1`. The cell's bounding box SHALL be cut into squares of the style step size `S` starting at its lower-left corner (`[xbot + iS, xbot + (i+1)S) × [ybot + jS, ...)`, rows from bottom to top, squares left to right; the last row/column extends past the bounding box). For each square r, with `rb` = r expanded by h:
Compatibility: exact

1. If no subcell overlaps `rb` expanded by h, nothing is done for r.
2. Otherwise the interaction area I SHALL be computed (Requirement: Interaction area). Every label of the cell that overlaps or touches r SHALL extend I by the label rectangle clipped to `rb` (when I was empty it becomes that rectangle, widened by one unit on each side in any zero-width dimension).
3. If I is non-empty, the interaction SHALL be processed with clip area `C = I ∩ r` (Requirement: Interaction processing). Otherwise the substrate of every subcell overlapping r SHALL still be connected (see `extract-substrate`).

(Magic ref: extract/ExtSubtree.c:120-284)

#### Scenario: Halo without coupling
- **GIVEN** `sidehalo 4` and `extract no coupling`
- **WHEN** a cell is extracted
- **THEN** interactions are searched with halo 1

### Requirement: Interaction area

Within search area `A = rb` expanded by h, the interaction area I SHALL be the union (bounding box) over every subcell use U overlapping A of: the bounding box of the cell's own non-space paint within `bbox(U)` expanded by h (clipped to A), itself expanded by h and clipped to that halo area; and, for each U whose halo area is not already covered by its own paint box, the bounding box of the other subcell uses overlapping U's halo area, expanded by h and clipped to that halo area. I SHALL be discarded unless it contains non-space paint of the cell itself together with paint of at least one subcell, or paint of at least two different subcells; finally I SHALL be clipped to `rb`.
Compatibility: exact

(Magic ref: drc/DRCsubcell.c:249-300, drc/DRCsubcell.c:405-520)

#### Scenario: Parent wire crossing a child
- **GIVEN** a parent metal1 wire crossing a child instance that contains metal1 in the crossed area
- **WHEN** the parent is extracted
- **THEN** an interaction area covering the crossing (plus halo) is processed

### Requirement: Hierarchical label names

When a subcell's paint is copied (flattened) for an interaction, each of its name-role labels, from every depth, whose type is not space SHALL be copied with its text prefixed by the instance path: for each level, the use identifier followed by `[x]` (1-D array in x), `[y]` (1-D array in y) or `[y,x]` (2-D array) for array uses, then `/`. Coordinates SHALL be transformed to the parent. Generated node names of subcells (no label) SHALL be referenced by the same prefix plus the child's generated name (e.g., `I1/metal1_10_4#`).
Compatibility: exact

(Magic ref: extract/ExtYank.c:113-246, database/DBcellbox.c:62-107)

#### Scenario: Two-level name
- **GIVEN** parent uses `X1` of cell `mid`, which uses a 1×4 array `R` (indices y = 0..3) of cell `bit` containing label `q`
- **WHEN** the parent is extracted
- **THEN** the node of `q` in element y=2 is referred to as `X1/R[2]/q`

### Requirement: Interaction processing and connections (pass 1)

For an interaction with area I and clip area C: the parent's paint in I SHALL form the first subtree (names from the parent cell). Then each subcell use overlapping I, in subcell search order, SHALL be flattened over `bbox(use) ∩ I` (all array elements) into a new subtree, its nodes computed within C, and connected against the *cumulative* area (parent paint plus all previously processed subtrees): for every tile of the new subtree within its area and every tile of the cumulative area on any plane that overlaps or touches it (the tile expanded by 1; on the same plane any active type, on other planes only types connecting to it): if the types connect, the two node names SHALL be unified; if they do not connect and the overlap has positive area, the error `Illegal overlap between <t1> and <t2> (types do not connect)` SHALL be reported as feedback and counted. After connection, the subtree's paint and labels SHALL be added to the cumulative area.
Compatibility: exact

Node names in this step: a cumulative-area tile SHALL be named by the first name-role label of its node if any; otherwise by the name of the parent-cell node at that tile's location (any parent node there); otherwise "the hard way" (Requirement: Hard-way node names). Names equal to `(none)` SHALL be ignored.

(Magic ref: extract/ExtSubtree.c:441-718, extract/ExtHier.c:397-860, extract/ExtSubtree.c:762-880)

#### Scenario: Overlap of non-connecting types
- **GIVEN** a parent `poly` rectangle overlapping a child `metal1` rectangle on the same plane under a tech where poly and metal1 share a plane but do not connect
- **WHEN** the parent is extracted
- **THEN** the error `Illegal overlap between poly and metal1 (types do not connect)` is reported

### Requirement: Name unification order

Unification SHALL maintain, for each hierarchical node, an ordered name list and a size (number of names). When unifying node N1 (from the cumulative side) with node N2 (from the new subtree), if `size(N1) < size(N2)` the names of N1 SHALL be inserted after the first name of N2; otherwise the names of N2 SHALL be placed before the names of N1. Names not yet seen SHALL create a node of size 1.
Compatibility: exact

(Magic ref: extract/ExtHier.c:740-845, extract/ExtHier.c:1210-1240)

#### Scenario: Child name first
- **GIVEN** a parent wire labeled `b` overlapping child `c1`'s wire labeled `a`
- **WHEN** the parent is extracted
- **THEN** the merge lists the names in the order `c1/a`, `b`

### Requirement: Merge records and pass-2 adjustments

At the end of a cell's subcell (and separately array) processing, for every hierarchical node with names n0..nk (k ≥ 1), the extractor SHALL write `merge "n0" "n1" <C> <a0> <p0> ... <aN-1> <pN-1>` followed by `merge "n(i)" "n(i+1)"` for i = 1..k−1 (no values). C (aF divided by `cscale`, `%lg`, may be negative) and the area/perimeter values SHALL be the node's accumulated adjustments; they are 0 when option `adjust` is off.
Compatibility: exact (values and name pairs); behavioral (record order, see Q-EXTRACT-7)

When `adjust` is on, for each interaction the cumulative area SHALL be re-extracted within C (nodes, labels, and coupling when `coupling` is on); for each cumulative node whose name is a hierarchical node, its capacitance and per-class area and perimeter SHALL be added to that node; then for each subtree (parent included) and each of its nodes that maps (by its reference point) to a cumulative node with a name in the table, the subtree node's values SHALL be subtracted. Thus the adjustment equals (value of the merged geometry) − (sum of values of the separately extracted pieces), restricted to C and summed over all interactions.

(Magic ref: extract/ExtSubtree.c:514-580, extract/ExtSubtree.c:582-612, extract/ExtHier.c:1014-1180)

#### Scenario: Identical overlapping wires (verify against Magic)
- **GIVEN** `areacap metal1 30`, `perimc metal1 space 20`, `cscale 1`, one resistance class for metal1, a parent metal1 (0,0)-(10,4) labeled `b` and a child `c1` with metal1 at the same place labeled `a`, coupling and adjust on
- **WHEN** the parent is extracted
- **THEN** the parent `.ext` contains `merge "c1/a" "b" -1760 -40 -28`

### Requirement: Hierarchical coupling records

When options `adjust` and `coupling` are on, after the node adjustments of each interaction, the coupling of the cumulative area within C minus the coupling of each subtree (mapped through node reference points; pairs that map to a single node or to no node ignored) SHALL be written immediately as `cap "<name1>" "<name2>" <V>` records (V = value/`cscale`, `%lg`, non-zero only), naming nodes as in pass 1 with hard-way lookup. Coupling between two pieces of the same hierarchical node SHALL therefore not appear. The same pair MAY appear in several records (one per interaction square); readers SHALL sum them.
Compatibility: exact (sum per unordered pair); behavioral (split across records)

(Magic ref: extract/ExtSubtree.c:582-612, extract/ExtHier.c:1030-1050)

#### Scenario: Parent wire over child wire
- **GIVEN** a parent metal2 wire crossing a child metal1 wire of a different net with an `overlap metal2 metal1` rule
- **WHEN** the parent is extracted
- **THEN** the parent `.ext` contains a `cap` record between the parent wire's node and the child node `<use>/<name>` with the overlap value

### Requirement: Hard-way node names

When a cumulative or subtree node has no label and no parent node exists at its location, its name SHALL be found by searching, in the subtrees processed so far for this interaction (or in the current subtree's use when naming its own nodes), the subcell paint under the tile on the tile's plane (types connecting to the tile type): first for a node with a name-role label in the deepest cell where one is found, producing `<path>/<label>`; failing that, for any node, producing `<path>/<generated name of that node in its cell>`. The found name SHALL be attached to the node. When the `labels` warning is on, `Warning: node labels should be inside overlap area` SHALL be added as feedback. If no name can be found, `Cannot find the name of this node (probable extractor error)` SHALL be reported, counted as an error, and the name `(none)` used (and then ignored for merges).
Compatibility: exact (names); informative (messages)

(Magic ref: extract/ExtSubtree.c:880-1050, extract/ExtHard.c:85-330)

#### Scenario: Unlabeled child node
- **GIVEN** an unlabeled child metal1 node whose reference point in the child is (10,4) on plane `metal1`, instance `I1`, touched by an unlabeled parent wire
- **WHEN** the parent is extracted
- **THEN** a merge pairs `I1/metal1_10_4#` with the parent wire's name

### Requirement: Array interactions

For each arrayed use (more than one element), with element pitch (`xsep`, `ysep`) and size (`xsize`, `ysize`) in parent coordinates (a non-arrayed dimension uses pitch = def size + 2), three interaction areas relative to the array's bounding box lower-left (xb, yb) and top yt SHALL be processed against the *primary* element:
Compatibility: exact

- Area A (if ysep ≤ ysize): `[xb, xb+xsize+h] × [yb+ysep−h, yb+ysize+h]`, primary = lower-left element, partners = elements above (same x index, or diagonal) as permitted by orientation;
- Area B (if xsep ≤ xsize): `[xb+xsep−h, xb+xsize+h] × [yb, yb+ysize+h]`, primary = lower-left element, partners = elements to the right in the same row;
- Area C (if both): `[xb+xsep−h, xb+xsize+h] × [yt−ysize−h, yt−ysep+h]`, primary = upper-left element, partners = diagonal elements below-right.

For each partner, connections, substrate connections and (with `adjust`) adjustments and coupling SHALL be computed between primary and partner as for subcells, with names formed as `<useid>[<range>]/<name>` where each array dimension present is written `lo:hi` (2-D as `[ylo:yhi,xlo:xhi]`); for the primary element the range runs from its index to the far end minus the partner offset, for the partner from its index to the far end. Array merges and couplings SHALL be written after all subcell interactions of the cell.
Compatibility: exact

(Magic ref: extract/ExtArray.c:120-330, extract/ExtArray.c:480-600)

#### Scenario: Abutting 1-D array
- **GIVEN** a 1×4 array `A` (x indices 0..3) of a cell whose metal1 node `n` touches the next element's `n`
- **WHEN** the parent is extracted
- **THEN** the record `merge "A[1:3]/n" "A[0:2]/n" ...` (names in unification order) represents all adjacent pairs at once

### Requirement: Sticky labels across the hierarchy

When option `labelcheck` is on: parent labels with the sticky flag that lie in the interaction area over no paint connecting to their type SHALL be copied into the cumulative area on the first subtree; parent sticky labels touching a subtree tile of a connecting type SHALL unify the label text with that tile's node; and subtree sticky labels touching cumulative tiles of connecting types SHALL unify their text with those tiles' nodes.
Compatibility: exact

See Q-EXTRACT-26.

(Magic ref: extract/ExtSubtree.c:615-668, extract/ExtHier.c:330-460, extract/ExtHier.c:418-447)

#### Scenario: Sticky pin over child metal
- **GIVEN** `extract do labelcheck` and a sticky parent label `VDD` over empty parent area above child metal1 named `c/vdd`
- **WHEN** the parent is extracted
- **THEN** a merge between `VDD` and `c/vdd` is written

### Requirement: Hierarchical section order in the file

After the flat records of the cell (see `extract-file-format`), the file SHALL contain, in order: subcell-interaction `cap` records (in interaction order), subcell `merge` records, array `cap` records, array `merge` records, and, for the root cell only with option `length`, `distance` records (see `extract-commands`).
Compatibility: exact

(Magic ref: extract/ExtCell.c:534-547, extract/ExtSubtree.c:286-289, extract/ExtArray.c:80-118)

#### Scenario: Caps before merges
- **GIVEN** a parent with both a crossing coupling and a connection to a child
- **WHEN** the parent is extracted
- **THEN** the hierarchical `cap` record precedes the `merge` record
