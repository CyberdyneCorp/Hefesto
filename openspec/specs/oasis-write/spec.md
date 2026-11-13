# OASIS Writing

## Purpose
Defines how Hefesto serializes a layout hierarchy to an OASIS file (the `oasis write` operation, capability `oasis-command`). Magic has no OASIS writer. The governing rule is **GDS consistency**: the OASIS file SHALL describe exactly the same cells, mask geometry, texts, references, arrays and reference properties as the GDS file that capability `gds-write` produces for the same layout, technology `cifoutput` style and equivalent options. It is obtained by translating the GDS element stream record by record. Encoding choices that have no GDS counterpart (name tables, modal compression, repetitions, CBLOCKs, strict mode, validation) are `native` and fully specified here, so that the output is byte-deterministic.

Byte-level encodings are owned by capability `oasis-format`. Mask geometry generation, structure ordering, structure naming, scaling, label selection and vendor-cell rules are owned by capability `gds-write` and are referenced, not repeated. The `cifoutput` grammar is owned by `mask-output-styles`. Option syntax is owned by `oasis-command`.

Terminology (in addition to `gds-write`):
- **GDS element stream of a cell**: the sequence of elements (SREF, AREF, BOUNDARY, TEXT, with their records) that gds-write "Structure body order" would write for that cell with the current settings. Dates are excluded.
- **Translation**: the mapping from a GDS element to an OASIS element defined in this capability.
- **Output name**: a cell name after gds-write "Structure naming" and the OASIS rules of "Cell names".

## Requirements

### Requirement: Write command and output file naming
Hefesto SHALL provide a "write OASIS" operation that writes the hierarchy rooted at the root cell of the current layout view to one file, with file-name rules mirroring gds-write "Write command and output file naming":

Compatibility: native

1. With no file argument (bare `oasis`), the name is the root cell name after its last `/`.
2. With `write <file>`, the name is `<file>`.
3. If the name contains no `.` anywhere, `.oas` is appended. If it contains a `.`, nothing is appended.
4. The path is resolved relative to the current working directory (on iPadOS, relative to the granted document folder; capability `hefesto-files-and-pdks`).

The output is never gzip-wrapped. Compression uses CBLOCKs (see "CBLOCK compression"). If the file cannot be created, nothing is written and an error is reported.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Extension added
- **GIVEN** root cell `inv`
- **WHEN** the user runs `oasis write out/inv`
- **THEN** the file `out/inv.oas` is created

#### Scenario: Bare command
- **GIVEN** the root cell of the window is `top`
- **WHEN** the user runs `oasis` with no arguments
- **THEN** the file `top.oas` is written

### Requirement: Write option settings
The writer SHALL honour these `oasis` settings (capability `oasis-command`). The options that share a name with gds-write options have the gds-write semantics:

Compatibility: native

| Option | Default | Effect |
|---|---|---|
| `labels`, `lower`, `library`, `addendum`, `arrays`, `merge`, `undefined`, `abstract`, `contacts` | as gds-write | as gds-write, applied to the GDS element stream before translation |
| `compress` | 6 | DEFLATE level 1–9 for CBLOCKs; 0 = no CBLOCKs |
| `strict` | yes | strict-mode file with name tables; `no` writes names inline |
| `validation` | `crc32` | END validation scheme: `none`, `crc32` or `checksum` |
| `repetitions` | yes | shape repetition detection (see "Shape repetition detection") |
| `layernames` | yes | write LAYERNAME records for output layers |

`datestamp` and `nodatestamp` do not affect writing, because OASIS has no dates. The output-style flags `units angstroms`, `options calma-permissive-labels` and `options string-limit` apply as in gds-write. The `cif` hierarchy and array generation options apply as in gds-write.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Uncompressed output
- **GIVEN** `oasis compress 0`
- **WHEN** a library is written
- **THEN** the file contains no CBLOCK record

### Requirement: GDS-consistent content
For every cell that gds-write would write from Hefesto's own data, the writer SHALL emit a CELL whose elements are the translations of that cell's GDS element stream, in the same order. The only exception is shape repetition grouping (see "Shape repetition detection"), which may group and reorder BOUNDARY translations within one mask layer.

Compatibility: exact

Consequently, the following are inherited unchanged from gds-write, and are NOT re-specified here:
- the preconditions and subtree loading, including `undefined` handling;
- the set and order of cells (depth-first post-order) and the cell id numbers;
- the generation area, mask geometry, tile enumeration order, and the unmerged and merged polygon output, including the 200-point limit (see Q-OASIS-62);
- the selection, order and coordinates of label TEXTs, label rectangles and `labellayer` TEXTs;
- the reference transforms, positions, array parameters, `arrays yes` flattening, and the use-name and array-index properties;
- the scale factors `Wscale` and `Pscale`, and the skipping of layers whose GDS layer number is outside 0–255;
- the read-only, abstract, addendum and full-dump rules (with the translation rules of "Read-only GDS vendor cells").

Translation table:

