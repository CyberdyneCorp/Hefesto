# OASIS Reading

## Purpose
Defines how Hefesto reads an OASIS file (the `oasis read` operation, capability `oasis-command`) into layout cells. Magic has no OASIS reader. The governing rule is **GDS equivalence**: every OASIS construct that has a GDSII counterpart SHALL be interpreted exactly as capability `gds-read` interprets that counterpart, with the same `cifinput` style and the same option values. The same layout delivered as GDS (read by `gds read`) and as OASIS (read by `oasis read`) then yields identical cells, geometry, labels, ports, uses, arrays and properties. Timestamps are the exception, because OASIS carries none.

This capability specifies:
- how each OASIS record maps to its GDS-equivalent element;
- how OASIS-only constructs map: shape repetitions, irregular placement repetitions, trapezoids, circles, name tables and standard properties;
- the OASIS variants of the read-only vendor view and its content-hash timestamp.

Byte-level decoding is owned by capability `oasis-format`. Layer mapping (`calma` statements), `labels`, `ignore` and the `cifinput` operators are owned by capability `mask-input-styles`. Option syntax is owned by capability `oasis-command`. Wherever this capability says "as in gds-read", the referenced `gds-read` requirement applies verbatim, including its messages, rounding and open questions.

Terminology (in addition to `gds-read`):
- **GDS-equivalent element**: the GDS element (BOUNDARY, BOX, PATH, TEXT, SREF, AREF), with its records, that this capability assigns to an OASIS element instance.
- **Element instance**: one occurrence of an OASIS element at one displacement of its repetition. An element without a repetition has one instance.
- **mpd**: meters per DB unit, defined for OASIS as `1.0e-6 / unit` (binary64).

## Requirements

### Requirement: Read command and file lookup
The `oasis read <file>` operation SHALL require a `cifinput` style. If none exists, it reports "Nothing in "cifinput" section of tech file." and reads nothing. If no edit cell exists, a new empty edit cell is created first.

Compatibility: native

The file SHALL be searched along the cell search path by appending, in order, `.oas`, `.oas.gz`, `.oasis` and `""` to the given name. The first file that opens is read. As in gds-read, the extension is always appended first, so `foo.oas` is tried as `foo.oas.oas` and so on, and finally as `foo.oas`. Gzip-compressed files are decompressed transparently. If no candidate opens, "Cannot open <f>.oas, <f>.oasis or <f> to read OASIS input." is reported.

The read SHALL NOT insert geometry into the edit cell, and SHALL NOT record undo events. Cells are created by name. After reading, the top cell is displayed (see "Top cell selection").

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Extension search
- **GIVEN** a file `chip.oas` in the current directory
- **WHEN** `oasis read chip` is issued
- **THEN** `chip.oas` is opened and its cells are read

#### Scenario: No input style
- **GIVEN** a technology without a `cifinput` section
- **WHEN** `oasis read chip` is issued
- **THEN** "Nothing in "cifinput" section of tech file." is reported and no cell is created

### Requirement: Two-pass decoding and name resolution
The reader SHALL decode the file in two passes, so that every name and refnum is known before any cell is interpreted.

Compatibility: native

**Pass 1 (index).** Decode the whole file with capability `oasis-format`, decompressing CBLOCKs, without interpreting geometry. Record:
- all name tables (CELLNAME, TEXTSTRING, PROPNAME, PROPSTRING, LAYERNAME, XNAME), with refnums resolved;
- the file-level properties;
- for each CELL record, the cell name (resolved through CELLNAME if it is given by refnum), and the position from which its content can be re-decoded: the file offset of the CELL record, or of the enclosing CBLOCK plus the offset inside its decompressed data;
- the validation result (see capability `oasis-format`).

**Pass 2 (interpretation).** Process the cells in file order, as in gds-read "Structure processing and cell creation". Look-ahead reads (see "Post-order reading") jump directly to the indexed position of the requested cell. No sequential search and no rewinding are needed.

Rules:
1. A CELL refnum, PLACEMENT refnum or TEXT refnum with no definition after pass 1 SHALL be reported. A cell refnum `n` is then named `#<n>` (see Q-OASIS-34). An undefined TEXTSTRING refnum yields an empty string, which is dropped as an empty label.
2. A fatal decoding error in pass 1 (one that "stops decoding" in `oasis-format`) SHALL abort the read before any cell is created, with the error and its byte position reported. Exception: errors located after the last complete CELL record still allow pass 2 for the cells indexed before the error, which keeps the behaviour of gds-read rule 6 ("cells already created are kept").
3. Strict-mode violations and validation failures are reported as warnings and do not change the result.
4. When the START or END record provides table offsets, Hefesto MAY use them to load the name tables before pass 1. The result SHALL be the same.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Name table at the end of the file
- **GIVEN** a file whose CELLNAME table follows all CELL records, as written by capability `oasis-write`
- **WHEN** it is read
- **THEN** every cell is created with its proper name, and no placeholder `#<n>` cell exists

