# GDSII Stream Reading

## Purpose
Defines how Hefesto reads a GDSII stream file (the `gds read` / `calma read` operation) into layout cells. The goal is data compatibility with Magic 8.3.694 (commit `fc5daa9e`): the same GDS file, read with the same technology `cifinput` style and the same option settings, must produce the same cells, the same geometry, the same labels and ports, the same cell uses and arrays, the same properties and the same timestamps. The spec covers:

- Record framing and data types, library and structure grammar, and units.
- The conversion of BOUNDARY, BOX, PATH and TEXT elements into input-layer geometry and labels.
- Structure references (SREF/AREF) with transforms, arrays and magnification.
- Hierarchy handling: ordering, flattening and duplicate cells.
- Read-only ("vendor") views and their content-hash timestamps.
- Every `gds` option that affects reading.

The interpretation of input layers into Magic types (the `cifinput` operators, `calma` layer mapping statements, `labels`, `ignore`, `options`, scale factors) is owned by capability `mask-input-styles`. CIF text reading and the `cif` command options (`cif istyle`, `cif limit`, ...) are owned by capability `cif-format-io`. Writing GDS, including the verbatim copy of read-only cells, is owned by capability `gds-write`. Mask hints (the `gds maskhints` option, the hint data model and its output semantics) are owned by capability `mask-hints`.

Terminology used throughout:
- **DB unit**: the integer coordinate unit stored in the GDS file.
- **Input unit**: the integer unit of the intermediate input-layer planes. One input unit is `1 / m` centimicrons, where `m` is the input style *multiplier*. See capability `mask-input-styles`.
- **S**: the input style *scale factor*, the number of input units per Magic internal unit.
- **Internal unit**: the Magic/Hefesto database grid unit.
- **Input layer**: a named layer of the `cifinput` style, selected by a GDS (layer, datatype) pair.

## Requirements

### Requirement: Record framing and primitive data types
The reader SHALL parse the file as a sequence of records. Each record has a 4-byte header: a 16-bit big-endian unsigned total record length `L` (header included), an 8-bit record type, and an 8-bit data-type code. The data-type code SHALL be ignored, and the record type alone SHALL determine how the data is interpreted.

Compatibility: exact

Data types:
- 2-byte integers (I2) and 4-byte integers (I4) are big-endian, two's complement.
- 8-byte reals (R8) use the GDSII excess-64 base-16 format. Byte 0 holds the sign (bit 7) and the exponent `e = (byte0 & 0x7f) - 64`. Bytes 1..7 hold the mantissa `M = Σ_{i=1..7} byte_i · 256^{-i}`. The value is `(-1)^sign · M · 16^e`.
- ASCII strings occupy `L - 4` bytes. The string value is the bytes up to the first NUL. A trailing pad NUL is therefore not part of the value.

Record type numbers SHALL be as in the GDSII standard: 0 HEADER, 1 BGNLIB, 2 LIBNAME, 3 UNITS, 4 ENDLIB, 5 BGNSTR, 6 STRNAME, 7 ENDSTR, 8 BOUNDARY, 9 PATH, 10 SREF, 11 AREF, 12 TEXT, 13 LAYER, 14 DATATYPE, 15 WIDTH, 16 XY, 17 ENDEL, 18 SNAME, 19 COLROW, 21 NODE, 22 TEXTTYPE, 23 PRESENTATION, 25 STRING, 26 STRANS, 27 MAG, 28 ANGLE, 31 REFLIBS, 32 FONTS, 33 PATHTYPE, 34 GENERATIONS, 35 ATTRTABLE, 36 STYPTABLE, 37 STRTYPE, 38 ELFLAGS, 42 NODETYPE, 43 PROPATTR, 44 PROPVALUE, 45 BOX, 46 BOXTYPE, 47 PLEX, 48 BGNEXTN, 49 ENDEXTN, 52 STRCLASS, 54 FORMAT, 55 MASK, 56 ENDMASKS, 57 LIBDIRSIZE, 58 SRFNAME, 59 LIBSECUR.

End of file inside a record header SHALL be treated as "no more records". Skipping a record SHALL consume exactly `L - 4` data bytes. A record with `L < 4` SHALL consume no data bytes.

The reader SHALL support a one-record lookahead: a header can be read and then "un-read", so that the next header read returns it again without re-reading the bytes. Byte positions reported by the reader (see "Read-only vendor views") are positions in the decompressed byte stream. When a header has been un-read, the reported position is the position *after* that header.

(Magic ref: calma/calmaInt.h:30-150, calma/CalmaRdio.c:420-480, calma/CalmaRdio.c:499-640)

#### Scenario: R8 decoding of a typical meters-per-DB-unit value
- **GIVEN** an R8 field with bytes `39 44 B8 2F A0 9B 5A 54`
- **WHEN** it is decoded
- **THEN** the exponent is `0x39 - 64 = -7`, the mantissa is `0x44B82FA09B5A54 / 2^56`
- **AND** the value is 1.0e-9 within double rounding

#### Scenario: Negative R8
- **GIVEN** an R8 with bytes `C1 10 00 00 00 00 00 00`
- **WHEN** it is decoded
- **THEN** the value is `-(1/16)·16^1 = -1.0`

#### Scenario: Padded string
- **GIVEN** a STRNAME record with `L = 8` and data bytes `41 42 43 00`
- **WHEN** it is read
- **THEN** the structure name is `ABC`

### Requirement: Library grammar and header processing
The reader SHALL accept a library with the grammar below. Records named in a `skip` group SHALL be consumed without effect, in any order and any number of times.

Compatibility: exact

```ebnf
library      = HEADER, BGNLIB,
               { LIBDIRSIZE | SRFNAME | LIBSECUR },        (* skipped *)
               LIBNAME,
               { FORMAT | MASK | ENDMASKS | REFLIBS | FONTS
                 | ATTRTABLE | STYPTABLE | GENERATIONS },  (* skipped *)
               UNITS,
               { structure },
               ENDLIB ;
structure    = BGNSTR, STRNAME, { STRCLASS | STRTYPE },    (* STRCLASS/STRTYPE skipped *)
               { element }, ENDSTR ;
element      = boundary | path | sref | aref | text | node | box ;
boundary     = BOUNDARY, [ELFLAGS], [PLEX], LAYER, DATATYPE, XY, {property}, ENDEL ;
path         = PATH, [ELFLAGS], [PLEX], LAYER, DATATYPE, [PATHTYPE], [WIDTH],
               [BGNEXTN], [ENDEXTN], XY, {property}, ENDEL ;
sref         = SREF, [ELFLAGS], [PLEX], SNAME, [strans], XY, {property}, ENDEL ;
aref         = AREF, [ELFLAGS], [PLEX], SNAME, [strans], COLROW, XY, {property}, ENDEL ;
text         = TEXT, [ELFLAGS], [PLEX], LAYER, TEXTTYPE, [PRESENTATION],
               [PATHTYPE], [WIDTH], [strans], XY, STRING, {property}, ENDEL ;
node         = NODE, [ELFLAGS], [PLEX], LAYER, NODETYPE, XY, {property}, ENDEL ;
box          = BOX, [ELFLAGS], [PLEX], LAYER, BOXTYPE, XY, {property}, ENDEL ;
strans       = STRANS, [MAG], [ANGLE] ;
property     = PROPATTR, PROPVALUE ;
```

Processing rules:
1. HEADER SHALL be read as I2 `version`. The informative message is `Library written using GDS-II Release <v>.0` if `version < 600`, otherwise `<version/100>.<version%100>`.
2. BGNLIB SHALL be skipped. Its dates are ignored.
3. LIBNAME SHALL be read. If the option `library` (see capability `gds-write`) is enabled, the library name is discarded. Otherwise every space in it is replaced by `_` and, if it contains `/`, only the part after the last `/` is kept. This sanitized name is the *library cell name*.
4. UNITS SHALL be processed as in "Units and input scale".
5. Structures SHALL be read until a record that is not BGNSTR is seen. That record SHALL then be consumed as ENDLIB. If it is not ENDLIB, an "Unexpected record type" error is reported.
6. Any failure in steps 1–4 (wrong record type, EOF) SHALL abort the read. Cells already created are kept, and the end-of-read cleanup still runs.
7. After all structures, if a library cell name exists and a cell of exactly that name was read from this file, that cell SHALL be loaded into the current layout window: the window under the pointer, or the only layout window. Magic writes the top cell's name as LIBNAME, so its own output reloads with the top cell displayed.

(Magic ref: calma/CalmaRead.c:173-332, calma/CalmaRdcl.c:372-410, calma/CalmaRdcl.c:733-790)

#### Scenario: Library name sanitizing
- **GIVEN** a GDS file whose LIBNAME is `lib/my top`, and that defines a structure named `my_top`
- **WHEN** it is read with `library` disabled
- **THEN** the library cell name is `my_top`
- **AND** cell `my_top` is loaded into the layout window after reading

#### Scenario: Header records skipped
- **GIVEN** a library with LIBDIRSIZE and SRFNAME before LIBNAME, and REFLIBS, FONTS and GENERATIONS records between LIBNAME and UNITS
- **WHEN** it is read
- **THEN** reading proceeds to UNITS without error and the skipped records have no effect

#### Scenario: Missing UNITS aborts
- **GIVEN** a library where a BGNSTR record directly follows LIBNAME
- **WHEN** it is read
- **THEN** the error "Unexpected record type in input: Expected UNITS record but got BGNSTR." is reported and no structure is read

### Requirement: Units and input scale
The reader SHALL take the two R8 values of the UNITS record: user units per DB unit, which is ignored, then meters per DB unit (`mpd`). It SHALL derive an integer rational read scale `num/den`, so that an input-unit coordinate equals `DB coordinate × num / den`:

Compatibility: exact

```
cu = mpd × 1.0e8 × m              (* input units per DB unit; m = input style multiplier *)
if cu >= 1.0:  num = (int)(cu + 0.5);  den = 1
else:          num = 1;                den = (int)(1.0 / cu + 0.5)
```

The ratio `num/den` SHALL be the starting scale for the file. It changes only through automatic rescaling (see "Coordinate conversion and automatic rescaling") and temporarily while a magnified copy is read (see "Magnified instances").

(Magic ref: calma/CalmaRead.c:824-920)

#### Scenario: Nanometer GDS with a centimicron style
- **GIVEN** UNITS with `mpd = 1e-9` and an input style multiplier `m = 1`
- **WHEN** units are parsed
- **THEN** `cu = 0.1`, `num = 1`, `den = 10`

#### Scenario: Nanometer GDS with a nanometer style
- **GIVEN** `mpd = 1e-9` and `m = 10` (a nanometer-based input style)
- **WHEN** units are parsed
- **THEN** `cu = 1.0`, `num = 1`, `den = 1`