| GDS element | OASIS element |
|---|---|
| SREF | PLACEMENT (see "Placements") |
| AREF | PLACEMENT with repetition (see "Array placements") |
| PROPATTR/PROPVALUE pairs of a reference | `S_GDS_PROPERTY` PROPERTY records after the PLACEMENT (see "Reference properties") |
| BOUNDARY | RECTANGLE or POLYGON (see "Boundary translation") |
| TEXT | TEXT (see "Text translation") |

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaWrite.c:284-388, calma/CalmaWrite.c:1419-1610)

#### Scenario: Same geometry as GDS
- **GIVEN** any layout, style and options, a GDS file `G` written by `gds write`, and an OASIS file `F` written by `oasis write` with equivalent options
- **WHEN** `F` is converted to GDS by an independent converter (KLayout `strm2gds`)
- **THEN** the converted file and `G` have the same structures, the same per-layer merged geometry in every structure, the same texts, and the same references with the same properties

### Requirement: File layout
The writer SHALL emit records in this order:

Compatibility: native

1. The magic bytes and the START record: version `1.0`; unit `1000` (real type 0) for nanometer styles, or `10000` for styles with `units angstroms`; offset-flag 1 in strict mode, or 0 followed by 12 zero bytes (six `(0, 0)` table entries) in non-strict mode.
2. The file-level standard PROPERTY records (see "Standard properties").
3. In non-strict mode only: the LAYERNAME records (see "Layer names").
4. The cells, in gds-write structure order. Each cell is a CELL record followed by its body, which is optionally CBLOCK-compressed (see "CBLOCK compression").
5. In strict mode only: the name tables in the order PROPNAME, PROPSTRING, LAYERNAME, TEXTSTRING, CELLNAME. Each table is optionally compressed. Empty tables are omitted.
6. The END record (see "END record and validation").

The writer SHALL never emit PAD, XYRELATIVE, XYABSOLUTE, PROPERTY-repeat (29), CELL-by-refnum in non-strict mode, or CELL-by-name in strict mode. It SHALL never emit TRAPEZOID, CTRAPEZOID, CIRCLE or PATH records for Hefesto's own data. PATH, TRAPEZOID, CTRAPEZOID, CIRCLE, XELEMENT and XGEOMETRY appear only in re-encoded vendor content.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Ångström unit
- **GIVEN** an output style with `units angstroms`
- **WHEN** a library is written
- **THEN** the START record is `01 03 31 2E 30 00 90 4E 01` (unit 10000)

### Requirement: Units
The START unit SHALL be chosen per write from the current output style: `1000` DB units per micrometre when the style does not have `units angstroms`, and `10000` when it does. Coordinates SHALL be the integer values that gds-write would write in the XY records, so one OASIS DB unit equals one GDS DB unit.

Compatibility: exact

Magic's session-sticky UNITS behaviour (Q-MASK-201) SHALL NOT be reproduced for OASIS: the unit always matches the coordinate scaling of the current write.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaWrite.c:3062-3103, calma/CalmaWrite.c:1450-1478)

#### Scenario: Centimicron style
- **GIVEN** `scalefactor 10` (expander 1) and a metal1 rectangle (0,0)-(4,2) in internal units
- **WHEN** it is written
- **THEN** the unit is 1000 and the rectangle is `[0,400] × [0,200]`, the same as the GDS XY values

### Requirement: Reference numbers and name tables
In strict mode, every name SHALL be written once in its table and referenced by implicit refnum. The refnums are assigned as follows:

Compatibility: native

- **PROPNAME**: 0 `S_MAX_SIGNED_INTEGER_WIDTH`, 1 `S_MAX_UNSIGNED_INTEGER_WIDTH`, 2 `S_TOP_CELL`, 3 `S_CELL_OFFSET`, and 4 `S_GDS_PROPERTY` if and only if at least one `S_GDS_PROPERTY` is written. Refnum 2 is defined even under `library yes`, when no `S_TOP_CELL` is written.
- **PROPSTRING**: first the `S_TOP_CELL` value (if written), then every distinct string property value in order of first use while writing the cells. A distinct string is referenced by one refnum.
- **TEXTSTRING**: every distinct text string in order of first use.
- **CELLNAME**: every distinct output name in order of first need. A name is needed when its CELL record is written or when a PLACEMENT references it, whichever comes first in output order. Referenced but unwritten cells (undefined, addendum, reference-only) get a CELLNAME too.
- **LAYERNAME**: see "Layer names" (no refnums).

Each table is written as consecutive implicit records (3, 5, 7, 9 or 11/12) in refnum order. Each CELLNAME is followed by one standard PROPERTY `S_CELL_OFFSET` (PROPNAME 3, unsigned value) holding the file offset of that cell's CELL record, or 0 for a cell with no CELL record.

In non-strict mode, no name tables are written. Names and strings are written inline (see "Strict and non-strict modes").

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Post-order refnums
- **GIVEN** strict mode and the gds-write structure order `c`, `b`, `a`, `top`, where `top` places `b` and `a`
- **WHEN** the file is written
- **THEN** the CELLNAME refnums are `c` = 0, `b` = 1, `a` = 2, `top` = 3, and the CELLNAME table lists them in that order

