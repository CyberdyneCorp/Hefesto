# Plowing (Stretching and Compaction)

## Purpose

Defines Magic's plowing facility: the `plow` and `straighten` commands, the `plowing` technology section, the plowing rule tables derived from the DRC section, the edge-queue algorithm and its search rules (umbra, penumbra, slivers, illegal edges, covered/fixed/drag/contact types, cells), jog insertion horizon, jog straightening, plow boundaries, and known limitations. Plowing results are layout geometry; for the same input, technology and parameters Hefesto must produce the same geometry.

Terminology: plowing is always described in a canonical frame where the plow moves to the right (+x). Plowing in another direction rotates the layout into this frame (north: 90°, south: 270°, west: 180°), plows, and rotates back. An *edge* is a maximal vertical boundary segment on one plane between a left type (LHS) and a right type (RHS); it has an original x, a final x (`newx`), and a y-range.

## Requirements

### Requirement: plowing technology section
The `plowing` section SHALL contain lines of exactly two fields:

```ebnf
plowing_section = "plowing" , NL , { plow_stmt , NL } , "end" ;
plow_stmt       = ( "fixed" | "covered" | "drag" ) , type_list ;
```

`fixed` types keep their width in the plow direction (e.g. transistors); `covered` types must stay covered by the material attached to their right corners (e.g. a gate must stay covered); `drag` types drag along trailing minimum-width material. Space is removed from every list. After the section, every contact type is automatically added to the fixed set. Errors: wrong field count `Malformed line`, unknown keyword `Illegal keyword "%s".`. Defaults: all three sets empty.

Example (scmos): `fixed allFet,glass,pad` / `covered allFet` / `drag allFet`.

Compatibility: exact
(Magic ref: plow/PlowTech.c:520-600, scmos/scmos.tech.in plowing section)

#### Scenario: Contacts are fixed
- **GIVEN** a technology whose plowing section lists only `fixed allFet`
- **WHEN** the technology is loaded
- **THEN** every contact type (e.g. m2contact) is also treated as fixed-width

### Requirement: Plowing rules derived from DRC
While the DRC section is read, plowing SHALL derive width and spacing rule tables indexed by (LHS type, RHS type), from `width`, `spacing`, `edge` and `edge4way` statements only (all other DRC statements are ignored by plowing):