#### Scenario: Ångström GDS
- **GIVEN** `mpd = 1e-10` and `m = 1`
- **WHEN** units are parsed
- **THEN** `num = 1`, `den = 100`

### Requirement: Coordinate conversion and automatic rescaling
Every XY coordinate value `v` (I4) SHALL be converted to input units as `w = v × num × k`, where `k` is the coordinate multiplier: 1 for BOUNDARY, BOX, TEXT and SREF/AREF points, and 2 for PATH centerline points. Then:

Compatibility: exact

1. If `w mod den == 0`, the result is `w / den`.
2. Otherwise let `g = gcd(den, |w|)` and `r = den / g`.
   - If `num × r <= 100` (the rescale limit), the reader SHALL rescale. `num` becomes `num × r`. Every input-unit quantity already held is multiplied by `r`: all input-layer planes of the cell being read, all saved planes of flatten-marked cells, the style scale factor `S`, the multiplier `m`, and every `cifinput` operator distance. The value becomes `w × r / den`, which is exact. A warning "units rescaled by factor of r / 1" is issued. Points of the same element read earlier SHALL be multiplied by `r` too.
   - Otherwise a warning "calma units at max scale; value rounded" SHALL be issued. The value becomes `(w + floor(den/2)) / den` for `w >= 0`, and `(w - floor((den-1)/2)) / den` for `w < 0`, using integer division truncating toward zero. This rounds halves toward +∞.
3. When `k = 0` (geometry on an unmapped layer), points SHALL be consumed without rescaling or storing.

PATH WIDTH, BGNEXTN and ENDEXTN values SHALL NOT trigger rescaling. They are converted by truncating integer division (see "PATH elements").

(Magic ref: calma/CalmaRdpt.c:85-215, cif/CIFrdcl.c:351-415, cif/cif.h:28)

#### Scenario: Off-grid nanometer coordinate triggers rescale
- **GIVEN** `num = 1`, `den = 10` and a BOUNDARY vertex x = 15
- **WHEN** the vertex is read
- **THEN** `w = 15`, `g = gcd(10, 15) = 5`, `r = 2`
- **AND** `num` becomes 2, `S` and `m` double, and the x coordinate is `30 / 10 = 3` input units

#### Scenario: Rescale limit reached
- **GIVEN** `num = 60`, `den = 7` and a coordinate value 1
- **WHEN** it is read
- **THEN** `w = 60`, `r = 7`, and `60 × 7 = 420 > 100`
- **AND** the value is rounded to `(60 + 3) / 7 = 9` with a warning

#### Scenario: Halfway values round toward +infinity at the limit
- **GIVEN** `num = 51`, `den = 2` and coordinate values 1 and -1
- **WHEN** they are read (`r = 2`, `51 × 2 = 102 > 100`, so rounding applies)
- **THEN** 1 gives `(51 + 1) / 2 = 26` (25.5 → 26)
- **AND** -1 gives `(-51 - 0) / 2 = -25` (-25.5 → -25)

### Requirement: Structure processing and cell creation
For each structure, the reader SHALL perform these steps:

Compatibility: exact

1. Read BGNSTR (creation date, see "Structure timestamps") and STRNAME.
2. Resolve the target cell (see "Duplicate structures" and "Cells already in memory").
3. Clear the cell: all paint, labels and uses are removed. Mark it available (defined).
4. Skip STRCLASS/STRTYPE.
5. Parse elements until a non-element record. Elements paint into fresh, empty input-layer planes for the cell. Labels go directly into the cell. Uses are placed directly.
6. Consume ENDSTR. If the record is not ENDSTR, report "Unexpected record type", skip to and including the next ENDSTR, and do not process the cell further. The cell's input planes are not converted. What happens to them depends on the `magic-compat` setting (Q-MASK-110, resolution: compat): with it ON (the default) they are not cleared, as in Magic, so their geometry is painted into the next structure that is processed; with it OFF they are cleared and the malformed structure's geometry is discarded.
7. Run the end-of-structure processing (see "End-of-structure processing").

Every structure SHALL become a separate cell named by STRNAME (subject to renaming rules). Structure names SHALL NOT be sanitized. The reader prints `Reading "<name>".` (informative). Subcell references to a name that has not yet been defined SHALL create an empty, not-yet-available cell of that name, which a later structure definition fills.

Reading SHALL NOT be undoable. It does not record undo events.

(Magic ref: calma/CalmaRdcl.c:372-724, calma/CalmaRdcl.c:1758-1840)

#### Scenario: Forward reference filled later
- **GIVEN** a file where structure `TOP` (first) contains an SREF to `A`, and structure `A` is defined after `TOP`
- **WHEN** read with default options
- **THEN** reading `TOP` creates an empty, undefined cell `A` and places a use of it
- **AND** when structure `A` is read, cell `A` receives its contents, and `TOP`'s use refers to the filled cell

#### Scenario: Malformed structure does not leak with magic-compat OFF
- **GIVEN** a file with structure `A` containing a BOUNDARY on a mapped layer followed by an unexpected record before ENDSTR, then a well-formed structure `B` with no elements
- **WHEN** read with `magic-compat` OFF
- **THEN** "Unexpected record type" is reported, `A` has no paint, and `B` has no paint
- **AND** with `magic-compat` ON `B` receives `A`'s boundary geometry, as in Magic

