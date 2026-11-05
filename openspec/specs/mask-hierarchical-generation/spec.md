# Mask Hierarchical Generation

## Purpose

Define how mask (CIF/GDS output) geometry is generated for a hierarchical layout so that Hefesto produces exactly the same per-cell output geometry as Magic. This covers:

- per-cell generation from the cell's own paint;
- the halo radius;
- the classification of output layers that need hierarchical correction;
- discovery of subcell interaction areas;
- the flattening rules used inside interaction areas;
- the correction formula (flat result minus the union of the per-component results);
- array element interactions and how their corrections are replicated;
- the operators that stop hierarchical evaluation;
- the non-local (maxrect) layers;
- the hierarchy and array enable switches and the `CIFhier` cell property;
- flat output;
- known limitations.

Related capabilities:

- `mask-output-operators` defines what each geometric operator does.
- `mask-output-styles` defines the cifoutput style grammar: layers, templayers, scalefactor and options.
- `gds-write` and `cif-format-io` define how the per-cell geometry produced here is serialized.

Terminology used in this spec:

- **Internal units**: Magic database coordinates.
- **Output units**: CIF/GDS coordinates. One internal unit equals `S` output units, where `S` is the style's integer scale factor (see `mask-output-styles`).
- **Layer result**: the set of output-unit polygons produced for one cifoutput layer, either a `layer` or a `templayer`.
- **Own paint**: the paint stored in a cell definition itself, excluding the paint of its subcells.
- **Element**: one instance of an array use, identified by its (x, y) index. A non-arrayed use has one element.
- **Maximal horizontal strip decomposition**: the canonical decomposition of a manhattan region into rectangles that are as wide as possible horizontally, merged vertically when their x-extents are identical. Magic's tile planes store geometry this way.

## Requirements

### Requirement: Per-cell output composition

For every cell definition D that is written as a structure or symbol, the implementation SHALL compute the final output geometry of each cifoutput layer L as the union of three parts, computed in this order:

1. `Own_L(D)`: the own-paint result.
2. `Hier_L(D)`: the subcell-interaction correction. Skipped when subcell interaction output is disabled for D.
3. `Array_L(D)`: the array-interaction correction. Skipped when array interaction output is disabled for D.

The writer then emits only the layers that are not templayers. Each definition SHALL be generated once per write, no matter how many times it is instantiated. The geometry of a subcell SHALL NOT be copied into its parent except through `Hier_L` and `Array_L`.

Compatibility: exact

Because every part is combined by union, the final per-layer region does not depend on the order in which interaction areas or arrays are processed. The output-unit geometry SHALL equal Magic's as a point set. For manhattan geometry its maximal horizontal strip decomposition is therefore canonical.

The intended invariant is the following. Let `flatten(D)` be the fully instantiated geometry of D. The union over all cells of each cell's output, each transformed into D's coordinates, approximates `Gen_L(flatten(D))`. It is exact in all areas except those listed in "Known limitations of hierarchical generation".

(Magic ref: cif/CIFwrite.c:358-405, calma/CalmaWrite.c:1419-1540, cif/CIFhier.c:46-57)

#### Scenario: Leaf cell has no hierarchical part
- **GIVEN** a cell `leaf` with no subcells containing a metal1 rectangle (0,0)-(10,4), and a style with scale factor S=10 whose layer `CMF` is `or m1`
- **WHEN** GDS is written
- **THEN** the structure `leaf` contains exactly one CMF boundary covering output units (0,0)-(100,40)
- **AND** `Hier_CMF(leaf)` and `Array_CMF(leaf)` are empty

#### Scenario: Parent does not repeat child geometry
- **GIVEN** a cell `top` with no own paint containing one use of `leaf` from the previous scenario, with the identity transform
- **WHEN** GDS is written
- **THEN** the structure `top` contains an SREF to `leaf` and no CMF boundary

### Requirement: Own-paint generation area

For a cell D, `Own_L(D)` SHALL be computed from D's own paint only. Subcells are not searched. All layers, templayers included, are generated in style order, and each later layer may use earlier layer results (see `mask-output-operators`).

The generation area A is D's bounding box expanded by the halo radius r on all four sides. When writing GDS, if D has a `FIXED_BBOX` property with four integer values (xbot ybot xtop ytop), A is further enlarged to the smallest rectangle containing both itself and that box. CIF output does not apply this enlargement.

Paint is read within A expanded by r again, so the search window is the bounding box grown by 2r. Each layer result is then clipped to `A × S` in output units. When a side of A is at the infinite plane boundary, that side is neither expanded nor scaled.

Compatibility: exact

(Magic ref: cif/CIFwrite.c:385-391, calma/CalmaWrite.c:1488-1511, cif/CIFgen.c:5735-5820, cif/CIFgen.c:5901-5935)

#### Scenario: Clip window of own-paint output
- **GIVEN** a cell with bounding box (0,0)-(100,50), halo radius r=7, and S=10
- **WHEN** its own-paint output is generated
- **THEN** paint is read in (-14,-14)-(114,64) internal units
- **AND** every layer result is clipped to (-70,-70)-(1070,570) output units

#### Scenario: FIXED_BBOX enlarges the GDS generation area
- **GIVEN** a cell with paint bounding box (0,0)-(10,10), a property `FIXED_BBOX` = `-20 -20 30 30`, r=2, and S=10
- **WHEN** GDS is written
- **THEN** the generation area is (-20,-20)-(30,30), which is the union of (-2,-2)-(12,12) and the fixed box
- **AND** the results are clipped to (-200,-200)-(300,300)

### Requirement: Halo radius