#### Scenario: Truncated file
- **GIVEN** a file truncated in the middle of the second of three cells
- **WHEN** it is read
- **THEN** "unexpected end of file" is reported with the byte position
- **AND** the first cell is created and interpreted, and the other two are not

### Requirement: Units and input scale
The reader SHALL derive the read scale from the START unit exactly as gds-read "Units and input scale" derives it from UNITS, with `mpd = 1.0e-6 / unit` (binary64 division):

Compatibility: exact

```
mpd = 1.0e-6 / unit
cu  = mpd × 1.0e8 × m              (* input units per DB unit; m = input style multiplier *)
if cu >= 1.0:  num = (int)(cu + 0.5);  den = 1
else:          num = 1;                den = (int)(1.0 / cu + 0.5)
```

The ratio `num/den` changes only through automatic rescaling and magnified copies, as in gds-read.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaRead.c:824-920)

#### Scenario: Nanometer OASIS with a centimicron style
- **GIVEN** a START unit of 1000 and an input style multiplier `m = 1`
- **WHEN** the units are processed
- **THEN** `mpd = 1e-9`, `cu = 0.1`, `num = 1` and `den = 10`, the same as a GDS file with UNITS meters-per-DB-unit 1e-9

#### Scenario: Ångström OASIS
- **GIVEN** a START unit of 10000 and `m = 1`
- **WHEN** the units are processed
- **THEN** `num = 1` and `den = 100`

### Requirement: Coordinate conversion
Every coordinate of an element instance SHALL be the OASIS absolute coordinate (after modal and relative-mode resolution, point-list accumulation and repetition displacement) converted exactly as gds-read "Coordinate conversion and automatic rescaling" converts the corresponding GDS XY value, including automatic rescaling, the rescale limit of 100, the rounding rules, and the coordinate multiplier `k` (1 for polygon, rectangle, text and placement points; 2 for path centerline points).

Compatibility: exact

Rules:
1. Any absolute coordinate, width, height or displacement outside the signed 32-bit range SHALL be reported ("coordinate overflow"), and the element instance is dropped. GDS cannot hold such values either.
2. Within one element instance, points are converted in vertex order, so a rescale triggered by a later point multiplies the earlier points, as in gds-read.
3. Path half-width and extension values are converted as in "PATH elements", never through rescaling.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaRdpt.c:85-215, cif/CIFrdcl.c:351-415)

#### Scenario: Off-grid coordinate triggers rescale
- **GIVEN** `num = 1`, `den = 10` and a RECTANGLE with x = 15
- **WHEN** it is read
- **THEN** the units are rescaled by 2 exactly as for a GDS vertex x = 15, and the x coordinate becomes 3 input units

### Requirement: Cell processing
Each CELL record SHALL be processed as one GDS structure, as in gds-read "Structure processing and cell creation", "Duplicate structures within one file", "Cells already in memory before reading", "End-of-structure processing" and "End-of-read cleanup".

Compatibility: exact

Mapping:
1. The structure name is the resolved cell name, used as-is with no sanitizing.
2. The element list is the cell's elements in decoded order. Each element contributes its element instances in repetition order (see "Shape repetitions"), and each instance is processed as its GDS-equivalent element.
3. A cell defined twice in the file, which is illegal OASIS, SHALL be handled by the gds-read duplicate rule, as if the second CELL record were a second BGNSTR with the same STRNAME.
4. `gds-read` messages that mention GDS keep their meaning. Hefesto MAY replace "GDS" by "OASIS" in the text.
5. A cell named in CELLNAME but never defined by a CELL record, and referenced by a PLACEMENT, follows gds-read "End-of-read cleanup" ("cell <name> was used but not defined."). A CELLNAME that is never referenced and never defined creates no cell.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaRdcl.c:372-724, calma/CalmaRdcl.c:418-560, cif/CIFrdcl.c:1734-1840)

#### Scenario: Forward-referenced cell
- **GIVEN** an OASIS file in which cell `TOP` (first) places cell `A`, and `A` is defined after `TOP`
- **WHEN** it is read with default options
- **THEN** the result is identical to reading the GDS file with structure `TOP` (SREF `A`) followed by structure `A`

