# Tapeout Fill and Density

## Purpose
Defines how Hefesto generates metal, poly and diffusion fill and checks layout density before tapeout:
- per-PDK fill recipes
- density windows and limits
- the reports

Where the open PDKs ship a Magic-based fill or density flow, Hefesto reproduces it exactly. This applies to sky130 (`generate_fill.py`, `check_density.py`) and to the gf180mcu `metfill` output style. Where a PDK or shuttle defines fill or density only as KLayout scripts, Hefesto implements the rule natively and checks itself against the rule values. This applies to IHP sg13g2 and to the gf180mcu LibreLane/wafer.space flow. It does not claim byte equality there (see Q-TAPEOUT-1, Q-TAPEOUT-7, Q-TAPEOUT-8).

Mask operators (`slots`, `grow`, `shrink`, `and-not`, `bbox`) are capability `mask-output-operators`. Output styles and variants are capability `mask-output-styles`. Flattening is capability `cmd-hierarchy`. GDS output is capability `gds-write`. Coverage measurement (`cif coverage`) is capability `cif-format-io`. The sign-off use of density results is capability `tapeout-signoff-checklist`.

Reference sources (read-only, never executed):
- open_pdks `RTimothyEdwards/open_pdks` @ `c6df28c1fccb05c286aa0a15e9c4c7dcd8502607` (`open_pdks ref:`)
- IHP-Open-PDK @ `5e6d592e4002946a4616f798c357f0f3c06cf3b6` (`IHP ref:`)
- mpw_precheck `efabless/mpw_precheck` @ `0941bdc1b62b5c3f99c8683bd11199d330af2ef3` (`mpw_precheck ref:`)
- LibreLane @ `8494d4fea0ddd9bee12ea5e42a38b59438c2b128` (`librelane ref:`)

## Requirements

### Requirement: Fill recipe registry
Hefesto SHALL select fill generation through a per-PDK fill recipe. A fill recipe is data, shipped in the PDK pack or in Hefesto's PDK profile. It SHALL name:
- the recipe kind: `magic-tiled-style` (a cifoutput style applied to flattened tiles), `magic-paint-style` (a cifoutput style whose layers are painted back as fill types), or `native` (a Hefesto algorithm with parameters)
- the technology file to load, where it differs from the project tech
- the style and variant
- the tile size and halo
- the fill layers and their output GDS layer/datatype
- the blockage layers

The built-in recipes SHALL be:

| PDK | kind | source |
|---|---|---|
| sky130A/B | `magic-tiled-style`, tech `<variant>-GDS.tech`, style `wafflefill(tiled)`, tile 700 µm, halo 1.6 µm | open_pdks `generate_fill.py` |
| gf180mcuA–D | `magic-paint-style`, style `metfill` | open_pdks `gf180mcu-cifout.tech` |
| ihp-sg13g2, ihp-sg13cmos5l | `native` | IHP KLayout filler macros (rules only) |

Hefesto SHALL NOT execute KLayout Ruby or Python fill scripts in-process.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Recipe lookup
- **WHEN** fill is requested for a project that pins sky130A
- **THEN** Hefesto SHALL use the `magic-tiled-style` recipe with style `wafflefill(tiled)` loaded from `sky130A-GDS.tech`

### Requirement: sky130 tiled fill generation
For a `magic-tiled-style` recipe, Hefesto SHALL produce the same fill geometry and the same fill cell structure as open_pdks `generate_fill.py`. Units are internal units with `scalegrid 1 2` and `snap internal`. `S` is the tile size (700 µm) converted to internal units.

