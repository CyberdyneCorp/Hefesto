# Tapeout Seal Ring

## Purpose
Defines how Hefesto generates the die seal ring for each supported PDK and places it around a chip. Where the PDK ships a Magic-based generator, Hefesto reproduces its geometry exactly:
- sky130 uses open_pdks `sky130_gen_sealring.py`.
- IHP sg13g2 uses the Magic `sealring_side.tcl` and `sealring_corner.tcl` cell scripts.

For gf180mcu, the PDK's only generator is a KLayout Python script. Hefesto implements that generator natively. The geometry is validated by XOR against the reference output (see Q-TAPEOUT-11).

The density boundary and fill keepout are in capability `tapeout-fill-and-density`. Read-only vendor cells and verbatim GDS copy are in capability `gds-write`. The `.mag` properties `GDS_FILE`, `GDS_START` and `FIXED_BBOX` are in capability `layout-properties`.

Reference sources (read-only, never executed):
- open_pdks @ `c6df28c1fccb05c286aa0a15e9c4c7dcd8502607` (`open_pdks ref:`)
- IHP-Open-PDK @ `5e6d592e4002946a4616f798c357f0f3c06cf3b6` (`IHP ref:`)
- LibreLane @ `8494d4fea0ddd9bee12ea5e42a38b59438c2b128` (`librelane ref:`)
- wafer.space gf180mcu-precheck @ `9b4e7dab6af073d02b81ecc368069540d2f67db9` (`ws-precheck ref:`)

## Requirements

### Requirement: Seal ring recipe registry
Hefesto SHALL generate seal rings through a per-PDK seal ring recipe. Each recipe SHALL declare:
- the generator kind: `sky130-quadrant`, `ihp-side-corner` or `native`
- the ring width added on each side of the chip area
- the placement convention
- the marker layer that identifies the seal ring (for gf180mcu, `GUARD_RING_MK` 167/5)
- the manufacturing grid used to snap the requested size (5 nm for sky130, gf180mcu and IHP)

The built-in recipes SHALL be:

| PDK | kind | ring per side | reference |
|---|---|---|---|
| sky130A/B | `sky130-quadrant` | 6 µm outside the chip area | open_pdks `sky130_gen_sealring.py` |
| ihp-sg13g2 | `ihp-side-corner` | see side cell | IHP Magic `sealring_side.tcl`, `sealring_corner.tcl` |
| gf180mcuA–D | `native` | 26 µm (wafer.space) | gf180mcu KLayout `sealring.py` |

A shuttle profile that provides its own seal ring SHALL disable generation for that program (capability `tapeout-shuttle-packages`). Examples are a harness or a frame such as Caravel or a Tiny Tapeout tile.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Seal ring disabled for Tiny Tapeout
- **GIVEN** a project targeting a Tiny Tapeout program profile
- **WHEN** the user opens the seal ring tool
- **THEN** Hefesto SHALL explain that the program supplies the seal ring, and SHALL not generate one

### Requirement: sky130 seal ring dimensions
For the sky130 recipe, the inputs SHALL be the chip width W and height H in µm, given as decimal numbers, plus the `outer` flag. With `outer`, W and H are the outer edge of the seal ring, and 12 is first subtracted from each. Hefesto SHALL then compute, in IEEE double precision:

    fwidth  = W + 12          fheight = H + 12
    swidth  = round(fwidth · 100)       sheight = round(fheight · 100)     (round half to even)
    swidthx5 = 5 · swidth
    dwidth  = trunc(fwidth · 200)       dheight = trunc(fheight · 200)
    qwidth  = round(fwidth · 50)        qheight = round(fheight · 50)
    nslotsX = trunc((fwidth − 25)/25) − 1      nslotsY = trunc((fheight − 25)/25) − 1

`swidth` and `dwidth` deliberately use different rounding (see Q-TAPEOUT-12). The GDS-generation layout is in Magic units with `magscale 1 2`, so 1 unit = 0.005 µm. In those units, `swidth` is half the seal ring outer width.

Compatibility: exact

(open_pdks ref: sky130/custom/scripts/seal_ring_generator/sky130_gen_sealring.py:125-145, sky130/custom/scripts/seal_ring_generator/sky130_gen_sealring.py:360-380)

#### Scenario: 1 mm square chip
- **WHEN** a sky130 seal ring is requested for W = H = 1000
- **THEN** fwidth SHALL be 1012, swidth 101200, dwidth 202400, qwidth 50600 and nslotsX 38

#### Scenario: Outer dimensions
- **WHEN** a sky130 seal ring is requested with `outer`, W = 2932, H = 3532
- **THEN** the computation SHALL use W = 2920 and H = 3520

