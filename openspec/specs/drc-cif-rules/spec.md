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
- Halo contribution: ceil(distance / σ) for cifwidth/cifspacing/cifmaxwidth and ceil(h / σ) for cifarea, then, with the `magic-compat` setting ON (the default), multiplied by D/N of the DRC scalefactor with ceiling as for all rules, as in Magic; with it OFF the CIF contributions are not scaled and the halo is the maximum of them and the scaled tech-file-unit halo (see Q-DRC-2, resolution: replicate under magic-compat, fix with it off; capability `drc-rule-language` "Halo and step size").
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
A mask-layer candidate rectangle C (mask units, already clipped to E) SHALL be converted to internal units according to the `magic-compat` setting (Q-DRC-13, resolution: compat):
Compatibility: exact

- With `magic-compat` ON (the default), as in Magic: each coordinate is divided by σ with C truncating division (toward zero); if a resulting extent is zero it SHALL be widened by one unit (xbot−1 if xbot < 0, otherwise xtop+1; same for y). Area and maxwidth errors convert the whole start tile rectangle by truncating division without the zero-extent widening, and an area or maxwidth error whose rectangle collapses to zero width or height is dropped (not reported).
- With `magic-compat` OFF: on every path (spacing, width, area, maxwidth) lower edges are mapped with floor(x/σ) and upper edges with ceil(x/σ), so no error rectangle is empty and area and maxwidth violations narrower than one internal unit are reported.

The result is clipped to L and reported if non-null. In both settings Hefesto SHALL count the area and maxwidth errors whose rectangle collapses under truncating division in a diagnostic, so an error dropped with `magic-compat` ON is visible.

(Magic ref: drc/DRCcif.c:1035-1075, drc/DRCcif.c:1052-1068, drc/DRCcif.c:1364-1367, drc/DRCcif.c:1468-1471, drc/DRCcif.c:1497-1500, drc/DRCcif.c:1554-1557)

#### Scenario: Sub-unit error widened
- **GIVEN** σ = 10 and a mask candidate [103,107]×[0,50]
- **WHEN** converted
- **THEN** x becomes [10,10], widened to [10,11], and the error is [10,11]×[0,5]

#### Scenario: Negative coordinate truncation
- **GIVEN** σ = 10 and a mask candidate [−15,−5]×[0,10]
- **WHEN** converted
- **THEN** the result is [−1,0]×[0,1]

#### Scenario: Negative coordinate with magic-compat OFF
- **GIVEN** `magic-compat` OFF, σ = 10 and a mask candidate [−15,−5]×[0,10]
- **WHEN** converted
- **THEN** the result is [−2,0]×[0,1]

#### Scenario: Collapsed area error
- **GIVEN** σ = 10, `cifarea VIA1 10000 50 "via area"`, and an isolated VIA1 rectangle [103,107]×[0,90] (area 360)
- **WHEN** checked with `magic-compat` ON
- **THEN** the tile truncates to [10,10]×[0,9], no error is reported, and the collapsed-error diagnostic count is 1
- **AND** with `magic-compat` OFF the error [10,11]×[0,9] is reported

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