1. Load the input GDS with `gds rescale false` and `gds drccheck no`. Load the top cell (named after the file stem) dereferenced.
2. The extent is the top cell's `FIXED_BBOX` property if present, otherwise its bounding box: `(xbase, ybase, xmax, ymax)`.
3. Compute `xtiles = ceil((xmax − xbase)/S)` and `ytiles = ceil((ymax − ybase)/S)`, using real division.
4. For `y` in `0..ytiles−1` (outer) and `x` in `0..xtiles−1` (inner):
   - `xlo = xbase + x·S`, `ylo = ybase + y·S`, `xhi = xlo + S`, `yhi = ylo + S`.
   - If `xhi > (xmax − xbase)` then `xhi = xmax − xbase`. Likewise for `yhi`. This compares an absolute coordinate with a width; see Q-TAPEOUT-4.
   - Grow the box `(xlo,ylo,xhi,yhi)` by 1.6 µm on every side.
   - Flatten that area, without labels, into a new cell `<top>_fill_pattern_<x>_<y>`.
   - In that cell, set `GDS_FILE` to empty, erase `COMMENT`, and paint `COMMENT` over exactly `(xlo,ylo,xhi,yhi)`.
   - Write that cell's GDS with output style `wafflefill(tiled)` to `<top>_fill_pattern_<x>_<y>.gds`.
   - Delete the cell.
5. Create a cell `<top>_fill_pattern`. Place each tile `<top>_fill_pattern_<x>_<y>` at the origin, with identity transform. Each tile is a read-only abstract whose `FIXED_BBOX` is `(xlo ylo xhi yhi)` without clipping, and which carries `GDS_FILE` and `GDS_START 0` properties.
6. Write `<top>_fill_pattern.gds.gz` with hierarchical and array generation disabled and compression level 9. Write it into `gds/` if that directory exists next to the input, else into `../gds/` if that exists, else next to the input.

Hefesto SHALL also offer this flow on `.mag` input, as open_pdks `generate_fill_mag.py` does. The PDK's `.magicrc` is used instead of the `-GDS.tech` file. When the layout contains abstract views of vendor GDS, Hefesto SHALL warn that implant and thick-oxide layers may be missing (open_pdks documents this hazard). The intermediate scripts, the multi-process mode and the temporary files of the reference are host mechanics and are informative. Hefesto MAY process tiles in parallel, provided the output is identical.

Compatibility: exact

(open_pdks ref: sky130/custom/scripts/generate_fill.py:180-376, sky130/custom/scripts/generate_fill_mag.py:18-25)

#### Scenario: Tile count and names
- **GIVEN** top cell `chip`, with `FIXED_BBOX` 0 0 2920 µm 3520 µm
- **WHEN** sky130 fill is generated
- **THEN** there SHALL be 5 × 6 = 30 tiles named `chip_fill_pattern_0_0` … `chip_fill_pattern_4_5`, and the output SHALL be `chip_fill_pattern.gds.gz`, whose top structure `chip_fill_pattern` instances the 30 tiles at the origin

#### Scenario: Last tile clipped
- **GIVEN** the same extent
- **WHEN** tile x=4 is processed
- **THEN** its `COMMENT` box SHALL span x from 2800 µm to 2920 µm

### Requirement: sky130 wafflefill style semantics
The fill geometry of a `magic-tiled-style` recipe SHALL be the result of evaluating the PDK's `wafflefill` output style. Evaluation SHALL follow capabilities `mask-output-styles` and `mask-output-operators` exactly, on the flattened tile. In particular, Hefesto SHALL implement these behaviors of the style used by sky130:
- **topbox.** In variant `(tiled)`, `topbox` is the `COMMENT` layer shrunk by 1.5 µm. The keepout between adjacent tiles is therefore 3 µm. In variant `()`, `topbox` is `bbox top`.
- **Slots.** Every fill templayer starts from `topbox` and applies `slots` with the recipe's parameters. It then subtracts its obstruction layer with `and-not`, intersects with `topbox` again, and finally applies `shrink d` and `grow d`. This drops any fill piece narrower than 2·d.
- **Pass order.** Later passes are obstructed by earlier passes. For FOM/POLY the order is: FOM pass 1, POLY pass 1, FOM pass 2, POLY coarse, POLY medium, POLY fine, FOM coarse, FOM fine. LI and met1–met5 use coarse, medium, fine (and very fine) passes.
- **Layer mapping.** Fill is written to the GDS layer/datatype given by the style's `calma` statements. In `sky130A-GDS.tech` these are the alternate fill layers 65/99 (diff), 66/99 (poly), 67/99 (li1) and 68/99 to 72/99 (met1–met5). The older style in `sky130A.tech` writes 23/28, 28/28, 56/28, 36/28, 41/28, 34/28, 51/28 and 59/28. Hefesto SHALL use whichever the recipe's tech file defines. See Q-TAPEOUT-9.
- **LI obstruction.** LI fill SHALL be obstructed by `obsm1` as well, because sky130 has no LI fill-blockage layer.