### Requirement: Cell names
The output name of a cell SHALL be the gds-write "Structure naming" result (the strict or permissive table, `lower`, `string-limit`, and `XXXXX<n>` for illegal names), as the character string without padding. Two OASIS-specific rules then apply:

Compatibility: exact

1. If the result is empty or contains a space (possible with the permissive table), the cell is illegal for OASIS and gets `XXXXX<n>`, with `n` its cell id number.
2. If the result equals the output name already given to a different cell in this write, the later cell (in order of first need) gets `XXXXX<n>` (with `_k` appended, smallest free `k ≥ 1`, if that is taken too), with a warning. OASIS forbids duplicate cell names. gds-write applies the same uniquification to mapped names (Q-MASK-209); this rule also covers names that only rule 1 makes collide (Q-OASIS-61).

The same output name is used for the CELL record, every PLACEMENT reference, and `S_TOP_CELL`.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaWrite.c:1877-1934)

#### Scenario: Dash mapped to underscore
- **GIVEN** the strict table
- **WHEN** cell `nand-2` is written
- **THEN** its CELLNAME string is `nand_2`, the same as the GDS STRNAME data

#### Scenario: Collision after mapping
- **GIVEN** the strict table and cells `a-b` (id 2) and `a_b` (id 3), with `a-b` needed first
- **WHEN** they are written
- **THEN** `a-b` is named `a_b`, and `a_b` is named `XXXXX3` with a warning

### Requirement: String mapping
Every TEXT string and every string property value SHALL be the byte sequence that gds-write "String records and character mapping" would place in the corresponding STRING or PROPVALUE record, without the padding byte and with these OASIS adjustments:

Compatibility: exact

1. A byte that gds-write maps to `0x00` (a control character or DEL) SHALL be written as `_`, because a-strings cannot contain `0x00` (see Q-OASIS-60).
2. Under `string-limit`, the kept characters are the same ones gds-write keeps (Q-MASK-207): let `n` be the source length and `n' = n` rounded up to even. With `magic-compat` ON, if `n' > 32`, the last `32 − (n' − n)` characters are kept; with it OFF, if `n > 32`, the last 32 characters are kept.
3. Text strings are a-strings and property values are b-strings (or propstring refnums).

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaWrite.c:197-235, calma/CalmaWrite.c:3158-3244)

#### Scenario: Strict mapping of a label
- **GIVEN** the strict table
- **WHEN** a label `A<0>` is written
- **THEN** the TEXTSTRING is `A_0_`

#### Scenario: String limit with odd length
- **GIVEN** `options string-limit` and a 35-character label
- **WHEN** it is written
- **THEN** the TEXTSTRING holds its last 31 characters, the same characters as the GDS STRING record

### Requirement: Boundary translation
A GDS BOUNDARY on `L/D` with points `q_0..q_m` (with `q_m == q_0`) SHALL be translated by first taking the vertex list `v_0..v_{k−1} = q_0..q_{m−1}`:

Compatibility: native

1. **RECTANGLE**: if `k == 4` and the vertices are the corners of an axis-parallel rectangle with positive width and height, in either rotational direction: RECTANGLE with layer `L`, datatype `D`, `x = min x`, `y = min y`, `w = max x − min x`, `h = max y − min y`. The S (square) bit is never used.
2. **POLYGON** otherwise, with `(x, y) = v_0` and a point list of type:
   - 0 or 1, if every edge (including the closing edge) is axis-parallel and nonzero, consecutive edges alternate between horizontal and vertical, and `k` is even. Use type 0 if `v_0 → v_1` is horizontal, else type 1. The list holds `k − 2` 1-deltas for the edges `v_0→v_1 … v_{k−3}→v_{k−2}`, and the remaining two vertices are implicit;
   - 2, if every edge is axis-parallel but rule 0/1 does not apply. The list holds `k − 1` 2-deltas for `v_0→v_1 … v_{k−2}→v_{k−1}`;
   - 3, if every edge is horizontal, vertical or at 45 degrees (`|dx| == |dy|`). The list holds `k − 1` 3-deltas;
   - 4 otherwise, with `k − 1` g-deltas.

   Type 5 is never written.

A BOUNDARY with fewer than 3 distinct vertices is never produced by gds-write. If one occurs, it SHALL be written as a POLYGON by these rules without validity checks.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Unmerged rectangle
- **GIVEN** the gds-write BOUNDARY `(0,0) (400,0) (400,200) (0,200) (0,0)` on 49/0, first in its cell
- **WHEN** it is translated
- **THEN** the record is `14 63 31 00 90 03 C8 01`

