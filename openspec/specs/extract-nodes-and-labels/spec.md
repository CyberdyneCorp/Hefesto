# Extract Nodes and Labels

## Purpose

Defines how the flat (single-cell) extractor partitions the paint of one cell into electrical nodes, how each node's reference point and generated name are computed, how labels are attached to nodes, which label names a node, and how node, port, attribute and alias (`equiv`) records are produced. Values attached to nodes (capacitance, resistance, area, perimeter) are specified in `extract-capacitance` and `extract-resistance-lumped`; the substrate node in `extract-substrate`; hierarchical names in `extract-hierarchy`; record syntax in `extract-file-format`.

Terminology: a *tile* is a maximal horizontal rectangle of one type in the corner-stitched plane representation (see capability `layout-geometry-model`); a *split tile* is a non-Manhattan tile holding two types separated by a diagonal. All coordinates are internal units.

## Requirements

### Requirement: Node connectivity

Two pieces of paint SHALL belong to the same node iff they are linked by a chain of the following relations, using the node-connectivity table of the current extract style (global `connect` section plus `connect`/`disconnect` overrides, see `extract-tech-section`):
Compatibility: exact

1. Same-plane adjacency: two areas on the same plane that share an edge of positive length, whose types connect. For split tiles only the side facing the shared edge counts.
2. Contacts: a contact type on plane p connects to the material at the same location on every other plane the contact occupies, when that material's type connects to the contact type.
3. Cross-plane overlap: a non-contact type t whose connectivity includes types on other planes (e.g., a well that connects to substrate diffusion) connects to every connecting tile on those planes that overlaps or touches the tile area expanded by 1 internal unit on all sides.

Only active (electrical) types SHALL form nodes (space and non-electrical types never do, see `extract-tech-section`). Every active-type tile SHALL belong to exactly one node. Disjoint pieces SHALL be distinct nodes even if they carry the same label (see Requirement: Duplicate label warning).

(Magic ref: extract/ExtBasic.c:5443-5915, extract/ExtNghbors.c:74-438)

#### Scenario: Well connects to tap diffusion by touch
- **GIVEN** a tech whose connect section connects `pwell` (plane well) with `psd` (plane active), a pwell rectangle (0,0)-(20,20) and a psd rectangle (20,5)-(24,9) that only abuts the well boundary
- **WHEN** the cell is extracted
- **THEN** the pwell and the psd are one node, because the 1-unit expanded search area of the pwell tile touches the psd

#### Scenario: Same label, unconnected
- **GIVEN** two metal1 rectangles that do not touch, both labeled `n1`
- **WHEN** the cell is extracted
- **THEN** two distinct node records are produced

### Requirement: Node reference point and generated name

Each node SHALL have a reference point: among the node's tiles on the lowest-numbered plane, the lower-left corner of a tile with minimum left x; among tiles with that minimum x, the minimum bottom y. The node also records the type at that point (for a split tile, the type of the side belonging to the node, plus a right-side flag when the node is the right/top side).
Compatibility: exact

When a node has no naming label, its name SHALL be generated as `<P>_<X>_<Y>#`, where P is the short name of the reference plane, X and Y are the absolute decimal values of the reference coordinates each prefixed with `n` when negative, and the suffix is `r#` instead of `#` when the reference side is the right side of a split tile. The substrate node without tiles uses the substrate name rule in `extract-substrate`.

(Magic ref: extract/ExtBasic.c:4908-4970, extract/ExtBasic.c:942-985)

#### Scenario: Generated name with negative coordinates
- **GIVEN** an unlabeled metal1 node (plane short name `metal1`) whose tiles have lower-left corners (−10,4) and (−10,−6) and (0,−20)
- **WHEN** the cell is extracted
- **THEN** the node name is `metal1_n10_n6#`

#### Scenario: Lowest plane wins
- **GIVEN** an unlabeled node with poly at (5,5) on plane 3 and metal1 at (−100,0) on plane 4 joined by a contact
- **WHEN** the cell is extracted
- **THEN** the reference point is (5,5) on the poly plane