Compatibility: exact

(open_pdks ref: sky130/magic/sky130gds.tech:904-1460, sky130/magic/sky130-cifout.tech:1217-1500)

#### Scenario: Narrow gap gets no fill
- **GIVEN** a tile whose only free met1 area is a 1.5 µm-wide channel between obstructions
- **WHEN** met1 fill is generated
- **THEN** no met1 fill SHALL be placed in that channel (verify against Magic)

### Requirement: gf180mcu metfill style
For a `magic-paint-style` recipe, Hefesto SHALL evaluate the named style over the top cell and paint each fill layer back into a fill cell as the corresponding fill type, as `cif paint LAYER TYPE CELL` does (capability `cif-format-io`). The gf180mcu `metfill` style (scalefactor 50 nm, gridlimit 5) SHALL be evaluated exactly:
- **Fill geometry.** `fill_geometry_off0`, `fill_geometry_off1` and `fill_geometry_off2` are `bbox top` followed by `slots 0 2000 1000 0 2000 1000 1000 <offset>`, with offsets 0, 660 and 1320. With `THICKMET3P0`, `fill_geometry_thick` uses `slots 0 6000 2000 0 6000 2000 3000 2000`.
- **Obstruction.** For each layer L, `obstruct_L` is `L ∪ fillL ∪ obsL ∪ pads`, followed by `grow 2010`, `shrink 500`, `grow 500`.
- **Fill layers.** Each is `fill_geometry_offK and-not <obstructions>`, followed by `shrink 995`, `grow 995`:

| layer | geometry | obstructions |
|---|---|---|
| difffill | off0 | none |
| polyfill | off1 | m1 |
| met1fill | off2 | poly, m1, m2 |
| met2fill | off0 | m1, m2 |
| met3fill | off1 | m2, m3 |
| met4fill | off2 | m3, m4 |
| met5fill | off0 | m4, m5 |
| mettpfill | off1 | m5, mtp |

The fill layers SHALL be conditioned on the metal-stack defines exactly as in the source. The `difffill` and `polyfill` obstructions as written look accidental. They SHALL be reproduced, and the fill SHALL be flagged by the DRC step (see Q-TAPEOUT-6).

Compatibility: exact

(open_pdks ref: gf180mcu/magic/gf180mcu-cifout.tech:729-904)

#### Scenario: Staggered metal fill
- **GIVEN** an empty 100 µm × 100 µm gf180mcuD top cell
- **WHEN** `metfill` is evaluated
- **THEN** met1fill SHALL be offset by 1320 scaled units relative to met2fill's grid origin, and met2fill SHALL equal the slot array of `fill_geometry_off0` (verify against Magic)

### Requirement: Native fill for KLayout-only PDKs
For `native` recipes, Hefesto SHALL generate rectangular fill shapes that satisfy every fill rule in the recipe:
- fill shape width and length bounds
- fill-to-fill spacing
- fill-to-drawn spacing per layer
- keepout from blockage layers (`*.nofill`, `NoFillerStack`, seal ring, `prBoundary` margin)
- per-window density targets

The algorithm SHALL work as follows:
1. Tile the chip into windows of the recipe's density window size.
2. Compute the free area per window, as the window minus the bloated drawn geometry and blockages.
3. Place fill cells on a fixed lattice anchored at the chip origin, so that the output is deterministic, until the window's density reaches the recipe target or no legal site remains.