#### Scenario: Merged L-shape
- **GIVEN** the merged BOUNDARY `(200,400) (0,400) (0,0) (400,0) (400,200) (200,200) (200,400)`
- **WHEN** it is translated
- **THEN** it is a POLYGON at (200, 400) with point list type 0, count 4, deltas −200, −400, +400, +200; the implicit vertices are (200,200), then the closing vertex (200,400)

#### Scenario: Lower-left triangle
- **GIVEN** the BOUNDARY `(0,10) (0,0) (10,0) (0,10)`
- **WHEN** it is translated
- **THEN** it is a POLYGON at (0,10) with point list type 3, count 2, 3-deltas S 10 and E 10

### Requirement: Modal field omission
Within a cell, the writer SHALL track the modal variables of capability `oasis-format` (reset at each CELL record), and SHALL omit an optional field exactly when its modal variable is defined and equal to the value to be written:

Compatibility: native

- layer, datatype, textlayer, texttype, x and y of geometry, text and placement records (coordinates always absolute; the initial x and y modal values are 0, so a 0 coordinate at the start of a cell is omitted);
- RECTANGLE width and height;
- POLYGON point list (equal type and identical deltas);
- PATH half-width, point list and extension scheme (the scheme is omitted only when both resulting extension values equal their modal values);
- the PLACEMENT cell reference (C = 0 when the modal `placement-cell` names the same cell);
- the TEXT string (C = 0 when the modal `text-string` is the same string);
- the repetition: when an element's repetition equals the modal `repetition` (same type and parameters), repetition type 0 is written.

PROPERTY records SHALL always be written in full: C = 1, N = 1 in strict mode, V = 0, and an explicit value count. They never rely on modal property state.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Second rectangle on the same layer
- **GIVEN** a cell whose first element is the RECTANGLE 49/0 `[0,400]×[0,200]`, followed by the RECTANGLE 49/0 `[1000,1400]×[0,200]`
- **WHEN** they are written
- **THEN** the second record is `14 10 D0 0F`: W, H, Y, layer and datatype are omitted, and only X = 1000 is written

### Requirement: Text translation
A GDS TEXT on `Lt/Dt` at point `(x, y)` with string `s` SHALL be translated to a TEXT record with textlayer `Lt`, texttype `Dt`, position `(x, y)` and string `s` (TEXTSTRING refnum in strict mode, inline a-string otherwise). PRESENTATION, STRANS, MAG and ANGLE have no OASIS counterpart and are dropped. Label font, size, rotation and justification are therefore not represented (see Q-OASIS-32).

Compatibility: exact

The BOUNDARY that gds-write emits after a label TEXT for a labels-only rectangle layer SHALL be translated by "Boundary translation" (it is always a RECTANGLE) and SHALL immediately follow the TEXT. It is not subject to shape repetition detection.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaWrite.c:1536-1606, calma/CalmaWrite.c:2829-2984, calma/CalmaWrite.c:3000-3048)

#### Scenario: Point label
- **GIVEN** `Wscale` 100 and a plain label `VDD` on metal1 at (5,5), with metal1 labels on 49/0, as the first text in its cell, and `VDD` being TEXTSTRING refnum 0
- **WHEN** the cell is written
- **THEN** the record is `13 7B 00 31 00 E8 07 E8 07` (string refnum 0, textlayer 49, texttype 0, x = 500, y = 500)

#### Scenario: Label with rectangle
- **GIVEN** a label layer `M1LBL` with `calma 49 10`, `Wscale` 100, and a plain label rectangle (0,0)-(2,1)
- **WHEN** the cell is written
- **THEN** a TEXT at (100, 50) is followed by a RECTANGLE 49/10 `[0,200]×[0,100]`

### Requirement: Placements
A GDS SREF of output name `n`, with STRANS flags `f`, optional ANGLE `a` and XY `(x, y)`, SHALL be translated to PLACEMENT record 17 with the cell reference `n`, F = 1 if `f & 0x8000`, AA = `a / 90` (0 when there is no ANGLE), and position `(x, y)`. Since gds-write only emits angles 90, 180 and 270 and never MAG, record 18 is never used for Hefesto's own data.

Compatibility: exact

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaWrite.c:1662-1863)

#### Scenario: Rotated SREF
- **GIVEN** `Wscale` 100 and a use `sub_0` of `sub` with transform a=0 b=1 d=−1 e=0 c=10 f=20, with `sub` being CELLNAME refnum 1, the first placement in its cell
- **WHEN** it is written
- **THEN** the record is `11 F6 01 D0 0F A0 1F` (C, N, X, Y, AA = 3; x = 1000, y = 2000)
- **AND** it is followed by the `S_GDS_PROPERTY` for attribute 61 with value `sub_0`

### Requirement: Array placements
A GDS AREF with COLROW `(nx, ny)` and XY points `P0`, `P1`, `P2` SHALL be translated to one PLACEMENT at `P0`, with the transform of the SREF rule and a repetition built from `a = (P1 − P0) / nx` and `b = (P2 − P0) / ny`. These divisions are exact, because gds-write computes `P1` and `P2` as `nx` and `ny` multiples. The repetition is chosen as follows:

Compatibility: exact

1. `nx > 1` and `ny > 1`:
   - if `a = (ax, 0)` with `ax >= 0` and `b = (0, by)` with `by >= 0`: type 1 with `(nx, ny, ax, by)`;
   - else if `a = (0, ay)` with `ay >= 0` and `b = (bx, 0)` with `bx >= 0`: type 1 with `(ny, nx, bx, ay)`;
   - else type 8 with `(nx, ny, a, b)`.
2. Exactly one of `nx`, `ny` greater than 1, with vector `w` (`a` or `b`) and count `n`: type 2 `(n, wx)` if `w = (wx, 0)` with `wx >= 0`; type 3 `(n, wy)` if `w = (0, wy)` with `wy >= 0`; else type 9 `(n, w)`.

Zero vectors are written as they are (for example type 2 with spacing 0). The rules of capability `oasis-read` "Placement repetitions" read every case back as the same Magic array (see Q-OASIS-10). Under `arrays yes`, gds-write emits individual SREFs, which are translated individually.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaWrite.c:1662-1863)

#### Scenario: AREF to regular repetition
- **GIVEN** `Wscale` 100 and an identity use of `sub` (refnum 0) at the origin with x 0..2, y 0..1, xsep 10 and ysep 20
- **WHEN** it is written as the first placement of its cell
- **THEN** the record is `11 C8 00 01 01 00 E8 07 D0 0F` (C, N, R; repetition type 1, nx = 3, ny = 2, sx = 1000, sy = 2000)

#### Scenario: Rotated array with negative pitch
- **GIVEN** a use rotated 180 degrees with x 0..1 and xsep 10, `Wscale` 100
- **WHEN** it is written
- **THEN** `a = (−1000, 0)`, so the repetition is type 9 with `n = 2` and g-delta `(−1000, 0)`

### Requirement: Reference properties
Each PROPATTR/PROPVALUE pair that gds-write emits for a reference SHALL be written, in the same order, as one standard PROPERTY `S_GDS_PROPERTY` immediately after the PLACEMENT, with two values: the attribute as an unsigned integer (type 8), and the mapped value string as a b-string. In strict mode the b-string is a propstring refnum (type 14). In non-strict mode it is inline (type 11).

Compatibility: exact

Hence attribute 61 (the use name) is always written, and attribute 99 (`xlo_xhi_ylo_yhi`) is written when `xlo != 0` or `ylo != 0`. Neither is written under `arrays yes`. See Q-OASIS-69 for the value type.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaWrite.c:1662-1863)

#### Scenario: Use name property in strict mode
- **GIVEN** strict mode, `S_GDS_PROPERTY` = PROPNAME 4, and the use name `sub_0` = PROPSTRING 1
- **WHEN** the reference property is written
- **THEN** the bytes are `1C 27 04 08 3D 0E 01`

### Requirement: Shape repetition detection
With `repetitions yes`, the writer SHALL group translated geometry elements of one mask layer with this deterministic algorithm. With `repetitions no`, every element is written individually in GDS order.

Compatibility: native

Candidates: the RECTANGLE and POLYGON translations of the mask-plane BOUNDARYs of one mask layer in one cell. Label rectangles, `labellayer` TEXTs and vendor content are never candidates. The *key* of a candidate is `(RECTANGLE, w, h)` or `(POLYGON, point-list type, deltas)`. Its *origin* is its `(x, y)`.

```
for each mask layer M in style order:
    groups := candidates of M grouped by key, ordered by first occurrence of the key in GDS order
    out := []
    for each group G:
        P := origins of G sorted ascending by (y, x)
        runs := []
        while P not empty:
            p := remove first of P
            if the next element q of P has q.y == p.y:
                d := q.x - p.x;  k := 1
                while P contains (p.x + k*d, p.y): remove it from P; k += 1
            else: d := 0; k := 1
            append (p, d, k) to runs
        sort runs ascending by (p.x, d, k, p.y)
        i := 0
        while i < |runs|:
            (p, d, k) := runs[i];  m := 1;  e := 0
            if i+1 < |runs| and runs[i+1] has the same (p.x, d, k):
                e := runs[i+1].p.y - p.y;  m := 2
                while i+m < |runs| and runs[i+m] has the same (p.x, d, k) and runs[i+m].p.y == p.y + m*e:
                    m += 1
            append element(G.key, p, repetition(k, d, m, e)) to out
            i += m
    write out sorted ascending by (origin.y, origin.x, group index)
```

`repetition(k, d, m, e)` is type 1 `(k, m, d, e)` if `k >= 2` and `m >= 2`; type 2 `(k, d)` if `k >= 2` and `m == 1`; type 3 `(m, e)` if `k == 1` and `m >= 2`; and none if `k == m == 1`. Elements of different mask layers keep the gds-write layer order.