### Requirement: Label attachment

Labels of the cell SHALL be processed in the cell's label list order (the order in the `.mag` file, newly created labels appended). A label SHALL be skipped if its type is space or not on a paint plane, or if it was marked as a non-exported alias by the previous node-output step (Requirement: Alias output). For each other label, with C = ((xbot+xtop) >> 1, (ybot+ytop) >> 1) (arithmetic shift, i.e. floor division by 2), the extractor SHALL probe the points C, C+(0,−1), C+(−1,−1), C+(−1,0) in that order on the label type's plane; at the first point whose containing tile's (left-side) type connects to the label type and belongs to a node, the label SHALL be attached to that node.
Compatibility: exact

Insertion order into the node's label list: a port label (any port direction flag set) SHALL be inserted at the head; a non-port label SHALL be inserted at the head only if the list is empty, otherwise immediately after the maximal run of port labels at the list head, or—when the head is not a port—immediately after the head element.

A label that attaches to no node SHALL be handled as follows: if it lies on the substrate plane, its type is a substrate type and no probe point had a connecting type, it SHALL be attached to the substrate node (prepended); otherwise, if its type is active and it touches or lies inside the extraction area, a new node SHALL be created for it ("sticky label" node) with the label's lower-left corner as reference point, the label type, zero capacitance, resistance, area and perimeter.

(Magic ref: extract/ExtRegion.c:180-360, database/DBlabel.c:216-224)

#### Scenario: Label on boundary of two shapes
- **GIVEN** a metal1 label rectangle (10,10)-(10,10) where metal1 node A occupies (0,0)-(10,20) and node B occupies (10,0)-(20,20)
- **WHEN** the cell is extracted
- **THEN** the probe C=(10,10) lies in B's tile, so the label attaches to B

#### Scenario: Floating label becomes a node
- **GIVEN** a metal1 label `orphan` over empty space
- **WHEN** the cell is extracted
- **THEN** a node record `node "orphan" 0 0 <x> <y> metal1 ...` with all area/perimeter values 0 is produced

### Requirement: Label types by trailing character

A label's role SHALL be determined by its last character: `@` node attribute, `$` terminal (source/drain) attribute, `^` gate attribute, anything else a node name. An empty text is none of these.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:4689-4709)

#### Scenario: Attribute is not a name
- **GIVEN** a node with labels `x@` and `vdd`
- **WHEN** the node name is chosen
- **THEN** the name is `vdd` and `x` is written as an attribute

### Requirement: Node name selection

The name of a node SHALL be the text of the first label in its label list (Requirement: Label attachment) whose role is "node name"; if there is none, the generated name (Requirement: Node reference point and generated name).
Compatibility: exact

Consequences that SHALL hold: (a) if any port label with a name role is attached, the port label that appears last in the cell label list wins; (b) with no ports, the first attached name label wins. With `magic-compat` ON (the default) there is one exception, as in Magic: if the first attached label was an attribute, each later non-port label is inserted after it, so the most recently attached name label wins. With `magic-compat` OFF non-port labels keep attachment order and the first attached name label always wins (Q-EXTRACT-4, resolution: compat). Port precedence is the same in both modes.

(Magic ref: extract/ExtBasic.c:993-1012, extract/ExtRegion.c:315-331, extract/ExtRegion.c:489-503)

#### Scenario: Port beats internal label
- **GIVEN** a node with labels in cell order `int1` (non-port), `A` (port 1), `B` (port 2)
- **WHEN** the cell is extracted
- **THEN** the node name is `B`
- **AND** `equiv "B" "A"` is written (A is a port)

#### Scenario: First name label wins
- **GIVEN** a node with non-port labels in cell order `alpha`, `beta`
- **WHEN** the cell is extracted
- **THEN** the node name is `alpha`

#### Scenario: Attribute attached before two name labels
- **GIVEN** a node whose non-port labels are attached in the order `cap=1f@`, `alpha`, `beta`
- **WHEN** the cell is extracted with `magic-compat` ON
- **THEN** the node name is `beta`
- **AND** with `magic-compat` OFF the node name is `alpha`