### Requirement: Structure timestamps
The BGNSTR record SHALL be interpreted as follows when its data length is exactly 24 bytes (12 I2 values). The first six values `(year, month, day, hour, minute, second)` form the creation date. They are converted to seconds since the Unix epoch by local-time conversion, where `month` is 1-based and daylight saving time is unknown (auto-determined). The year depends on the `magic-compat` setting (Q-MASK-134, resolution: compat): with it ON (the default) `year` is taken as years since 1900 (the `tm_year` field) and the result is truncated to 32 bits (low 32 bits, two's complement), as in Magic; with it OFF a `year >= 1900` is a full year, a smaller value is years since 1900, and the result is stored without overflow. The modification date (last six values) SHALL be ignored. If the data length is not 24, the creation timestamp is 0 and the error "Unknown timestamp format; setting timestamp to zero." is reported.

Compatibility: exact

The cell timestamp SHALL be set as follows, according to the `datestamp` option:
- default (`datestamp yes`): the converted creation date.
- `datestamp no` (or `nodatestamp yes`): 0.
- `datestamp <integer N>`: `N`. If `N != 0`, the cell is also marked *fixed-stamp*: later edits never assign it a new timestamp.

At the end of the read, every cell read from the file whose timestamp is nonzero SHALL be marked as *not needing a new timestamp*. A cell with timestamp 0 receives the current time when next saved. Read-only cells are overridden by the content hash (see "Content-hash timestamps for read-only cells").

(Magic ref: calma/CalmaRdio.c:308-372, calma/CalmaRdcl.c:500-512, cif/CIFrdcl.c:1734-1790, commands/CmdCD.c:919-975)

#### Scenario: Full year with magic-compat OFF
- **GIVEN** the host time zone is UTC and a BGNSTR whose creation date is (2024, 1, 1, 0, 0, 0)
- **WHEN** read with `magic-compat` OFF
- **THEN** the cell timestamp is 1704067200 (2024-01-01 00:00:00 UTC)
- **AND** with `magic-compat` ON the year is read as 1900 + 2024 = 3924 and the timestamp is the low 32 bits of that time

#### Scenario: Magic-written date round trip
- **GIVEN** BGNSTR data `124 1 15 10 30 0` followed by six more values, and the host time zone UTC
- **WHEN** read with the default datestamp
- **THEN** the cell timestamp is the epoch value of 2024-01-15 10:30:00 UTC, which is 1705314600

#### Scenario: Fixed datestamp
- **GIVEN** `gds datestamp 1700000000`
- **WHEN** any GDS file is read
- **THEN** every cell read gets timestamp 1700000000 and is fixed-stamp

#### Scenario: Short BGNSTR
- **GIVEN** a BGNSTR record with 12 data bytes
- **WHEN** it is read
- **THEN** the timestamp is 0 and an "Unknown timestamp format" error is reported

### Requirement: Duplicate structures within one file
The reader SHALL track every structure name defined so far in the current file. When a STRNAME repeats a name already defined in this file:

Compatibility: exact

- If that name was pre-loaded by look-ahead (see "Post-order reading"), the structure is skipped silently, without being parsed. The pre-load mark is cleared.
- Else, if the earlier definition was completely processed, the structure is skipped. The errors "Cell "<name>" was already defined in this file." and "Ignoring duplicate definition" are reported, unless post-order reading is enabled or a file has been rewound during this session (`magic-compat` ON, the default, as in Magic) or during this read (`magic-compat` OFF) (Q-MASK-111, resolution: compat). Only these diagnostics depend on the setting; the geometry is the same.
- Else (the earlier definition is still incomplete), the new structure is read under the name `<name>_<n>`, where `n` is the smallest integer ≥ 1 such that `<name>_<n>` is not a structure name in this file. The errors "Cell ... was already defined" and "Giving this cell a new name: <newname>" are reported.

Skipping a structure SHALL advance the input to the next BGNSTR or ENDLIB record without interpreting the elements.

(Magic ref: calma/CalmaRdcl.c:418-470, calma/CalmaRdcl.c:211-245)

#### Scenario: Second definition ignored
- **GIVEN** a file that defines structure `A` twice: first a 10×10 box, later a 20×20 box
- **WHEN** read with default options
- **THEN** cell `A` contains only the 10×10 box and the duplicate error is reported

### Requirement: Cells already in memory before reading
When a structure (or a reference) names a cell that already exists in memory and was not created by this read, the reader SHALL report "Warning: cell <name> already existed before reading GDS!". By default the existing cell is then cleared and overwritten by the GDS definition.

Compatibility: behavioral

- With `gds unique yes`, before a structure is read, the existing in-memory cell (if it is defined and was not created by the current file) SHALL be renamed to `<name>_<n>`, where `n` is the smallest integer ≥ 1 such that no cell of that name exists. The warning "cell definition "<name>" reused." is reported. The GDS structure then creates a fresh cell `<name>`.
- With `gds noduplicates yes`, a cell that exists in memory when its name is first encountered in the file SHALL be *predefined*. The messages "Note: cell <name> already existed before reading GDS." and "Using pre-existing cell definition" are printed. When the structure of a predefined cell is reached:
  - if the existing cell is not an abstract view (it has no property `LEFview`), the structure is skipped and the cell is unchanged;
  - if it is an abstract view, the structure is skipped, but the cell is annotated with `GDS_START`, `GDS_END` and `GDS_FILE` (see "Read-only vendor views"). This makes it a read-only cell whose output is copied from the GDS.

A cell is predefined only if its first occurrence in the file is its structure definition. If the name was first seen as a reference, the definition overwrites the existing cell (see Q-MASK-112).

(Magic ref: calma/CalmaRdcl.c:472-560, calma/CalmaRdcl.c:1698-1840)

#### Scenario: noduplicates keeps a pre-loaded cell
- **GIVEN** a cell `INV` already loaded from `INV.mag`, `gds noduplicates yes`, and a GDS file defining `INV` before any reference to it
- **WHEN** the file is read
- **THEN** cell `INV` keeps its `.mag` contents

#### Scenario: noduplicates annotates a LEF abstract
- **GIVEN** a cell `PAD` with property `LEFview`, `gds noduplicates yes`, and a GDS file whose structure `PAD` starts at byte 1000 (after STRNAME) and whose ENDSTR ends at byte 5000
- **WHEN** the file is read
- **THEN** `PAD` keeps its abstract geometry and gets `GDS_START = 1000`, `GDS_END = 5000` and `GDS_FILE` = the file path
- **AND** `PAD` becomes a read-only cell

#### Scenario: unique renames the old cell
- **GIVEN** an in-memory defined cell `A`, cells `A_1` already existing, and `gds unique yes`
- **WHEN** a GDS file defining `A` is read
- **THEN** the old cell is renamed `A_2` and a new cell `A` holds the GDS contents

### Requirement: Element dispatch and unsupported content
Inside a structure, the reader SHALL dispatch on the element record: BOUNDARY, BOX, PATH, TEXT, SREF and AREF are interpreted as specified in this capability. NODE SHALL be reported ("NODE elements not supported: skipping.") and ignored. After an element's interpreted records, all records up to and including the next ENDEL SHALL be skipped. This covers element properties not listed in "Instance properties", and records after an error inside the element.

Compatibility: exact

A non-element record SHALL end the element list. ELFLAGS and PLEX SHALL be ignored. Elements whose layer, datatype or XY record is missing or malformed SHALL be reported and dropped. A BOUNDARY, BOX and PATH element each count as one *shape* for the `flatten` criterion. SREF and AREF count as one *reference*. TEXT and NODE count as neither.

(Magic ref: calma/CalmaRdcl.c:733-790, calma/CalmaRead.c:150)

#### Scenario: Unknown properties ignored
- **GIVEN** a BOUNDARY with PROPATTR 1 / PROPVALUE "x" before ENDEL
- **WHEN** it is read
- **THEN** the boundary is painted and the property is discarded

### Requirement: Layer and datatype mapping
Each BOUNDARY, BOX, PATH and TEXT SHALL be assigned to an input layer by looking up its (layer, datatype) pair in the style's GDS mapping in this order: the exact `(layer, datatype)`, then `(layer, *)`, then `(*, datatype)`, then `(*, *)`. The first match wins. The mapping statements and wildcard syntax are defined in capability `mask-input-styles`. Layer and datatype values are not range-checked on input.

Compatibility: exact

If no mapping matches, the geometry SHALL be discarded. The first occurrence of each unmapped pair in a structure is reported as `<kind>, layer=<L> type=<D>`, where the kind is one of "Unknown layer/datatype in boundary", "... in box", "... in path", or "Label on unknown layer/datatype". The message is suppressed entirely in read-only cells. TEXT on an unmapped pair is handled as in "Label layer assignment".

(Magic ref: cif/CIFrdtech.c:184-232, calma/CalmaRdpt.c:1251-1276)

#### Scenario: Datatype wildcard precedence
- **GIVEN** mappings `calma M1 68 20` and `calma M1PIN 68 *`
- **WHEN** a boundary on 68/20 and another on 68/16 are read
- **THEN** the first goes to input layer M1 and the second to M1PIN

### Requirement: BOX elements
A BOX element SHALL be read as LAYER, BOXTYPE (used as the datatype for mapping) and an XY record with exactly 5 points. With any other point count, the error "Box doesn't have 5 points." is reported and the element is dropped. The painted rectangle SHALL be the bounding box of the 5 converted points, painted onto the mapped input layer. Whether the points form a rectangle is not checked.

Compatibility: exact

(Magic ref: calma/CalmaRdpt.c:439-530)

#### Scenario: Box painted as bounding box
- **GIVEN** `num = den = 1` and a BOX on a mapped layer with points (0,0) (0,5) (8,5) (8,0) (0,0)
- **WHEN** it is read
- **THEN** the rectangle x∈[0,8], y∈[0,5] input units is painted

### Requirement: BOUNDARY elements
A BOUNDARY SHALL be read as LAYER, DATATYPE and an XY record of `n` points converted with `k = 1`. The resulting region is painted onto the mapped input layer as follows:

Compatibility: exact

1. If the last point differs from the first, the error "Boundary is not closed." is reported and the first point is appended.
2. Non-Manhattan edges are resolved as in "Non-Manhattan boundary decomposition".
3. If the closed path has fewer than 4 edges, the error "polygon with fewer than 4 points." is reported and nothing more is painted.
4. The remaining (Manhattan) outline is filled using the **nonzero winding rule**. Upward vertical edges count +1, downward vertical edges count −1, and horizontal edges count 0. A point is inside if the sum over vertical edges to its left that span its y-range is nonzero. The area is painted as the union of the maximal x-intervals with nonzero winding in each y-band between consecutive distinct vertex y values.
5. Painting into input planes is a union with existing material on that layer.

If the layer is unmapped, the points SHALL be consumed without effect.

(Magic ref: calma/CalmaRdpt.c:235-430, cif/CIFrdpoly.c:129-345)

#### Scenario: L-shaped boundary
- **GIVEN** `num = den = 1` and a BOUNDARY (0,0) (0,20) (10,20) (10,10) (20,10) (20,0) (0,0)
- **WHEN** it is read
- **THEN** the painted region is the union of x∈[0,20]×y∈[0,10] and x∈[0,10]×y∈[10,20]

#### Scenario: Self-overlapping boundary uses nonzero winding
- **GIVEN** a boundary that traverses the square [0,10]×[0,10] twice in the same direction
- **WHEN** it is read
- **THEN** the square is painted once, because the winding number 2 is nonzero

#### Scenario: Unclosed boundary
- **GIVEN** a boundary (0,0) (0,10) (10,10) (10,0) without the closing point
- **WHEN** it is read
- **THEN** "Boundary is not closed." is reported and the 10×10 square is painted

### Requirement: Non-Manhattan boundary decomposition
For each boundary edge that is neither horizontal nor vertical, the reader SHALL represent the triangle between that edge and an inserted axis-parallel corner as one *split tile*. A split tile is a rectangle divided by its diagonal, with material on one side. The reader then continues with the Manhattan polygon in which the diagonal edge is replaced by the two axis-parallel legs.

Compatibility: exact

The union of the split-tile triangles and the Manhattan fill SHALL equal the original polygon exactly, with no stair-step approximation. Vertices stay on the integer input grid.

The corner is chosen from the polygon's orientation. With the edge oriented as if the polygon were traversed so that its interior is on a fixed side (orientation detected from the signed area), the corner is `(first.x, last.y)` for edges heading down-left or up-right, and `(last.x, first.y)` otherwise. If the new leg would cross another polygon edge, the corner is clipped to the crossing point, and an extra point is inserted on the diagonal at that x (or y). The intersection uses 64-bit integer arithmetic: `y = y0 + (x − x0)·(y1 − y0)/(x1 − x0)`, truncated. Degenerate triangles (zero width or height) are not painted.

A boundary that has at least one non-Manhattan edge SHALL be called *non-Manhattan* for the `polygon subcells` option.

(Magic ref: cif/CIFrdutils.c:1052-1188, cif/CIFrdutils.c:747-1050, calma/CalmaRdpt.c:1145-1245)

#### Scenario: 45-degree triangle
- **GIVEN** `num = den = 1` and a boundary (0,0) (10,10) (10,0) (0,0)
- **WHEN** it is read
- **THEN** the input layer holds exactly the triangle with vertices (0,0), (10,0) and (10,10), as one split tile over [0,10]×[0,10] with material below the diagonal

#### Scenario: Octagon (verify against Magic)
- **GIVEN** a regular octagon with 45-degree edges and vertices on even coordinates
- **WHEN** it is read
- **THEN** the painted region equals the octagon exactly (no stair-stepping), with each diagonal edge represented by split-tile triangles

### Requirement: PATH elements
A PATH SHALL be read as LAYER, DATATYPE, an optional PATHTYPE (default 0), an optional WIDTH (default 0), optional BGNEXTN and ENDEXTN, and an XY record. The optional records are recognized only in this order.

Compatibility: exact

1. Supported path types are 0 (flush), 2 (half-width extension) and 4 (custom extension). Any other type SHALL be reported ("Warning: pathtype <t> unsupported (ignored).") and treated as type 0. Type 1 (round) depends on the `magic-compat` setting (Q-MASK-113, resolution: compat): with it ON (the default) it is reported and treated as type 0, as in Magic; with it OFF it is treated as type 2 and a warning reports that the round cap was approximated by a half-width square extension.
2. `W = (width × num) / den` (C truncation). If the product is not divisible by `den`, "Wire width snapped to nearest integer boundary." is reported.
3. For BGNEXTN/ENDEXTN value `e`: `E = (e × num × 2) / den` (truncating), with the same warning on inexact division. `E` is in doubled input units.
4. Centerline points are converted with `k = 2` (doubled input units). If a rescale by `r` occurs while reading them, `W` and both `E` values are multiplied by `r`.
5. End extensions depend on the `magic-compat` setting (Q-MASK-114, resolution: compat). With it ON (the default), as in Magic: if `E_begin > 0`, the first point moves away from the second point by `E_begin` independently in x and in y; each coordinate moves only if it differs from the second point's coordinate, in the direction away from it; `E_end > 0` moves the last point away from the second-to-last point the same way; negative or zero extensions are ignored. With it OFF: the first point moves by `E_begin` along the direction from the second point to the first, and the last point by `E_end` along the direction from the second-to-last point to the last, each coordinate rounded to the doubled-unit grid (round half away from zero); a negative extension moves the point the other way (shortening the path). Manhattan paths with non-negative extensions give the same result in both settings. Extensions apply whenever the records are present, regardless of the path type.
6. If `W == 0`, the path SHALL be dropped. A negative width, which GDSII defines as an absolute width, depends on the `magic-compat` setting (Q-MASK-115, resolution: compat): with it ON (the default) the path is dropped silently, as in Magic; with it OFF `|W|` is used, converted with the unit conversion of item 2 but without the magnification of the enclosing SREF/AREF, and a warning is reported.
7. If the layer is unmapped, the path is reported and dropped.
8. If `savepaths` is enabled, the centerline is recorded (see "Path centerline properties").
9. The outline is generated by "Path-to-polygon conversion", with endcap enabled only for path type 2.

(Magic ref: calma/CalmaRdpt.c:534-750)

#### Scenario: Flush horizontal path
- **GIVEN** `num = den = 1`, a PATH with PATHTYPE 0, WIDTH 10 and points (0,0) (100,0)
- **WHEN** it is read
- **THEN** the rectangle x∈[0,100], y∈[-5,5] is painted

#### Scenario: Half-width extended path
- **GIVEN** the same path with PATHTYPE 2
- **WHEN** it is read
- **THEN** the rectangle x∈[-5,105], y∈[-5,5] is painted

#### Scenario: Custom extensions
- **GIVEN** PATHTYPE 4, WIDTH 10, BGNEXTN 3, ENDEXTN 7 and points (0,0) (100,0)
- **WHEN** it is read
- **THEN** the rectangle x∈[-3,107], y∈[-5,5] is painted

#### Scenario: Odd width rounds outward
- **GIVEN** `num = den = 1`, PATHTYPE 0, WIDTH 5 and points (0,0) (100,0)
- **WHEN** it is read
- **THEN** the edges are at round(−2.5) = −3 and round(2.5) = 3, so y∈[-3,3] is painted

#### Scenario: Round path type
- **GIVEN** a PATH with PATHTYPE 1, WIDTH 10 and points (0,0) (100,0)
- **WHEN** it is read
- **THEN** a warning is reported and the rectangle x∈[0,100], y∈[-5,5] is painted, with no extension
- **AND** with `magic-compat` OFF the rectangle x∈[-5,105], y∈[-5,5] is painted and the warning says the round cap was approximated

#### Scenario: Diagonal extension with magic-compat OFF
- **GIVEN** `num = den = 1`, PATHTYPE 4, WIDTH 10, BGNEXTN 0, ENDEXTN 5 and points (0,0) (30,40)
- **WHEN** it is read with `magic-compat` OFF
- **THEN** the last point moves along the segment to (33,44)
- **AND** with `magic-compat` ON it moves by 5 in x and in y, to (35,45)

#### Scenario: Negative width
- **GIVEN** `num = den = 1`, PATHTYPE 0, WIDTH -10 and points (0,0) (100,0)
- **WHEN** it is read with `magic-compat` OFF
- **THEN** a warning is reported and the rectangle x∈[0,100], y∈[-5,5] is painted
- **AND** with `magic-compat` ON nothing is painted

### Requirement: Path-to-polygon conversion
The reader SHALL convert a centerline `P_0..P_{n-1}` (doubled input units), half-width offset `w` (= `W` in doubled units) and endcap flag into one or more polygons with this algorithm. All trigonometry is IEEE double precision. `H` depends on the `magic-compat` setting (Q-MASK-118, resolution: compat): with it ON (the default) `H = 1.5708`, as in Magic; with it OFF `H = π/2` (`M_PI_2`).

Compatibility: exact

```
remove P_i when P_i == P_{i+1}, for 1 <= i <= n-2   (* magic-compat ON: the head is never removed *)
                                                    (* magic-compat OFF: for 0 <= i <= n-2; Q-MASK-116 *)
if n == 1: segments = [(P0, P0)] else segments = [(P_{i-1}, P_i)]
theta = 0; first = true
for each segment (A = prev point, B = current point), with N = next point if any:
    savetheta = theta
    theta = atan2(B.y - A.y, B.x - A.x)
    if first:
        if endcap: A -= w·(cos theta, sin theta)
        o = w·(cos(theta - H), sin(theta - H))
        start a new polygon with points  R0 = round((A + o)/2),  L0 = round((A - o)/2)
        first = false
    if N exists: phi = atan2(N.y - B.y, N.x - B.x)
    else:        phi = theta; if endcap: B += w·(cos theta, sin theta)
    alpha = (phi - theta)/2
    if |cos alpha| < 0.25:
        if |cos alpha| < 1e-10:            (* direction reversal *)
            report "direction reversal in path" (or "duplicate point in path")
            phi = theta; if endcap: B += w·(cos theta, sin theta); alpha = 0
            first = true
        else:
            report "mitre limit exceeded ... Route has been truncated."
            stop: the polygon in progress is NOT painted        (* see Q-MASK-117 *)
    delta = (phi + theta)/2 - H
    c = w / cos(alpha)
    q = c·(cos delta, sin delta)
    append to left chain:  round((B - q)/2)
    append to right chain: round((B + q)/2)
    if first or N is absent:
        polygon = reverse(left chain) ++ right chain; fill it (BOUNDARY rules, nonzero winding)
        clear chains
    else:  inner-corner correction (below)
```

`round` is round-half-away-from-zero, applied to each coordinate. The polygon vertex order is: left chain from newest to oldest, then right chain from oldest to newest.

Inner-corner correction, done after appending the joint points `Lk`, `Rk` when the path continues: let `S` be the previous right-chain point and `T` the previous left-chain point.
- Let `a1 = |atan2(Rk − S) − theta|`. If `0.1 < a1 < 6.1`:
  - compute `d1 = cos(savetheta)·sin(phi) − sin(savetheta)·cos(phi)`;
  - if `|d1| > 1e-4`, then `r2 = (sin(phi)·(Rk.x − S.x) − cos(phi)·(Rk.y − S.y)) / d1` and `S += (round(r2·cos savetheta), round(r2·sin savetheta))`.
- Else, let `a2 = |atan2(Lk − T) − theta|`. If `0.1 < a2 < 6.1`, the same correction is applied to `T` using `Lk − T`.

For the first joint, `savetheta = 0` with `magic-compat` ON, as in Magic, and the angle of the first segment with it OFF (Q-MASK-118, resolution: compat). Savepaths properties are recorded before duplicate removal, so they keep a head duplicate in both settings (Q-MASK-116, resolution: compat).

(Magic ref: cif/CIFrdpt.c:339-600)

#### Scenario: Right-angle bend
- **GIVEN** `num = den = 1`, PATHTYPE 0, WIDTH 10 and points (0,0) (100,0) (100,100)
- **WHEN** it is read
- **THEN** the painted region is the union of [0,105]×[-5,5] and [95,105]×[-5,100], with a square outer mitre corner at (105,-5)

#### Scenario: Single-point square path
- **GIVEN** PATHTYPE 2, WIDTH 10 and a single point (50,50)
- **WHEN** it is read
- **THEN** the square [45,55]×[45,55] is painted

#### Scenario: Single-point flush path paints nothing
- **GIVEN** PATHTYPE 0, WIDTH 10 and a single point (50,50)
- **WHEN** it is read
- **THEN** nothing is painted, because the polygon is degenerate

#### Scenario: 45-degree path segment (verify against Magic)
- **GIVEN** `num = den = 1`, PATHTYPE 0, WIDTH 10 and points (0,0) (100,100)
- **WHEN** it is read
- **THEN** a parallelogram with corners round(±3.5355…) offsets is painted: (4,-4), (104,96), (96,104) and (-4,4), represented with split tiles

### Requirement: Path centerline properties
With `gds savepaths yes`, the reader SHALL record each mapped, nonzero-width path's centerline as a property of the cell receiving the path. The property is a dimension list `x0 y0 x1 y1 …`. Each value is the doubled input-unit coordinate after end extensions, shifted right by 1 (arithmetic shift, i.e. floor division by 2). The units depend on the `magic-compat` setting (Q-MASK-119, resolution: compat): with it ON (the default) the values stay in input units, as in Magic; with it OFF each value is converted to internal units the way geometry is (divided by the style scale factor `S`), so the property matches the layout coordinates. The settings agree when `S = 1`.

Compatibility: exact

The first such property in a cell is named `path`. Later ones are named `path_<i>`, where `i` is the smallest integer ≥ 0 not yet used as a property name. The default is `no`.

(Magic ref: calma/CalmaRdpt.c:720-725, cif/CIFrdpt.c:239-330, commands/CmdCD.c:745-764)

#### Scenario: Two paths recorded
- **GIVEN** `savepaths yes` and a structure with two paths
- **WHEN** it is read
- **THEN** the cell has properties `path` and `path_0`

### Requirement: TEXT element parsing
A TEXT element SHALL be read as LAYER, TEXTTYPE (used as datatype for mapping), then the optional records below, then XY and STRING:

Compatibility: exact

1. If PRESENTATION follows: `font = (p >> 4) & 3`, and the anchor nibble `p & 0xF` maps to a label position (the side of the point on which the text lies):

| nibble | GDS anchor | label position |
|---|---|---|
| 0x0 | top-left | southeast |
| 0x1 | top-center | south |
| 0x2 | top-right | southwest |
| 0x4 | middle-left | east |
| 0x5 | middle-center | center |
| 0x6 | middle-right | west |
| 0x8 | bottom-left | northeast |
| 0x9 | bottom-center | north |
| 0xA | bottom-right | northwest |
| other | — | southeast (default) |

   If PRESENTATION is absent, no font is set and the position is southeast.
2. If PRESENTATION was absent and the next record is not STRANS, every PATHTYPE and WIDTH record is skipped. Then, if the next record is WIDTH, it is honored. Otherwise, if it is not STRANS, every PATHTYPE and WIDTH record is skipped. As a result, with `magic-compat` ON (the default) WIDTH is honored only when it directly follows PRESENTATION, as in Magic; with it OFF a WIDTH record anywhere in the PATHTYPE/WIDTH run between PRESENTATION (or TEXTTYPE) and STRANS is honored (Q-MASK-120, resolution: compat). Files without PRESENTATION are unaffected in both settings. A honored WIDTH `t` gives `size = (t·num)/den` (truncating, with the warning "Text width snapped to nearest integer boundary." if inexact), converted to internal units with nearest-grid rounding (the any-snap rule of "Instance placement scaling").
3. If STRANS follows, its flags are ignored, including reflection. An optional MAG `d` with `0 < d <= 10000` sets `size = (int)(0.5 + d·1000·magscale·m / S)`. Otherwise the error "Invalid text magnification" is reported. An optional ANGLE `a` sets `rotation = (int)a`, truncated and not normalized.
4. The XY record must have at least 8 data bytes. The first point is converted with `k = 1`, and extra points are skipped. The label point SHALL be `(x / S, y / S)`, using integer division truncating toward zero with `magic-compat` ON (the default), as in Magic, and floor division with it OFF (Q-MASK-121, resolution: compat). On-grid points give the same result in both settings.
5. STRING is the label text. It is used as-is, with no character filtering.

Default size: if the label's Magic type `t > 0` has a minimum DRC width `w_t > 0` (internal units), then `size = (w_t·den·m·8) / (num·S·10)`, computed as `size = w_t·(den·m·8)` followed by `size /= (num·S·10)`. Then, if `size == 0` or `size > (800·m)/S`, `size = (800·m)/S`. These are integer operations. The three size formulas — this default, the MAG formula of item 3 with its `+0.5` rounding, and the WIDTH formula of item 2 (truncating `(t·num)/den`, then the any-snap conversion, without the `8/10` factor) — are replicated exactly in both `magic-compat` settings (Q-MASK-122, resolution: replicate). Size, font and rotation SHALL be applied only when PRESENTATION was present: the label is created as a font label. Otherwise the label is a plain label at the position above, and size and rotation are discarded.

(Magic ref: calma/CalmaRdpt.c:767-960, calma/CalmaRdpt.c:1015-1135)

#### Scenario: Negative off-grid label point
- **GIVEN** `S = 10`, `num = den = 1`, and a TEXT at (−15, 25)
- **WHEN** it is read
- **THEN** with `magic-compat` ON the label point is (−1, 2)
- **AND** with `magic-compat` OFF it is (−2, 2)

#### Scenario: Presentation anchor mapping
- **GIVEN** a TEXT with PRESENTATION 0x0005 and string `VDD` at (1000, 2000), with `S = 10` and `num = den = 1`
- **WHEN** it is read
- **THEN** a font label `VDD` at (100, 200), font 0, position center, is created

#### Scenario: No presentation
- **GIVEN** a TEXT without PRESENTATION, with STRANS and MAG 2.0
- **WHEN** it is read
- **THEN** a plain (non-font) label is created with position southeast, and the MAG value has no visible effect

#### Scenario: MAG sets size with magscale
- **GIVEN** PRESENTATION 0x0000, MAG 0.5, `magscale 2`, `m = 1` and `S = 1`
- **WHEN** it is read
- **THEN** `size = (int)(0.5 + 0.5·1000·2·1/1) = 1000`

#### Scenario: Empty string ignored
- **GIVEN** a TEXT whose STRING is empty
- **WHEN** it is read
- **THEN** the warning "Ignoring empty string label at (x, y)" is reported and no label is created

### Requirement: Label layer assignment
The Magic type of a label SHALL be determined from the mapped input layer's `labels` assignment in the input style (capability `mask-input-styles`):

Compatibility: exact

- Mapped layer with a label type `T`: the label gets type `T`. It is *sticky* if the layer's label kind is `text`/`sticky`, `port` or `cell`. Otherwise it is not sticky.
- Mapped layer declared `ignore`d (no label type): the label is discarded with the warning "label "<s>" at (x, y) is on unhandled layer:purpose pair L:D and will be discarded." (suppressed by the input option `ignore-unknown-layer-labels`).
- Mapped layer with no labels assignment: the label is attached to type space and is not sticky.
- Unmapped pair: if the input option `ignore-unknown-layer-labels` is set, the label is silently discarded. Otherwise the unknown-layer error is reported and the label is attached to type space (not sticky).
- Label kind `cell` on the mapped layer: the label is not created. Instead, the cell being read is renamed to the label string. For unmapped pairs, Magic evaluates this test with an invalid layer index (see Q-MASK-133). Hefesto SHALL treat unmapped pairs as not `cell`.

A label whose layer kind is `port` SHALL become a port. Its port index is assigned as in "Pin shapes and port labels", and its port directions are set to north, south, east and west.

(Magic ref: calma/CalmaRdpt.c:780-800, calma/CalmaRdpt.c:1008-1130, cif/CIFrdtech.c:851-940)

#### Scenario: Port layer label
- **GIVEN** `labels metal1 port` on the input layer mapped from 68/5, and the first two TEXTs on 68/5 being `A` then `B`
- **WHEN** read
- **THEN** `A` is a port with index 1, `B` a port with index 2, both sticky with all four directions

#### Scenario: Cell-id label
- **GIVEN** a `labels ... cell` layer mapped from 63/0, and structure `XYZ` containing TEXT `real_name` on 63/0
- **WHEN** read
- **THEN** cell `XYZ` is renamed `real_name`

### Requirement: Pin shapes and port labels
For a BOUNDARY on an input layer whose label kind is `text`, `port` or `cell`, the reader SHALL associate the shape with labels. Let `R` be the rectangle produced last by the fill scan (the topmost band's rightmost rectangle), converted to internal units by dividing each coordinate by `S` (truncating toward zero with `magic-compat` ON, floor division with it OFF; Q-MASK-121), and let `T` be the layer's label type.

Compatibility: exact

1. The first existing label in the cell with type `T` whose rectangle lies inside `R` (borders inclusive) gets its rectangle replaced by `R`. If there is none, a placeholder label with empty text, position center and type `T` is created at `R`.
2. If the kind is `port` and that label has no port direction yet, it gets a port index and all four directions.
3. When a TEXT is later read, an empty-text label of the same type `T` whose rectangle encloses the text point is consumed, and the new label takes the placeholder's rectangle. Port data depends on the `magic-compat` setting (Q-MASK-123, resolution: compat): with it ON (the default) the new label also takes a port index and all four directions, even when the kind is `text`, as in Magic; with it OFF it takes a port index and directions only if the placeholder already had port directions (it was created on a `port`-kind layer), and otherwise only the rectangle.

Port index assignment for label `X` with text `s`: scan the cell's labels in list order. Track `i` = the maximum port index seen, starting at −1. If a label other than `X` with index > 0 has text `s`, use its index and stop. Otherwise use `i + 1`.

After the structure, all labels that are still empty SHALL be deleted.

(Magic ref: calma/CalmaRdpt.c:320-395, calma/CalmaRdpt.c:1040-1100, calma/CalmaRdcl.c:660-663)

#### Scenario: Pin shape after text
- **GIVEN** `S = 1`, `labels m1 port` for 68/16 (pin) and 68/5 (text), a TEXT `IN` at (5,5) on 68/5, then a BOUNDARY [0,10]×[0,10] on 68/16
- **WHEN** read
- **THEN** port label `IN` has rectangle [0,10]×[0,10] and port index 1

#### Scenario: Pin shape before text
- **GIVEN** the same shapes in reverse order
- **WHEN** read
- **THEN** the result is identical: port `IN` with rectangle [0,10]×[0,10] and index 1

#### Scenario: Text-kind pin shape
- **GIVEN** `S = 1`, `labels m1 text` for 68/16 and 68/5, a BOUNDARY [0,10]×[0,10] on 68/16, then a TEXT `IN` at (5,5) on 68/5
- **WHEN** read
- **THEN** with `magic-compat` ON label `IN` has rectangle [0,10]×[0,10] and is a port with index 1
- **AND** with `magic-compat` OFF label `IN` has rectangle [0,10]×[0,10] and is not a port

#### Scenario: Pin with no text
- **GIVEN** a pin BOUNDARY with no TEXT inside it
- **WHEN** the structure ends
- **THEN** the placeholder label is deleted

#### Scenario: Same-named ports share an index
- **GIVEN** port TEXTs `A`, `B`, `A` in that order on a port layer
- **WHEN** read
- **THEN** the indices are 1, 2, 1

### Requirement: Structure reference transforms
For SREF and AREF, the reader SHALL read SNAME, then an optional STRANS group, and build the use transform as follows:

Compatibility: exact

- STRANS flag bit 0x8000 (reflection) mirrors about the x axis, `(x,y)→(x,−y)`, first. All other flag bits (absolute magnification and absolute angle) are ignored.
- MAG `d`: if `d <= 0`, the error "Invalid magnification (d) in instance of <name> (ignored)" is reported and `d = 1`.
- ANGLE `a` (degrees, counterclockwise): `ai = (int)a`, truncated toward zero. Add 360 while `ai < 0`, subtract 360 while `ai > 360`, and map 360 to 0. If `ai` is not 0, 90, 180 or 270, report "Non-Manhattan angle (ai) in transform" and round: `[0,45)→0`, `[45,135)→90`, `[135,225)→180`, `[225,315)→270`, `[315,360)→0`.
- The transform is: reflect (if set), then rotate counterclockwise by `ai`, then translate by the first reference point (see "Instance placement scaling").
- A STRANS record whose length is not 6, or MAG/ANGLE records whose length is not 12, abort the element.

(Magic ref: calma/CalmaRdio.c:81-205, calma/CalmaRdcl.c:1195-1240)

#### Scenario: Rotated and mirrored SREF
- **GIVEN** an SREF with STRANS 0x8000, ANGLE 90 and XY (100, 0), with `S = 1` and `num = den = 1`
- **WHEN** read
- **THEN** a child point (1, 2) maps to: reflect (1, −2), rotate CCW 90 (2, 1), translate (102, 1)

#### Scenario: Non-Manhattan angle rounded
- **GIVEN** ANGLE 44.9
- **WHEN** read
- **THEN** `ai = 44` is rounded to 0 with an error

### Requirement: Instance placement scaling
Reference points SHALL be converted to input units (`k = 1`, see "Coordinate conversion and automatic rescaling"), then to internal units by exact division by `S`. If an input-unit value `c` is not divisible by `S`:

Compatibility: exact

- If grid subdivision is allowed (`gds rescale yes`, the default) and the `cif limit` setting (capability `cif-format-io`) does not forbid it: let `g = gcd(|c|, S)` and `d = S / g`. The warning "Input off lambda grid by r/d; grid redefined." is issued once. The whole layout database, all technology scaled values and the input/output styles are scaled by `d`: the internal grid is subdivided by `d`. Then `c / S'` is exact with the new `S' = S / d`. Already-converted reference points of the same element are multiplied by `d`.
- Otherwise (any-snap mode), with a one-time warning "snapped to grid": the result is `(c + floor((S−1)/2)) / S` for `c >= 0` and `(c − floor(S/2)) / S` for `c < 0`, truncating. Values exactly halfway between grid points therefore round down.

(Magic ref: calma/CalmaRdcl.c:1352-1380, cif/CIFrdutils.c:208-345)

#### Scenario: Half-grid instance origin subdivides the grid
- **GIVEN** `S = 10`, rescale allowed, and an SREF at input-unit x = 25
- **WHEN** read
- **THEN** `g = 5`, `d = 2`, the database grid is subdivided by 2, and the instance x is 5 in the new internal units

#### Scenario: Rescale disallowed snaps
- **GIVEN** `gds rescale no`, `S = 10` and an SREF at x = 25
- **WHEN** read
- **THEN** x = (25 + 4) / 10 = 2

### Requirement: Array references
For an AREF, the reader SHALL read COLROW `(cols, rows)` and an XY record of three points `P0, P1, P2`. More than three points: the error "Too many points" is reported and only three are used. Fewer than one point: the error is reported and all points are (0,0). The array SHALL be built as follows:

Compatibility: exact

1. `rotated` is true if the transform's rotation is 90 or 270. When reading `P1` and `P2`:
   - if (not rotated and `rows == 1`) or (rotated and `cols == 1`), their y values are ignored and replaced by `P0.y`;
   - else, if (not rotated and `cols == 1`) or (rotated and `rows == 1`), their x values are replaced by `P0.x`.
2. After scaling (see "Instance placement scaling"), apply the inverse use transform to `P1` and `P2`, giving `p1` and `p2` in child coordinates. Compute `xsep = p1.x / cols`. If `p1.x mod cols != 0`, report "# cols doesn't divide displacement ref pt" and use `(p1.x + (cols+1)/2) / cols`. Compute `ysep = p2.y / rows` (truncating). The same error is reported when not divisible; with `magic-compat` ON (the default) the truncated value is used, as in Magic, and with it OFF the rounded value `(p2.y + (rows+1)/2) / rows`, as for columns (Q-MASK-124, resolution: compat). The y component of `p1` and the x component of `p2` are ignored.
3. The use is an array with x indices `0..cols−1` and y indices `0..rows−1`, separations `(xsep, ysep)` in child coordinates. This is overridden by property 99 (see "Instance properties").

(Magic ref: calma/CalmaRdcl.c:1243-1505)

#### Scenario: Simple 3×2 array
- **GIVEN** `S = 1`, `num = den = 1`, an AREF with COLROW 3 2, XY (0,0) (300,0) (0,400) and no STRANS
- **WHEN** read
- **THEN** the use is an array x 0..2, y 0..1 with xsep = 100 and ysep = 200

#### Scenario: Rotated array
- **GIVEN** the same with ANGLE 90 and XY (0,0) (0,300) (−400,0)
- **WHEN** read
- **THEN** the inverse-rotated `p1 = (300,0)` and `p2 = (0,400)`, so xsep = 100 and ysep = 200

#### Scenario: Single row ignores bogus y data
- **GIVEN** COLROW 4 1 and XY (0,0) (400,999) (0,12345)
- **WHEN** read
- **THEN** xsep = 100 and ysep = 0

#### Scenario: Row spacing not divisible
- **GIVEN** `S = 1`, `num = den = 1`, an AREF with COLROW 1 3, XY (0,0) (0,0) (0,302) and no STRANS
- **WHEN** read
- **THEN** "# rows doesn't divide displacement ref pt" is reported
- **AND** ysep = 100 with `magic-compat` ON and ysep = (302 + 2) / 3 = 101 with it OFF

### Requirement: Instance properties
After the XY record of an SREF/AREF, the reader SHALL process PROPATTR/PROPVALUE pairs:

Compatibility: exact

- Attribute 61 or 98: PROPVALUE is the instance identifier (instance name). Each `/` or `,` in it is replaced by `_` (informative message "character cannot be used in instance name", limited to 100 reports).
- Attribute 99: PROPVALUE `"%d_%d_%d_%d"` gives the array index limits `xlo_xhi_ylo_yhi`, which replace `0..cols−1` and `0..rows−1`. A malformed value is reported and ignored.
- Any other attribute: with `magic-compat` ON (the default) it ends property processing for the element, and its value and all following properties are skipped, as in Magic; with it OFF only that PROPATTR/PROPVALUE pair is skipped and processing continues, so a later 61, 98 or 99 property is honored (Q-MASK-125, resolution: compat).

Uses without a instance identifier, and duplicate identifiers, SHALL get unique identifiers at the end of the structure, following the layout database's default naming (capability `layout-geometry-model`).

(Magic ref: calma/CalmaRdcl.c:1385-1445, calma/calmaInt.h:113-116)

#### Scenario: Named instance with array limits
- **GIVEN** an AREF 2×1 with properties 61=`x/y` and 99=`1_2_0_0`
- **WHEN** read
- **THEN** the use id is `x_y` and the array x indices are 1..2, y 0..0

### Requirement: Use placement rules
Before placing a use, the reader SHALL check the following:

Compatibility: exact

- If the referenced cell is the cell being read or one of its ancestors, the errors "Cell X is an ancestor of Y and can't be used as a subcell." and "(Use skipped)" are reported and the element is dropped.
- If an identical use (same cell, transform and array parameters) is already placed in the cell, the new one is discarded with "Warning: cell "<name>" placed on top of itself. Ignoring the extra one."

(Magic ref: calma/CalmaRdcl.c:1196-1203, calma/CalmaRdcl.c:1505-1525)

#### Scenario: Exact duplicate SREF dropped
- **GIVEN** two identical SREFs of `A` at (0,0)
- **WHEN** read
- **THEN** only one use of `A` exists and a warning is reported

### Requirement: Magnified instances
A use cannot be scaled, so an SREF/AREF with MAG `d != 1`, or any reference read inside a magnified copy, SHALL be resolved to a *magnified copy* of the referenced structure as follows:

Compatibility: exact

1. Approximate `d` by the continued fraction `a/b` with `b <= 10000`. Stop when `|a/b − d| <= 1e-12·d`, the next denominator would exceed 10000, or the integer limits would overflow. If the result is inexact, report "Magnification <d %.15g> of cell <s> approximated as a/b". If the numerator is ≤ 0, use 1/10000.
2. Total magnification = (enclosing total `N/D`) × `a/b`, reduced by the GCD. If the total is 1, the original cell is used.
3. Copy name: format the total with `%.6g` and replace each `.` by `p`. Name = `<sname>_mag<text>`, for example `cell_mag1p5` or `cell_mag2`.
4. If a cell with the copy name was already read in this file, reuse it.
5. Otherwise search for the structure `sname` from the start of the file. If it is not found, report "Cannot magnify cell s, which is not defined in this file; placing it without magnification." and use the original. If it is found, read it as a new structure named with the copy name. Every coordinate is scaled by the extra factor (`num ×= a`, `den ×= b`). Nested references inside it are magnified by the new total. The copy is never read-only. It gets the string property `GDS_MAG_SOURCE = <sname>`. The input position, the lookahead state and the scale are then restored.
6. The magnification is applied before the reference points are read, so that the points are read at the outer scale.

(Magic ref: calma/CalmaRdcl.c:826-1058, calma/CalmaRdcl.c:1205-1232)

#### Scenario: 1.5x instance
- **GIVEN** structure `C` with a box [0,10]×[0,10] (`num = den = 1`, `S = 1`) and an SREF of `C` with MAG 1.5 at (0,0)
- **WHEN** read
- **THEN** cell `C_mag1p5` exists with box [0,15]×[0,15] and property `GDS_MAG_SOURCE = C`
- **AND** the use refers to `C_mag1p5`

#### Scenario: Nested magnification cancels
- **GIVEN** an SREF with MAG 2 of `P`, where `P` contains an SREF of `Q` with MAG 0.5
- **WHEN** read
- **THEN** `P_mag2` contains a use of the original `Q`, because the total is 1

### Requirement: Post-order reading and file repositioning
The reader SHALL read the definition of a referenced structure ahead of its use (*look-ahead*) when the referenced cell is not yet known in this file and at least one of these holds: `ordering` is on, `flatten` is on, or the referenced name matches a `flatglob` pattern.

Compatibility: exact

Look-ahead:
1. Save the current position. Search forward from it for a BGNSTR whose STRNAME equals the name. On reaching the end, if the saved position is not 0, rewind to the start of the file and search again. This sets a session-wide *rewound* flag. Unless `ordering` is on, it also reports "Rewinding input. Cells may have been instanced before they were defined. Consider using "gds ordering on"."
2. If found, read that structure completely (recursively, with the same rules), restore the position and the in-progress planes of the current cell, and mark the read cell *pre-loaded*. When the sequential pass later reaches it, it is skipped silently.
3. If not found, create an empty cell for the name and mark it *dereferenced*. If no cell of that name exists in memory, report "Cell "<name>" is used but not defined in this file." A name already in memory is assumed to come from a library read earlier, as an addendum, and is not reported.

Position bookkeeping: after a STRNAME match, the reader moves back to the start of the BGNSTR record. It moves back to the start of the BGNSTR record using the actual length of the STRNAME record from its header (or a saved BGNSTR offset), in both `magic-compat` settings (Q-MASK-126, resolution: fix). For conforming files this equals Magic's `L_BGNSTR + 4 + len'`, where `len'` is the string length rounded up to even; for a STRNAME padded beyond that, Magic loses synchronisation.

With default options (`ordering off`, no flattening), structures SHALL be read strictly in file order without seeking.

(Magic ref: calma/CalmaRdcl.c:119-205, calma/CalmaRdcl.c:1100-1190, commands/CmdCD.c:580-602)

#### Scenario: Ordering on with top-down file
- **GIVEN** `gds ordering on` and a file defining `TOP` (SREF of `A`) before `A`
- **WHEN** read
- **THEN** `A` is read while processing `TOP`'s SREF, and the later `A` structure is skipped with no duplicate error

### Requirement: Flattening small cells on input
With `gds flatten yes` (or `gds flatten <N>` with N > 0, which sets the limit to N), a structure SHALL be *flatten-marked* if all of the following hold:
- it was not referenced or present in memory before its definition was reached (a look-ahead read counts as "not referenced");
- it has fewer shapes than the limit (default 10);
- it has no references.

Compatibility: exact

With `gds flatglob <pattern>`, a structure whose name matches any pattern SHALL be flatten-marked if it was not referenced or present before its definition. Shape and reference counts are not checked. Patterns use shell glob syntax: `*`, `?`, `[...]` and `\` escape. The pattern list defaults to the single pattern `*_CDNS_*`. `flatglob none` empties the list, and each `flatglob <pattern>` appends one pattern.

For a flatten-marked structure:
1. The structure's raw input-layer planes (before `cifinput` operators) are saved with the cell. The cell receives no paint and all its labels are removed.
2. Each reference to it from a parent copies the saved input-layer geometry into the parent's input-layer planes. The copy uses the reference transform in unscaled input units (no internal-unit snapping). For arrays, it copies once per element: `cols × rows` copies at offsets `(x·xsep, y·ysep)` in input units, with `x ∈ [0,cols)` and `y ∈ [0,rows)`. Array limit properties are ignored. Diagonal (split-tile) geometry is transformed with its diagonal orientation.
3. A use of the cell is also placed if the cell is read-only (vendor), or if it contains paint or subcell uses. Otherwise no use is placed.
4. The flattened geometry then goes through the parent's `cifinput` processing together with the parent's own geometry.
5. At the end of the read, the saved planes are discarded. The flatten-marked cell remains in memory.

The flattened copies are merged *before* boolean interpretation, so that layers split between child and parent (for example contact arrays) combine correctly.

The `flatten` and `flatglob` settings SHALL default to `flatten no` (limit 10) and `flatglob` = {`*_CDNS_*`}. `gds flatten 0` sets the limit to 0 and disables flattening. `yes` and `no` toggle without changing the limit.

(Magic ref: calma/CalmaRdcl.c:584-648, calma/CalmaRdcl.c:1527-1636, calma/CalmaRead.c:1085-1105, commands/CmdCD.c:453-578)

#### Scenario: Contact cell flattened into parent
- **GIVEN** `gds flatten yes` and structure `VIA` (2 boundaries: cut and metal, no references) used by an AREF 2×2 in `TOP` with xsep = ysep = 20
- **WHEN** read
- **THEN** `TOP`'s input planes receive 4 copies of the cut and metal shapes before `cifinput` processing
- **AND** `TOP` contains no use of `VIA`

#### Scenario: Default flatglob
- **GIVEN** default options and structure `pcell_CDNS_123` with 50 shapes and an SREF to another cell
- **WHEN** read (defined before any reference)
- **THEN** it is flatten-marked
- **AND** its parent also receives a use, because the cell contains a subcell use

#### Scenario: Shape limit
- **GIVEN** `gds flatten 3` and a structure with 3 boundaries
- **WHEN** read
- **THEN** it is not flattened, because 3 is not less than 3

### Requirement: Polygon subcells
With `gds polygon subcells <mode>`, each non-Manhattan BOUNDARY on a mapped layer SHALL be read into its own new cell instead of the cell being read. The cell is named `polygon%05d` with a per-read counter starting at 1. It is processed through `cifinput` on its own and placed in the current cell with the identity transform.

Compatibility: exact

Mode values:
- `none` or `no` (the default): disabled.
- `temporary` or `yes`: at the end of each structure, every use whose cell name begins with `polygon` is flattened back into the structure's cell (its Magic-layer paint is copied), and the polygon cell is deleted. See Q-MASK-127.
- `keep`: the subcells remain.

(Magic ref: calma/CalmaRdpt.c:280-310, calma/CalmaRdpt.c:405-430, calma/CalmaRdcl.c:323-350, calma/CalmaRdcl.c:665-667, commands/CmdCD.c:701-743)

#### Scenario: Keep polygon subcells
- **GIVEN** `gds polygon subcells keep` and a structure with two triangles and one rectangle
- **WHEN** read
- **THEN** cells `polygon00001` and `polygon00002` exist, each used once in the structure, and the rectangle is in the structure itself

### Requirement: Path subcells
With `gds path subcells yes`, each mapped, nonzero-width PATH SHALL be read into its own new cell named `path%05d`, with a per-read counter starting at 1. The cell is processed through `cifinput` and placed with the identity transform. These subcells are always kept. The default is `no`.

Compatibility: exact

(Magic ref: calma/CalmaRdpt.c:694-745, commands/CmdCD.c:855-875)

#### Scenario: Paths in subcells
- **GIVEN** `gds path subcells yes` and a structure with three paths
- **WHEN** read
- **THEN** cells `path00001`..`path00003` are created and used once each

### Requirement: End-of-structure processing
After the element list of a structure that is not flatten-marked, the reader SHALL apply these steps in order:

Compatibility: exact

1. Interpret the input-layer planes through the `cifinput` style into Magic types (capability `mask-input-styles`). If mask hints are requested, generate them (see "Mask hints on input"). Clear the input planes.
2. Delete labels with empty text.
3. If polygon subcells mode is `temporary` and this structure created polygon cells, flatten them (see "Polygon subcells").
4. Re-attach labels to the material under them (label adjustment, capability `layout-geometry-model`).
5. Recompute the bounding box.
6. Unless the read is read-only or `gds drccheck no` is set, mark the whole cell for DRC checking.
7. Mark the cell modified.
8. Assign unique instance identifiers.

Flatten-marked structures skip step 1. Their planes are saved instead (see "Flattening small cells on input"), and steps 2–8 still apply.

(Magic ref: calma/CalmaRdcl.c:608-700)

#### Scenario: DRC check suppressed
- **GIVEN** `gds drccheck no`
- **WHEN** a file is read
- **THEN** no cell read is queued for DRC checking

### Requirement: End-of-read cleanup
After the last structure, the reader SHALL do the following:

Compatibility: exact

- Report "cell <name> was used but not defined." for each cell referenced in the file that never received a definition.
- Mark every cell created or referenced by the read as modified, and (unless `drccheck no`) for DRC checking.
- Mark cells with a nonzero timestamp as not needing a new timestamp.
- Discard all saved flatten planes.
- Compute content-hash timestamps if the read was read-only (see "Content-hash timestamps for read-only cells").

(Magic ref: cif/CIFrdcl.c:1734-1840, calma/CalmaRead.c:290-332)

#### Scenario: Undefined reference reported
- **GIVEN** a file whose `TOP` references `MISSING`, which is never defined and not in memory
- **WHEN** read
- **THEN** "cell MISSING was used but not defined." is reported and an empty cell `MISSING` exists

### Requirement: Read-only vendor views
With `gds readonly yes`, every structure read (except magnified copies) SHALL produce a *read-only* cell that carries three properties:

Compatibility: exact

- `GDS_START` (floating point): the byte offset in the decompressed stream immediately after the structure's STRNAME record.
- `GDS_END` (floating point): the byte offset immediately after the structure's ENDSTR record.
- `GDS_FILE` (string): the path of the file with substitution. The path is the file name as found on the search path with `magic-compat` ON (the default), as in Magic, and the absolute canonical path with it OFF (Q-MASK-129, resolution: compat). Then, if a PDK path variable (`PDK_PATH`, `PDKPATH`, `PDK_ROOT` or `PDKROOT`, checked in this order) is defined and is a prefix of the path, the prefix is replaced by `$<VARNAME>`; otherwise, if the path is absolute and starts with the home directory followed by `/`, that prefix is replaced by `~`. The `~` substitution is applied to the GDS path itself in both settings (Q-MASK-128, resolution: fix; Magic applies it to another string and can store a `.mag` path, which loses the vendor cell's contents on the next write). The library-name comparison of `gds-write` full dumps uses the same substitution.

Setting the `GDS_FILE` property on any cell, by any means, SHALL make the cell read-only, and removing it SHALL clear that state. Read-only cells are still interpreted through `cifinput`. Their Magic contents are representational, and GDS output copies the bytes `[GDS_START, GDS_END)` verbatim (capability `gds-write`).

In read-only mode:
- the read SHALL be refused unless the UNITS `mpd` satisfies `(int)(0.5 + mpd·1e11) == 100` (1 nm), or `(int)(0.5 + mpd·1e12) == 100` (1 Å) when the current *output* style uses ångström units. Otherwise the error "Incompatible scale factor of <mpd>, must be 1e-9." (or "1e-10.") and "Cannot read this file in read-only mode." are reported and nothing is read;
- unknown-layer messages are suppressed for read-only cells;
- cells are not queued for DRC checking when read, and background DRC ignores read-only cells.

The default is `readonly no`.

(Magic ref: calma/CalmaRdcl.c:514-575, calma/CalmaRead.c:842-875, database/DBio.c:6060-6111, database/DBprop.c:98-105, drc/DRCcontin.c:770)

#### Scenario: Offsets of a read-only structure
- **GIVEN** `gds readonly yes` and an uncompressed file where structure `A` has BGNSTR at offset 100 (28 bytes) and STRNAME `A` (6 bytes, padded), so its first element starts at offset 134, and its ENDSTR record occupies bytes 400..403
- **WHEN** read
- **THEN** `GDS_START = 134` and `GDS_END = 404`

#### Scenario: Wrong units refused
- **GIVEN** `gds readonly yes` and a file with `mpd = 1e-8`
- **WHEN** read
- **THEN** the read stops after UNITS with "Incompatible scale factor of 1e-08, must be 1e-9."

#### Scenario: PDK path substitution
- **GIVEN** `PDK_ROOT=/opt/pdk` and a file opened as `/opt/pdk/sky130A/libs.ref/gds/cells.gds`
- **WHEN** read with `readonly yes`
- **THEN** `GDS_FILE = $PDK_ROOT/sky130A/libs.ref/gds/cells.gds`

### Requirement: Content-hash timestamps for read-only cells
The timestamp of a read-only cell SHALL be a content hash of its GDS structure data, computed as follows:

Compatibility: exact

**Structure table.** Scan the file from byte 0, record by record (`L` from the header; stop at EOF, at `L < 4`, or at ENDLIB):
- At BGNSTR, a new structure begins. The next record must be STRNAME. If it is not, the structure is not tabulated.
- At STRNAME: create an entry with `h = 14695981039346656037` (FNV-1a 64-bit offset basis), keyed by the byte offset just after the STRNAME record. Also key it by name (the string up to the first NUL), unless that name is already keyed. When names repeat, references resolve to the first.
- Every following record of the structure, through and including ENDSTR, updates `h` with FNV-1a over its 4 header bytes and then its `L − 4` data bytes.
- For each SNAME record, append its name to the structure's child list, unless it equals the immediately preceding child.
- Records outside a tabulated structure (including BGNSTR, STRNAME and library records) are not hashed.

**FNV-1a step** for each byte `b`: `h = (h XOR b) × 1099511628211 mod 2^64`.

**Stamp of a structure** (memoized; states: not started / in progress / done):
```
stamp(s):
  if s.done: return s.stamp
  h = s.h
  if not s.in_progress:
      s.in_progress = true
      for child name c in s.children (in order):
          if c names a tabulated structure t:
              v = stamp(t)
              h = FNV1a(h, [v>>24 & 255, v>>16 & 255, v>>8 & 255, v & 255])
  h = h XOR (h >> 31) XOR (h >> 62)
  s.stamp = h AND 0x7fffffff;  if s.stamp == 0: s.stamp = 1
  s.done = true
  return s.stamp
```
A reference cycle, which is illegal GDS, is not followed: the re-entered structure is folded without children. Children not defined in the file contribute only through their SNAME bytes.

**Lookup.** A cell's stamp is the stamp of the structure whose table offset equals the cell's `GDS_START`. It is 0 (no stamp) if the cell lacks `GDS_END` or `GDS_START`, or if no structure starts there.

(Magic ref: calma/CalmaRead.c:334-720)

#### Scenario: Leaf structure stamp
- **GIVEN** a structure `A` whose records after STRNAME are the 70 bytes `00 04 08 00 | 00 06 0d 02 00 01 | 00 06 0e 02 00 00 | 00 2c 10 03` + the five points (0,0) (0,10) (10,10) (10,0) (0,0) as I4 pairs + `00 04 11 00 | 00 04 07 00`
- **WHEN** its stamp is computed
- **THEN** the 64-bit hash is `0x0c4e03681012e6ae` and the stamp is 143581310

#### Scenario: Parent stamp includes child stamp
- **GIVEN** a structure `B` in the same file whose records after STRNAME are `00 04 0a 00 | 00 06 12 06 41 00 | 00 0c 10 03 00 00 00 00 00 00 00 00 | 00 04 11 00 | 00 04 07 00` (an SREF of `A` at the origin)
- **WHEN** its stamp is computed
- **THEN** its own hash `0xae940992e596efeb` is extended with bytes `08 8E 6F 7E` (143581310), giving `0x5e59f8ba5134dfa3`
- **AND** the stamp is 1837575894

#### Scenario: Empty structure
- **GIVEN** a structure with no elements (only ENDSTR after STRNAME)
- **WHEN** its stamp is computed
- **THEN** the stamp is 1595775465

#### Scenario: Dates excluded
- **GIVEN** two files that differ only in BGNSTR/BGNLIB dates
- **WHEN** both are read read-only
- **THEN** corresponding cells get identical timestamps

### Requirement: When content-hash timestamps are assigned
Content-hash timestamps SHALL be applied as follows:

Compatibility: exact

1. **At read time**, if `readonly` is on, the read was not interrupted, and `datestamp` is either the default or `no`: the file is re-scanned from the start. Every read-only cell created by this read that is not fixed-stamp and has a nonzero stamp gets that stamp and is marked as not needing a new timestamp. A `datestamp <N>` with N ≠ 0 overrides the hash.
2. **When timestamps are updated** (before saving), a read-only cell that is not fixed-stamp and not non-editable SHALL have its stamp recomputed from the file named by `GDS_FILE`. If the result is nonzero and different, it replaces the timestamp, and the change propagates to parents like any timestamp change. If the file cannot be found or yields 0, the timestamp is unchanged. Each GDS file is scanned at most once per update pass. The scan cache is cleared at the end of the pass.
3. File lookup for `GDS_FILE` `f`: try `f` and then `f.gz` along the cell search path and the library path. If `f` does not start with `/`, `~` or `$` and the cell came from a `.mag` file, also try `<directory of that .mag file>/f` and then `.../f.gz`.

The hash SHALL NOT depend on the cell's Magic contents. Two views of the same GDS data, for example a full GDS view and a LEF abstract annotated with the same `GDS_FILE`/`GDS_START`/`GDS_END`, SHALL have the same timestamp.

(Magic ref: calma/CalmaRead.c:313-318, calma/CalmaRead.c:681-790, database/DBtimestmp.c:225-320, calma/CalmaWrite.c:835-890)

#### Scenario: Edit does not change a vendor stamp
- **GIVEN** a read-only cell with stamp 143581310 whose Magic view is then edited
- **WHEN** timestamps are updated and the cell is saved
- **THEN** its timestamp remains 143581310

#### Scenario: Pointing to new data changes the stamp
- **GIVEN** a read-only cell whose `GDS_START`/`GDS_END` are changed to another structure in the same file
- **WHEN** timestamps are updated
- **THEN** its timestamp becomes the other structure's stamp, and its parents receive new timestamps

### Requirement: Mask hints on input
With `gds maskhints yes` (all input layers) or `gds maskhints <layer,layer,...>` (named input layers), after each structure's `cifinput` interpretation the reader SHALL do the following for every requested input layer `I`. A layer is processed only if the current output style has a layer with the same name `O` that contains a `mask-hints` operator.

Compatibility: exact

1. Scale the input planes to output-style units (multiply by `S_out / S`).
2. Generate the output style's layers from the cell's Magic contents.
3. Compute `I AND-NOT O`: the input material that output generation would not reproduce.
4. Divide each resulting rectangle's coordinates by `S_out` (truncating), and paint them into the cell's plane property `MASKHINTS_<I>`, creating the property if needed.

Only additive differences are recorded. Material that output would add beyond the input is not recorded. Non-Manhattan hints are not supported. `gds maskhints no` (the default) disables this. This step is owned by capability `mask-hints` requirement "Mask hints generated by gds maskhints", which also owns the hint data model and the output semantics of `MASKHINTS_*` properties; if the two texts ever differ, `mask-hints` governs.

(Magic ref: cif/CIFrdcl.c:825-990, commands/CmdCD.c:643-677)

#### Scenario: Hint for a slot lost in translation
- **GIVEN** `gds maskhints yes`, input layer `MET1` and an output layer `MET1` with `mask-hints MET1`, and a GDS rectangle on MET1 that the output rules would not regenerate
- **WHEN** read
- **THEN** the cell gets property `MASKHINTS_MET1` containing that rectangle in internal units

### Requirement: Text magnification scale option
`gds magscale <value>` SHALL set the factor `magscale` used for text MAG records (see "TEXT element parsing"). The value is read as a floating-point number. `gds magscale default` sets it to 1.0, and with no argument the current value is reported. The default is 1.0, meaning MAG 1 equals a 1 µm text height.

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:432-451, calma/CalmaRead.c:85-90)

#### Scenario: Reset magscale
- **GIVEN** `gds magscale 0.1`
- **WHEN** `gds magscale default` is issued
- **THEN** the factor is 1.0

### Requirement: Grid subdivision option
`gds rescale yes|no` SHALL allow or forbid internal grid subdivision when instance reference points fall off the internal grid (see "Instance placement scaling"). The default is `yes`. With `magic-compat` ON (the default), setting `no` SHALL also set the warning level to `limit` (see "Read diagnostics"), as in Magic; with it OFF it leaves the warning level unchanged (Q-MASK-130, resolution: compat).

Compatibility: exact

(Magic ref: commands/CmdCD.c:897-917, cif/CIFrdutils.c:85-95)

#### Scenario: Disallow rescale sets warning limit
- **GIVEN** `gds warning default`
- **WHEN** `gds rescale no` is issued
- **THEN** the warning level becomes `limit`
- **AND** with `magic-compat` OFF it stays `default`

### Requirement: Read option syntax and defaults
The `gds` command (alias `calma`) SHALL accept the read-related options below. The option keyword and enumerated values are matched by unique prefix. Boolean values accept `no|false|off|0` and `yes|true|on|1`. Giving an option without a value reports its current state.

Compatibility: behavioral

| Option | Values | Default |
|---|---|---|
| `read <file>` | path | — |
| `datestamp` | `yes`, `no`, integer | `yes` (use the GDS date) |
| `nodatestamp` | bool (`yes` ≡ `datestamp no`) | `no` |
| `drccheck` | bool | `yes` |
| `flatten` | bool or integer limit | `no`, limit 10 |
| `flatglob` | pattern or `none` | `*_CDNS_*` |
| `magscale` | float or `default` | 1.0 |
| `maskhints` | bool or comma list of input layers | `no` |
| `noduplicates` | bool | `no` |
| `ordering` | bool | `off` |
| `polygon subcells` | `none`/`temporary`/`keep`/bool | `none` |
| `path subcells` | bool | `no` |
| `readonly` | bool | `no` |
| `rescale` | bool | `yes` |
| `savepaths` | bool | `no` |
| `unique` | bool | `no` |
| `warning` | `default`/`none`/`align`/`limit`/`redirect [file]` | `default` |
| `library` | bool (shared with write) | `no` |

The options persist for the session and apply to subsequent reads. `cif istyle` selects the input style used (capability `cif-format-io`).

(Magic ref: commands/CmdCD.c:131-1075, calma/CalmaRead.c:60-110, calma/CalmaRdcl.c:50-60)

#### Scenario: Prefix match
- **GIVEN** the command `gds noduplicates t`
- **WHEN** executed
- **THEN** noduplicates is enabled

### Requirement: Read command and file lookup
`gds read <file>` SHALL require a cifinput style. If none exists, it reports "Nothing in "cifinput" section of tech file." and reads nothing. If no edit cell exists, a new empty edit cell is created first.

Compatibility: behavioral

The file SHALL be searched along the cell search path by appending, in order, `.gds`, `.gds.gz`, `.gds2`, `.strm` and `""` to the given name. The extension is always appended, so `foo.gds` is tried as `foo.gds.gds`, … and finally `foo.gds`. The first file that opens is read. Gzip-compressed files are decompressed transparently, and uncompressed files are read as-is. If no candidate opens, "Cannot open <f>.gds, <f>.strm or <f> to read GDS-II stream input." is reported.

The read SHALL NOT insert any geometry into the edit cell. Cells are created by name, and the library cell is displayed afterwards. The help text ("into edit cell") does not match this behavior; see Q-MASK-131.

(Magic ref: commands/CmdCD.c:1012-1033, calma/CalmaRead.c:185-215, utils/path.c:832-900)

#### Scenario: Extension search
- **GIVEN** a file `chip.gds.gz` in the current directory
- **WHEN** `gds read chip` is issued
- **THEN** `chip.gds` is not found, `chip.gds.gz` is opened and decompressed, and its structures are read

### Requirement: Read diagnostics
Errors SHALL be counted for every reported problem.

Compatibility: informative

Reporting depends on the warning level:
- `default`: all messages are printed.
- `none`: nothing is printed.
- `limit`: only the first 99 messages are printed, then "Error limit set: Remaining errors will not be reported.".
- `redirect <file>`: messages are written to `<file>`, which is opened for writing at the start of each read. Without a file name, messages are discarded.

Each GDS read error is prefixed `Error while reading cell "<cell>" (byte position <offset>): `. Hefesto MAY format messages differently. The conditions that produce messages are specified in the other requirements.

(Magic ref: calma/CalmaRead.c:936-1035, commands/CmdCD.c:977-1005)

#### Scenario: Limit mode
- **GIVEN** `gds warning limit` and a file producing 150 errors
- **WHEN** read
- **THEN** 99 error messages and the limit notice are printed

### Requirement: Host mechanisms used by the reader
Magic resolves PDK path variables through Tcl global variables and the home directory through `$HOME`, and opens files through zlib. Hefesto SHALL resolve `PDK_PATH`, `PDKPATH`, `PDK_ROOT` and `PDKROOT` from the Tcl global variables of the project's interpreter (Tcl is embedded on every platform), seeded from the settings store on iPadOS; SHALL take the home directory for `~` from the platform services; and SHALL bundle zlib so gzip input works on all platforms. Local-time conversion of GDS dates uses the host time zone by default; a time-zone override setting (for example UTC) replaces it, and golden captures and CI SHALL run with the zone pinned (Q-MASK-132, resolution: platform).

Compatibility: informative

(Magic ref: database/DBio.c:6060-6111, calma/CalmaRdio.c:335-370)

#### Scenario: iPadOS without environment variables
- **GIVEN** Hefesto on iPadOS with a configured PDK root `/pdk`
- **WHEN** a read-only GDS under `/pdk/...` is read
- **THEN** `GDS_FILE` is stored with the `$PDK_ROOT` prefix, matching Magic on a desktop host configured with the same PDK root