Fill SHALL go on the PDK's filler datatypes (for IHP, `<layer>.filler`, datatype 22 as in `Activ` 1/22). The result SHALL be checked against the PDK's density limits (see Requirement: Density rule sets). Hefesto SHALL NOT claim equality with IHP's KLayout filler scripts (see Q-TAPEOUT-8).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: IHP metal fill meets window minimum
- **GIVEN** an ihp-sg13g2 chip of 1000 µm × 1000 µm with 5% Metal2 drawn coverage
- **WHEN** native fill runs
- **THEN** every 800 µm × 800 µm density window SHALL reach Metal2 coverage (drawn plus filler) of at least 25% and at most 75% (IHP rules M2Fil.h and M2Fil.k), unless blockages prevent it, in which case the window SHALL be reported

### Requirement: Fill blockage and protected regions
Every recipe SHALL treat these as obstructions:
- the PDK's fill-blockage layers (sky130 `fillblock`, `obsm*`; gf180 `obs*`; IHP `*.nofill`)
- the seal ring cell area (capability `tapeout-seal-ring`)
- any region a shuttle profile reserves (capability `tapeout-shuttle-packages`)

Hefesto SHALL let the user paint fill-blockage on any layer in the editor. The user SHALL be able to mark a cell instance as "no fill over", which adds its bounding box to every fill layer's obstruction. Hefesto SHALL never place fill that overlaps drawn geometry on the same mask layer.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Analog block protected
- **GIVEN** an instance marked "no fill over"
- **WHEN** fill runs
- **THEN** no fill shape on any layer SHALL intersect that instance's bounding box

### Requirement: Fill output and regeneration
Generated fill SHALL be stored in a separate cell. For sky130 this is `<top>_fill_pattern`, as in the reference. Other PDKs use `<top>_fill`. The fill cell is instanced once in the top cell at the origin with identity transform. Fill SHALL never be merged into drawn cells. Regenerating fill SHALL replace the previous fill cell and its tiles atomically, within one undo group. Fill SHALL be stale whenever any drawn geometry in the top hierarchy changes after generation. The sign-off checklist SHALL report stale fill (capability `tapeout-signoff-checklist`). The fill cell SHALL be shown by default as a dimmed overlay that can be toggled.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Edit after fill
- **GIVEN** fill was generated
- **WHEN** a metal2 wire is added
- **THEN** the fill status SHALL become stale, and the checklist's fill item SHALL show "stale"

### Requirement: Window coverage measurement
The density of layer set L in window W SHALL be:

    D(L, W) = Σ area(tiles of generated layer L) / (area(W) · S²)

Here the tiles are generated by output style evaluation over the flattened contents of W (capability `cif-format-io`, `cif coverage`), and `S` is the style scale factor. Areas SHALL be summed as 64-bit integers. If `area(W) = 0` then `D = 0`. Non-Manhattan tiles count as their full rectangle (see Q-MASK-325). The result SHALL be rounded to single precision before use, as Magic returns a `float`. For sky130 the layer sets are those of output style `density`:
- `fom_all` = alldiff ∪ fomfill
- `poly_all`
- `li_all`
- `m1_all` … `m5_all`, each the drawn layer ∪ its fill layer

Compatibility: exact

(Magic ref: cif/CIFsee.c:425-530; open_pdks ref: sky130/magic/sky130-cifout.tech:1192-1215)

#### Scenario: Half-covered window
- **GIVEN** a 70 µm × 70 µm window whose `m1_all` covers 35 µm × 70 µm
- **WHEN** coverage is measured
- **THEN** D SHALL be 0.5

### Requirement: sky130 stepped density check
Hefesto SHALL reproduce open_pdks `check_density.py`. `T` is 70 µm in internal units.