#### Scenario: noduplicates keeps a pre-loaded cell
- **GIVEN** cell `INV` already loaded from `INV.mag`, `oasis noduplicates yes`, and an OASIS file defining `INV` before any placement of it
- **WHEN** the file is read
- **THEN** cell `INV` keeps its `.mag` contents

### Requirement: Cell timestamps
OASIS has no cell dates. The reader SHALL set the timestamp of each cell it reads from the `datestamp` option as follows:

Compatibility: native

- default (`datestamp yes`) or `datestamp no`: 0. The cell receives the current time when it is next saved, as in gds-read for timestamp 0.
- `datestamp <integer N>`: `N`. If `N != 0`, the cell is also fixed-stamp, as in gds-read.

Read-only cells are overridden by the content hash (see "Content-hash timestamps for read-only OASIS cells"). See Q-OASIS-30 for alternatives.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Default timestamp
- **GIVEN** default options
- **WHEN** an OASIS file is read
- **THEN** every cell read has timestamp 0

#### Scenario: Fixed datestamp
- **GIVEN** `oasis datestamp 1700000000`
- **WHEN** an OASIS file is read
- **THEN** every cell read has timestamp 1700000000 and is fixed-stamp

### Requirement: Layer and datatype mapping
Each geometry element (RECTANGLE, POLYGON, PATH, TRAPEZOID, CTRAPEZOID, CIRCLE) SHALL be assigned an input layer by looking up its (layer, datatype) pair, and each TEXT by its (textlayer, texttype) pair, exactly as in gds-read "Layer and datatype mapping", including the wildcard lookup order and the unknown-layer messages.

Compatibility: exact

Rules:
1. OASIS layer and datatype numbers are unsigned 32-bit values. A value above 255 can only match a `*` wildcard, because `calma` statements accept 0..255 (see Q-OASIS-39).
2. LAYERNAME records SHALL NOT affect the mapping (see Q-OASIS-38). They are kept only for re-encoding read-only cells (capability `oasis-write`).
3. The unknown-layer message kinds are: "Unknown layer/datatype in boundary" for POLYGON, TRAPEZOID, CTRAPEZOID and CIRCLE; "... in box" for RECTANGLE; "... in path" for PATH; "Label on unknown layer/datatype" for TEXT. Each is reported once per (kind, pair) per cell, as in gds-read.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: cif/CIFrdtech.c:184-232, calma/CalmaRdpt.c:1251-1276)

#### Scenario: Datatype wildcard precedence
- **GIVEN** mappings `calma M1 68 20` and `calma M1PIN 68 *`
- **WHEN** a RECTANGLE on 68/20 and a POLYGON on 68/16 are read
- **THEN** the first goes to input layer M1 and the second to M1PIN

#### Scenario: Large layer number
- **GIVEN** a RECTANGLE on layer 1000, datatype 0, and no `*` layer wildcard
- **WHEN** it is read
- **THEN** it is discarded with "Unknown layer/datatype in box, layer=1000 type=0"

### Requirement: Shape repetitions
A geometry or TEXT element with a repetition SHALL be read as one GDS-equivalent element per displacement, in the displacement order defined by capability `oasis-format`. The instance at displacement `d` is the element translated by `d` in DB units, before coordinate conversion.

Compatibility: exact

Rules:
1. Every instance counts as one shape for the `flatten` criterion (gds-read "Flattening small cells on input"). TEXT instances count as neither shapes nor references.
2. Coincident instances (zero spacing) are all painted. Painting is a union, so the result is the same as one instance.
3. With `polygon subcells` or `path subcells`, each qualifying instance creates its own subcell, as in gds-read.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaRdcl.c:733-790)

#### Scenario: Rectangle array
- **GIVEN** a RECTANGLE `[0,10]×[0,10]` on a mapped layer with repetition type 2, `nx = 3`, `sx = 20`
- **WHEN** it is read
- **THEN** the result equals reading three GDS BOUNDARY elements `[0,10]`, `[20,30]` and `[40,50]` in x, in that order

#### Scenario: Flatten count includes repetitions
- **GIVEN** `oasis flatten yes` (limit 10) and a cell with no placements whose only element is a RECTANGLE repeated 12 times
- **WHEN** it is read
- **THEN** the cell is not flatten-marked, because it has 12 shapes and 12 is not less than 10

### Requirement: RECTANGLE elements
A RECTANGLE instance `[x, x+w] × [y, y+h]` SHALL be read as a GDS BOUNDARY with the five points `(x,y) (x+w,y) (x+w,y+h) (x,y+h) (x,y)`. It therefore takes part in pin-shape association (gds-read "Pin shapes and port labels").

Compatibility: exact