When a cifoutput style is loaded, or rescaled, the implementation SHALL compute its halo radius r in internal units as follows.

For each layer i, in style order, compute a grow distance `g_i` and a shrink distance `s_i` in output units. Both start at 0. Then walk the layer's operator list in order:

- If the operator is `bbox`, `net`, `tagged` or `mask-hints`, stop walking this layer. That operator and every later one are ignored.
- If the operator reads earlier cifoutput layers j, set `g = max(g, g_j)` and `s = max(s, s_j)` over those layers.
- `grow`, `grow-min` and `grow-grid` add their distance to `g`.
- `shrink` adds its distance to `s`.
- `bloat-or` adds the largest positive per-type distance to `g`. It adds the largest magnitude among negative distances to `s`. When scanning types, a negative value is considered only if it did not first exceed the running positive maximum.
- All other operators contribute nothing. This includes `and`, `and-not`, `or`, `interact` and its variants, `bridge`, `bridge-lim`, `squares`, `squares-grid`, `slots`, `close`, `bloat-max`, `bloat-min`, `bloat-all`, `maxrect`, `boundary` and `orthogonal`/`manhattan`.

Then:

```
G = max_i g_i ;  H = max_i s_i
d = (G > H) ? 2*G : 2*H                (output units)
r = (S > 0) ? floor(d / S) + 1 : d + 1  (integer division)
```

Compatibility: exact

See Q-MASK-404 for the operators that are not counted.

(Magic ref: cif/CIFtech.c:1668-1760, cif/CIFtech.c:1777-1800, cif/CIFtech.c:2049, cif/CIFtech.c:2718-2719)

#### Scenario: Radius from a grow distance
- **GIVEN** a style with S=10 whose largest grow on any layer is 30 output units and whose largest shrink is 0
- **WHEN** the style is loaded
- **THEN** r = floor(60/10) + 1 = 7 internal units

#### Scenario: Shrink dominates, and ties use the shrink
- **GIVEN** S=10, a layer `templayer A; or m1; grow 20` and a layer `layer B; or A; shrink 25`
- **WHEN** the radius is computed
- **THEN** g_B = 20 is inherited from A and s_B = 25
- **AND** r = floor(50/10) + 1 = 6

#### Scenario: Empty style still has radius 1
- **GIVEN** a style whose layers use only `or` and `and`
- **WHEN** the radius is computed
- **THEN** r = 1

### Requirement: Classification of hierarchically processed layers

After computing the radius, the implementation SHALL compute two sets:

- `HierLayers`: the cifoutput layers that are recomputed in interaction areas.
- `YankTypes`: the paint types copied when flattening for those layers.

The sets are built in four passes.

1. **Backward pass**, i from the last layer down to the first. Layer i is needed if it is already in `HierLayers` (because a later layer depends on it) or if any of its operators is one of: `and`, `and-not`, `interact` (any variant), `shrink`, `close`, `bridge`, `bridge-lim`, `bloat-or`, `bloat-max`, `bloat-min`, `bloat-all`.
   - Let `Depend_i` be the union of the cifoutput layers read by its operators.
   - Let `Yank_i` be the union of their paint-type masks, plus:
     - for `bloat-or`, `bloat-max` and `bloat-min`: every type whose bloat distance differs from the distance given for space;
     - for `bloat-all`: every type with a non-zero distance. These go into `Depend_i` when the bloat-all refers to cifoutput layers, and into `Yank_i` otherwise.
   - If layer i is needed, add i and `Depend_i` to `HierLayers`, and add `Yank_i` to `YankTypes`.
2. **Forward pass**, i from first to last. Any layer that reads a layer already in `HierLayers` is added to `HierLayers`.
3. **Removal pass**. Any layer that contains a `boundary` or `bbox` operator is removed from `HierLayers`. Layers that depend on it are not removed. See Q-MASK-403.

Layers not in `HierLayers` get no correction in interaction areas or array overlaps. Their parent output is `Own_L` only.

Compatibility: exact

See Q-MASK-405.

(Magic ref: cif/CIFtech.c:2051-2132, cif/CIFtech.c:2203-2238)

#### Scenario: Plain OR layer is not hierarchical
- **GIVEN** a style with `layer CMF; or m1,m1c` only
- **WHEN** the style is loaded
- **THEN** CMF is not in HierLayers and YankTypes is empty

#### Scenario: AND-NOT drags in its dependency
- **GIVEN** `templayer NW; or nwell; grow 5` followed by `layer PSD; or pdiff; and-not NW`
- **WHEN** the style is loaded
- **THEN** HierLayers = {NW, PSD} and YankTypes ⊇ {pdiff, nwell}

#### Scenario: A bbox layer is excluded but its dependents are kept
- **GIVEN** `templayer BB; bbox` followed by `layer X; or m1; and-not BB; shrink 1`
- **WHEN** the style is loaded
- **THEN** X is in HierLayers and BB is not

### Requirement: Non-local layers

The implementation SHALL treat as non-local every layer that contains a `maxrect` operator, and every layer that reads a non-local layer. These are found in one forward pass. It SHALL also compute:

- `NonlocalGen`: the non-local layers plus every layer they transitively read. Found in a backward pass.
- `NonlocalYank`: the union of the paint-type masks of the operators of the layers in `NonlocalGen`.
- `NonlocalSeed`: the paint types that can create material in a maxrect layer. Starting from the maxrect layers and walking backward through their dependencies, collect the paint masks of `or`, `bloat-or`, `bloat-max`, `bloat-min` and `bloat-all` operators, and follow the cifoutput layers those operators read.

During `Hier_L(D)` the non-local correction is enabled when all three of these hold:

- the style has at least one non-local layer;
- D has at least one subcell;
- D's own paint, or the own paint of any descendant (each definition is checked once), contains a `NonlocalSeed` type.

When enabled:

- the non-local layers are removed from the layer set used for local interaction areas;
- after all local interaction areas are processed, one additional correction is computed over the entire generation area A of D, using types `NonlocalYank` and layers `NonlocalGen`.

When disabled, layers that are both non-local and hierarchical are processed in local interaction areas like any other layer.

Compatibility: exact

(Magic ref: cif/CIFtech.c:2134-2201, cif/CIFhier.c:1003-1027, cif/CIFhier.c:1104-1108, cif/CIFhier.c:787-850)

#### Scenario: Maxrect layer corrected over the whole cell
- **GIVEN** a style with `layer FILLBLK; or m1; maxrect` and a parent cell whose two children each contain m1 that abuts the other child's m1
- **WHEN** the parent is written
- **THEN** FILLBLK is corrected once over the parent's whole generation area: the flat maxrect result minus the per-child results

#### Scenario: Maxrect without seed types falls back to local processing
- **GIVEN** the same style and a parent whose subtree contains no m1
- **WHEN** the parent is written
- **THEN** no whole-cell correction is run

### Requirement: Interaction search grid

To compute `Hier_L(D)`, the implementation SHALL tile the region `T = A ∩ bbox(D)` into squares of side `step`. Here `step` is the style's `stepsize` value in internal units if it is positive. Otherwise `step = max(20·r, 50)`.

Squares are visited with y in the outer loop and x in the inner loop, starting at T's lower-left corner. Each square is `[x, min(x+step, T.xtop)] × [y, min(y+step, T.ytop)]`.

Each square is expanded by r. Interaction areas are then searched in the expanded square (see the next requirement). For every square that yields an interaction area I, a correction is computed over I using `YankTypes` and the local hierarchical layers.

Compatibility: exact

The squares only bound the size of each interaction area. Overlapping interaction areas from neighbouring squares are each computed, and their results are combined by union.

(Magic ref: cif/CIFhier.c:993-999, cif/CIFhier.c:1043-1068, cif/CIFtech.c:762-771)

#### Scenario: Default step size
- **GIVEN** r=7 and no `stepsize` statement
- **WHEN** a parent is written
- **THEN** step = 140

#### Scenario: Minimum step size
- **GIVEN** r=2 and no `stepsize` statement
- **WHEN** a parent is written
- **THEN** step = 50

#### Scenario: Grid over a 300-unit-wide cell
- **GIVEN** bbox(D) = (0,0)-(300,100), r=7 and step=140
- **WHEN** squares are enumerated
- **THEN** the squares are x∈{[0,140],[140,280],[280,300]} × y∈{[0,100]}, visited left to right
- **AND** the first square is searched as (-7,-7)-(147,107)

### Requirement: Interaction area discovery

Given a cell D, a search rectangle Q (an expanded square) and the radius r, the implementation SHALL compute an interaction rectangle I, or none, with the algorithm below. The rectangle algebra is defined as follows:

- `overlap` means the two rectangles share positive area.
- `expand(R, r)` grows R by r on every side.
- `R ∩ Q` clips each side of R to Q. The result may be degenerate.
- `include(acc, R)` replaces acc by the bounding box of acc and R. It does nothing when R is degenerate (xbot ≥ xtop or ybot ≥ ytop).
- The empty accumulator is the rectangle (0,0)-(0,0).

```
SA := expand(Q, r);  Int := (0,0,0,0);  found := false;  List := []
for each child use U of D whose bbox overlaps SA:          # whole array = one use
    found := true
    H := expand(U.bbox, r) ∩ SA
    P := (0,0,0,0)
    for every own-paint tile t of D (any non-space type, any plane)
                       whose rectangle overlaps H:
        include(P, rect(t))                                # whole tile, unclipped
    if not (P contains H):  List.append((U, H))
    if P non-degenerate:  include(Int, expand(P, r) ∩ H)
if not found: return NONE (no subcells)
for each (U, H) in List:
    B := (0,0,0,0)
    for each other use V ≠ U whose bbox overlaps H: include(B, V.bbox)
    include(Int, expand(B, r) ∩ H)        # B may still be (0,0,0,0); see Q-MASK-400
if Int degenerate: return NONE
sources := 0
if D has own paint (any non-space type) overlapping Int: sources := 1
for each use or array element at ANY depth below D whose subtree has
        paint (any type except space and DRC-check types) overlapping Int
        (in its transformed position): sources += 1; stop when sources = 2
if sources < 2: return NONE
I := Int ∩ Q;  if I degenerate: return NONE
return I
```

Compatibility: exact

Each use found by the depth search counts as a separate source. This includes each array element, and each nested descendant use in addition to its ancestor. Two elements of the same array therefore satisfy the two-source test. A single child that contains a nested subcell with paint in Int also satisfies it. Elements of one array are never detected as interacting with each other in the first two passes, because the whole array is a single use there. Those interactions are handled by the array correction.

(Magic ref: drc/DRCsubcell.c:249-300, drc/DRCsubcell.c:133-141, drc/DRCsubcell.c:352-366, drc/DRCsubcell.c:406-525, drc/DRCcontin.c:834-844, database/DBcellsrch.c:86-108, database/DBcellsrch.c:871-904, utils/geometry.c:77)