The set of written shapes, expanded by their repetitions, SHALL equal the set of GDS BOUNDARYs of the layer. Only element order and grouping differ (see Q-OASIS-67).

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Contact array
- **GIVEN** six unmerged 10×10 tiles on one layer with origins (0,0), (20,0), (40,0), (0,30), (20,30), (40,30)
- **WHEN** the layer is written with `repetitions yes`
- **THEN** one RECTANGLE with w = h = 10 at (0,0) is written, with repetition type 1 (nx = 3, ny = 2, sx = 20, sy = 30)

#### Scenario: Irregular rows stay separate
- **GIVEN** 10×10 tiles at (0,0), (20,0), (50,0)
- **WHEN** the layer is written
- **THEN** a RECTANGLE at (0,0) with type 2 repetition (nx = 2, sx = 20) and a single RECTANGLE at (50,0) are written, in that order

### Requirement: CBLOCK compression
With `compress z`, 1 ≤ z ≤ 9, the writer SHALL compress each cell body and each name table into CBLOCKs as follows. With `compress 0` no CBLOCK is written.

Compatibility: native

1. **Chunks.** The CELL record itself is never compressed. The records after it, up to the next CELL record, are appended to a buffer, whole records only. When the buffer reaches 1,048,576 bytes or more after a record is appended, the chunk is flushed. The rest is flushed at the end of the cell. Each name table is chunked the same way, as its own sequence. In strict mode, the CELLNAME table chunks include the `S_CELL_OFFSET` PROPERTY records.
2. **Flush.** The chunk is compressed as one raw DEFLATE stream (RFC 1951, window 2^15, memory level 8, default strategy, level `z`, a single finishing flush). Let `U` be the chunk length, `C` the compressed length, and `H = 2 + len(uint(U)) + len(uint(C))` the CBLOCK header length. If `C + H < U`, the writer emits `CBLOCK(type 0, U, C, data)`. Otherwise it emits the chunk uncompressed.
3. **Implementation.** Hefesto SHALL use one pinned DEFLATE implementation on all platforms, so that the compressed bytes are identical everywhere (see Q-OASIS-64).
4. The file-level PROPERTY records, START and END are never compressed.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Tiny cell stays uncompressed
- **GIVEN** `compress 6` and a cell whose body is one 8-byte RECTANGLE record
- **WHEN** it is written
- **THEN** no CBLOCK is emitted for it, because the compressed size plus the header is not smaller than 8 bytes

#### Scenario: Decompressed bytes match uncompressed output
- **GIVEN** the same layout written with `compress 0` and with `compress 9`
- **WHEN** every CBLOCK of the second file is replaced by its decompressed bytes, and the table offsets, `S_CELL_OFFSET` values and validation signature are recomputed
- **THEN** the result equals the first file byte for byte

### Requirement: Strict and non-strict modes
With `strict yes`, the writer SHALL produce a strict-mode file (capability `oasis-format` "Strict mode"):

Compatibility: native

- START has offset-flag 1, and END carries six table entries, each with strict-flag 1 and the file offset of the first byte of that table (a CBLOCK or the first name record). An absent table has offset 0. The XNAME entry is always `01 00`.
- All CELL records are by refnum (13), PLACEMENTs by refnum (N = 1), TEXTs by refnum, PROPERTY names by refnum, and string values by propstring refnum.

With `strict no`, the writer SHALL write no name tables and no `S_CELL_OFFSET`:
- START has offset-flag 0 and twelve zero bytes;
- CELL records are by name (14), PLACEMENTs carry inline n-strings (N = 0), TEXTs inline a-strings, PROPERTY names inline n-strings (C = 1, N = 0), and string values are inline (`S_TOP_CELL` as type 12, `S_GDS_PROPERTY` values as type 11);
- LAYERNAME records follow the file properties.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Non-strict cell record
- **GIVEN** `oasis strict no` and cell `top`
- **WHEN** it is written
- **THEN** its CELL record is `0E 03 74 6F 70`

### Requirement: Standard properties
Directly after START, the writer SHALL emit these standard file properties, in this order:

Compatibility: native

1. `S_MAX_SIGNED_INTEGER_WIDTH` = unsigned 4;
2. `S_MAX_UNSIGNED_INTEGER_WIDTH` = unsigned 4 (file offsets in table entries and `S_CELL_OFFSET` are not counted; see Q-OASIS-66);
3. `S_TOP_CELL` = the root cell's output name, unless `library yes`, in which case it is omitted.

No `S_BOUNDING_BOXES_AVAILABLE` and no `S_BOUNDING_BOX` are written. In strict mode, every CELLNAME carries `S_CELL_OFFSET` (see "Reference numbers and name tables").

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Library mode omits S_TOP_CELL
- **GIVEN** `oasis library yes`
- **WHEN** a library is written
- **THEN** no `S_TOP_CELL` property appears, and the root cell has no CELL record

### Requirement: Layer names
With `layernames yes`, the writer SHALL emit, for each mask layer of the output style in style order that is not temporary and has a GDS layer number in 0–255 and a datatype in 0–65535, the pair LAYERNAME 11 and LAYERNAME 12. Each record holds the mask layer name as an n-string, an exact layer interval (type 3, `L`) and an exact datatype interval (type 3, `D`). A pair identical to one already written (same name, `L` and `D`) is skipped.