A rectangle with `w = 0` or `h = 0` SHALL be read as that degenerate BOUNDARY, which gds-read paints as nothing.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaRdpt.c:235-430)

#### Scenario: Pin rectangle
- **GIVEN** `S = 1`, `labels m1 port` for 68/16 and 68/5, a TEXT `IN` at (5,5) on 68/5, and a RECTANGLE `[0,10]×[0,10]` on 68/16
- **WHEN** read
- **THEN** port label `IN` has rectangle `[0,10]×[0,10]` and port index 1, exactly as with a GDS BOUNDARY

### Requirement: POLYGON, TRAPEZOID and CTRAPEZOID elements
A POLYGON, TRAPEZOID or CTRAPEZOID instance SHALL be read as a GDS BOUNDARY whose points are the decoded vertices in order (see capability `oasis-format`), followed by the first vertex again. This includes the synthesized vertex of type 0/1 point lists and the merging of coincident vertices of trapezoids. The BOUNDARY rules of gds-read then apply: nonzero winding fill, non-Manhattan split-tile decomposition, the "polygon with fewer than 4 points." check, and the `polygon subcells` option.

Compatibility: exact

A closed OASIS polygon never produces the gds-read "Boundary is not closed." message, because the closing point is always supplied.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaRdpt.c:235-430, cif/CIFrdpoly.c:129-345, cif/CIFrdutils.c:1052-1188)

#### Scenario: 45-degree triangle
- **GIVEN** `num = den = 1` and a POLYGON at (0,0) with type 3 deltas E 10 and N 10
- **WHEN** it is read
- **THEN** the input layer holds the triangle (0,0), (10,0), (10,10), as one split tile, as in the gds-read 45-degree triangle scenario

#### Scenario: CTRAPEZOID triangle
- **GIVEN** a CTRAPEZOID type 16 with w = 10 at (0,0)
- **WHEN** it is read
- **THEN** it is read as the BOUNDARY (0,0) (0,10) (10,0) (0,0)

### Requirement: CIRCLE elements
A CIRCLE instance with radius `r > 0` and centre `(cx, cy)` SHALL be read as a GDS BOUNDARY approximating the disk by a regular polygon with `N` vertices, where `N` is the `circlepoints` setting (default 64, minimum 8, see capability `oasis-command`). Vertex `k`, for `k = 0..N−1`, is `(cx + round(r·cos(2πk/N)), cy + round(r·sin(2πk/N)))` in DB units. `round` is round-half-away-from-zero, and `cos`/`sin` are binary64. The first vertex is appended again to close the polygon. Consecutive coincident vertices are merged.

Compatibility: native

A radius of 0 SHALL produce no geometry and no message. See Q-OASIS-31 for the choice of `N` and the inscribed approximation.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Small circle
- **GIVEN** `circlepoints 8` and a CIRCLE with radius 10 at (0,0)
- **WHEN** it is read
- **THEN** the BOUNDARY vertices are (10,0), (7,7), (0,10), (−7,7), (−10,0), (−7,−7), (0,−10), (7,−7), (10,0)

### Requirement: PATH elements
A PATH instance SHALL be read as a GDS PATH with the same (layer, datatype), `WIDTH = 2 × half-width`, the centerline vertices as XY, and PATHTYPE and extensions chosen from the start extension `es` and end extension `ee` as follows:

Compatibility: exact

| Condition | GDS-equivalent |
|---|---|
| `es == 0` and `ee == 0` | PATHTYPE 0 |
| `es == half-width` and `ee == half-width`, half-width > 0 | PATHTYPE 2 |
| otherwise | PATHTYPE 4 with BGNEXTN `es` and ENDEXTN `ee` |

The gds-read rules "PATH elements", "Path-to-polygon conversion", "Path centerline properties" and "Path subcells" then apply unchanged. In particular, end extensions follow gds-read and depend on the `magic-compat` setting: negative extensions are ignored with it ON and shorten the path with it OFF (see Q-OASIS-33 and Q-MASK-114), and a half-width of 0 drops the path.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaRdpt.c:534-750, cif/CIFrdpt.c:339-600)

#### Scenario: Half-width extended path
- **GIVEN** `num = den = 1` and a PATH with half-width 5, scheme half-width/half-width, centerline (0,0) → (100,0)
- **WHEN** it is read
- **THEN** it is read as GDS PATHTYPE 2, WIDTH 10, and the rectangle `x ∈ [−5,105], y ∈ [−5,5]` is painted

#### Scenario: Mixed extensions
- **GIVEN** a PATH with half-width 5, start scheme half-width and end scheme flush, centerline (0,0) → (100,0)
- **WHEN** it is read
- **THEN** it is read as PATHTYPE 4 with BGNEXTN 5 and ENDEXTN 0, and the rectangle `x ∈ [−5,100], y ∈ [−5,5]` is painted

