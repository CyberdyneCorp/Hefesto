# Font Labels

## Purpose

Defines rendered ("font") labels: labels drawn with a vector outline font at a given size, rotation and offset, as opposed to default labels drawn with a fixed screen font. Covers loading outline fonts from FontForge-generated Type 3 PostScript files (FreeSans, FreeSerif, FreeMono), the glyph model, the exact computation of a font label's geometry (corners and bounding box), how that geometry affects cell bounding boxes and searches, transformation of font labels under instance transforms, the font-related `setlabel`/`label` behaviour, and the GDS representation of font attributes.

Related capabilities (referenced, not duplicated): `cmd-labels-properties` (general `label`/`setlabel` commands, label text/layer/sticky/port semantics), `cell-file-format` (`flabel` record layout), `gds-read` and `gds-write` (TEXT element records), `ui-windows-and-views` (on-screen label drawing in general), `plot-output` (plots).

## Requirements

### Requirement: Outline font file loading

The system SHALL load an outline font from a Type 3 PostScript font file, located by searching the current directory and then the system library path, by tokenising the file on whitespace (lines whose first non-blank character is `%` and blank lines are skipped; at most 510 characters per line are significant) and interpreting tokens as follows:

1. Before the glyph dictionary: the token `/FontBBox` is followed by four numbers (a leading `{` on the first is ignored) giving the font extents llx lly urx ury, each rounded to the nearest integer.
2. The token `CharProcs` followed by `begin` starts the glyph dictionary and creates a new font appended to the font list (index = number of fonts loaded before it). The font's extent top is set to trunc(ury × scale), where `scale` is the load scale argument (default 1.0).
3. Inside the dictionary, a token starting with `/` names the next glyph; the token `{` starts a glyph definition if the name is one of the 95 PostScript names `space` … `asciitilde` (character codes 32–126, standard Adobe names in code order, with `quoteright` for code 39 and `quoteleft` for code 96) or `.notdef` (stored at code 127); other glyphs are skipped.
4. Inside a glyph, numeric tokens are pushed (rounded to nearest integer) into a 16-entry ring; `setcachedevice` takes the last six as advance-x, advance-y, bbox llx, lly, urx, ury. After it, each `moveto`/`lineto` (and any other non-numeric token other than `curveto`, `closepath`, `}`) appends the last two numbers as a path point; `curveto` appends the last six as two control points and an end point and replaces the two control points by the curve's approximation (requirement "Bezier flattening"); `closepath` closes the current path into one glyph contour; `}` ends the glyph.
5. `currentdict` followed by `end` ends the dictionary; the token `definefont` ends parsing and names the font with the most recent `/`-prefixed token (without the slash), truncated at its last `.`.

Loading fails (and no font is added) if the file cannot be opened or contains no glyph dictionary. Glyph coordinates are in font units (1000 per em for the supplied fonts).

Compatibility: exact

(Magic ref: database/DBlabel.c:1280-1320, database/DBlabel.c:1586-1824, magic/proto.magicrc.in:315-317)

#### Scenario: Load FreeSans
- **GIVEN** the supplied file `FreeSans.pt3` whose header has `/FontBBox {-631 -462 1632 1231 }` and ends with `/FreeSans exch definefont`
- **WHEN** the font is loaded with scale 0.58
- **THEN** a font named `FreeSans` is appended with extents (-631, -462, 1632, 713)
- **AND** glyph `A` has advance (666, 0) and bounding box (17, 0, 653, 729)

#### Scenario: Character outside the printable range
- **GIVEN** a loaded font
- **WHEN** a label contains a character with code below 32 or above 126
- **THEN** the `.notdef` glyph metrics (code 127) are used for it

### Requirement: Glyph contour construction

Each glyph contour SHALL be the sequence of its path points with consecutive duplicate points removed and, if the last point equals the first, the last point removed; contours are closed polygons, and a glyph is the ordered list of its contours, rendered with the nonzero winding fill rule. A glyph with no contours (e.g. `space`) only advances.

Compatibility: exact

(Magic ref: database/DBlabel.c:1736-1780, graphics/grTCairo3.c:472-498)