#### Scenario: Two abutting-by-gap children
- **GIVEN** a parent with no own paint and two children: U1 with bbox (0,0)-(100,100) and U2 with bbox (105,0)-(205,100), each with metal touching the facing bbox edges, r=7, and Q=(-7,-7)-(212,107)
- **WHEN** interactions are searched
- **THEN** U1's sibling pass gives expand((105,0,205,100),7) ∩ (-7,-7,107,107) = (98,-7)-(107,107)
- **AND** U2's sibling pass gives the same rectangle
- **AND** I = (98,-7)-(107,107)

#### Scenario: Parent paint near a child
- **GIVEN** a parent with an own metal tile (104,40)-(130,60), one child U with bbox (0,0)-(100,100) and paint reaching x=100, r=7, and Q=(-7,-7)-(147,107)
- **WHEN** interactions are searched
- **THEN** H = (-7,-7)-(107,107), the tile overlaps H, and P = (104,40)-(130,60)
- **AND** expand(P,7) ∩ H = (97,33)-(107,67)
- **AND** the sibling pass for U finds no other use, so B=(0,0,0,0) and expand(B,7) ∩ H = (-7,-7)-(7,7), which is included
- **AND** Int = (-7,-7)-(107,67), and the two-source test passes (parent plus U), so I = (-7,-7)-(107,67)

#### Scenario: Overlapping bounding boxes without paint in the overlap
- **GIVEN** two children whose bounding boxes overlap, but where only one of them has paint inside the computed Int and the parent has no paint there
- **WHEN** interactions are searched
- **THEN** the result is NONE and no correction is computed

### Requirement: Flattening for interaction processing

When the hierarchy below a cell is flattened into a scratch cell for an area W, the implementation SHALL:

1. Copy every paint tile of the requested types from every cell of the subtree, at every depth and for every array element, whose rectangle overlaps W in root coordinates. The **whole tile** is copied, not clipped to W. A tile is a maximal horizontal strip of one type on one plane of the source cell, or the relevant half of a split (non-manhattan) tile.
2. Transform each tile into root coordinates. For a split tile, transform its diagonal direction as well.
3. Paint the tile onto every plane on which its type exists, using the technology's paint-composition rules. Overlapping material from different cells therefore composes exactly as if the layout had been flattened by painting, for example poly over diffusion yields the transistor type.
4. Skip space tiles.
5. Copy every cell property whose name begins with `MASKHINTS_` from the root use and from every use in the subtree that overlaps W. Transform each property's rectangles into root coordinates and OR them into the same-named property of the scratch cell. The whole hint geometry is copied, not clipped.

Paint of cells marked as vendor (read-only GDS) cells SHALL also be copied. See Q-MASK-401.

Compatibility: exact

(Magic ref: cif/CIFhier.c:376-424, cif/CIFhier.c:242-366)

#### Scenario: Whole tiles are copied
- **GIVEN** a child metal strip (0,0)-(500,10) and a flattening window (90,-5)-(110,15)
- **WHEN** flattening runs
- **THEN** the scratch cell contains metal (0,0)-(500,10), not (90,0)-(110,10)

#### Scenario: Cross-cell composition
- **GIVEN** a technology in which painting poly over ndiff gives ntransistor, a parent with poly (0,-2)-(2,6), and a child with ndiff (−4,0)-(6,4)
- **WHEN** the area (−5,−5)-(7,7) is flattened
- **THEN** the scratch cell contains ntransistor over (0,0)-(2,4)

### Requirement: Interaction area correction

For a parent cell D, an interaction rectangle I (internal units), a type set Y and a layer set M, the implementation SHALL compute the correction and add it into D's output layers as follows:

1. **Total.** Flatten D's own paint and all subcells (types Y) within `expand(I, r)` into an empty scratch cell, with mask hints. Generate layers M from it over I with replace semantics (see "Own-paint generation area" for search and clip). Hierarchy-stopping operators are applied (next requirement). Call the per-layer result `Total_L`.
2. **Components.** Start with empty `Comp_L`. For every child use element E whose bounding box overlaps I:
   - flatten E's subtree alone (types Y) within `expand(I, r)` into an emptied second scratch cell;
   - generate layers M over I;
   - OR the result into `Comp_L`.

   Then generate layers M from D's own paint alone over I and OR that into `Comp_L` as well. Error reporting is suppressed during component generation.
3. **Residual.** For each layer L in style order:
   - if L is not a templayer, every point of `Comp_L` that is not covered by `Total_L` is a parent/child disagreement (next-but-one requirement);
   - then set `Total_L := Total_L − Comp_L`. This applies to templayers too.
4. **Add.** For every layer L, templayers included, OR `Total_L` into D's output layer L. When the style has `min-width` on any layer, each residual tile is first adjusted as described in "Sliver growth of hierarchical pieces".

Layers not in M contribute nothing in this step.

Compatibility: exact

In set notation, the contribution of one interaction area I to `Hier_L(D)`, for each layer L in M, is:

```
Contrib_L(I) = Gen_L(flat(D), I) − ( ∪_E Gen_L(subtree(E), I) ∪ Gen_L(own(D), I) )
```

Here `Gen_L(X, I)` is layer L generated from the material of X that lies within `expand(I, r)`, using whole tiles, and clipped to `I·S`. `Hier_L(D)` is the union of `Contrib_L` over all interaction areas, plus the non-local correction.

(Magic ref: cif/CIFhier.c:867-945, cif/CIFhier.c:449-500, cif/CIFhier.c:685-707, cif/CIFhier.c:559-597, cif/CIFhier.c:607-627, cif/CIFhier.c:643-664)