Compatibility: native

A mask layer name that is not a legal n-string is mapped as a cell name would be. If it is still illegal, the layer's records are skipped. A layer whose datatype is negative is not written by gds-write rules either, and is skipped with an error. With `layernames no`, no LAYERNAME records are written.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Layer name records
- **GIVEN** a mask layer `M1` with `calma 49 0`
- **WHEN** the layer names are written
- **THEN** the records are `0B 02 4D 31 03 31 03 00` and `0C 02 4D 31 03 31 03 00`

### Requirement: END record and validation
The writer SHALL write the END record with the table offsets (strict mode), the padding rules of capability `oasis-format`, and the validation scheme selected by `validation`: 0 for `none`, 1 for `crc32` (the default) and 2 for `checksum`. For schemes 1 and 2, the signature is computed over the final file bytes, as defined in `oasis-format`. See Q-OASIS-65 for the default.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: No validation
- **GIVEN** `oasis validation none`
- **WHEN** a file is written
- **THEN** the last byte of the file is `00` and the END record is 256 bytes long

### Requirement: Determinism
The output SHALL be a pure function of the layout, the technology output style, the `oasis` and `cif` settings, the contents of referenced vendor files, and the pinned DEFLATE implementation. Two writes of the same inputs SHALL produce byte-identical files on every platform.

Compatibility: native

OASIS carries no dates, so unlike GDS no field depends on the current time. The only Magic-inherited order dependencies are those of gds-write (use enumeration order, Q-MASK-204; tile enumeration order) and, with `magic-compat` ON, the full-dump prefix sequence (Q-MASK-211 and Q-OASIS-68, settled the same way: with `magic-compat` OFF both the use order and the prefixes are independent of session history).

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Repeated write
- **GIVEN** a layout and fixed settings
- **WHEN** it is written twice, at different times, on macOS and on Linux
- **THEN** the two files are byte-identical

### Requirement: Read-only GDS vendor cells
A read-only cell with `GDS_FILE`, which gds-write copies verbatim, SHALL instead be *transcoded* into OASIS. The gds-write file lookup, the sanity checks and the fallback to generated output (gds-write "Verbatim copy of read-only cells") are unchanged. The GDS records in `[GDS_START, GDS_END)` are decoded and each element is translated as follows:

Compatibility: native

| GDS element | OASIS |
|---|---|
| BOUNDARY | "Boundary translation". A BOUNDARY with fewer than 3 distinct vertices is dropped with a warning |
| BOX | RECTANGLE of the points' bounding box, datatype = BOXTYPE |
| PATH | PATH: half-width `w/2`; type 0 flush/flush, type 2 half-width/half-width, type 4 explicit BGNEXTN/ENDEXTN (0 if absent); type 1 and others flush/flush with a warning; negative `w` uses `|w|` with a warning; odd `w` uses half-width `(w+1)/2` with a warning (Q-OASIS-63); a single-point path is dropped with a warning. The point list type is chosen as in "Boundary translation", without closure |
| TEXT | TEXT with LAYER, TEXTTYPE, the first XY point and the mapped STRING. Other records are dropped |
| SREF | PLACEMENT. Record 17 when there is no MAG (or MAG = 1) and ANGLE is a multiple of 90. Otherwise record 18 with the MAG and ANGLE values as reals. STRANS bit 0x8000 sets F. The absolute flags are ignored |
| AREF | PLACEMENT with `a = (P1−P0)/cols` and `b = (P2−P0)/rows` and the "Array placements" rules, when both divisions are exact. Otherwise repetition type 10 with the displacements `trunc(i·(P1−P0)/cols) + trunc(j·(P2−P0)/rows)`, `j` outer and `i` inner |
| NODE | dropped, with one warning per cell |
| PROPATTR/PROPVALUE | `S_GDS_PROPERTY` on the translated element |

Rules:
1. Element order is the order in the GDS data. No shape repetition detection is applied.
2. SNAME strings follow gds-write (Q-MASK-216): with `magic-compat` ON they are used as cell names without re-mapping, with gds-write's warning when a child's current output name differs; with it OFF each SNAME is replaced by the referenced child's current output name. An SNAME that is not a legal n-string is mapped as in "Cell names".
3. Coordinates are copied unchanged. If the vendor file's UNITS (meters per DB unit) do not round to the output unit (1e-9, or 1e-10 for ångström styles), the transcoding is refused with an error, and the cell is generated from its layout.
4. Full-dump libraries (gds-write "Full dump of a vendor library") are transcoded structure by structure with the same naming and prefix rules.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Vendor standard cell
- **GIVEN** a read-only cell `sc_inv` whose GDS bytes hold two BOUNDARYs, one PATH of width 140 (pathtype 2) and one TEXT
- **WHEN** a hierarchy using it is written to OASIS
- **THEN** cell `sc_inv` contains, in order, two RECTANGLE or POLYGON records, a PATH with half-width 70 and half-width extensions, and one TEXT