### Requirement: TEXT elements
A TEXT instance SHALL be read as a GDS TEXT element with LAYER = textlayer, TEXTTYPE = texttype, no PRESENTATION, no WIDTH, no STRANS, XY = the text position, and STRING = the resolved text string. The gds-read rules "TEXT element parsing" (the no-PRESENTATION branch), "Label layer assignment" and "Pin shapes and port labels" then apply unchanged.

Compatibility: exact

Consequences:
- the label is a plain (non-font) label with position southeast;
- the label point is `(x / S, y / S)` with truncating division;
- an empty string produces the warning "Ignoring empty string label at (x, y)" and no label;
- a `cell`-kind label layer renames the cell.

OASIS text has no font, size, rotation or anchor, so font labels do not survive a round trip through OASIS (see Q-OASIS-32).

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaRdpt.c:767-960, calma/CalmaRdpt.c:1008-1135)

#### Scenario: Port label from OASIS text
- **GIVEN** `labels metal1 port` on the input layer mapped from 68/5, and TEXTs `A` then `B` on 68/5
- **WHEN** read
- **THEN** `A` is port 1 and `B` is port 2, both sticky with all four directions

### Requirement: Placement transforms
A PLACEMENT instance SHALL be read as a GDS SREF (or AREF, see "Placement repetitions") whose STRANS reflection bit is the flip bit, whose ANGLE is the placement angle in degrees, and whose MAG is the magnification. The gds-read rules "Structure reference transforms", "Instance placement scaling", "Use placement rules" and "Magnified instances" then apply unchanged.

Compatibility: exact

Specifically:
- record 17: angle = `AA × 90`, magnification 1;
- record 18: angle = the decoded real (0 if absent), magnification = the decoded real (1 if absent). The angle goes through gds-read truncation and normalization, so a non-quarter-turn angle is rounded to a quarter turn with the "Non-Manhattan angle" error (see Q-OASIS-12). A magnification `d <= 0` is reported and treated as 1, and `d != 1` produces a magnified copy `<name>_mag<text>`.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaRdio.c:81-205, calma/CalmaRdcl.c:1195-1240, calma/CalmaRdcl.c:826-1058, calma/CalmaRdcl.c:1352-1380)

#### Scenario: Rotated and flipped placement
- **GIVEN** a record 17 placement with AA = 1, F = 1 at (100, 0), with `S = 1` and `num = den = 1`
- **WHEN** read
- **THEN** a child point (1, 2) maps to (102, 1), as in the gds-read rotated and mirrored SREF scenario

#### Scenario: 1.5x placement
- **GIVEN** cell `C` with a rectangle `[0,10]×[0,10]` and a record 18 placement of `C` with magnification 1.5 at (0,0)
- **WHEN** read
- **THEN** cell `C_mag1p5` exists with the box `[0,15]×[0,15]` and the property `GDS_MAG_SOURCE = C`, and the use refers to `C_mag1p5`

### Requirement: Placement repetitions
A PLACEMENT with a repetition SHALL be read as one Magic array use when the repetition is regular and its vectors are axis-aligned in the child coordinate system. Otherwise it SHALL be read as one use per displacement.

Compatibility: exact

Let `T` be the placement transform without translation and magnification (flip, then rotation, rounded to a quarter turn as in "Placement transforms"). The repetition gives vectors and counts `(a, na)` and `(b, nb)` as follows:

| Type | `(a, na)` | `(b, nb)` |
|---|---|---|
| 1 | `((sx,0), nx)` | `((0,sy), ny)` |
| 2 | `((sx,0), nx)` | `((0,0), 1)` |
| 3 | `((0,0), 1)` | `((0,sy), ny)` |
| 8 | `(a, n)` | `(b, m)` |
| 9 | `(a, n)` | `((0,0), 1)` |

Let `u = T⁻¹(a)` and `v = T⁻¹(b)`, in DB units. Then:
1. If (`na == 1` or `u.y == 0`) and (`nb == 1` or `v.x == 0`): the use is an array with x indices `0..na−1`, y indices `0..nb−1`, `xsep = u.x` and `ysep = v.y`.
2. Else if (`na == 1` or `u.x == 0`) and (`nb == 1` or `v.y == 0`): the use is an array with x indices `0..nb−1`, y indices `0..na−1`, `xsep = v.x` and `ysep = u.y`.
3. Otherwise, and for repetition types 4–7, 10 and 11, the placement is read as one SREF-equivalent per displacement, in displacement order. Each one has the placement's transform, translated by that displacement.