#### Scenario: Gap closed by grow/shrink across two children
- **GIVEN** S=10 and `layer M1X; or m1; grow 30; shrink 30`, which gives r=7
- **AND** children U1 with m1 (90,10)-(100,90) and U2 with m1 (105,10)-(115,90) in parent coordinates
- **AND** the interaction area I=(98,-7)-(107,107)
- **WHEN** the correction is computed
- **THEN** Total = shrink30(grow30(both)) ∩ (980,-70)-(1070,1070) = (980,100)-(1070,900)
- **AND** Comp = (980,100)-(1000,900) ∪ (1050,100)-(1070,900)
- **AND** the parent gets the M1X rectangle (1000,100)-(1050,900) in output units

#### Scenario: Purely additive interaction gives no residual
- **GIVEN** a hierarchical layer `layer X; or m1; and-not m2` and two children whose m1 overlap only each other, with no m2 anywhere
- **WHEN** the correction is computed
- **THEN** Total equals the union of the components, and nothing is added to the parent

### Requirement: Hierarchy-stopping operators

While generating layers for an interaction area or array overlap (both the total and the components), the implementation SHALL stop processing a layer's operator list when it reaches one of these operators:

- `squares`, `squares-grid` or `slots`, unless the operator is immediately followed by a `grow` operator;
- `net`;
- `tagged`;
- `boundary`;
- `bbox`.

The stopping operator and all later operators of that layer are skipped. The layer result is whatever had been computed before the stopping operator. That partial result then takes part in the residual and add steps like any other layer. Own-paint generation and flat generation never stop.

Compatibility: exact

See Q-MASK-402.

(Magic ref: cif/CIFgen.c:5079-5089, cif/CIFgen.c:5332-5376, cif/CIFgen.c:5433-5462, cif/CIFgen.c:5601-5636, cif/CIFgen.c:5700-5705)

#### Scenario: Squares followed by grow still runs
- **GIVEN** `templayer CUTETCH; or m1c; and-not foo; squares 0 4 4; grow 2`
- **WHEN** an interaction area is processed
- **THEN** squares and grow are both applied in the total and in the components

#### Scenario: Squares at end of list stops
- **GIVEN** `layer CC; or pc; and-not nwell; squares 2 4 4`
- **WHEN** an interaction area is processed
- **THEN** CC's hierarchical result is the `or pc; and-not nwell` region, without square cuts
- **AND** its residual, if any, is added to the parent's CC layer as uncut area

### Requirement: Parent/child disagreement reporting

When a non-templayer component tile is not fully covered by the total, the implementation SHALL report a CIF generation error on the parent cell. The error area is the uncovered part of the component tile, in output units. The report SHALL NOT be made when the style option `no-errors` is set.

For split (non-manhattan) total tiles, space inside the split tile is not an error when the component tile's left edge coincides with a left-side split tile, or its right edge with a right-side split tile.

In Magic the report is a feedback area at `area / S` on the parent, with the text `CIF error in cell <cell>, layer <layer>: parent and child disagree on CIF`. Hefesto MAY present it differently.

Compatibility: behavioral

(Magic ref: cif/CIFhier.c:515-541, cif/CIFhier.c:559-597, cif/CIFmain.c:417-440)

#### Scenario: Subtractive rule causes disagreement
- **GIVEN** `layer PSD; or pdiff; and-not nwell`, a child with pdiff (0,0)-(10,10), and a parent with nwell (0,0)-(10,10)
- **WHEN** the parent is written
- **THEN** a disagreement error is reported over (0,0)-(100,100) output units with S=10
- **AND** the child's PSD output stays in the child, so the flattened mask keeps the PSD that a flat generation would have removed

### Requirement: Array interaction geometry parameters

For every child use U of D whose bounding box overlaps D's generation area and that has more than one element, the implementation SHALL compute the following, all in D's internal coordinates:

- Child-coordinate pitch rectangle `(0,0)-(X,Y)`:
  - `X = cu_xsep` when the x index range has more than one value, otherwise `X = r + width(bbox(child))`;
  - `Y = cu_ysep` when the y index range has more than one value, otherwise `Y = r + height(bbox(child))`.
- `xsep`, `ysep`: the width and height of the pitch rectangle after U's transform.
- `xsize`, `ysize`: the width and height of the child bounding box after U's transform.
- `nx = (width(U.bbox) − xsize) / xsep + 1` and `ny = (height(U.bbox) − ysize) / ysep + 1`, using integer division. `U.bbox` is the bounding box of the whole array.

The pitch is taken from child coordinates, so a 90° rotation swaps the roles of the two separations.

Single-element uses are skipped.

Compatibility: exact

See Q-MASK-407.

(Magic ref: cif/CIFhier.c:1403-1450, cif/CIFhier.c:1749-1790, database/DBcellsrch.c:1185-1229)

#### Scenario: One-dimensional array
- **GIVEN** a child with bbox (0,0)-(10,20), an array x 0..3 with cu_xsep=12 and y 0..0, the identity transform, and r=3
- **WHEN** the parameters are computed
- **THEN** xsep=12, ysep=23, xsize=10, ysize=20, U.bbox=(0,0)-(46,20), nx=4 and ny=1

#### Scenario: Rotated array
- **GIVEN** the same child arrayed y 0..2 with cu_ysep=25 and x 0..0, with a 90° rotation transform, and r=3
- **WHEN** the parameters are computed
- **THEN** xsep=25, ysep=13 (that is, r + child width 10), xsize=20, ysize=10, and nx=3

### Requirement: Array interaction regions

Write `bb = U.bbox`. The implementation SHALL define four regions in parent internal units:

- `A = [bb.xbot−r, bb.xbot+xsize+r] × [bb.ybot+ysep−r, bb.ybot+ysize+r]`. Used only if `ysep < ysize + r`.
- `B = [bb.xbot+xsep−r, bb.xbot+xsize+r] × [bb.ybot−r, bb.ybot+ysep−r]`. Used only if `xsep < xsize + r` and `ysep < ysize + r`.
- `C = [bb.xtop−xsize−r, bb.xtop−xsep+r] × [bb.ytop−ysize−r, bb.ytop+r]`. Used only if `xsep < xsize + r`.
- `D' = [bb.xtop−xsep+r, bb.xtop+r] × [bb.ytop−ysize−r, bb.ytop−ysep+r]`. Used only if `xsep < xsize + r` and `ysep < ysize + r`.

Regions A and B are processed together as the low phase, using the low-corner elements. Regions C and D' are processed together as the high phase, using the high-corner elements.

Compatibility: exact

(Magic ref: cif/CIFhier.c:1452-1520, cif/CIFhier.c:1631-1666)

#### Scenario: 2×2 abutting array
- **GIVEN** a child with bbox (0,0)-(10,10), an array 0..1 × 0..1 with separations 10/10, the identity transform, and r=2
- **WHEN** the regions are computed
- **THEN** A=(-2,8)-(12,12), B=(8,-2)-(12,8), C=(8,8)-(12,22) and D'=(12,8)-(22,12)

#### Scenario: Widely spaced array has no regions
- **GIVEN** a 3×3 array with a child bbox of 10×10, separations 20, and r=2
- **WHEN** the regions are computed
- **THEN** no condition holds and the array contributes no correction

### Requirement: Array interaction correction

For each phase, the implementation SHALL compute the correction as follows. Each region uses the same procedure.

1. Let `R` be the region and `E` its expansion by r. Visit every element whose bounding box overlaps E. The element filter is:
   - low phase: `x − xlo < 2` and `y − ylo < 2`;
   - high phase: `xhi − x < 2` and `yhi − y < 2`.

   Here xlo, xhi, ylo and yhi are the index bounds as stored (see Q-MASK-406).
2. For each such element:
   - add its subtree material (types `YankTypes`), within `expand(R in element coordinates, r)` and with mask hints, into a total scratch cell. This cell is shared by both regions of the phase and is not cleared between them;
   - also flatten the same material alone into a cleared component scratch cell;
   - generate layers `HierLayers` over R, with error reporting suppressed, and OR the result into `Comp_L`.
3. Generate layers `HierLayers` from the total scratch cell over R and OR the result into `Total_L`. The parent's own paint is not included in either the total or the components.
4. If the phase's first region (A or C) was used, apply the residual step from "Interaction area correction" (disagreement errors on the parent, then `Total_L − Comp_L`). Then replicate as described in "Array correction replication".
5. Clear all scratch state before the next phase.

Compatibility: exact

(Magic ref: cif/CIFhier.c:1137-1258, cif/CIFhier.c:1452-1520, cif/CIFhier.c:1631-1666, cif/CIFhier.c:1527-1530, cif/CIFhier.c:1625-1629, cif/CIFhier.c:1734)

#### Scenario: Seam bloat in a 1-D array
- **GIVEN** the 1-D array from the parameters scenario with S=10, each element containing m1 (0,5)-(10,15) in child coordinates, and `layer M; or m1; grow 10; shrink 10`, which gives r = floor(20/10)+1 = 3
- **WHEN** the high phase runs
- **THEN** C=(33,-3)-(37,23), and the elements with x ∈ {2,3} are used
- **AND** the residual is the 2-unit gap between the elements, (340,50)-(360,150) output units

### Requirement: Array correction replication

After the residual of a phase is formed, the implementation SHALL copy it into D's output layer L. This applies to every layer, templayers included. Each copy takes every residual tile (maximal horizontal strip, or split tile) that overlaps the scaled window, and paints it **whole**, not clipped to the window, at the given offsets. Offsets are applied as `(i·dx, j·dy)` for `0 ≤ i < cx` and `0 ≤ j < cy`. All windows are multiplied by S, and so are dx and dy.

Low phase:

1. If `ny > 1` and `ysep < ysize+r`: window A, dx=0, dy=ysep, cx=1, cy=ny−1.
2. If `nx > 1`, `ny > 1`, `xsep < xsize+r` and `ysep < ysize+r`:
   1. Window B, dx=xsep, dy=0, cx=nx−1, cy=1.
   2. Core window A″ = `[bb.xbot, bb.xbot+xsep] × [bb.ybot+ysep−e, bb.ybot+ysize+e]`, where `e = ysep >> 1` if `2r > ysep`, otherwise `e = r`. Copy with dx=xsep, dy=ysep, cx=nx−1, cy=ny−1.
   3. If `2r < ysep`, also core window B″ = `[bb.xbot+xsep−f, bb.xbot+xsize+f] × [bb.ybot, min(bb.ybot+ysep, A″.ybot)]`, where `f = xsep >> 1` if `2r > xsep`, otherwise `f = r`. Same offsets as A″.

High phase:

1. If `nx > 1` and `xsep < xsize+r`: window C, dx=−xsep, dy=0, cx=nx−1, cy=1.
2. If `nx > 1`, `ny > 1` and both conditions hold: window D', dx=0, dy=−ysep, cx=1, cy=ny−1.

Compatibility: exact

See Q-MASK-408.

(Magic ref: cif/CIFhier.c:1522-1623, cif/CIFhier.c:1668-1730, cif/CIFhier.c:1330-1360, cif/CIFhier.c:100-104)

#### Scenario: Replication along a row
- **GIVEN** the 1-D array (nx=4) with residual (340,50)-(360,150) in window C×10 = (330,-30)-(370,230)
- **WHEN** it is replicated
- **THEN** the parent receives copies at x offsets 0, −120 and −240: (340,50)-(360,150), (220,50)-(240,150) and (100,50)-(120,150)