### Requirement: Port records

For every node and every port label attached to it, in node output order and label list order, a record `port "<text>" <portnum> <xbot> <ybot> <xtop> <ytop> <typeShortName>` SHALL be written before any `node` record. If the port text equals the node's chosen name, the node's reference point and type SHALL be replaced by the label's lower-left corner, the label type and that type's plane.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:697-726)

#### Scenario: Port relocates node reference
- **GIVEN** a metal1 node whose tiles start at (0,0) and a port label `OUT` (port 3) at (40,2)-(42,4)
- **WHEN** the cell is extracted
- **THEN** `port "OUT" 3 40 2 42 4 metal1` is written and the node record reads `node "OUT" ... 40 2 metal1 ...`

### Requirement: Node and attribute records

For each node in node-list order the extractor SHALL write `node "<name>" <R> <C> <x> <y> <type> <a0> <p0> ... <aN-1> <pN-1>` (or the `substrate` form for the substrate node, see `extract-substrate`), where R and C are defined in `extract-resistance-lumped` and `extract-capacitance`, (x,y,type) is the reference point and type short name, and (ai,pi) are the node's area and perimeter in resistance class i. It SHALL follow it immediately with one `attr <name> <xbot> <ybot> <xtop> <ytop> <typeShortName> "<text-without-last-char>"` line per attached node-attribute label, in label list order (note: the node name is not quoted in `attr`).
Compatibility: exact

The node list order SHALL be: the reverse of node discovery order, where discovery first finds the substrate node and then scans planes in increasing plane number and, within each plane, tiles in the plane's area-enumeration order; sticky-label nodes are prepended as they are created. See Q-EXTRACT-5 for portability of this order.

(Magic ref: extract/ExtBasic.c:728-768, extract/ExtBasic.c:5057-5200)

#### Scenario: Attribute record
- **GIVEN** node `n5` with an attached label `cap=10f@` at (2,2)-(2,2) of type metal1
- **WHEN** the cell is extracted
- **THEN** the line `attr n5 2 2 2 2 metal1 "cap=10f"` follows the node record

### Requirement: Alias output

After each node record, the extractor SHALL locate the label whose text is the chosen name in the node's label list, and for each later label L in the list with a name role and text different from the chosen name: if L is a port label or the `aliases` option is on, write `equiv "<name>" "<L>"`, except when L's text equals the first port name seen for this node or equals the text of the immediately preceding written alias; otherwise mark L as a non-exported alias, so it SHALL NOT be used for hierarchical merges or coupling names and SHALL be skipped by later label attachment in the same extraction.
Compatibility: exact

If the chosen name is a port, or once a port alias is written, a later port label with a different text SHALL produce the warning `Warning:  Ports "<a>" and "<b>" are electrically shorted.` unless the two names are equal after truncating each at its first `_uq`, or it is suppressed (informative message). Suppression depends on the `magic-compat` setting (Q-EXTRACT-22, resolution: compat): with it ON (the default) the warning is suppressed whenever lumped resistance extraction is on, as in Magic, so it never prints in the default configuration; with it OFF it is suppressed only when full R-C extraction (`extresist`) runs in the same pass. The `.ext` output is the same in both modes. Non-exported alias marks SHALL be cleared at the start of every cell extraction.

(Magic ref: extract/ExtBasic.c:770-860, extract/ExtCell.c:520-523, extract/ExtRegion.c:197-201)

#### Scenario: Aliases off
- **GIVEN** `extract no aliases` and a node with non-port labels `a` then `b`
- **WHEN** the cell is extracted
- **THEN** the node is named `a` and no `equiv` record is written

#### Scenario: Aliases on
- **GIVEN** `extract do aliases` and the same node
- **WHEN** the cell is extracted
- **THEN** `equiv "a" "b"` is written

### Requirement: Duplicate label warning