The separations are converted from DB units to internal units like reference points (gds-read "Coordinate conversion and automatic rescaling", then "Instance placement scaling"), and may be negative or zero. The array origin is the placement position. The array-index property (attribute 99, see "Instance properties") overrides the index ranges, as in gds-read. With a magnified placement, the array refers to the magnified copy.

Under these rules, the AREF that gds-write produces and the PLACEMENT that oasis-write produces for the same Magic array read back as the same array. See Q-OASIS-10 for zero spacings.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaRdcl.c:1243-1505)

#### Scenario: Simple 3×2 array
- **GIVEN** `S = 1`, `num = den = 1`, and a placement at (0,0) with no rotation and repetition type 1, `nx = 3`, `ny = 2`, `sx = 100`, `sy = 200`
- **WHEN** read
- **THEN** the use is an array with x 0..2 and y 0..1, xsep = 100 and ysep = 200, the same as the gds-read "Simple 3×2 array" scenario

#### Scenario: Rotated array
- **GIVEN** the same with rotation 90 (AA = 1) and repetition type 1, `nx = 2` (`sx = 400`), `ny = 3` (`sy = 100`)
- **WHEN** read
- **THEN** `u = T⁻¹(400,0) = (0,−400)` and `v = T⁻¹(0,100) = (100,0)`, so rule 2 applies
- **AND** the use is an array with x 0..2, y 0..1, xsep = 100 and ysep = −400

#### Scenario: Irregular repetition
- **GIVEN** a placement of `A` with repetition type 4 giving displacements (0,0), (5,0), (12,0)
- **WHEN** read
- **THEN** three separate uses of `A` are created, at x = 0, 5 and 12

### Requirement: Instance properties
A standard property `S_GDS_PROPERTY` attached to a PLACEMENT SHALL be read as the GDS PROPATTR/PROPVALUE pair (attribute = first value, value = second value as a string), in PROPERTY record order. The gds-read rules "Instance properties" then apply unchanged: attributes 61 and 98 give the instance identifier, attribute 99 gives the array index limits, and any other attribute ends property processing for the element.

Compatibility: exact

Rules:
1. A non-standard property, or an `S_GDS_PROPERTY` whose value list is not (unsigned or signed integer, string or propstring refnum), SHALL be ignored, and does not end property processing.
2. When a placement is read as several separate uses (repetition rule 3), every use receives the same identifier, and the end-of-structure uniquing of gds-read resolves the duplicates.
3. Properties attached to geometry and TEXT elements, to cells, to name records and to the file SHALL be discarded, except the standard properties used elsewhere in this capability. See Q-OASIS-37.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaRdcl.c:1385-1445)

#### Scenario: Named instance with array limits
- **GIVEN** a placement with repetition type 2, `nx = 2`, and `S_GDS_PROPERTY` values (61, `x/y`) and (99, `1_2_0_0`)
- **WHEN** read
- **THEN** the use id is `x_y`, and the array x indices are 1..2 and y indices 0..0

### Requirement: Post-order reading and flattening options
The options `ordering`, `flatten`, `flatglob`, `polygon subcells`, `path subcells`, `savepaths`, `maskhints`, `rescale`, `unique`, `noduplicates` and `drccheck` SHALL have exactly the effect specified for them in gds-read. The pass 1 index replaces the file search of gds-read "Post-order reading and file repositioning":

Compatibility: exact

1. Look-ahead for a cell that is defined in the file jumps to its indexed position. It never rewinds and never prints the "Rewinding input" message.
2. Look-ahead for a name that is not defined in the file creates a dereferenced empty cell, with the same message rules as gds-read.
3. A magnified copy re-reads the source cell from its indexed position at the scaled ratio, as in gds-read "Magnified instances".
4. Flatten-marked cells, their saved input planes and their copying into parents behave as in gds-read. For array placements the copies are made per array element, as in gds-read. For separate uses (repetition rule 3) one copy is made per use.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaRdcl.c:119-205, calma/CalmaRdcl.c:584-648, calma/CalmaRdcl.c:1100-1190, calma/CalmaRdcl.c:1527-1636)

#### Scenario: Ordering on with top-down file
- **GIVEN** `oasis ordering on` and a file defining `TOP` (placing `A`) before `A`
- **WHEN** read
- **THEN** `A` is read while processing `TOP`'s placement, and the later `A` cell is skipped with no duplicate error

#### Scenario: Default flatglob
- **GIVEN** default options and a cell `pcell_CDNS_7` with 3 rectangles and no placements, placed by `TOP` and defined before `TOP`
- **WHEN** read
- **THEN** `pcell_CDNS_7` is flatten-marked, its rectangles are copied into `TOP`'s input planes, and `TOP` contains no use of it