### Requirement: sky130 seal ring assembly
The sky130 generator SHALL build the ring from the PDK's quarter-ring template cells: `seal_ring_corner`, the 21 `sr_polygonNNNNN` polygon cells, `sealring_slots`, `seal_ring_slots_array` and `nikon_sealring_shape`. They are interpreted in the PDK's seal-ring-only technology `sky130seal_ring`. Hefesto SHALL stretch the templates by substituting tokens in their text:
- In every polygon cell and in `seal_ring_corner`, the token `51200` SHALL become `swidth` and `51210` SHALL become `sheight`.
- In every polygon cell, the token `256000` SHALL become `swidthx5`, because polygon 39 is at scale 10.
- In `seal_ring_corner_abstract`, `25600` SHALL become `qwidth` and `25605` SHALL become `qheight`.

Substitution SHALL replace every occurrence of the token as a substring, as the reference does. The slot arrays SHALL be rewritten as `array 0 <n> 5000 0 0 430`. Here `n` is `nslotsX` for the first `array` line after a line containing `slots_X`, and `nslotsY` otherwise.

The top cell `advSeal_6um_gen`, in tech `sky130seal_ring` with `magscale 1 2`, SHALL contain six instances, in this order:
- `seal_ring_corner_0`: `transform 1 0 0 0 1 0`
- `seal_ring_corner_3`: `transform -1 0 dwidth 0 1 0`
- `seal_ring_corner_1`: `transform 1 0 0 0 -1 dheight`
- `seal_ring_corner_2`: `transform -1 0 dwidth 0 -1 dheight`
- `seal_ring_slots_array_0`: `transform 1 0 0 0 1 0`
- `seal_ring_slots_array_1`: `transform -1 0 dwidth 0 -1 dheight`

Its GDS SHALL be written as `advSeal_6um_gen.gds`. Hefesto SHALL also write an abstract view `advSeal_6um_gen.mag` in tech `<variant>`, without `magscale`. This view has four `seal_ring_corner_abstract` instances, with transforms `1 0 0 0 1 0`, `-1 0 swidth 0 1 0`, `1 0 0 0 -1 sheight` and `-1 0 swidth 0 -1 sheight`, and these properties:
- `LEFview no_prefix`
- `GDS_FILE advSeal_6um_gen.gds`
- `GDS_START 0`
- `FIXED_BBOX 0 0 swidth sheight`

`seal_ring_corner_abstract.mag` SHALL be written next to it. Existing target files SHALL NOT be overwritten unless forced.

Compatibility: exact

(open_pdks ref: sky130/custom/scripts/seal_ring_generator/sky130_gen_sealring.py:52-120, sky130/custom/scripts/seal_ring_generator/sky130_gen_sealring.py:146-330)

#### Scenario: Mirrored corners
- **GIVEN** W = H = 1000
- **WHEN** the ring is generated
- **THEN** instance `seal_ring_corner_2` SHALL have `transform -1 0 202400 0 -1 202400`

#### Scenario: Existing output without force
- **GIVEN** `advSeal_6um_gen.gds` exists in the target folder
- **WHEN** the generator runs without force
- **THEN** that file SHALL be left unchanged, and an error SHALL name it

### Requirement: sky130 seal ring placement
The sky130 seal ring cell SHALL be placed at (0 µm, 0 µm) in the chip top cell. The chip contents, without the ring, SHALL have their lower-left corner at (6 µm, 6 µm). Hefesto SHALL offer a "wrap chip" action, which does the following:
1. Reads the chip area from the top cell's boundary.
2. Generates the ring for that W × H.
3. Creates a new top cell `<top>_sealed`.
4. Places the ring at the origin and the original top at (6 µm, 6 µm).

The seal ring cell SHALL be read-only: it is an abstract view backed by its GDS. It SHALL be copied verbatim on GDS output (capability `gds-write`).

Compatibility: behavioral

(open_pdks ref: sky130/custom/scripts/seal_ring_generator/sky130_gen_sealring.py:336-338)

#### Scenario: Wrap chip
- **GIVEN** top cell `chip`, with boundary 0 0 1000 µm 1000 µm
- **WHEN** "wrap chip" runs for sky130A
- **THEN** `chip_sealed` SHALL contain `advSeal_6um_gen` at (0, 0) and `chip` at (6 µm, 6 µm), and its extent SHALL be 1012 µm × 1012 µm

### Requirement: IHP seal ring cells
For ihp-sg13g2, Hefesto SHALL generate the cells `sealring_side<suffix>` and `sealring_corner` with exactly the geometry of the PDK's Magic scripts. All values are in Magic internal units of the ihp-sg13g2 technology. `L` is the side length, with default 21600.

`sealring_side`, drawn for the bottom side. Each layer is listed with its rectangles:

| layer | rectangles (x1 y1 x2 y2) |
|---|---|
| pwell | −26 −26 L+26 726 |
| sealcont | 0 260 L 292 |
| psubdiff | 0 292 L 700; 0 0 L 260 |
| metal1 … metal4 | 0 298 L 700; 0 0 L 260 |
| sealvia1 … sealvia4 | 0 260 L 298 |
| metal5 | 0 344 L 700; 0 0 L 260 |
| sealvia5 | 0 260 L 344 |
| metal6 | 0 440 L 700; 0 0 L 260 |
| sealvia6 | 0 260 L 440 |
| metal7 | 0 0 L 700 |
| seal | 0 −1440 L −600 |

with `FIXED_BBOX 0 −1440 L 700`.

`sealring_corner` SHALL be the 219 painted rectangles of the PDK's corner script, in script order, with `FIXED_BBOX −1440 −1440 4200 4200`. The PDK scripts provide cells only. Hefesto SHALL assemble the ring:
- one corner at each chip corner, rotated by 0°, 90°, 180° and 270°
- one side cell per edge, rotated to face outward, with `L` set to that edge's length minus the two corner spans and suffix `_h` or `_v` when the chip is not square

See Q-TAPEOUT-13 for agreement with IHP's KLayout seal ring PCell.

Compatibility: exact

(IHP ref: ihp-sg13g2/libs.tech/magic/sealring_side.tcl:18-93, ihp-sg13g2/libs.tech/magic/sealring_corner.tcl:18-469)

#### Scenario: Default side cell
- **WHEN** `sealring_side` is generated with no width set
- **THEN** its metal7 SHALL be the rectangle 0 0 21600 700, and its `FIXED_BBOX` SHALL be 0 −1440 21600 700

#### Scenario: Custom side length
- **WHEN** `sealring_side_long` is generated with L = 50000
- **THEN** its pwell SHALL be −26 −26 50026 726

### Requirement: Native gf180mcu seal ring
For gf180mcu, Hefesto SHALL generate the seal ring natively. The generator takes the die width and height and produces the same layers and geometry as the PDK's KLayout `sealring.py` generator, which LibreLane runs as the `KLayout.SealRing` step. Conformance SHALL be shown by XOR with zero difference against reference GDS. The reference GDS is produced outside Hefesto and stored in the golden test set. The generated ring SHALL include the `GUARD_RING_MK` marker (167/5) over the ring, because the wafer.space precheck rejects a die without it. When used for wafer.space, the ring SHALL add 26 µm on each side of the user project area.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15; librelane ref: librelane/steps/klayout.py:898-1018; ws-precheck ref: scripts/klayout/check_size.py:8-16, scripts/klayout/check_size.py:64-73)

#### Scenario: wafer.space 1x1 die
- **GIVEN** a gf180mcuD user project of 3880 µm × 5070 µm
- **WHEN** the seal ring is generated for wafer.space
- **THEN** the die SHALL be 3932 µm × 5122 µm, and layer 167/5 SHALL be non-empty

### Requirement: Seal ring input validation
Requested dimensions SHALL be positive. After the recipe's ring width is added, they SHALL be multiples of the manufacturing grid. Otherwise Hefesto SHALL offer the nearest larger on-grid size. Each side SHALL be at least long enough for the recipe's corners and one slot pitch:
- sky130: fwidth ≥ 50 µm, so that `nslots` ≥ 0
- IHP: each side at least two corner spans

Smaller requests SHALL be rejected with the minimum stated.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: sky130 too small
- **WHEN** a sky130 ring is requested for W = 20
- **THEN** generation SHALL fail, stating that the minimum chip width is 38 µm

### Requirement: Seal ring interaction with fill, density and checks
After a seal ring is placed:
- its area SHALL be an obstruction for every fill recipe
- the boundary for density checks SHALL be the outer edge of the seal ring, unless the PDK profile names a boundary layer that is present
- the sign-off checklist SHALL verify that a seal ring is present for programs that require one

Under a seal ring, DRC errors produced by the Magic DRC deck SHALL be reported like any others. They SHALL NOT be auto-waived.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: No fill under ring
- **GIVEN** a sealed sky130 chip
- **WHEN** fill is generated
- **THEN** no fill shape SHALL intersect the `advSeal_6um_gen` bounding region inside the ring's 6 µm band (verify against Magic)

### Requirement: Reference generator host mechanics
The reference sky130 generator is a Python script with these behaviors:
- It copies its templates into `temp/`, or into `/tmp/sky130_sealring` when `temp/` exists.
- It runs `magic -dnull -noconsole generate_gds.tcl`.
- It deletes the temporary directory unless asked to keep it.

These are host mechanics. Hefesto SHALL run the generator in-process on every platform, including iPadOS, without temporary directories visible to the user.

Compatibility: informative

(open_pdks ref: sky130/custom/scripts/seal_ring_generator/sky130_gen_sealring.py:76-112, sky130/custom/scripts/seal_ring_generator/sky130_gen_sealring.py:296-330)

#### Scenario: iPad generation
- **WHEN** a sky130 seal ring is generated on iPadOS
- **THEN** the output GDS SHALL be identical to the macOS output for the same inputs, apart from the GDS date records