#### Scenario: Exclamation mark contours
- **GIVEN** glyph `exclam` of FreeSans with first path 208 729, 208 391, 186 168, 147 168, 125 391, 125 729, 208 729
- **WHEN** the glyph is built
- **THEN** its first contour has the six points (208,729) (208,391) (186,168) (147,168) (125,391) (125,729)

### Requirement: Bezier flattening

A cubic Bezier segment from P0 through control points P1, P2 to P3 SHALL be replaced by the points B(t) for t = k/6, k = 1..5, followed by P3, where with c = 3(P1 − P0), b = 3(P2 − P1) − c, a = (P3 − P0) − c − b (single precision) B(t) = a·t³ + b·t² + c·t + P0, each coordinate truncated toward zero to an integer, and a point equal to the previously emitted point omitted.

Compatibility: exact

(Magic ref: database/DBlabel.c:1188-1276, database/DBlabel.c:1782-1820)

#### Scenario: Straight-line curve
- **GIVEN** a curveto from (0,0) with controls (0,0) and (60,0) to (60,0)
- **WHEN** it is flattened
- **THEN** the emitted points are (4,0) (15,0) (30,0) (44,0) (55,0) (60,0)

### Requirement: Font label attributes

A font label SHALL carry, in addition to the ordinary label attributes (text, attachment rectangle, justification, layer, flags, port), a font index (−1 means the default non-outline font), a size, a rotation in integer degrees and an offset point; size and offset are stored in units of 1/8 internal unit. Creating a label with a font index outside [0, number of loaded fonts) makes it a default-font label with size 0, rotation 0 and offset (0,0). When justification is not given, it is chosen from the label's position relative to the cell bounding box exactly as for default labels (see `cmd-labels-properties`).

Compatibility: exact

(Magic ref: database/DBlabel.c:135-229, database/fonts.h:20-40)

#### Scenario: Out-of-range font index
- **GIVEN** two fonts are loaded
- **WHEN** a label is created with font index 5, size 80 and rotation 45
- **THEN** the stored label has font −1, size 0, rotation 0 and offset (0,0)

### Requirement: Font label geometry

For a font label with font F, size s (1/8 units), justification J, offset (ox, oy) (1/8 units), rotation r and attachment rectangle R, the system SHALL compute its four corners and bounding box exactly as follows (all integer arithmetic truncates toward zero, `>>` is an arithmetic shift):