### Requirement: Unsupported and extension content
The reader SHALL ignore XNAME, XELEMENT and XGEOMETRY records for interpretation. The first occurrence of each kind per cell SHALL be reported ("XGEOMETRY not supported: skipping."). Their modal side effects (see capability `oasis-format`) still apply. PAD, XYABSOLUTE and XYRELATIVE have no interpretation beyond decoding.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: XGEOMETRY skipped
- **GIVEN** a cell with one XGEOMETRY and one RECTANGLE on a mapped layer
- **WHEN** read
- **THEN** the rectangle is painted and one "XGEOMETRY not supported" message is reported

### Requirement: Top cell selection
After all cells are read, the reader SHALL load one cell into the current layout window, which takes the role of the gds-read "library cell name" rule. The cell is chosen as follows:

Compatibility: native

1. the first `S_TOP_CELL` file property whose value names a cell defined in this file;
2. else, if exactly one cell defined in this file is not placed by any other cell defined in this file, that cell;
3. else, no cell is loaded.

The `library` option has no effect on reading OASIS.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: S_TOP_CELL chooses the cell
- **GIVEN** a file with `S_TOP_CELL = chip` and cells `chip` and `pad`, where `pad` is unplaced
- **WHEN** read
- **THEN** cell `chip` is loaded into the layout window

#### Scenario: Two unplaced cells and no S_TOP_CELL
- **GIVEN** a file with cells `a` and `b`, neither placed, and no `S_TOP_CELL`
- **WHEN** read
- **THEN** no cell is loaded into the window

### Requirement: Read-only OASIS vendor views
With `oasis readonly yes`, every cell read (except magnified copies) SHALL produce a read-only cell carrying two string properties:

Compatibility: native

- `OASIS_FILE`: the path of the file as opened, with the PDK path-variable substitution of gds-read for `GDS_FILE`;
- `OASIS_CELL`: the cell's name as defined in the file, before any renaming by `unique`, duplicates or `cell` labels.

Rules:
1. Setting `OASIS_FILE` on a cell, by any means, SHALL make the cell read-only, and removing it SHALL clear that state, exactly as `GDS_FILE` does. Read-only cells are still interpreted through `cifinput`. Their Hefesto contents are representational, and OASIS output re-encodes them from the file (capability `oasis-write`). How GDS output treats them is open (Q-OASIS-35).
2. In read-only mode, the read SHALL be refused unless `(int)(0.5 + mpd·1e11) == 100` (1 nm), or `(int)(0.5 + mpd·1e12) == 100` (1 Å) when the current output style uses ångström units. Otherwise "Incompatible scale factor of <mpd>, must be 1e-9." (or "1e-10.") and "Cannot read this file in read-only mode." are reported, and nothing is read. This is the same check as gds-read.
3. Unknown-layer messages are suppressed for read-only cells, and they are not queued for DRC, as in gds-read.
4. `noduplicates` with a pre-existing abstract view (`LEFview`) SHALL annotate the abstract with `OASIS_FILE` and `OASIS_CELL`, as gds-read does with `GDS_FILE`/`GDS_START`/`GDS_END`.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Vendor cell properties
- **GIVEN** `oasis readonly yes`, `PDK_ROOT=/opt/pdk`, and a file `/opt/pdk/lib/cells.oas` defining `sc_inv`
- **WHEN** read
- **THEN** cell `sc_inv` is read-only, with `OASIS_FILE = $PDK_ROOT/lib/cells.oas` and `OASIS_CELL = sc_inv`

#### Scenario: Wrong unit refused
- **GIVEN** `oasis readonly yes` and a START unit of 100 (10 nm)
- **WHEN** read
- **THEN** "Incompatible scale factor of 1e-08, must be 1e-9." is reported and nothing is read

### Requirement: Content-hash timestamps for read-only OASIS cells
The timestamp of a read-only OASIS cell SHALL be a content hash of its OASIS data, computed like the gds-read hash but over decoded OASIS bytes:

Compatibility: native