#### Scenario: Core windows of a 3×3 array
- **GIVEN** a 3×3 array with a child bbox of 10×10, separations 10, bb=(0,0)-(30,30), and r=2
- **WHEN** the low phase is replicated
- **THEN** A″=(0,8)-(10,12), since 2r=4 is not greater than 10 and e=2
- **AND** B″=(8,0)-(12,8)
- **AND** both are copied at offsets {0,10}×{0,10}

### Requirement: Sliver growth of hierarchical pieces

When any layer of the style has a `min-width` value, the implementation SHALL adjust each residual tile before painting it into the parent. This applies to interaction-area residuals and to array copies. Let `w` and `h` be the tile's width and height in output units, and `m` the current layer's `min-width` in output units, which is never rescaled. Then:

```
if m == 0: unchanged
up   := left or right corner-stitch neighbour of the tile in the residual plane is non-space
side := bottom or top corner-stitch neighbour is non-space
if (h < w) or up:
    if h >= m: stop (unchanged)
    ytop += (m-h) div 2 ; ybot -= (m-h) div 2
if (w < h) or side:
    if w >= m: stop
    xtop += (m-w) div 2 ; xbot -= (m-w) div 2
```

Here `div` is integer division truncating toward zero. The "stop" in the first branch also skips the second branch.

Compatibility: exact

Magic also prints two diagnostic lines to standard output. That part is informative. See Q-MASK-409.

(Magic ref: cif/CIFhier.c:1268-1305, cif/CIFhier.c:651-652, cif/CIFhier.c:1343, cif/CIFtech.c:979-989)

#### Scenario: Thin seam widened
- **GIVEN** `min-width 40` on layer M and an isolated residual tile (1000,100)-(1010,900) with space neighbours
- **WHEN** it is painted into the parent
- **THEN** since w=10 < h=800, it becomes (985,100)-(1025,900)

### Requirement: Hierarchy and array output switches

The implementation SHALL provide two independent session switches. Both default to enabled.

- **Subcell interaction output** controls `Hier_L`. Magic sets it with `cif *hier write enable|disable`.
- **Array interaction output** controls `Array_L`. Magic sets it with `cif *array write enable|disable`.

In the third argument, any word starting with `dis` disables, any word starting with `en` enables, and any other word only reports the current state. The switches affect both CIF and GDS writing.

When GDS is written, a cell definition that carries a property named `CIFhier` (any value) forces both switches on for two things:

- the generation of that cell itself;
- the processing of every descendant that is first written while that cell's subcells are being processed. Descendants already written earlier keep the setting that was in effect when they were written.

The switches are restored afterwards. CIF writing ignores `CIFhier` (see Q-MASK-410).

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:1858-1903, cif/CIFwrite.c:87-88, cif/CIFwrite.c:393-396, calma/CalmaWrite.c:1026, calma/CalmaWrite.c:1058-1080, calma/CalmaWrite.c:1352-1370, doc/html/cif.html:200-220)

#### Scenario: Disabling subcell interactions
- **GIVEN** the grow/shrink gap scenario above
- **WHEN** `cif *hier write disable` is issued and GDS is written
- **THEN** the parent receives no M1X rectangle at (1000,100)-(1050,900)

#### Scenario: CIFhier overrides the switch
- **GIVEN** subcell interaction output is disabled and the parent has the property `CIFhier` = `true`
- **WHEN** GDS is written
- **THEN** the parent receives the M1X rectangle (1000,100)-(1050,900)

### Requirement: Hierarchical layer preview

The implementation SHALL let the user display the hierarchical contribution of a named output layer within a rectangle of the edit root. In Magic these are the wizard commands `cif *hier <layer>` and `cif *array <layer>`.

The display shows only `Hier_L` (or only `Array_L`) computed for the root cell over the rectangle. Output planes are cleared first, and own-paint output is not included. An unknown layer name is an error that lists the valid names.

Compatibility: behavioral

(Magic ref: cif/CIFsee.c:353-406, commands/CmdCD.c:1858-1908)

#### Scenario: Preview of an array seam
- **GIVEN** the 1-D array scenario and a box covering the whole array
- **WHEN** `cif *array M` is run
- **THEN** exactly the three seam rectangles are highlighted

### Requirement: Flat region generation

For flat views of a region R of a root cell, the implementation SHALL generate the layers as follows:

1. Flatten all paint types except space and the DRC types from the whole subtree within `expand(R, r)` into a scratch cell, using whole tiles, paint composition and mask hints.
2. Generate the requested layer and its transitive dependencies over R, without hierarchy stopping, from the flattened material.

Flat views are the layer display (`cif see`), painting a layer back into the database (`cif paint`) and coverage measurement (`cif coverage`). Their command surfaces belong to `cif-format-io`. When `bloat-all` refers to cifoutput layers, those layers are added to the dependencies.

Compatibility: exact

(Magic ref: cif/CIFsee.c:136-200, cif/CIFsee.c:263-330, cif/CIFsee.c:431-470, cif/CIFmain.c:323-385)

#### Scenario: Flat view shows interaction geometry
- **GIVEN** the grow/shrink gap scenario with R covering both children
- **WHEN** flat generation of M1X runs over R
- **THEN** the result is the single rectangle (900,100)-(1150,900)

### Requirement: Flat CIF output

When the user requests flat CIF output, the implementation SHALL:

1. Flatten all paint types except space and DRC types from the whole hierarchy of the root, within its bounding box expanded by r, into a single scratch definition. Labels and mask hints are not copied.
2. Recompute that definition's bounding box.
3. Write it as the only symbol, using own-paint generation only. The scratch definition has no subcells, so `Hier_L` and `Array_L` are empty.

Magic names the scratch definition `__CIF2__`, so the symbol name record is `9 __CIF2__;`. See Q-MASK-411.

Compatibility: exact

(Magic ref: cif/CIFwrite.c:729-790)

#### Scenario: Flat CIF of a two-child parent
- **GIVEN** the grow/shrink gap layout
- **WHEN** `cif flat out.cif` is run
- **THEN** out.cif contains one symbol, `DS 1 …`, whose M1X geometry is one box covering (900,100)-(1150,900) in output units, and no labels
- **AND** the file ends with `C 1;` followed by `End`

### Requirement: Non-manhattan merge option

When the `unfracture` switch is on (default off), the implementation SHALL merge adjacent non-manhattan (split) tiles of each generated layer result, wherever they can be represented by fewer tiles. This happens after each layer is generated and before clipping. It applies to all generation modes and changes only the decomposition, not the covered point set.

In Magic the switch is set with `cif unfracture <any> yes|no`. The value is the fourth word. With only three words the command reports the state. See Q-MASK-413.

Compatibility: exact

(Magic ref: cif/CIFgen.c:51, cif/CIFgen.c:5784-5786, commands/CmdCD.c:2056-2080)

#### Scenario: Unfracture changes polygon count, not area
- **GIVEN** a 45° wire on a layer generated from many split tiles
- **WHEN** GDS is written with unfracture on and again with it off
- **THEN** the union of the boundaries is identical in both files
- **AND** the version with unfracture on has no more boundary records than the version with it off

### Requirement: Vendor cells in hierarchical generation

A cell whose output is copied verbatim from a vendor GDS file (see `gds-write`) SHALL NOT be generated by own-paint generation. Its database paint SHALL still take part in the interaction-area and array corrections of its parents, exactly like the paint of any other cell.

Compatibility: exact

See Q-MASK-401.

(Magic ref: calma/CalmaWrite.c:1323-1335, calma/CalmaWrite.c:1350-1372, cif/CIFhier.c:396-405)

#### Scenario: Parent paint touching a vendor cell
- **GIVEN** a vendor cell with m1 at its edge, and a parent with own m1 abutting it under a layer `or m1; grow 5; shrink 5`
- **WHEN** GDS is written
- **THEN** the vendor structure is copied byte-for-byte
- **AND** the parent receives the same interaction residual it would receive for an ordinary child

### Requirement: Known limitations of hierarchical generation

The implementation SHALL reproduce the following known limitations of Magic exactly when it claims `exact` compatibility. It SHALL NOT silently correct them; deviations are decided through the referenced open questions.

1. **Subtractive interactions.** A hierarchical layer whose flat result inside an interaction area is smaller than the union of the components produces a disagreement error. The extra child geometry remains in the child.
2. **Non-hierarchical layers.** Layers not in `HierLayers` are generated per cell only, even when their operators do not distribute over union. Examples are `squares` on a contact area split across cells, `grow-min`, and `not-square`. See Q-MASK-405.
3. **Stopping operators.** See Q-MASK-402.
4. **Material beyond the halo.** Corrections only see material within the halo r and within the whole tiles that overlap it. Operators whose reach exceeds r (Q-MASK-404) may differ from flat generation.
5. **Array scope.** Array corrections consider only interactions among elements of the same array. They exclude the parent's own paint and other uses; those are covered by the subcell-interaction step.

Compatibility: informative

(Magic ref: cif/CIFhier.c:46-57, cif/CIFhier.c:515-541, cif/CIFtech.c:2051-2132)

#### Scenario: Cut layer split across cells
- **GIVEN** a contact area split by a cell boundary, with half in the parent and half in a child, under `layer CC; or pc; squares 2 4 4`
- **WHEN** GDS is written
- **THEN** each half is cut independently, and the cut pattern can differ from the flat one

### Requirement: Generation statistics

The implementation SHALL keep counters of total geometric tile operations, tile operations due to hierarchy, rectangles output, and rectangles due to hierarchical interactions. Each counter has a recent value and a cumulative value. Magic's `cif statistics` prints them as four lines and resets the recent values:

```
CIF statistics (recent/total):
    Geometrical tile operations: %d/%d
    Tile operations for hierarchy: %d/%d
    CIF rectangles output: %d/%d
    CIF rectangles due to hierarchical interactions: %d/%d
```

Compatibility: informative

(Magic ref: cif/CIFmain.c:98-117)

#### Scenario: Statistics after a write
- **GIVEN** a GDS write that produced hierarchical residuals
- **WHEN** `cif statistics` is run twice
- **THEN** the second run shows 0 for every recent value and the same totals

### Requirement: Progress reporting and interruptibility

During interaction processing Magic prints `Completed N%` roughly every five seconds once more than one interaction area has been processed, and `Completed 100%` at the end if it printed anything. While doing so it blocks user input events. Hefesto SHALL provide equivalent non-blocking progress and SHALL keep the database from being edited during a write, but MAY use a different mechanism.

Compatibility: informative

[platform] See Q-MASK-414.

(Magic ref: cif/CIFhier.c:974-988, cif/CIFhier.c:1035-1041, cif/CIFhier.c:1070-1100, cif/CIFhier.c:1110-1124)

#### Scenario: Long write shows progress
- **GIVEN** a parent with thousands of interaction areas that takes more than five seconds to write
- **WHEN** it is written
- **THEN** percentage progress is reported and editing is blocked until the write finishes