### Requirement: Read-only OASIS vendor cells
A read-only cell with `OASIS_FILE` and `OASIS_CELL` (capability `oasis-read`) SHALL be *re-encoded* from its source file:

Compatibility: native

1. Its children are traversed first, as in gds-write. A magnified-copy child is replaced by its `GDS_MAG_SOURCE` cell, as in gds-write.
2. The file is looked up like `GDS_FILE`. The cell named `OASIS_CELL` is decoded. If the file or the cell cannot be found, the gds-write rules for an unopenable vendor file apply.
3. If the source unit differs from the output unit, an error is reported and the cell is generated from its layout.
4. Every element is re-encoded with the same record type (RECTANGLE, POLYGON with the same point list type, PATH, TRAPEZOID as record 23, CTRAPEZOID, CIRCLE, TEXT, PLACEMENT as 17 or 18, XELEMENT, XGEOMETRY), the same resolved field values, and the same repetition (type 0 resolved to the actual repetition and then re-subjected to "Modal field omission"). Coordinates are absolute. Names and strings are remapped to the output tables. Every PROPERTY record is kept, with its names and strings remapped.
5. XNAME records of the source file are not copied. XELEMENT and XGEOMETRY payloads are copied unchanged (see Q-OASIS-40).

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Re-encoded vendor cell keeps circles
- **GIVEN** a read-only OASIS cell whose source contains a CIRCLE of radius 50 with a type 2 repetition
- **WHEN** a hierarchy using it is written to OASIS
- **THEN** the output cell contains a CIRCLE record of radius 50 with the same repetition, not its polygon approximation

### Requirement: Write result and error propagation
The write operation SHALL return failure exactly when gds-write would return failure for the same layout and settings, or when an OASIS-specific I/O or encoding error occurs. Even on failure, the cells already written, the name tables they need and the END record SHALL be written, so the file stays decodable.

Compatibility: behavioral

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaWrite.c:345-388)

#### Scenario: Disallowed abstract
- **GIVEN** `top` uses an abstract cell `macro` and `abstract` is disallow
- **WHEN** OASIS is written
- **THEN** the operation reports failure, neither `macro` nor `top` has a CELL record, and the file ends with a valid 256-byte END record

### Requirement: Complete annotated example
Hefesto SHALL produce exactly the following 419-byte file for the reference case of gds-write "Complete annotated example": output style `scalefactor 10` (expander 1, nanometer units), layer `M1` = `or metal1` with `calma 49 0`, root cell `top` containing only metal1 (0,0)-(4,2) and no labels, `merge no`. The `oasis` settings are the defaults except `compress 0`.

Compatibility: native

```
offset  bytes                                            meaning
0       25 53 45 4D 49 2D 4F 41 53 49 53 0D 0A           magic
13      01 03 31 2E 30 00 E8 07 01                       START "1.0", unit 1000, offsets at END
22      1C 17 00 08 04                                   S_MAX_SIGNED_INTEGER_WIDTH = 4
27      1C 17 01 08 04                                   S_MAX_UNSIGNED_INTEGER_WIDTH = 4
32      1C 17 02 0F 00                                   S_TOP_CELL = PROPSTRING#0
37      0D 00                                            CELL CELLNAME#0
39      14 63 31 00 90 03 C8 01                          RECTANGLE 49/0 w=400 h=200 at (0,0)
47      07 1A "S_MAX_SIGNED_INTEGER_WIDTH"               PROPNAME#0 (PROPNAME table)
75      07 1C "S_MAX_UNSIGNED_INTEGER_WIDTH"             PROPNAME#1
105     07 0A "S_TOP_CELL"                               PROPNAME#2
117     07 0D "S_CELL_OFFSET"                            PROPNAME#3
132     09 03 74 6F 70                                   PROPSTRING#0 "top" (PROPSTRING table)
137     0B 02 4D 31 03 31 03 00                          LAYERNAME geometry "M1" 49/0 (LAYERNAME table)
145     0C 02 4D 31 03 31 03 00                          LAYERNAME text "M1" 49/0
153     03 03 74 6F 70                                   CELLNAME#0 "top" (CELLNAME table)
158     1C 17 03 08 25                                   S_CELL_OFFSET = 37
163     02                                               END
164     01 99 01  01 00  01 2F  01 84 01  01 89 01  01 00
                                                         CELLNAME@153 TEXTSTRING@- PROPNAME@47
                                                         PROPSTRING@132 LAYERNAME@137 XNAME@-
179     E9 01 + 233 x 00                                 padding b-string
414     01                                               validation scheme CRC32
415     D3 3B 7E EE                                      CRC32 0xEE7E3BD3, least significant byte first
```

The END record spans offsets 163..418 (256 bytes).

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Reference file
- **GIVEN** the reference case above
- **WHEN** OASIS is written
- **THEN** the file is 419 bytes long and matches the listing byte for byte