When the `dup` warning is enabled (default) and the cell is not an abstract view (`LEFview` property true), each name-role label text attached to more than one node SHALL produce one feedback entry per occurrence, `Label "<text>" attached to more than one unconnected node: <generatedName>`, at the label's lower-left corner expanded by 1, and SHALL increment the warning count; extraction output SHALL be unaffected.
Compatibility: behavioral (warning count and feedback); informative (text)

(Magic ref: extract/ExtBasic.c:1029-1105, extract/ExtBasic.c:333-335)

#### Scenario: Duplicate warning count
- **GIVEN** labels `x` on two unconnected nodes
- **WHEN** the cell is extracted
- **THEN** two feedback entries are created and the cell reports `2 warnings`

### Requirement: Unique naming of duplicate labels

Making labels unique (command `extract unique`, permanent, or option `do unique`, temporary; see `extract-commands`) SHALL, for each cell: collect all name-role label texts of the cell; find nodes (and sticky-label nodes) with their labels; then, iterating nodes in node-list order and labels in list order, when a label text T was first seen on a different node R1 and the current node is R2: if the current label is not a port and R1 carries a port label with text T, keep R1 and rename T on every other node; otherwise keep R2 and rename T on every other node, and mark T processed. Renaming SHALL replace each occurrence on a renamed node by `T_uqK`, with K starting at 0, incremented while `T_uqK` is an existing label text of the cell, and incremented once after each renamed node. A renamed port label SHALL receive port number = 1 + the maximum port number in the cell; non-ports get 0.
Compatibility: exact

Mode rules: mode `all` and the temporary mode SHALL rename every duplicated text including global names; modes `noports`, `notopports` and temporary-noports SHALL rename only non-port labels; otherwise texts ending in `#` SHALL be renamed, texts ending in `!` SHALL be left alone silently, and other duplicates SHALL only be warned as `Non-global label "<T>" attached to more than one unconnected node: <name>` (ports silently skipped in `noports`/`notopports`). Abstract views (`LEFview`) SHALL be skipped. Temporarily renamed labels SHALL be flagged and, when reverted, truncated at their last `_`, and a renamed port SHALL take back the port number of another label with the restored text.

Which node keeps the name is Magic's choice in both `magic-compat` settings (Q-EXTRACT-23, resolution: replicate): the node visited later keeps it, except that a port on the earlier node wins over a non-port. The result depends on the node-list order, which is reproduced exactly (Requirement: Node and attribute records; Q-EXTRACT-5).

After every extraction run with the temporary mode, the temporary labels SHALL be reverted in every cell that was renamed, the top cell of `extract`/`extract cell` included, whether or not the cell has isolated substrate regions, and the cell's modified flag SHALL be restored if the renaming was its only change, in both `magic-compat` settings (Q-EXTRACT-6, resolution: fix). Magic leaves the `_uqN` labels in some cells, and a later save writes them to the `.mag` file; the revert does not change `.ext` content.

(Magic ref: extract/ExtUnique.c:83-408, extract/ExtUnique.c:155-188, extract/ExtMain.c:842-848, extract/ExtMain.c:1108-1115)

#### Scenario: Temporary unique
- **GIVEN** `extract do unique` and two unconnected metal1 shapes both labeled `net1`, neither a port
- **WHEN** the cell is extracted
- **THEN** the `.ext` file contains one node named `net1` and one named `net1_uq0`
- **AND** after extraction both labels read `net1` again in the layout, and the cell is not left modified

#### Scenario: Temporary unique labels reverted without isolated substrate
- **GIVEN** `extract do unique`, a style with no default substrate type, and a top cell with two unconnected metal1 shapes both labeled `net1`
- **WHEN** `extract cell` runs on the top cell and the cell is then saved
- **THEN** the `.ext` file contains the nodes `net1` and `net1_uq0`
- **AND** both labels read `net1` in the layout and in the saved `.mag` file, in both `magic-compat` settings

#### Scenario: Global name preserved by tagged mode
- **GIVEN** two unconnected shapes labeled `vdd!` and command `extract unique #`
- **WHEN** the command runs
- **THEN** neither label is changed and no warning is issued
