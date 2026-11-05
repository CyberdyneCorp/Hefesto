# DRC Rules on Generated Mask Layers

## Purpose

Defines the `cifstyle`, `cifwidth`, `cifspacing`, `cifarea` and `cifmaxwidth` statements of the `drc` tech section, which check geometry of mask (CIF/GDS output) layers generated from the layout by a mask output style, and the exact way these checks are run and their errors mapped back to layout coordinates. Mask-layer generation itself is defined by capabilities `mask-output-styles`, `mask-output-operators` and `mask-hierarchical-generation`. Hefesto MUST produce the same error rectangles and why-texts as Magic 8.3.694.

Units: a *mask unit* is the unit of the mask output style (centimicrons, nanometres or ångströms as declared there); `σ` is the style's scale, mask units per internal unit (integer). Mask layers are planes with two types, space and solid.

## Requirements

### Requirement: Mask style binding
The `cifstyle NAME` statement SHALL bind subsequent mask-layer rules of the DRC style to mask output style NAME: if NAME is the current mask output style the rules are compiled; if NAME is another known style the rules are ignored for now and the DRC style is reloaded whenever the current mask output style changes; if NAME is unknown `Unknown DRC cifstyle <NAME>` is reported.
Compatibility: exact

Without a preceding valid binding, each mask-layer rule is ignored and `Missing cif style for drc` (followed by `This message will not be repeated.`) is reported once per style load. At check time, if the bound style is not current but has been declared, the checker temporarily loads it, generates layers, and restores the current style (printing `Loading DRC CIF style.`); if loading fails it reports `Error:  Failed to load CIF DRC style.` and skips mask-layer rules.

(Magic ref: drc/DRCcif.c:115-160, drc/DRCcif.c:515-560)

#### Scenario: Rules ignored without binding
- **GIVEN** a drc section containing `cifwidth MET1 140 "m1 width"` and no `cifstyle` line
- **WHEN** the tech file is loaded
- **THEN** `Missing cif style for drc` is reported once and no mask-layer rule exists

### Requirement: Mask-layer rule statements
The four statements SHALL compile into mask-layer primitive rules attached to the named mask layer (by exact, case-sensitive layer name in the bound style), with distances in mask units and not scaled by the DRC scalefactor, as follows.
Compatibility: exact

- `cifwidth L w why`: on L's *solid list*: FWD rule dist w, cdist w, OK = {solid}, CORNER = {solid}, check layer L.
- `cifspacing L1 L2 d adjacency why`:
  - adjacency `touching_ok` requires L1 = L2 (else `Spacing check with touching ok must all be in one plane.`) and sets corner set Kc = {space}; `touching_illegal` sets Kc = {space, solid} and *reverse-needed*; anything else reports `Badly formed drc spacing line`.
  - L1 *space list*: FWD dist d, cdist d, OK={space}, CORNER=Kc, check layer L2 (+BOTHCORNERS if reverse-needed).
  - L1 solid list: REVERSE dist d, cdist d, OK={space}, CORNER=Kc, check layer L2 (+BOTHCORNERS if reverse-needed).
  - If reverse-needed: L2 space list FWD|BOTHCORNERS and L2 solid list REVERSE|BOTHCORNERS, check layer L1; and if L1 = L2, two further identical REVERSE|BOTHCORNERS rules on the solid list — duplicates with no extra effect on the error plane.
  - If L1 ≠ L2: L2 solid list FWD dist σ, cdist σ, OK={space}, CORNER=∅, check layer L1; and L1 solid list FWD dist σ (same) check layer L2 — these flag coincident (overlapping) material.
- `cifarea L a h why`: solid list AREA rule, dist h, cdist a (mask units²).
- `cifmaxwidth L w bends why`: solid list MAXWIDTH rule with variant bend_illegal / bend_ok (BENDS) / both (MAXWIDTH_BOTH); unknown bends report `unknown bend option <x>`.
- An unknown layer reports `Unknown cif layer: <name>`.
- Halo contribution: ceil(distance / σ) for cifwidth/cifspacing/cifmaxwidth and ceil(h / σ) for cifarea, then (as for all rules) multiplied by D/N of the DRC scalefactor with ceiling (see Q-DRC-2).
- New rules are pushed at the front of their list (last declared is evaluated first).
- Mask-layer rule distances are rescaled with the mask style when the internal grid changes (multiplied by n/d with truncation).

The *solid list* of a layer is evaluated on its solid tiles, the *space list* on its space tiles (see next requirement).

(Magic ref: drc/DRCcif.c:181-380, drc/DRCcif.c:1177-1300, drc/DRCcif.c:388-415)

#### Scenario: Compiled cifspacing touching_illegal
- **GIVEN** σ = 10 and `cifspacing NW PW 600 touching_illegal "well spacing"`
- **WHEN** compiled
- **THEN** NW's space list holds FWD|BOTHCORNERS dist 600 checking PW, NW's solid list holds REVERSE|BOTHCORNERS dist 600 checking PW and FWD dist 10 checking PW, and PW's lists hold the mirrored rules checking NW
- **AND** the halo contribution is 60 internal units