1. For text characters c1..cn with glyph advances adv_i and glyph boxes (bx1, by1, bx2, by2)_i: W = Σ_{i<n} adv_i.x + bx2_n; Y1 = min(0, min_i by1_i); Y2 = max(0, max_i by2_i, extent top of F). Start box (0, Y1, W, Y2).
2. Scale: each of the four values v becomes trunc(v × s / Y2).
3. Vertical justification: for J in {S, SW, SE}: ybot ← ybot − ytop, ytop ← 0. For J in {W, E, CENTER}: ytop ← ytop >> 1, ybot ← ybot − ytop. Otherwise unchanged.
4. Horizontal justification: for J in {W, NW, SW}: xbot ← xbot − xtop, xtop ← 0. For J in {N, S, CENTER}: xtop ← xtop >> 1, xbot ← xbot − xtop. Otherwise unchanged.
5. Add (ox, oy) to both corners.
6. Normalise r once: if r < 0 add 360; if r ≥ 360 subtract 360 (stored back).
7. Corners in order (xbot,ybot), (xtop,ybot), (xtop,ytop), (xbot,ytop) are rotated counter-clockwise by r degrees using θ = r × 0.0174532925 (double), x' = x cosθ − y sinθ, y' = x sinθ + y cosθ, each rounded to nearest (half away from zero). These are the stored corners (1/8 units, relative to the centre of R).
8. Bounding box: min/max of the corners, plus 4 × (R.xbot + R.xtop) in x and 4 × (R.ybot + R.ytop) in y; each bound v is converted to internal units by e = ((v mod 128) ≠ 0) (two's complement low seven bits), v ← v >> 3, then the lower bounds are decremented and the upper bounds incremented when e holds (see Q-MISC-20).

The geometry SHALL be recomputed whenever the text, font, size, rotation, offset, justification or rectangle changes. Default-font labels have no such geometry.

Compatibility: exact

(Magic ref: database/DBlabel.c:1327-1495)

#### Scenario: Centered FreeSans "A"
- **GIVEN** FreeSans loaded with scale 0.58 and a label "A" at point (0,0), justification CENTER, size 80 (10 internal units), offset (0,0), rotation 0
- **WHEN** its geometry is computed
- **THEN** W = 653, Y1 = 0, Y2 = 729, the scaled box is (0, 0, 71, 80), the justified box is (−35, −40, 35, 40)
- **AND** the corners are (−35,−40) (35,−40) (35,40) (−35,40) and the bounding box in internal units is (−6, −6, 5, 6)

#### Scenario: Same label rotated 90 degrees
- **GIVEN** the label of the previous scenario with rotation 90
- **WHEN** its geometry is computed
- **THEN** the corners are (40,−35) (40,35) (−40,35) (−40,−35) and the bounding box is (−6, −6, 6, 5)

#### Scenario: Negative rotation normalised
- **GIVEN** a font label with rotation −90
- **WHEN** its geometry is computed
- **THEN** its stored rotation becomes 270

### Requirement: Font labels in bounding boxes and searches

The rendered bounding box of every font label SHALL be included in the cell's extended (display) bounding box, but not in the cell's geometric bounding box, and only when the cell contains at least one paint tile, label or subcell; a label search for display purposes SHALL also find a font label whose rendered bounding box touches (or, for area searches, overlaps) the search area, while connectivity/attachment searches use only the attachment rectangle.

Compatibility: behavioral

(Magic ref: database/DBcellbox.c:640-675, database/DBcellsrch.c:610-660, database/DBcellsrch.c:745-760)

#### Scenario: Extended bbox grows with text
- **GIVEN** a cell whose only contents are a metal1 box (0,0)-(4,4) and a font label at (2,2) whose rendered bounding box is (−20,−6,24,6)
- **WHEN** the cell bounding boxes are recomputed
- **THEN** the geometric bounding box is (0,0)-(4,4) and the extended bounding box is (−20,−6)-(24,6)

### Requirement: Transforming font labels

When a font label is copied through an instance transform T (copying into a parent, selection, yank/stuff, flattening in place), the system SHALL transform the attachment rectangle and justification as for any label, transform the offset as a vector by T's linear part (no translation), and transform the rotation r as: if T's diagonal terms are both zero (90°/270° rotations, possibly mirrored) add 90 when T's top-right term is positive else 270, and the result is mirrored when T's top-right and bottom-left terms are equal; otherwise add 180 if T's top-left term is negative, and mirror when the top-left and bottom-right terms differ; then subtract 360 if the sum exceeds 360; mirroring maps a to 360 − a when the original angle lies strictly between 90 and 270, else to −a; finally add 360 if negative. Here T maps (x, y) to (a·x + b·y + c, d·x + e·y + f); "top-left", "top-right", "bottom-left", "bottom-right" are a, b, d, e. Size is unchanged. The plain `flatten` command copies rotation and offset untransformed (see Q-MISC-21).

Compatibility: exact

(Magic ref: utils/geometry.c:154-189, database/DBcellcopy.c:1228-1262, select/selOps.c:1160-1172, commands/CmdFI.c:1990-2015)

#### Scenario: Rotation through a 90-degree instance
- **GIVEN** a font label with rotation 30 and offset (8,0) in a child placed with transform (x,y) → (−y, x)
- **WHEN** the label is copied to the parent
- **THEN** the copied label has rotation 300 (= 30 + 270) and offset (0,8)
- **AND** its corners are recomputed with the counter-clockwise rotation of 300°, which does not match the geometric image of the original corners (see Q-MISC-24)

### Requirement: Font-related label commands

The system SHALL provide the following font-related forms (general command semantics are owned by `cmd-labels-properties`):

- `setlabel fontlist` lists the names of loaded fonts in load order.
- `setlabel font <index>` (integer) prints the name of that font (`default` for −1) or reports an out-of-range index.
- `setlabel font <name> [scale]`: if no loaded font has that name (case-insensitive; `default` means −1), the argument is treated as a font file name and loaded with `scale` (default 1.0) — the newly loaded font is then applied only if its name matches the argument; otherwise the font is applied to all selected labels in the edit cell (or to the default-label settings with `setlabel -default`). Applying a font to a label whose size is 0 sets its size to the integer numerator of the lambda-to-internal ratio (in 1/8 units, see Q-MISC-22).
- `setlabel size`, `setlabel offset` and `setlabel rotate` (values in user units stored ×8; integer degrees; queries divide by 8) are specified in capability `cmd-labels-properties` requirements "Setlabel command" and "Label units and attributes" (see Q-MISC-23 and Q-CMDEDIT-90).
- `label <text> <font> [size [rotation [offx offy [justification [layer]]]]]`, where `<font>` is a font name or index: size defaults to the lambda ratio numerator (in 1/8 units) unless defaults were set with `setlabel -default`, size/offset are user-unit values stored ×8.

Compatibility: behavioral

(Magic ref: commands/CmdRS.c:2150-2340, commands/CmdRS.c:2369-2457, commands/CmdRS.c:2527-2557, commands/CmdRS.c:1785-2111, commands/CmdLQ.c:200-345)

#### Scenario: Loading fonts the way the startup file does
- **GIVEN** no fonts loaded
- **WHEN** the user runs `setlabel font FreeSans.pt3 0.58`
- **THEN** the font `FreeSans` is loaded with index 0 and no selected label is changed (the name `FreeSans.pt3` does not match the loaded font name)

#### Scenario: Apply a font to a selected label
- **GIVEN** FreeSans loaded, 1 internal unit per lambda, and a selected default-font label of size 0
- **WHEN** the user runs `setlabel font FreeSans` then `setlabel size 2`
- **THEN** the label's font is FreeSans and its size is 16 (eighth units), and its rendered bounding box is recomputed

### Requirement: Font label rendering

The system SHALL render a font label upright on screen by drawing its glyph contours filled (nonzero rule) along the rotated baseline: glyphs are scaled by (label size on screen) / (font extent top), shifted up by the minimum glyph-box bottom of the text, advanced by each glyph's advance, rotated by the transformed rotation angle, and when that angle lies in [90, 270) the text is rotated by a further 180° and started from the opposite corner so that it reads upright; glyphs whose on-screen size is below 5 pixels are not drawn. The attachment rectangle is drawn as for ordinary labels. Exact pixels are host-dependent.

Compatibility: informative

(Magic ref: dbwind/DBWdisplay.c:785-903, graphics/grTCairo3.c:472-560, graphics/grText.c:50-80)

#### Scenario: Label rotated 180 degrees reads upright
- **GIVEN** a font label with rotation 180
- **WHEN** it is displayed
- **THEN** the glyphs are drawn with rotation 0 starting from the corner that is lower-left on screen

### Requirement: GDS representation of font attributes

The system SHALL represent a font label in GDS as a TEXT element carrying PRESENTATION (font index AND 3 in bits 4–5, justification in the low four bits), STRANS 0, MAG (size / 800 × scalefactor / expander, the text height in micrometres) and, only for a non-zero rotation, ANGLE. The exact records, formulas and rounding are owned by capability `gds-write` requirement "Label TEXT records" (writing) and capability `gds-read` requirement "TEXT element parsing" (reading); this summary does not override them. In addition, when a TEXT element read with a PRESENTATION record names a font index that is not loaded, the label becomes a default-font label.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:2856-2916, calma/CalmaRdpt.c:796-935, calma/CalmaRdpt.c:1088-1092)

#### Scenario: Write MAG for a 1 µm label
- **GIVEN** a cifoutput style with scalefactor 100 (1 internal unit = 1 µm) and expander 1, and a FreeSans (index 0) label of size 8, rotation 0, justification CENTER
- **WHEN** GDS is written
- **THEN** the TEXT element carries PRESENTATION 0x0005, STRANS 0, MAG 1.0 and no ANGLE record