1. The extent is `FIXED_BBOX` of the top cell, or else the box after `select top cell`.
2. `xtiles = ceil(W/T)` and `ytiles = ceil(H/T)`.
3. `xfrac = 1 − (xtiles·T − W)/T` and `yfrac` likewise. A value of exactly 0.0 becomes 1.0.
4. For each tile, in row-major order (y outer, x inner), flatten the tile box without labels into cell `tile`. Measure D for FOM, POLY, LI1 and MET1 to MET5 over that cell's bounding box (see Q-TAPEOUT-5). Delete the cell.
5. If `xtiles < 10` or `ytiles < 10`, fail with `Layout is < 700um x 700um;  cannot run density checks.`
6. For each window origin `(x, y)`, with `0 ≤ x ≤ xtiles−10` and `0 ≤ y ≤ ytiles−10`:
   - `fx = xfrac` if `x = xtiles−10`, else 1. `fy` likewise.
   - `A = 81 + 9·fx + 9·fy + fx·fy`.
   - D_win = (Σ of the 9×9 full tiles + fx·Σ of column x+9, rows y..y+8 + fy·Σ of row y+9, columns x..x+8 + fx·fy·tile(x+9,y+9)) / A.
7. Global density: `A_g = (xtiles−1)(ytiles−1) + (ytiles−1)·xfrac + (xtiles−1)·yfrac + xfrac·yfrac`. The numerator is weighted the same way over all tiles.

Limits (strict `<` min, else strict `>` max):

| layer | min | max |
|---|---|---|
| FOM | 0.33 | 0.57 |
| POLY | none | none |
| LI1, MET1–MET4 | 0.35 | 0.60 |
| MET5 | 0.45 | 0.76 |

Output SHALL be, in this order:
- `Stepped area density results (total tiles = N):` with N = (ytiles−9)(xtiles−9)
- `Side adjustment = %.3f` and `Top adjustment = %.3f`
- per layer, a header `<LAYER> Density:`, then `Tile (x, y):   %.3f` for every window, with x inner, and `***Error:  <LAYER> Density < NN%` or `> NN%` after a failing window
- `Whole-chip (global) density results:` and per layer `<LAYER> Density: %.3f`, with the same error lines

Compatibility: exact

(open_pdks ref: sky130/custom/scripts/check_density.py:180-275, sky130/custom/scripts/check_density.py:394-825)

#### Scenario: Exactly 700 µm square
- **GIVEN** a 700 µm × 700 µm layout where every 70 µm tile has MET1 coverage 0.5
- **WHEN** the density check runs
- **THEN** `total tiles = 1`, `Side adjustment = 1.000`, the single window SHALL report `Tile (0, 0):   0.500`, and no MET1 error SHALL be printed

#### Scenario: Fractional last column
- **GIVEN** a 735 µm × 700 µm layout
- **WHEN** the density check runs
- **THEN** xtiles SHALL be 11, xfrac SHALL be 0.500, window (1, 0) SHALL use A = 95.0, and the global A_g SHALL be 105.0

#### Scenario: Too small
- **GIVEN** a 500 µm × 900 µm layout
- **WHEN** the density check runs
- **THEN** it SHALL fail with `Layout is < 700um x 700um;  cannot run density checks.`

### Requirement: Density rule sets
Each PDK profile SHALL define density rules as data. Each rule has:
- id
- layer expression (drawn ∪ fill datatypes)
- scope: `global` over the chip boundary, or `window` with a size and a step
- min and/or max (inclusive bounds; a violation is strictly outside)
- the source deck rule name

The built-in rule sets SHALL include:

| PDK | rule | scope | bound |
|---|---|---|---|
| sky130 | check_density FOM/LI/MET (see above) | window 700 µm, step 70 µm, and global | as above |
| sky130 | li1/m1/m2/m3/m4 `.pd.1d` (precheck) | global over boundary 235/4 | density ≤ 0.60 (clear area ≥ 0.40) |
| sky130 | m5 `.pd.1d` (precheck) | global over boundary 235/4 | density ≤ 0.76 (clear area ≥ 0.24) |
| gf180mcu | COMP `comp.density` (precheck) | global over boundary | density ≤ 0.70 |
| ihp-sg13g2 | AFil.g / AFil.g1 | global | 0.35 ≤ Activ ≤ 0.55 |
| ihp-sg13g2 | AFil.g2 / AFil.g3 | window 800 µm, step 400 µm | 0.25 ≤ Activ ≤ 0.65 |
| ihp-sg13g2 | GFil.g | global | GatPoly ≥ 0.15 |
| ihp-sg13g2 | M1.j/k, Mn.j/k (n = 2..5) | global | 0.35 ≤ Mn ≤ 0.60 |
| ihp-sg13g2 | MnFil.h/k (n = 1..5) | window 800 µm, step 400 µm | 0.25 ≤ Mn + filler ≤ 0.75 |
| ihp-sg13g2 | TM1.c/d, TM2.c/d | global | 0.25 ≤ TMn ≤ 0.70 |