1. **Cell bytes.** For a cell `c`, take the decompressed byte stream from the first byte after its CELL record through the last byte of its last cell-content record. CBLOCK record headers and compressed bytes are excluded, and their decompressed bytes are included.
2. **Own hash.** `h` = FNV-1a 64-bit over those bytes, with offset basis 14695981039346656037 and prime 1099511628211.
3. **Children.** The cell names referenced by PLACEMENT records with C = 1, in record order, resolved to names, with consecutive duplicates removed.
4. **Stamp.** The memoized `stamp(s)` procedure of gds-read "Content-hash timestamps for read-only cells" is applied with these hashes and children, including cycle handling, the folding of each child stamp as 4 bytes, the final mix `h XOR (h >> 31) XOR (h >> 62)`, the mask `0x7fffffff`, and the replacement of 0 by 1.
5. **Lookup.** A cell's stamp is the stamp of the cell named `OASIS_CELL` in the file `OASIS_FILE`. It is 0 if either property is missing or the cell is not defined there.

The application rules of gds-read "When content-hash timestamps are assigned" apply with `OASIS_FILE` in place of `GDS_FILE`, including the file lookup with `.gz`, the once-per-pass scan cache, and the precedence of `datestamp <N>`.

Because refnums are file-specific, the same geometry in two files with different name tables can hash differently. The stamp is only guaranteed stable for an unchanged file.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Edit does not change a vendor stamp
- **GIVEN** a read-only OASIS cell whose Hefesto view is then edited
- **WHEN** timestamps are updated and the cell is saved
- **THEN** its timestamp is unchanged, because it depends only on the file bytes

#### Scenario: Recompression does not change the stamp
- **GIVEN** two files with identical records, one with the cell body in a CBLOCK and one without
- **WHEN** both are read read-only
- **THEN** the cell gets the same stamp from both

### Requirement: Read option set
The reader SHALL honour the following settings of the `oasis` command (capability `oasis-command`). Their semantics are those of the same-named gds-read options:

Compatibility: exact

`datestamp` and `nodatestamp` (with the OASIS rules of "Cell timestamps"), `drccheck`, `flatten`, `flatglob`, `maskhints`, `noduplicates`, `ordering`, `polygon subcells`, `path subcells`, `readonly`, `rescale`, `savepaths`, `unique` and `warning`.

`magscale` is accepted and has no effect on reading, because OASIS TEXT carries no magnification. The OASIS-only read setting is `circlepoints`. `cif istyle` selects the input style.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: commands/CmdCD.c:131-1075)

#### Scenario: Rescale disallowed snaps placements
- **GIVEN** `oasis rescale no`, `S = 10`, and a placement at input-unit x = 25
- **WHEN** read
- **THEN** x = (25 + 4) / 10 = 2, as in gds-read

### Requirement: Read diagnostics
Errors SHALL be counted and reported according to the `warning` level, as in gds-read "Read diagnostics".

Compatibility: informative

Each OASIS read error is prefixed `Error while reading cell "<cell>" (byte position <offset>): `. The offset is the position in the decompressed record stream. Inside a CBLOCK, it is the file offset of the CBLOCK followed by `+<offset inside block>`. Message wording is not normative.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Limit mode
- **GIVEN** `oasis warning limit` and a file producing 150 errors
- **WHEN** read
- **THEN** 99 error messages and the limit notice are printed

### Requirement: GDS-equivalence cross-check
For every OASIS file `F` whose content has a GDS equivalent (no CIRCLE, no XGEOMETRY, no magnification or angle outside what GDS expresses, and all values in GDS ranges), reading `F` with `oasis read` SHALL produce the same cells, paint, labels, ports, uses, arrays, instance identifiers and properties as reading the GDS file `G` with `gds read`, with the same style and equivalent option values. Here `G` is obtained by converting each OASIS element instance to its GDS-equivalent element as defined in this capability. Cell timestamps are excluded from the comparison.

Compatibility: exact

This is the acceptance criterion for this capability. The golden cases in `tests/golden/OASIS/CASES.md` check it using an independent converter (KLayout `strm2gds` / `strm2oas`), with Magic's `gds read` as the reference.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaRead.c:173-332)

#### Scenario: Converted standard-cell library
- **GIVEN** a GDS standard-cell library `G` and `F = strm2oas(G)`
- **WHEN** Magic reads `G` with `gds read` and Hefesto reads `F` with `oasis read`
- **THEN** the `.mag` files saved from both are identical, except for timestamp lines

### Requirement: Host mechanisms used by the reader
Hefesto SHALL provide PDK path-variable substitution for `OASIS_FILE` from its own configuration on every platform, as for `GDS_FILE`, and SHALL support gzip-wrapped and CBLOCK-compressed input on all platforms, including iPadOS. Files are opened through the platform document model (capability `hefesto-files-and-pdks`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: iPadOS vendor library
- **GIVEN** Hefesto on iPadOS with a configured PDK root `/pdk`
- **WHEN** a read-only OASIS file under `/pdk/...` is read
- **THEN** `OASIS_FILE` is stored with the `$PDK_ROOT` prefix