### Requirement: Mask-layer check procedure
At the end of every basic check with check rectangle K and clip L (see `drc-rule-semantics`), the checker SHALL generate all layers of the bound mask style for K from the checked cell (flat, as a basic check sees it), and then, for each layer index i in style order, for each list (solid list first, then space list) and for each rule in list order, enumerate the layer's tiles of the list's kind overlapping K·σ and evaluate the rule on each such tile.
Compatibility: exact

Per tile T:
- AREA and MAXWIDTH rules are evaluated on every tile (no edge requirement).
- Otherwise the left edge (if LEFT(T) ≥ K.xbot·σ, skipped for the right half of a split tile) and the bottom edge (if BOTTOM(T) ≥ K.ybot·σ, skipped for split halves whose bottom is the diagonal) are processed as in `drc-rule-semantics` "Standard edge rule check region" with these differences: segments are skipped only when both sides have the same type (no per-pair rule lookup), there is no edge-end continuation test on any edge, no OUTSIDE, TRIGGER, RUNLENGTH, MANHATTAN or DERIVED handling, and the Euclidean corner exemption uses the plain test `sqx² + sqy² ≥ s²` with the global Euclidean setting.
- The corner samples are the same unit squares as in the layout rules (top end: tile containing (X−1, ytop) bottom type for FWD; bottom end: tile containing (X, ybot−1) top type for REVERSE; horizontal FWD: (xl−1, Y−1) and (xr, Y−1); horizontal REVERSE: (xr, Y) and (xl−1, Y)).
- Candidate tiles: tiles of the check layer overlapping E whose type is not in OK.

(Magic ref: drc/DRCcif.c:515-600, drc/DRCcif.c:606-1030)

#### Scenario: Mask width violation
- **GIVEN** σ = 10, `cifwidth MET1 140 "m1 < 1.4"`, and a generated MET1 rectangle [0,100]×[0,1000] (mask units)
- **WHEN** checked
- **THEN** from the left edge E = [0,140]×[0,1000] and the space tile right of x=100 yields the mask-unit candidate [100,140]×[0,1000]
- **AND** the reported error is [10,14]×[0,100] in internal units

### Requirement: Mapping mask errors to layout coordinates
A mask-layer candidate rectangle C (mask units, already clipped to E) SHALL be converted to internal units by dividing each coordinate by σ with C truncating division (toward zero); if a resulting extent is zero it SHALL be widened by one unit (xbot−1 if xbot < 0, otherwise xtop+1; same for y); the result is clipped to L and reported if non-null.
Compatibility: exact

Area and maxwidth errors convert the whole start tile rectangle by truncating division without the zero-extent widening. See Q-DRC-13 for the asymmetric truncation of negative coordinates.

(Magic ref: drc/DRCcif.c:1035-1075, drc/DRCcif.c:1395-1405, drc/DRCcif.c:1490-1505)

#### Scenario: Sub-unit error widened
- **GIVEN** σ = 10 and a mask candidate [103,107]×[0,50]
- **WHEN** converted
- **THEN** x becomes [10,10], widened to [10,11], and the error is [10,11]×[0,5]

#### Scenario: Negative coordinate truncation
- **GIVEN** σ = 10 and a mask candidate [−15,−5]×[0,10]
- **WHEN** converted
- **THEN** the result is [−1,0]×[0,1]

### Requirement: Mask area rule
A mask AREA rule SHALL flood-fill the 4-connected solid region containing each solid tile, summing full tile areas in mask units², and report the start tile's rectangle (converted, clipped to L) when the total is below the limit; the fill stops without error when the sum reaches the limit or a visited tile has an edge exactly equal to an edge of K·σ.
Compatibility: exact

Each tile of an undersized region is reported separately, so the whole region is flagged.

(Magic ref: drc/DRCcif.c:1311-1395)

#### Scenario: Small via
- **GIVEN** σ = 10, `cifarea VIA1 10000 50 "via area"`, and an isolated VIA1 square [0,90]×[0,90] (area 8100)
- **WHEN** checked
- **THEN** the error [0,9]×[0,9] is reported

### Requirement: Mask maxwidth rule
A mask MAXWIDTH rule SHALL be evaluated per solid tile: `both` reports the tile if its width or height exceeds w; `bend_ok` reports a non-split tile if both its width and height exceed w; `bend_illegal` flood-fills the 4-connected solid region and reports the start tile if the region's bounding box exceeds w in both dimensions; reported rectangles are converted by truncating division and clipped to L.
Compatibility: exact

(Magic ref: drc/DRCcif.c:1437-1600)

#### Scenario: bend_ok per tile
- **GIVEN** σ = 10, `cifmaxwidth MET5 3000 bend_ok "slot"`, and a solid MET5 tile [0,3500]×[0,3500]
- **WHEN** checked
- **THEN** the error [0,350]×[0,350] is reported