- `width types d`: for every LHS type i on the plane not in `types` and RHS type j in `types` (same plane): a width rule with distance d, ok-types = `types`.
- `spacing types1 types2 d touching_ok`: only if both sets lie on the same plane; for LHS i in types1 and RHS j in (neither set), a spacing rule with distance d whose ok-types are the plane's types not in types2; also the reverse (types2 → types1) unless the sets are equal.
- `spacing types1 types2 d touching_illegal`: as above with RHS in the complement of types1 (reverse: complement of types2); rules are placed on every plane of the other set.
- `edge types1 types2 d ok corner cdist [plane]`: distance = max(d, cdist); for i in types1, j in types2 a spacing rule (ok-types = ok, checked on the given plane or the corner types' plane); if types2 differs from the corner set an extra "penumbra only" rule for j in the corner set. `edge4way` additionally generates backward approximations: for each t not in `ok` (on the check plane) and j in `ok`, a rule with LHS t and ok-types = complement of types1.
- After reading, rules in each bin are reduced: a rule is removed when another rule in the same bin has distance >= its distance, the same flags, plane and LHS set, and ok-types that are a subset of its ok-types. The per-type maximum rule distance is recorded.
- When the grid is rescaled by n/d, rule distances are scaled with remainder tracking so that repeated scaling is reversible (distance rounded up when not exact).

Compatibility: exact
(Magic ref: plow/PlowTech.c:90-470, plow/PlowTech.c:600-700)

#### Scenario: Width rule bin
- **GIVEN** the DRC rule `width metal1 3`
- **WHEN** plowing rules are derived
- **THEN** the bin (space, metal1) contains a width rule of distance 3 whose ok-types are {metal1}

### Requirement: plow command
The `plow` command SHALL take a first argument matched case-insensitively as a unique prefix of: `boundary`, `help`, `horizon`, `jogs`, `selection`, `straighten`, `noboundary`, `nojogs`, `nostraighten`; if it matches none it is parsed as a Manhattan direction name (`north`, `south`, `east`, `west`, `up`, `down`, `left`, `right` and their accepted abbreviations), transformed from root to edit-cell orientation, meaning "plow in that direction". Behaviour:

| Form | Effect / output |
|---|---|
| `plow` *dir* `[layers]` | Plow the box (see "Box plow"); `layers` default `*,l,subcell,space`. Prints `Reduced plow size to stay within the boundary.` if a boundary limited it and moves the box accordingly. |
| `plow boundary` | Set the plow boundary to the box (one boundary per edit cell at a time). |
| `plow noboundary` | Remove the boundary. |
| `plow horizon [n]` | Set (if n given) and print the jog horizon: `Jog horizon set to %d units.` or `Jog horizon set to infinity.` |
| `plow jogs` | Horizon = 0, prints `Jog insertion re-enabled (horizon 0).` |
| `plow nojogs` | Horizon = infinity, prints `Jog insertion disabled.` |
| `plow straighten` / `nostraighten` | Enable/disable automatic straightening after each plow; prints `Jogs will be straightened after each plow.` / `Jogs will not be straightened automatically.` |
| `plow selection [dir [amount]]` | Plow the selection (see "Selection plow"). |
| `plow help` | Lists the options and `Option may also be any Manhattan direction, which` / `causes the plow to be moved in that direction.` |

Defaults: horizon 0, straightening off, no boundary. Errors: ambiguous option `Ambiguous plowing option: "%s"`; invalid `"%s" isn't a valid plow option.` + `  Type "plow help" for help.`; wrong argument count `Wrong number of arguments to %s option.`; no window `Point to a window first`; no edit cell `There is no edit cell!`.

Compatibility: behavioral
(Magic ref: plow/PlowCmd.c:40-330, plow/PlowMain.c:40-60)

#### Scenario: Disable jogs
- **WHEN** `plow nojogs` is executed
- **THEN** `Jog insertion disabled.` is printed and subsequent plows insert no new jogs

#### Scenario: Ambiguous option
- **WHEN** `plow no` is executed
- **THEN** `Ambiguous plowing option: "no"` is reported

### Requirement: Box plow
Plowing the box in direction D SHALL treat the box side opposite to D as the plow and sweep it to the side in direction D. In the canonical frame the plow is the box's left edge moving to its right edge; distance = box width. If the box has zero extent in the plow direction nothing happens. Initial edges: on every plane, the edges found by shadow-searching the box area extended 1 unit to the left, treating every type NOT in the layer set as transparent (so the edges found are those whose RHS type is one of the plowed layers, `space` included by default); each gets `newx = box.xtop`. Subcells (if `subcell` is in the layer set) overlapping the plow: a cell whose left side is left of the plow and whose right side reaches the plow's far side is not moved; a cell straddling the plow is dragged by its front (`move = box.xtop - cell.xtop`); otherwise pushed (`move = box.xtop - cell.xbot`). No edge ever moves farther than the plow distance.

Compatibility: exact
(Magic ref: plow/PlowMain.c:491-556, plow/PlowMain.c:660-800, plow/PlowMain.c:1425-1520, plow/PlowQueue.c:120-140)

#### Scenario: Plow right across a wire
- **GIVEN** a metal1 rectangle (10,0)-(13,20) in an otherwise empty cell, DRC `width metal1 3`, and the box (5,0)-(20,20)
- **WHEN** `plow right` is executed
- **THEN** the metal1 rectangle is pushed to (20,0)-(23,20)

### Requirement: Edge queue processing
Edges SHALL be processed from a queue ordered by original x (leftmost first, ties: lowest plane index, then within a bin the most recently queued first), merged per plane and x so that queued edges are disjoint in y: a newly queued edge overlapping an existing one with the same LHS/RHS types is split so each y-interval keeps the maximum `newx`, and vertically adjacent pieces with equal type and `newx` are merged. Processing an edge:
1. A cell edge moves its cell by `newx - x` if larger than the cell's previous move, then applies the cell rule (search the cell area plus the DRC halo for geometry and cells that must move with it).
2. A paint edge whose tiles have all already reached `newx` is skipped.
3. If the jog horizon h > 0, the edge is first extended up and down along its LHS outline to the nearest jog within h (stopping where the RHS type changes), queuing the extensions.
4. Search rules are applied (next requirement) and the edge's tiles are moved: tiles left of the edge are stretched and tiles right of it are shrunk so the boundary lies at `newx`, splitting tiles at the edge's top and bottom and re-merging.
The whole operation is computed on a copy of the affected area plus a halo of the DRC maximum distance (grown as needed), then the changed area of the edit cell is erased and repainted from the copy, labels attached to moved paint are moved, cells are moved, the cell is marked modified, and DRC is re-run over the changed area. If straightening is enabled, the changed area is then straightened in the same direction.

Compatibility: exact
(Magic ref: plow/PlowQueue.c:140-470, plow/PlowMain.c:558-660, plow/PlowMain.c:1521-1650, plow/PlowMain.c:1832-2040)

#### Scenario: Already moved edge skipped
- **GIVEN** an edge queued twice with newx 20 and 18
- **WHEN** the queue is processed
- **THEN** the edge moves once to x = 20

### Requirement: Plow search rules
For each moving edge E (LHS l, RHS r, from x to newx), the following rules SHALL be applied in this order, each only when (l, r) belong to the rule's type sets; "shadow search" of an area finds the first edges to the right whose RHS is not in the given transparent set; an impacted edge F is queued with `newx(F) = max(newx(F), newx(E) + min(d, x(F) - x(E)))` where d is the rule distance (0 if none):

1. Clear umbra: shadow-search E's swept area with only r transparent (edges cannot cross).
2. Umbra width (real width) and umbra spacing: for each width rule (distance replaced by the actual width of the material if larger) and each spacing rule, shadow-search the swept area extended to `newx + d` with the rule's ok-types transparent.
3. Top/bottom penumbra (width, spacing): search above/below E within d, following the outline of the rule's LHS types, out to `newx + d`.
4. Fixed-RHS penumbra when no spacing rule applies across E.
5. Top/bottom slivers (min-width and spacing rules): move material that would otherwise leave a sliver narrower than the rule.
6. Inside slivers when the plow height is less than the DRC halo (types not fixed).
7. Top/bottom illegal edges: the LHS sliding right must not form illegal edges with material above/below.
8. Covered types: drag the material at the covered LHS tile's top-right and bottom-right corners.
9. Fixed LHS / fixed RHS: the opposite edge of a fixed-width tile moves by the same amount (all fixed tiles touching along top/bottom move together).
10. Fixed RHS dragging stubs: when RHS is fixed and LHS is neither fixed nor space nor drag type, drag LHS material of minimum width or less.
11. Contact LHS/RHS: couple the contact's edge on every plane the contact connects.
12. Find cells within a DRC halo of the swept area and move them.

Compatibility: exact
(Magic ref: plow/PlowMain.c:2045-2200, plow/PlowRules1.c, plow/PlowRules2.c, plow/PlowRules3.c, plow/PlowSearch.c, plow/PlowWidth.c)

#### Scenario: Spacing preserved, not increased
- **GIVEN** DRC `spacing metal1 metal1 3 touching_ok`, metal1 boxes A=(0,0)-(3,10) and B=(5,0)-(8,10) (existing separation 2), and A's right edge moves from x=3 to x=10
- **WHEN** the umbra spacing rule is applied
- **THEN** B's left edge is queued with `newx = 10 + min(3, 5 - 3) = 12`

### Requirement: Selection plow
`plow selection` SHALL plow the selected paint and cells of the edit cell as if each selected piece were itself a plow, in direction `dir` by `amount` (default 1 unit) when given; without arguments the displacement is from the box lower-left to the cursor point, using the larger-magnitude axis (ties choose the x axis: `|dy| <= |dx|`) and its sign. Requirements and errors: the box must be in a window containing the selection (`"plow selection" uses the box lower-left corner as a place to pick up the selection for plowing, but the box isn't in a window containing the selection`, printed on four lines) and the point must be over the edit cell. After plowing, the box is translated by the (possibly reduced) displacement and the selection is cleared; if a boundary reduced the distance, `Reduced distance to stay in the boundary.` is printed.

Compatibility: behavioral
(Magic ref: plow/PlowCmd.c:210-300, plow/PlowMain.c:438-470, plow/PlowMain.c:828-1180)

#### Scenario: Default distance
- **WHEN** `plow selection right` is executed
- **THEN** the selection is plowed 1 unit to the right and the box moves right by 1

### Requirement: Plow boundary
When a boundary is set for the edit cell, plowing SHALL be confined: edges left of the boundary may move only up to its left side; edges right of it are not processed; edges above or below it may not move. If anything would move outside, the plow (or selection distance) is reduced by the largest excess and the whole plow is recomputed until it fits; the user is informed (`Reduced plow size to stay within the boundary.`). The boundary is displayed as a highlight in windows of its root cell.

Compatibility: behavioral
(Magic ref: plow/PlowMain.c:189-330, plow/PlowMain.c:1364-1420, plow/PlowMain.c:780-800)

#### Scenario: Plow reduced by boundary
- **GIVEN** a boundary whose right side is 5 units right of material that a plow would push 10 units
- **WHEN** the plow is executed
- **THEN** the plow is reduced so the material moves at most 5 units and the reduction message is printed

### Requirement: Straightening
`straighten <dir>` SHALL remove jogs inside the box by pulling material in direction `dir`: on a copy of the box area plus the DRC halo (with jog horizon and boundary checking temporarily disabled), scanning from right to left in the canonical frame, each jog in a space/material outline is eliminated if dragging the jogged piece does not force any other edge to move; this repeats until no jog can be removed; the changed area is erased and repainted in the edit cell and DRC is re-run. Errors: `Usage: straighten manhattan-direction`, `The box is not in a window over the edit cell.`. Automatic straightening after a plow uses the plow's changed area and direction.

Compatibility: exact
(Magic ref: plow/PlowCmd.c:330-385, plow/PlowMain.c:332-436, plow/PlowJogs.c)

#### Scenario: Straighten a single jog
- **GIVEN** a metal1 wire with a 2-unit jog and free space around it
- **WHEN** `straighten left` is executed with the box over the jog
- **THEN** the jog is removed by pulling the offset segment left, provided no other material is forced to move

### Requirement: Plowing limitations
Hefesto SHALL reproduce these documented limitations rather than silently improving them: plowing operates only on the edit cell's paint and its direct subcell uses (subcell contents are never modified); existing design-rule violations are preserved (spacing is never increased beyond the original separation); edges cannot move farther than the plow distance; labels move with the paint they are attached to; non-Manhattan (split) tiles are treated by their bounding tiles as in the corner-stitched representation (see Q-LEGACY-50).

Compatibility: behavioral
(Magic ref: plow/PlowRules1.c:1005-1028, plow/PlowQueue.c:150-160, plow/TODO)

#### Scenario: Existing violation preserved
- **GIVEN** two metal1 boxes 1 unit apart while the rule requires 3
- **WHEN** the left box is plowed right by 4
- **THEN** the right box moves by 4 and their separation remains 1

### Requirement: Plow diagnostics
The wizard command `*plow` and the random plow tester SHALL be treated as developer diagnostics.

Compatibility: informative
(Magic ref: plow/PlowTest.c, plow/PlowRandom.c, plow/PlowCmd.c:385-403)

#### Scenario: Diagnostics have no compatibility obligation
- **WHEN** `*plow` is executed
- **THEN** Hefesto MAY print different diagnostics or none