Global density SHALL use the merged area of the layer expression. Overlapping shapes are counted once. The sky130 KLayout precheck sums unmerged polygon areas in deep mode. Hefesto SHALL report both values when they differ (see Q-TAPEOUT-10). The IHP window placement SHALL reproduce the deck's "backup" edge windows: an extra top row and an extra right column when the chip is not covered by whole steps. The gf180mcu window rules of the PDK density deck SHALL be added once their values are captured (see Q-TAPEOUT-7).

Compatibility: behavioral

(mpw_precheck ref: checks/drc_checks/klayout/met_min_ca_density.lydrc:68-131, checks/drc_checks/klayout/gf180mcu_density.lydrc:17-31; IHP ref: ihp-sg13g2/libs.tech/klayout/tech/drc/rule_decks/density.drc:438-530, ihp-sg13g2/libs.tech/klayout/tech/drc/docs/precheck_rules.md:1-80)

#### Scenario: sky130 met2 over-density
- **GIVEN** a sky130 chip boundary of 1 mm² with merged met2 ∪ met2fill area of 0.62 mm²
- **WHEN** global density rules run
- **THEN** rule `m2.pd.1d` SHALL fail with density 0.620 > 0.600

#### Scenario: IHP edge window
- **GIVEN** an ihp-sg13g2 chip 1000 µm wide
- **WHEN** the 800/400 µm Activ window rules run
- **THEN** windows SHALL start at x = 0 and, because 800 + 0·400 < 1000, a right-edge backup column ending at x = 1000 SHALL also be evaluated

### Requirement: Density boundary selection
The area for global density, and the region that windows tile, SHALL be chosen in this order:
1. the PDK profile's boundary layer in the top cell (sky130 `prBoundary` 235/4; IHP `prBoundary`, then `EdgeSeal` boundary; gf180mcu `PR_bndry`)
2. the top cell's `FIXED_BBOX`
3. the top cell's bounding box

The chosen source SHALL be stated in the report. When both IHP boundary layers exist and their areas differ, a warning SHALL be reported, and `prBoundary` SHALL be used.

Compatibility: behavioral

(IHP ref: ihp-sg13g2/libs.tech/klayout/tech/drc/rule_decks/density.drc:543-602)

#### Scenario: No boundary layer
- **GIVEN** a sky130 top cell with neither 235/4 nor `FIXED_BBOX`
- **WHEN** global density runs
- **THEN** the cell bounding box SHALL be used, and the report SHALL say so

### Requirement: Density report
Every density run SHALL produce:
- a structured report (JSON) with PDK, version, rule set, boundary source, per-rule results (window origin, size, measured value with 3 decimals, bound, pass or fail) and global values
- a human-readable table
- for `exact` recipes, the reference text output byte for byte
- an on-canvas heat map per layer, with failing windows outlined and selectable

The JSON SHALL list windows in row-major order, y then x ascending, so that it is deterministic (capability `hefesto-output-determinism`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Navigate to failing window
- **WHEN** the user selects a failing MET3 window in the report
- **THEN** the view SHALL zoom to that window and show its coverage value

### Requirement: In-process execution on every platform
Fill generation and every density rule SHALL run in-process with no external tools. This applies on Linux, macOS and iPadOS. The reference scripts are host mechanics and are informative. Hefesto SHALL NOT invoke a Magic subprocess, Python, Tcl wrapper scripts, `/tmp` files or multiprocessing pools. Long runs SHALL report progress per tile and SHALL be cancellable. Cancelling SHALL leave the previous fill cell unchanged.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Fill on iPad
- **GIVEN** a sky130A project on iPadOS
- **WHEN** fill is generated
- **THEN** the result SHALL be identical to the result on macOS for the same input
