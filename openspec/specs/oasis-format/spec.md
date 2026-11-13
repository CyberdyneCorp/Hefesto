# OASIS File Format

## Purpose
Defines the OASIS (SEMI P39, version 1.0) byte-level format as Hefesto reads and writes it: the magic bytes, the START and END records, the primitive encodings (unsigned and signed integers, the eight real forms, a-/b-/n-strings, 1-/2-/3-/g-deltas), point lists, repetitions, intervals, the name tables and their reference numbers, every record type, modal variables and their reset rules, CBLOCK compression, strict mode and the validation signature.

Magic has no OASIS support. Every requirement in this capability is `native` and traces to decision P-6. This capability is the shared decoding and encoding layer for capabilities `oasis-read` (interpretation into layout cells) and `oasis-write` (choice of records when serializing). It specifies what a conforming file contains and how each construct decodes. It does not specify which constructs the writer chooses; `oasis-write` owns that.

Where this capability states a detail that could not be confirmed against the SEMI P39 text, the requirement says so and points to an entry in `docs/decisions/open-questions/OASIS.md`. Behaviour was cross-checked against two independent open implementations (KLayout and gdstk) for observable behaviour only.

Terminology:
- **DB unit**: the integer coordinate unit of the file. One DB unit is `1 / unit` micrometres, where `unit` is the real value in the START record.
- **Record**: one record-ID byte (encoded as an unsigned integer that always fits in one byte) followed by record-specific fields.
- **Modal variable**: a state variable that a record may set and later records may reuse by omitting the field.
- **Element**: a PLACEMENT, TEXT, RECTANGLE, POLYGON, PATH, TRAPEZOID, CTRAPEZOID, CIRCLE, XELEMENT or XGEOMETRY record.

## Requirements

### Requirement: File structure and magic bytes
An OASIS file SHALL consist of the 13 magic bytes `25 53 45 4D 49 2D 4F 41 53 49 53 0D 0A` (`%SEMI-OASIS` followed by CR LF), then exactly one START record, then any number of records, then exactly one END record, which ends the file.

Compatibility: native

```ebnf
oasis-file   = magic, start-record, { file-record }, end-record ;
file-record  = pad | cblock | property | name-record | cell ;
name-record  = cellname | textstring | propname | propstring | layername | xname ;
cell         = cell-record, { cell-content } ;
cell-content = pad | cblock | xyabsolute | xyrelative | property | element ;
element      = placement | text | rectangle | polygon | path | trapezoid
             | ctrapezoid | circle | xelement | xgeometry ;
```

Rules:
1. A file whose first 13 bytes differ from the magic bytes SHALL be rejected with "not an OASIS file", and nothing is read.
2. A cell ends at the next record that is not a `cell-content` record: a CELL record, a name record, a PROPERTY record that is not inside a cell, or END. A PROPERTY record following an element or a CELL record belongs to the cell (see "PROPERTY records").
3. A file that ends before the END record is complete SHALL be reported as "unexpected end of file". Everything decoded before that point remains decoded.
4. Bytes after the END record SHALL be reported as an error ("data after END record"). The decoded content is still used.
5. A gzip-wrapped file (first two bytes `1F 8B`) SHALL be transparently decompressed first. All byte offsets in this capability are offsets in the decompressed byte stream.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Magic bytes accepted
- **GIVEN** a file that starts with `25 53 45 4D 49 2D 4F 41 53 49 53 0D 0A 01`
- **WHEN** it is opened as OASIS
- **THEN** the magic is accepted and the next byte `01` is decoded as a START record

#### Scenario: GDS file rejected
- **GIVEN** a GDS file starting with `00 06 00 02 00 03`
- **WHEN** it is opened as OASIS
- **THEN** "not an OASIS file" is reported and no cell is created

### Requirement: Record identifiers
Each record SHALL start with a record-ID byte with the values below. Any other value SHALL be reported as "invalid record type <n>", and decoding stops.

Compatibility: native

| ID | Record | ID | Record |
|---|---|---|---|
| 0 | PAD | 18 | PLACEMENT (with magnification and real angle) |
| 1 | START | 19 | TEXT |
| 2 | END | 20 | RECTANGLE |
| 3 | CELLNAME (implicit refnum) | 21 | POLYGON |
| 4 | CELLNAME (explicit refnum) | 22 | PATH |
| 5 | TEXTSTRING (implicit) | 23 | TRAPEZOID (delta-a and delta-b) |
| 6 | TEXTSTRING (explicit) | 24 | TRAPEZOID (delta-a only) |
| 7 | PROPNAME (implicit) | 25 | TRAPEZOID (delta-b only) |
| 8 | PROPNAME (explicit) | 26 | CTRAPEZOID |
| 9 | PROPSTRING (implicit) | 27 | CIRCLE |
| 10 | PROPSTRING (explicit) | 28 | PROPERTY |
| 11 | LAYERNAME (geometry) | 29 | PROPERTY (repeat last) |
| 12 | LAYERNAME (text) | 30 | XNAME (implicit) |
| 13 | CELL (by refnum) | 31 | XNAME (explicit) |
| 14 | CELL (by name) | 32 | XELEMENT |
| 15 | XYABSOLUTE | 33 | XGEOMETRY |
| 16 | XYRELATIVE | 34 | CBLOCK |
| 17 | PLACEMENT (no magnification, quarter-turn angle) | | |

Records 0, 15 and 16 have no fields. A record ID SHALL be read as an unsigned integer (see "Unsigned and signed integers"). A value greater than 34 is invalid.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Unknown record type
- **GIVEN** a valid START record followed by the byte `23` (35)
- **WHEN** the file is decoded
- **THEN** "invalid record type 35" is reported and decoding stops

### Requirement: Unsigned and signed integers
An unsigned integer SHALL be encoded as a little-endian base-128 sequence: each byte carries 7 value bits in bits 0..6, starting with the least significant group, and bit 7 is set on every byte except the last. A signed integer SHALL be encoded as the unsigned integer `(|v| << 1) | s`, where `s = 1` for negative values.

Compatibility: native

Decoding rules:
1. The reader SHALL accept up to 64 value bits. A value that does not fit in 64 bits SHALL be reported as "integer overflow", and decoding stops.
2. Non-minimal encodings (redundant trailing `0x80`-continued zero groups, for example `80 00` for 0) SHALL be accepted and decode to the same value (Q-OASIS-1, resolution: fix). Acceptance is required in practice: KLayout writes an overlong zero in the END padding length (dbOASISWriter.cc, END record).
3. A signed encoding of negative zero (`01`) SHALL decode to 0.
4. The writer SHALL always use the minimal encoding, except where "END record and padding" requires otherwise.

Where a field is declared 32-bit (layer, datatype, text layer, text type, point-list type, repetition type, interval type, real type, CBLOCK compression type, CTRAPEZOID type), a decoded value above 2^32 − 1 SHALL be reported as "integer overflow" and decoding stops.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Unsigned encodings
- **WHEN** the unsigned values 0, 1, 127, 128, 300, 16383 and 16384 are encoded
- **THEN** the bytes are `00`, `01`, `7F`, `80 01`, `AC 02`, `FF 7F` and `80 80 01`

#### Scenario: Signed encodings
- **WHEN** the signed values 0, 1, −1, 63, −64, 64 and −200 are encoded
- **THEN** the bytes are `00`, `02`, `03`, `7E`, `81 01`, `80 01` and `91 03`

#### Scenario: Non-minimal zero accepted
- **GIVEN** the bytes `80 80 00` where an unsigned integer is expected
- **WHEN** they are decoded
- **THEN** the value is 0 and three bytes are consumed

### Requirement: Real numbers
A real SHALL be encoded as an unsigned type code `t` followed by type-specific data, and SHALL decode to an IEEE-754 binary64 value as follows:

Compatibility: native

| t | Data | Value |
|---|---|---|
| 0 | unsigned `n` | `+n` |
| 1 | unsigned `n` | `−n` |
| 2 | unsigned `d` | `+1/d` |
| 3 | unsigned `d` | `−1/d` |
| 4 | unsigned `n`, unsigned `d` | `+n/d` |
| 5 | unsigned `n`, unsigned `d` | `−n/d` |
| 6 | 4 bytes | IEEE-754 binary32, little-endian, widened to binary64 |
| 7 | 8 bytes | IEEE-754 binary64, little-endian |

Rules:
1. `n` and `d` are converted to binary64 before dividing, with round-to-nearest-even.
2. A zero divisor in types 2–5 SHALL be reported as "division by zero in real", and decoding stops.
3. Any `t > 7` SHALL be reported as "invalid real type", and decoding stops.
4. The writer SHALL choose the form as follows. If `v` is integral and `|v| < 2^53`: type 0 (or 1 if negative). Else, if `1/v` computed in binary64 is integral, `|1/v| < 2^53`, and `1/(1/v) == v` exactly: type 2 (or 3). Otherwise type 7. Types 4, 5 and 6 are never written.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Integer unit
- **WHEN** the real 1000.0 is written
- **THEN** the bytes are `00 E8 07`

#### Scenario: Ratio form decoded
- **GIVEN** the bytes `04 03 02`
- **WHEN** they are decoded as a real
- **THEN** the value is 1.5

#### Scenario: IEEE double form
- **GIVEN** the bytes `07 00 00 00 00 00 00 F8 3F`
- **WHEN** they are decoded as a real
- **THEN** the value is 1.5

#### Scenario: Writer choice for 0.5
- **WHEN** the real 0.5 is written
- **THEN** the bytes are `02 02`

### Requirement: Strings and character sets
A string SHALL be encoded as an unsigned byte count `n` followed by `n` bytes, with no terminator. Three string kinds are distinguished:

Compatibility: native

| Kind | Allowed bytes | Used for |
|---|---|---|
| a-string | 0x20–0x7E (printable ASCII including space) | TEXTSTRING, TEXT strings, version string, a-string property values |
| b-string | any byte 0x00–0xFF | padding, XNAME/XELEMENT/XGEOMETRY data, b-string property values |
| n-string | 0x21–0x7E (printable ASCII without space), length ≥ 1 | CELLNAME, PROPNAME and LAYERNAME names, n-string property values |

Rules:
1. The reader SHALL report a string that contains a byte outside its kind's set, or an empty n-string, as a warning ("illegal character in <kind>") and SHALL use the bytes as they are.
2. The writer SHALL never emit an illegal string. The character mapping that guarantees this is owned by capability `oasis-write`.
3. String length is limited only by available input. A declared length that runs past the end of the input is "unexpected end of file".

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Version string
- **WHEN** the a-string `1.0` is written
- **THEN** the bytes are `03 31 2E 30`

#### Scenario: Space in a cell name
- **GIVEN** a CELLNAME record whose n-string is `my cell`
- **WHEN** it is decoded
- **THEN** a warning "illegal character in n-string" is reported and the name `my cell` is used

### Requirement: Delta encodings
Displacements SHALL be encoded in one of four delta forms. Every decoded component is a signed 64-bit integer, and a component outside the signed 32-bit range SHALL be reported as "coordinate overflow", after which the element is dropped.

Compatibility: native

- **1-delta**: a signed integer giving a displacement along the axis implied by context.
- **2-delta**: an unsigned `u`. Magnitude `m = u >> 2`. Direction `u & 3`: 0 east `(+m, 0)`, 1 north `(0, +m)`, 2 west `(−m, 0)`, 3 south `(0, −m)`.
- **3-delta**: an unsigned `u`. Magnitude `m = u >> 3`. Direction `u & 7`: 0 E `(m,0)`, 1 N `(0,m)`, 2 W `(−m,0)`, 3 S `(0,−m)`, 4 NE `(m,m)`, 5 NW `(−m,m)`, 6 SW `(−m,−m)`, 7 SE `(m,−m)`.
- **g-delta**: an unsigned `u`.
  - If `u & 1 == 0` (form 1, octangular), `m = u >> 4`, and the direction is `(u >> 1) & 7`, with the same table as 3-delta.
  - If `u & 1 == 1` (form 2, arbitrary), `x = ±(u >> 2)`, negative when `u & 2 != 0`, followed by a signed integer `y`.

The writer SHALL encode a g-delta in form 1 when the vector is axis-parallel or diagonal (`x == 0`, `y == 0` or `|x| == |y|`), and in form 2 otherwise. A zero vector SHALL be written in form 1 as `00`.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: 2-delta west
- **GIVEN** the unsigned value 42 (`2A`) as a 2-delta
- **WHEN** it is decoded
- **THEN** the displacement is `(−10, 0)`, because `42 >> 2 = 10` and `42 & 3 = 2`

#### Scenario: g-delta arbitrary form
- **WHEN** the vector `(3, −5)` is written as a g-delta
- **THEN** the bytes are `0D 0B`: `(3 << 2) | 1 = 13`, then the signed −5 is 11

#### Scenario: g-delta octangular form
- **WHEN** the vector `(−7, −7)` is written as a g-delta
- **THEN** the encoding is the single byte `7C`: form 1, direction SW (6), magnitude 7, so `(7 << 4) | (6 << 1) = 124`

### Requirement: Point lists
A point list SHALL be encoded as an unsigned type `t` (0–5), an unsigned count `n ≥ 1`, and `n` deltas. It describes vertices relative to an implicit first vertex `P0 = (0, 0)`. The element's (x, y) position is added to every vertex. A count of 0 or a type above 5 SHALL be reported as "invalid point list", and the element is dropped.

Compatibility: native

| t | Deltas | Vertex rule |
|---|---|---|
| 0 | 1-deltas, alternating horizontal, vertical, horizontal, … (first horizontal) | `P_i = P_{i−1} + delta` along the alternating axis |
| 1 | 1-deltas, alternating vertical first | as type 0 with the first delta vertical |
| 2 | 2-deltas | `P_i = P_{i−1} + delta` |
| 3 | 3-deltas | `P_i = P_{i−1} + delta` |
| 4 | g-deltas | `P_i = P_{i−1} + delta` |
| 5 | g-deltas, second differences | `D_i = D_{i−1} + delta` with `D_0 = (0,0)`, then `P_i = P_{i−1} + D_i` |

Polygon closure (POLYGON records only):
- The polygon is implicitly closed from the last vertex back to `P0`. A point list never repeats `P0` at its end.
- For types 0 and 1, one more vertex is synthesized after `P_n`, so that the two closing edges are axis-parallel and alternate with the last delta. If the last delta was vertical, the extra vertex is `(0, P_n.y)`. If it was horizontal, the extra vertex is `(P_n.x, 0)`. The polygon then has `n + 2` vertices. An odd `n` for a polygon SHALL be reported as a warning ("type 0/1 point list with odd count"), and the synthesized vertex is still added by the same rule.

For PATH records, the point list is the centerline: `P0..P_n`, with no closure and no synthesized vertex.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Rectangle as a type 0 polygon
- **GIVEN** a POLYGON at (10, 20) with point list type 0, count 2, deltas `+40` and `+30`
- **WHEN** it is decoded
- **THEN** the vertices are (10,20), (50,20), (50,50) and the synthesized (10,50)

#### Scenario: L-shape as type 1
- **GIVEN** a POLYGON at (0,0) with type 1, count 4, deltas `+20, +10, −10, +20` (vertical first)
- **WHEN** it is decoded
- **THEN** the vertices are (0,0), (0,20), (10,20), (10,10), (30,10) and the synthesized (30,0)

#### Scenario: Type 5 second differences
- **GIVEN** a PATH point list type 5 with g-deltas `(10,0)`, `(0,10)`, `(−10,−10)`
- **WHEN** it is decoded
- **THEN** the differences are (10,0), (10,10), (0,0), and the centerline is (0,0), (10,0), (20,10), (20,10)

### Requirement: Repetitions
A repetition SHALL be encoded as an unsigned type `r` (0–11) followed by type-specific fields. It decodes to an ordered list of displacements `d_0 = (0,0), d_1, …`. The element is instantiated once at its position plus each displacement, in list order. A type above 11 SHALL be reported as "invalid repetition type", and decoding stops.

Compatibility: native

| r | Fields | Displacements (in this order) |
|---|---|---|
| 0 | — | reuse the modal `repetition` (error if undefined) |
| 1 | `nx' ny' sx sy` (unsigned) | `nx = nx'+2`, `ny = ny'+2`; for `j` in 0..ny−1 (outer), for `i` in 0..nx−1 (inner): `(i·sx, j·sy)` |
| 2 | `nx' sx` | `nx = nx'+2`; `(i·sx, 0)` for i in 0..nx−1 |
| 3 | `ny' sy` | `ny = ny'+2`; `(0, j·sy)` for j in 0..ny−1 |
| 4 | `n'` then `n'+1` unsigned spacings `s_k` | `n = n'+2`; `x_0 = 0`, `x_k = x_{k−1} + s_k`; `(x_k, 0)` |
| 5 | `n'`, unsigned `grid`, then `n'+1` spacings | as type 4 with `s_k · grid` |
| 6 | `n'` then `n'+1` spacings | as type 4 along y: `(0, y_k)` |
| 7 | `n'`, `grid`, then `n'+1` spacings | as type 6 with `s_k · grid` |
| 8 | `n' m'` (unsigned), g-delta `a`, g-delta `b` | `n = n'+2`, `m = m'+2`; for `j` in 0..m−1 (outer), `i` in 0..n−1 (inner): `i·a + j·b` |
| 9 | `n'`, g-delta `a` | `n = n'+2`; `i·a` for i in 0..n−1 |
| 10 | `n'` then `n'+1` g-deltas `g_k` | `n = n'+2`; `p_0 = (0,0)`, `p_k = p_{k−1} + g_k` |
| 11 | `n'`, unsigned `grid`, then `n'+1` g-deltas | as type 10 with `g_k · grid` |

Rules:
1. Dimensions are stored minus 2, so every repetition (other than type 0) yields at least two displacements.
2. Zero spacings and zero displacement vectors are legal. They SHALL produce coincident displacements, which the decoder keeps; capability `oasis-read` decides what they become (Q-OASIS-10, resolution: replicate).
3. The order of the displacement list is significant for `oasis-read` (it determines element order).
4. A modal `repetition` is set by every element record that carries a repetition of type 1–11. Type 0 reuses it.
5. A `grid` of 0 in types 5, 7 and 11 SHALL be reported as "zero repetition grid", and decoding stops.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Regular 2D array
- **GIVEN** repetition bytes `01 01 00 0A 14`
- **WHEN** they are decoded
- **THEN** `nx = 3`, `ny = 2`, `sx = 10`, `sy = 20`
- **AND** the displacements in order are (0,0), (10,0), (20,0), (0,20), (10,20), (20,20)

#### Scenario: Irregular x list
- **GIVEN** repetition bytes `04 01 05 07`
- **WHEN** they are decoded
- **THEN** the displacements are (0,0), (5,0), (12,0)

#### Scenario: Diagonal vector array
- **GIVEN** repetition bytes `09 00 48`, where g-delta `48` is form 1 with magnitude 4 and direction NE (4)
- **WHEN** they are decoded
- **THEN** the displacements are (0,0), (4,4)

### Requirement: Intervals
An interval SHALL be encoded as an unsigned type `k` (0–4) followed by bounds, and SHALL denote a closed range of unsigned 32-bit values:

Compatibility: native

| k | Fields | Range |
|---|---|---|
| 0 | — | `0 .. 2^32−1` |
| 1 | `b` | `0 .. b` |
| 2 | `a` | `a .. 2^32−1` |
| 3 | `a` | `a .. a` |
| 4 | `a`, `b` | `a .. b` |

A type above 4 SHALL be reported as "invalid interval type", and decoding stops. An interval with `a > b` SHALL be treated as empty, with a warning.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Exact layer interval
- **GIVEN** interval bytes `03 31`
- **WHEN** they are decoded
- **THEN** the range is 49..49

### Requirement: START record and table offsets
The START record SHALL have the fields: version (a-string, must be `1.0`), unit (real, DB units per micrometre, must be positive and finite), offset-flag (unsigned), and, only if offset-flag is 0, the table-offsets structure.

Compatibility: native

```ebnf
start-record  = 0x01, a-string(version), real(unit), unsigned(offset-flag),
                [ table-offsets ] ;          (* present iff offset-flag = 0 *)
table-offsets = table-entry(CELLNAME), table-entry(TEXTSTRING),
                table-entry(PROPNAME), table-entry(PROPSTRING),
                table-entry(LAYERNAME), table-entry(XNAME) ;
table-entry   = unsigned(strict-flag), unsigned(byte-offset) ;
```

Rules:
1. A version other than `1.0` SHALL be reported ("unsupported OASIS version <v>"), and the file is not read.
2. A unit that is not positive and finite SHALL be reported ("invalid unit"), and the file is not read.
3. When offset-flag is nonzero, the table-offsets structure is located in the END record instead.
4. A table-entry offset of 0 means "table not present or position not given". A nonzero offset is the byte offset, from the first magic byte, of the first record of that table. This record may be a CBLOCK that contains the table (Q-OASIS-8, resolution: fix). KLayout's writer produces such offsets (dbOASISWriter.cc begin_table/begin_cblock), so a reader must accept them.
5. A strict-flag of 1 declares that the table is *strict* (see "Strict mode").

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: START with offsets at END
- **GIVEN** the bytes `01 03 31 2E 30 00 E8 07 01`
- **WHEN** they are decoded
- **THEN** the version is `1.0`, the unit is 1000 (1 nm DB unit), and the table offsets are in the END record

#### Scenario: Wrong version
- **GIVEN** a START record with version `2.0`
- **WHEN** it is decoded
- **THEN** "unsupported OASIS version 2.0" is reported and nothing is read

### Requirement: END record and padding
The END record SHALL be exactly 256 bytes long, counted from its record-ID byte through the last byte of the validation signature, and SHALL be the last record of the file.

Compatibility: native

```ebnf
end-record = 0x02, [ table-offsets ],      (* present iff START offset-flag != 0 *)
             b-string(padding), unsigned(validation-scheme), [ signature ] ;
signature  = 4 * byte ;                     (* present iff validation-scheme != 0 *)
```

Rules:
1. The padding b-string content is ignored on read. The writer SHALL fill it with `00` bytes.
2. The writer SHALL compute the padding length as follows. Let `T` be the byte length of the table-offsets (0 when they are in START), and let `S` be 4 if the validation scheme is nonzero, else 0. Then `R = 256 − (1 + T + 1 + S)` is the total length of the padding b-string, including its length field. Since `T <= 66`, `R >= 184`. The length field is two bytes and the content length is `L = R − 2`, encoded minimally (`128 <= L <= 252`).
3. The reader SHALL locate END as the last 256 bytes of the (decompressed) file when it needs the table offsets before reading sequentially (see capability `oasis-read`). A file whose END record is not exactly 256 bytes SHALL be reported with a warning ("END record length <n>"). The record is still decoded by its grammar.
4. A validation scheme other than 0, 1 or 2 SHALL be reported as a warning ("unknown validation scheme"), and validation is skipped.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Padding length with table offsets at END
- **GIVEN** table offsets at END occupying 13 bytes and validation scheme 1 (CRC32)
- **WHEN** the END record is written
- **THEN** `R = 256 − (1 + 13 + 1 + 4) = 237`, the padding length field is `EB 01` (235), followed by 235 zero bytes
- **AND** the END record is 256 bytes long

#### Scenario: Padding with offsets in START and no validation
- **GIVEN** table offsets in START and validation scheme 0
- **WHEN** the END record is written
- **THEN** `R = 254`, the length field encodes 252 (`FC 01`), and the record is 1 + 254 + 1 = 256 bytes

### Requirement: Validation signature
The validation scheme SHALL be 0 (none), 1 (CRC32) or 2 (CHECKSUM32). For schemes 1 and 2, the signature SHALL be computed over every byte of the file from the first magic byte up to and including the validation-scheme byte, and stored as 4 bytes, least significant byte first.

Compatibility: native

- **CRC32**: the CRC-32 of ISO 3309 / ITU-T V.42 (reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, final XOR `0xFFFFFFFF`). This is the value computed by zlib's `crc32`.
- **CHECKSUM32**: the sum of all covered bytes as unsigned values, modulo 2^32.

On read, Hefesto SHALL verify the signature when the scheme is 1 or 2. A mismatch SHALL be reported as "OASIS validation failed (expected <hex>, computed <hex>)". The read result is still used, because geometry was decoded independently. A file compressed with gzip is validated over its decompressed bytes.

The coverage and the byte order follow gdstk (library.cpp oas_validate) and were not checked against the SEMI P39 text (Q-OASIS-2, resolution: fix). Golden cases OASIS-012, OASIS-030 and OASIS-061, with files from commercial writers and readers, are release gates for every rule that rests on KLayout and gdstk parity (Q-OASIS-2, resolution: fix; the SEMI P39 text was not bought).

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: CRC32 of the reference file
- **GIVEN** the 419-byte reference file of capability `oasis-write` (requirement "Complete annotated example")
- **WHEN** its signature is computed over bytes 0..414
- **THEN** the CRC32 is `0xEE7E3BD3` and the last four bytes of the file are `D3 3B 7E EE`

#### Scenario: Corrupted file
- **GIVEN** a file with CRC32 validation in which one geometry byte has been changed after writing
- **WHEN** it is read
- **THEN** "OASIS validation failed" is reported and the cells are still created

### Requirement: Name records and reference numbers
The name records CELLNAME, TEXTSTRING, PROPNAME, PROPSTRING, LAYERNAME and XNAME SHALL define the file's name tables as follows:

Compatibility: native

```ebnf
cellname   = (0x03, n-string) | (0x04, n-string, unsigned(refnum)) ;
textstring = (0x05, a-string) | (0x06, a-string, unsigned(refnum)) ;
propname   = (0x07, n-string) | (0x08, n-string, unsigned(refnum)) ;
propstring = (0x09, string)   | (0x0A, string, unsigned(refnum)) ;  (* a-, b- or n-string *)
layername  = (0x0B | 0x0C), n-string, interval(layer), interval(datatype) ;
xname      = (0x1E, unsigned(attribute), b-string)
           | (0x1F, unsigned(attribute), b-string, unsigned(refnum)) ;
```

Rules:
1. **Implicit refnums**: the k-th implicit record of a table (counting from 0, over the whole file) gets refnum k.
2. **Mixing**: one table SHALL NOT mix implicit and explicit records. A mix SHALL be reported ("implicit and explicit <TABLE> records mixed"), and decoding stops.
3. **Uniqueness**: a refnum defined twice in the same table SHALL be reported ("duplicate <TABLE> refnum <n>"), and decoding stops. The same CELLNAME string defined twice SHALL be reported ("duplicate cell name <s>"), and the later refnum is an alias of the same name.
4. **Forward references**: a refnum may be used before its name record appears. All references SHALL be resolved after the whole file has been scanned (see capability `oasis-read`). A refnum that is still undefined then SHALL be reported ("undefined <TABLE> refnum <n>").
5. **Layer names**: LAYERNAME 0x0B names geometry layers and 0x0C names text layers. Each associates the name with every (layer, datatype) pair in the product of the two intervals. Several names may overlap. LAYERNAME carries no refnum.
6. **XNAME**: an XNAME defines an extension name with a vendor attribute. Hefesto keeps it only for re-encoding (see capability `oasis-write`).
7. A PROPERTY record directly after a name record is attached to that name record (see "PROPERTY records").

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Implicit cell names
- **GIVEN** records `03 01 41`, `03 01 42`
- **WHEN** they are decoded
- **THEN** refnum 0 names cell `A` and refnum 1 names cell `B`

#### Scenario: Mixed modes rejected
- **GIVEN** records `03 01 41` then `04 01 42 05`
- **WHEN** they are decoded
- **THEN** "implicit and explicit CELLNAME records mixed" is reported and decoding stops

#### Scenario: Forward reference to a cell name
- **GIVEN** a CELL record `0D 00` that appears before the record `03 03 74 6F 70`
- **WHEN** the file is read
- **THEN** the cell defined by the CELL record is named `top`

### Requirement: CELL record and cell content
A CELL record SHALL begin a cell definition. It identifies the cell by refnum (record 13: unsigned refnum) or by name (record 14: n-string). The cell's content is every following `cell-content` record up to the end of the cell (see "File structure and magic bytes").

Compatibility: native

Rules:
1. A cell SHALL be defined at most once per file. A second CELL record for the same name (directly or through a refnum) SHALL be reported ("cell <name> defined twice"). `oasis-read` specifies how it is handled.
2. A CELL record SHALL reset all modal variables (see "Modal variables and reset rules").
3. A cell may be empty.
4. The order of cells in the file is unconstrained. A cell may be referenced before or after its definition.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Empty cell
- **GIVEN** a CELL record immediately followed by END
- **WHEN** it is decoded
- **THEN** a defined, empty cell results

### Requirement: Modal variables and reset rules
The decoder SHALL maintain the modal variables below. Each one is *undefined* or holds a value. An element that omits a field SHALL take the modal value. If that value is undefined, the decoder SHALL report "modal variable <name> undefined", and decoding stops.

Compatibility: native

| Modal variable | Set by | Initial value after reset |
|---|---|---|
| `repetition` | any element with a repetition of type 1–11 | undefined |
| `placement-x`, `placement-y` | PLACEMENT | 0 |
| `placement-cell` | PLACEMENT with C=1 | undefined |
| `layer`, `datatype` | RECTANGLE, POLYGON, PATH, TRAPEZOID, CTRAPEZOID, CIRCLE, XGEOMETRY | undefined |
| `textlayer`, `texttype` | TEXT | undefined |
| `text-x`, `text-y` | TEXT | 0 |
| `text-string` | TEXT with C=1 | undefined |
| `geometry-x`, `geometry-y` | all geometry elements and XGEOMETRY | 0 |
| `xy-mode` | XYABSOLUTE, XYRELATIVE | absolute |
| `geometry-w`, `geometry-h` | RECTANGLE, TRAPEZOID, CTRAPEZOID | undefined |
| `polygon-point-list` | POLYGON | undefined |
| `path-halfwidth`, `path-point-list` | PATH | undefined |
| `path-start-extension`, `path-end-extension` | PATH | undefined |
| `ctrapezoid-type` | CTRAPEZOID | undefined |
| `circle-radius` | CIRCLE | undefined |
| `last-property-name`, `last-value-list`, `last-property-standard` | PROPERTY | undefined |

Reset events:
1. At START, and at every CELL record, all modal variables SHALL be reset to their initial values.
2. At every name record (CELLNAME, TEXTSTRING, PROPNAME, PROPSTRING, LAYERNAME, XNAME), all modal variables SHALL also be reset. This matches the KLayout reader, which resets after each name record (dbOASISReader.cc do_read) (Q-OASIS-5, resolution: fix). Whether SEMI P39 requires it is not confirmed; a file that relied on modal state surviving a name record would also fail in KLayout. The writer of capability `oasis-write` never depends on modal state across a name record.

Coordinate modes: in absolute mode a coordinate field `v` sets the modal variable to `v`. In relative mode it sets it to `modal + v`. The xy-mode applies to the x and y fields of PLACEMENT, TEXT and all geometry elements, but not to point-list deltas, repetition displacements or widths.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Layer inherited by the next element
- **GIVEN** a RECTANGLE that sets layer 49 and datatype 0, then a POLYGON with L=0 and D=0
- **WHEN** they are decoded
- **THEN** the polygon is on layer 49, datatype 0

#### Scenario: Relative coordinates
- **GIVEN** an XYRELATIVE record, then RECTANGLEs with x fields 100, 100, 100 and no earlier geometry in the cell
- **WHEN** they are decoded
- **THEN** their x positions are 100, 200 and 300

#### Scenario: Undefined modal layer
- **GIVEN** the first geometry element in a cell is a RECTANGLE with L=0
- **WHEN** it is decoded
- **THEN** "modal variable layer undefined" is reported and decoding stops

### Requirement: PLACEMENT records
A PLACEMENT SHALL place a cell with a transform and optional repetition. Its info byte is `CNXYRAAF` for record 17 and `CNXYRMAF` for record 18 (bit 7 first):

Compatibility: native

```ebnf
placement17 = 0x11, info, [ cellref ], [ signed(x) ], [ signed(y) ], [ repetition ] ;
placement18 = 0x12, info, [ cellref ], [ real(mag) ], [ real(angle) ],
              [ signed(x) ], [ signed(y) ], [ repetition ] ;
cellref     = unsigned(refnum) | n-string ;      (* refnum iff N = 1 *)
```

Bits:
- C (0x80): a cell reference follows and sets `placement-cell`. N (0x40) selects refnum (1) or name (0).
- X (0x20), Y (0x10): coordinate present.
- R (0x08): repetition present.
- Record 17: AA = bits 0x06, giving a counterclockwise rotation of `AA × 90` degrees.
- Record 18: M (0x04) means a magnification real follows (default 1.0). A (0x02) means an angle real in degrees follows (default 0).
- F (0x01): flip.

Transform: a point `p` of the placed cell maps to `translate(x, y) ∘ magnify(mag) ∘ rotate(angle) ∘ flip(p)`, where `flip(px, py) = (px, −py)`. Flipping happens first and translation last. Rotation is counterclockwise. This is the same convention as GDS STRANS reflection followed by ANGLE.

A magnification of 0 or less, or a non-finite value, SHALL be reported ("invalid magnification"), and the element is dropped. Angles may be any real. `oasis-read` specifies how non-quarter-turn angles and magnifications are mapped.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Rotated, flipped placement
- **GIVEN** the record `11 C7 03` as the first placement in a cell; info `0xC7` has C=1, N=1, X=0, Y=0, R=0, AA=3 and F=1
- **WHEN** it is decoded
- **THEN** the cell is CELLNAME refnum 3, the rotation is 270 degrees, flip is set, and the position is the modal (0, 0)
- **AND** a child point (1, 2) maps to: flip (1, −2), rotate 270 counterclockwise (−2, −1)

#### Scenario: Magnified placement
- **GIVEN** the record `12 C4 02 00 02` (C=1, N=1, M=1, no angle, no position)
- **WHEN** it is decoded
- **THEN** the cell refnum is 2 and the magnification is 2.0 (real type 0, value 2)

### Requirement: TEXT record
A TEXT record SHALL define a text string at a point on a (textlayer, texttype) pair. Its info byte is `0CNXYRTL`:

Compatibility: native

```ebnf
text = 0x13, info, [ text-ref ], [ unsigned(textlayer) ], [ unsigned(texttype) ],
       [ signed(x) ], [ signed(y) ], [ repetition ] ;
text-ref = unsigned(refnum) | a-string ;          (* refnum iff N = 1 *)
```

Bits: C (0x40) string present, N (0x20) by refnum, X (0x10), Y (0x08), R (0x04), T (0x02) texttype present, L (0x01) textlayer present. Bit 7 SHALL be 0. A set bit 7 is reported as a warning and ignored.

TEXT has no font, size, orientation or anchor. It uses the text modal variables (`text-x`, `text-y`, `textlayer`, `texttype`, `text-string`), not the geometry ones.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Text by refnum
- **GIVEN** record `13 7B 00 31 00 E8 07 D0 0F` (C, N, X, Y, T, L set; R clear)
- **WHEN** it is decoded
- **THEN** the string is TEXTSTRING refnum 0, the text layer is 49, the text type is 0, and the position is (500, 1000)

### Requirement: RECTANGLE record
A RECTANGLE SHALL define an axis-parallel rectangle `[x, x+w] × [y, y+h]`. Its info byte is `SWHXYRDL`:

Compatibility: native

```ebnf
rectangle = 0x14, info, [ unsigned(layer) ], [ unsigned(datatype) ],
            [ unsigned(width) ], [ unsigned(height) ],
            [ signed(x) ], [ signed(y) ], [ repetition ] ;
```

Bits: S (0x80) square, W (0x40), H (0x20), X (0x10), Y (0x08), R (0x04), D (0x02), L (0x01).

With S=1, the height equals the width (taken from the W field or the modal `geometry-w`), and H SHALL be 0. A set H with S=1 SHALL be reported, and the height field is read and ignored. After an S=1 rectangle, the modal `geometry-h` is set to the width, as KLayout reads it (dbOASISReader.cc do_read_rectangle) and as its writer assumes (dbOASISWriter.cc, square rectangles) (Q-OASIS-3, resolution: fix). KLayout does not read the H field when S=1 and the H bit is set, so reporting it and reading the field is Hefesto's own behaviour; a cross-check golden for that case does not compare against KLayout. Zero width or height is legal and yields an empty rectangle.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Plain rectangle
- **GIVEN** record `14 63 31 00 90 03 C8 01` at the start of a cell
- **WHEN** it is decoded
- **THEN** the rectangle is layer 49, datatype 0, `[0, 400] × [0, 200]`

#### Scenario: Square rectangle
- **GIVEN** record `14 DB 05 00 0A 28 50` (S, W, X, Y, D, L set)
- **WHEN** it is decoded
- **THEN** the rectangle is layer 5, datatype 0, `[20, 30] × [40, 50]`, and `geometry-h` becomes 10

### Requirement: POLYGON record
A POLYGON SHALL define a simple or self-intersecting closed polygon whose first vertex is (x, y) and whose other vertices come from the point list. Its info byte is `00PXYRDL`:

Compatibility: native

```ebnf
polygon = 0x15, info, [ unsigned(layer) ], [ unsigned(datatype) ],
          [ point-list ], [ signed(x) ], [ signed(y) ], [ repetition ] ;
```

Bits: P (0x20) point list present, X (0x10), Y (0x08), R (0x04), D (0x02), L (0x01). Closure follows "Point lists". A polygon with fewer than 3 vertices after closure SHALL be reported ("polygon with fewer than 3 vertices"), and the element is dropped.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Triangle with a 3-delta list
- **GIVEN** a POLYGON at (0,0) with point list type 3, count 2, 3-deltas E 10 (`50`) and NW 10 (`55`)
- **WHEN** it is decoded
- **THEN** the vertices are (0,0), (10,0) and (0,10)

### Requirement: PATH record
A PATH SHALL define a centerline with a half-width and start and end extensions. Its info byte is `EWPXYRDL`:

Compatibility: native

```ebnf
path = 0x16, info, [ unsigned(layer) ], [ unsigned(datatype) ],
       [ unsigned(half-width) ], [ extension-scheme ], [ point-list ],
       [ signed(x) ], [ signed(y) ], [ repetition ] ;
extension-scheme = unsigned(0000SSEE), [ signed(start-ext) ], [ signed(end-ext) ] ;
```

Bits: E (0x80) extension scheme present, W (0x40) half-width present, P (0x20), X (0x10), Y (0x08), R (0x04), D (0x02), L (0x01).

Extension scheme: SS (bits 2–3) is for the start, EE (bits 0–1) for the end. 0 reuses the modal value, 1 is flush (extension 0), 2 is half-width (extension = half-width), and 3 means an explicit signed value follows. The start value, if any, precedes the end value.

Modal updates: the scheme values 1, 2 and 3 SHALL set the corresponding extension modal variable to the resulting numeric value (0, the half-width in effect, or the explicit value). KLayout does the same (dbOASISReader.cc do_read_path) (Q-OASIS-11, resolution: fix). P39 conformance of this rule is not confirmed.

The total width is `2 × half-width`, so every path width is even in DB units. The centerline is the point list placed at (x, y) (see "Point lists"). A centerline with fewer than 2 vertices SHALL be reported, and the element dropped.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Path with half-width extensions
- **GIVEN** a PATH with half-width 5, extension scheme `0A`, point list type 0 count 1 delta +100, at (0,0)
- **WHEN** it is decoded
- **THEN** the centerline is (0,0) → (100,0), the width is 10, and both extensions are 5

#### Scenario: Explicit negative extension
- **GIVEN** extension scheme `0F` with start −2 and end 7
- **WHEN** it is decoded
- **THEN** the start extension is −2 and the end extension is 7

### Requirement: TRAPEZOID records
A TRAPEZOID SHALL define a four-vertex polygon inside the box `[x, x+w] × [y, y+h]`, using two signed offsets `a` (delta-a) and `b` (delta-b). Its info byte is `OWHXYRDL`:

Compatibility: native

```ebnf
trapezoid = (0x17, info, [layer], [datatype], [w], [h], signed(a), signed(b), [x], [y], [rep])
          | (0x18, info, [layer], [datatype], [w], [h], signed(a), [x], [y], [rep])   (* b = 0 *)
          | (0x19, info, [layer], [datatype], [w], [h], signed(b), [x], [y], [rep]) ;  (* a = 0 *)
```

Bit O (0x80) selects vertical orientation. The other bits are as for RECTANGLE. The vertices relative to (x, y), in order:
- horizontal (O=0): `(max(a,0), h)`, `(w + min(b,0), h)`, `(w − max(b,0), 0)`, `(−min(a,0), 0)`
- vertical (O=1): `(0, max(a,0))`, `(0, h + min(b,0))`, `(w, h − max(b,0))`, `(w, −min(a,0))`

Consecutive coincident vertices SHALL be merged, so a trapezoid may degenerate into a triangle. If fewer than 3 distinct vertices remain, the element is dropped with a warning. The vertex formulas match the KLayout implementation (Q-OASIS-4, resolution: fix).

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Horizontal trapezoid
- **GIVEN** a horizontal TRAPEZOID (record 23) at (0,0) with w = 100, h = 20, a = 20, b = −20
- **WHEN** it is decoded
- **THEN** the vertices are (20,20), (80,20), (100,0), (0,0)

### Requirement: CTRAPEZOID record
A CTRAPEZOID SHALL define a constrained trapezoid or triangle selected by a type 0–25 inside a box at (x, y). Its info byte is `TWHXYRDL`:

Compatibility: native

```ebnf
ctrapezoid = 0x1A, info, [ unsigned(layer) ], [ unsigned(datatype) ],
             [ unsigned(ctrapezoid-type) ], [ unsigned(w) ], [ unsigned(h) ],
             [ signed(x) ], [ signed(y) ], [ repetition ] ;
```

Bit T (0x80) means a type is present. The other bits are as for RECTANGLE. The vertices, relative to (x, y), for each type are:

| Type | Vertices |
|---|---|
| 0 | (0,0) (0,h) (w−h,h) (w,0) |
| 1 | (0,0) (0,h) (w,h) (w−h,0) |
| 2 | (0,0) (h,h) (w,h) (w,0) |
| 3 | (h,0) (0,h) (w,h) (w,0) |
| 4 | (0,0) (h,h) (w−h,h) (w,0) |
| 5 | (h,0) (0,h) (w,h) (w−h,0) |
| 6 | (0,0) (h,h) (w,h) (w−h,0) |
| 7 | (h,0) (0,h) (w−h,h) (w,0) |
| 8 | (0,0) (0,h) (w,h−w) (w,0) |
| 9 | (0,0) (0,h−w) (w,h) (w,0) |
| 10 | (0,0) (0,h) (w,h) (w,w) |
| 11 | (0,w) (0,h) (w,h) (w,0) |
| 12 | (0,0) (0,h) (w,h−w) (w,w) |
| 13 | (0,w) (0,h−w) (w,h) (w,0) |
| 14 | (0,0) (0,h−w) (w,h) (w,w) |
| 15 | (0,w) (0,h) (w,h−w) (w,0) |
| 16 | (0,0) (0,w) (w,0) |
| 17 | (0,0) (0,w) (w,w) |
| 18 | (0,0) (w,w) (w,0) |
| 19 | (0,w) (w,w) (w,0) |
| 20 | (0,0) (h,h) (2h,0) |
| 21 | (0,h) (2h,h) (h,0) |
| 22 | (0,0) (0,2w) (w,w) |
| 23 | (w,0) (0,w) (w,2w) |
| 24 | (0,0) (0,h) (w,h) (w,0) |
| 25 | (0,0) (0,w) (w,w) (w,0) |

Rules:
1. Types 16–19, 22, 23 and 25 use only `w`. Types 20 and 21 use only `h`. A field that the type does not use may be absent, and its modal value is then not required.
2. After decoding, the modal `geometry-w` and `geometry-h` SHALL be set to the width and height of the resulting vertex set's bounding box. KLayout and gdstk do the same (Q-OASIS-4, resolution: fix).
3. A type above 25 SHALL be reported ("invalid CTRAPEZOID type"), and the element is dropped.
4. Degenerate vertex sets follow the TRAPEZOID rule.

This table matches KLayout (dbOASISReader.cc ctraps_table) and gdstk (library.cpp CTRAPEZOID) for all 26 types (Q-OASIS-4, resolution: fix). Golden case OASIS-030 (all 26 types, from a commercial writer) is the release check.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Type 0 trapezoid
- **GIVEN** a CTRAPEZOID type 0 with w = 30, h = 10 at (0,0)
- **WHEN** it is decoded
- **THEN** the vertices are (0,0), (0,10), (20,10), (30,0)

#### Scenario: Type 25 square
- **GIVEN** a CTRAPEZOID type 25 with w = 8 and no h field, and `geometry-h` undefined
- **WHEN** it is decoded
- **THEN** the vertices are (0,0), (0,8), (8,8), (8,0), and `geometry-h` becomes 8

### Requirement: CIRCLE record
A CIRCLE SHALL define a disk of radius `r` centred at (x, y). Its info byte is `00rXYRDL`:

Compatibility: native

```ebnf
circle = 0x1B, info, [ unsigned(layer) ], [ unsigned(datatype) ], [ unsigned(radius) ],
         [ signed(x) ], [ signed(y) ], [ repetition ] ;
```

Bit r (0x20) means a radius is present. A radius of 0 SHALL be decoded and then ignored by `oasis-read` (no geometry). `oasis-read` owns the polygonal approximation.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Circle fields
- **GIVEN** record `1B 3B 02 00 0A 64 64` (r, X, Y, D, L)
- **WHEN** it is decoded
- **THEN** the circle is layer 2, datatype 0, radius 10, centre (50, 50)

### Requirement: PROPERTY records
A PROPERTY record SHALL attach a named list of values to the preceding object. Record 28 has an info byte `UUUUVCNS`. Record 29 repeats the last property (modal name, standard flag and value list) with no fields.

Compatibility: native

```ebnf
property  = (0x1C, info, [ prop-name ], [ value-count ], { prop-value })
          | 0x1D ;
prop-name = unsigned(refnum) | n-string ;   (* present iff C = 1; refnum iff N = 1 *)
value-count = unsigned ;                    (* present iff V = 0 and UUUU = 15 *)
prop-value  = real                          (* types 0..7 *)
            | 0x08, unsigned | 0x09, signed
            | (0x0A | 0x0B | 0x0C), string  (* a-, b-, n-string *)
            | (0x0D | 0x0E | 0x0F), unsigned(propstring-refnum) ;
```

Bits: UUUU (bits 4–7) is the value count (0–14), or 15 when an explicit count follows. V (0x08) reuses the modal value list, in which case UUUU must be 0 and no values follow. C (0x04) means a name is present (else the modal name is used). N (0x02) means the name is a PROPNAME refnum. S (0x01) marks a standard property.

Attachment: a PROPERTY applies to the closest preceding object in this list:
- the file, if no record other than START, PAD or PROPERTY precedes it;
- the immediately preceding name record;
- the CELL record, if no element has followed it yet;
- the immediately preceding element.

PAD and CBLOCK are transparent for attachment. XYABSOLUTE and XYRELATIVE end element attachment: a PROPERTY that follows one of them (with only PAD, CBLOCK or PROPERTY records in between) attaches to the current CELL, as KLayout reads it (Q-OASIS-6, resolution: fix). This rests on KLayout parity, not on the SEMI P39 clause. The writer never emits it. Several PROPERTY records may follow one object.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Standard file property
- **GIVEN** the bytes `1C 17 00 08 04` directly after START
- **WHEN** they are decoded
- **THEN** a standard file property whose name is PROPNAME refnum 0 has one unsigned value 4

#### Scenario: Repeat property
- **GIVEN** a PROPERTY on one RECTANGLE, then a second RECTANGLE followed by record `1D`
- **WHEN** they are decoded
- **THEN** the second rectangle carries the same property name and values

#### Scenario: PROPERTY after XYRELATIVE belongs to the cell
- **GIVEN** a cell containing a PLACEMENT, then an XYRELATIVE record, then a PROPERTY
- **WHEN** they are decoded
- **THEN** the property is attached to the cell, not to the placement

### Requirement: Standard properties
Hefesto SHALL recognise the following standard properties (S = 1) by name. All other standard-property names SHALL be kept as ordinary properties with a warning ("unknown standard property").

Compatibility: native

| Name | Attached to | Values |
|---|---|---|
| `S_MAX_SIGNED_INTEGER_WIDTH` | file | unsigned: byte width of the largest signed integer used |
| `S_MAX_UNSIGNED_INTEGER_WIDTH` | file | unsigned: byte width of the largest unsigned integer used |
| `S_TOP_CELL` | file | n-string (or n-string propstring refnum): a top cell name; may occur several times |
| `S_BOUNDING_BOXES_AVAILABLE` | file | unsigned 0 (none), 1 (some), 2 (all cells) |
| `S_BOUNDING_BOX` | CELLNAME | unsigned flags, signed x, signed y, unsigned w, unsigned h |
| `S_CELL_OFFSET` | CELLNAME | unsigned: byte offset of the cell's CELL record, 0 if not given |
| `S_GDS_PROPERTY` | element | unsigned GDS attribute number, then a b-string (or b-string propstring refnum) value |

Rules:
1. `S_CELL_OFFSET` SHALL point to a CELL record that is not inside a CBLOCK. A reader that uses it SHALL check that a CELL record for the same cell is there, and SHALL otherwise ignore the property, with a warning.
2. The integer-width properties are informative. Hefesto SHALL NOT reject a file because of them.
3. `S_BOUNDING_BOX` is informative. Hefesto computes bounding boxes from geometry.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: GDS property on a placement
- **GIVEN** a PLACEMENT followed by a standard PROPERTY named `S_GDS_PROPERTY` with values (unsigned 61, b-string `u1`)
- **WHEN** it is decoded
- **THEN** the placement carries GDS attribute 61 with value `u1`

### Requirement: Extension records
XNAME, XELEMENT and XGEOMETRY SHALL be decoded according to the grammar below. Hefesto has no semantics for their payloads.

Compatibility: native

```ebnf
xelement  = 0x20, unsigned(attribute), b-string ;
xgeometry = 0x21, info(000XYRDL), unsigned(attribute), [ unsigned(layer) ],
            [ unsigned(datatype) ], b-string, [ signed(x) ], [ signed(y) ], [ repetition ] ;
```

XGEOMETRY updates the modal `layer`, `datatype`, `geometry-x`, `geometry-y` and `repetition` exactly like other geometry elements.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: XGEOMETRY updates modal layer
- **GIVEN** an XGEOMETRY with L=1 (layer 7) followed by a RECTANGLE with L=0
- **WHEN** they are decoded
- **THEN** the rectangle is on layer 7

### Requirement: CBLOCK compression
A CBLOCK SHALL contain a compressed run of records:

Compatibility: native

```ebnf
cblock = 0x22, unsigned(comp-type), unsigned(uncomp-byte-count),
         unsigned(comp-byte-count), comp-byte-count * byte ;
```

Rules:
1. comp-type 0 is raw DEFLATE (RFC 1951) with no zlib or gzip header. Any other type SHALL be reported ("unsupported CBLOCK compression type"), and decoding stops.
2. The decompressed bytes SHALL replace the CBLOCK in the record stream. The decoder continues with the decompressed bytes and then with the bytes after the CBLOCK.
3. The decompressed length SHALL equal uncomp-byte-count. The DEFLATE stream SHALL end exactly at comp-byte-count bytes. A mismatch SHALL be reported ("CBLOCK size mismatch"), and decoding stops.
4. A CBLOCK SHALL NOT contain a START, END or CBLOCK record. If one is found, it is reported and decoding stops.
5. The writer SHALL put only whole records in a CBLOCK. The reader SHALL accept a record that spans the end of a CBLOCK at any byte, including inside a string, by continuing into the following bytes (Q-OASIS-9, resolution: fix). KLayout accepts a span only outside strings. P39 may forbid this, but accepting it never changes how a conforming file decodes.
6. CBLOCKs may appear anywhere a file record or cell-content record may appear, including inside a cell and around name tables.
7. Byte offsets used by S_CELL_OFFSET and table-offsets are offsets in the file, not in decompressed data.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Cell body in a CBLOCK
- **GIVEN** a CELL record followed by a CBLOCK whose decompressed bytes are two RECTANGLE records
- **WHEN** the file is decoded
- **THEN** the cell contains the two rectangles

#### Scenario: Unsupported compression
- **GIVEN** a CBLOCK with comp-type 1
- **WHEN** it is decoded
- **THEN** "unsupported CBLOCK compression type 1" is reported and decoding stops

### Requirement: Strict mode
A name table whose table-entry strict-flag is 1 SHALL be treated as *strict*. A file in which all six table entries are strict is a *strict-mode file*. For a strict table:

Compatibility: native

1. All records of that table SHALL be contiguous, starting at the table offset. Only PAD, CBLOCK and the PROPERTY records attached to those name records may be interleaved.
2. No record of that table type SHALL appear anywhere else in the file.
3. References to that table's names SHALL be by refnum only. A strict CELLNAME table forbids CELL-by-name and PLACEMENT-by-name. A strict TEXTSTRING table forbids inline TEXT strings. A strict PROPNAME table forbids inline property names. A strict PROPSTRING table forbids inline string property values (types 10–12).
4. A strict table with offset 0 means the file has no records of that type.

On read, a violation SHALL be reported as a warning ("strict mode violation: <detail>") and SHALL NOT change the decoded content. The set of violations matches the warnings of the KLayout reader (Q-OASIS-7, resolution: fix). Rule 3 for PROPSTRING and the exact scope of "contiguous" are not confirmed against SEMI P39.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Inline text in strict file
- **GIVEN** a file whose TEXTSTRING table entry has strict-flag 1, and a TEXT record with C=1 and N=0 (inline string)
- **WHEN** it is read
- **THEN** a strict-mode violation warning is reported and the text is still created

#### Scenario: Strict file written by Hefesto reads back clean
- **GIVEN** a file written by `oasis write` with `strict yes`
- **WHEN** it is read back
- **THEN** no strict-mode violation warning is reported

### Requirement: PAD record
A PAD record (ID 0) SHALL have no fields and no effect, except that it is transparent for PROPERTY attachment. It may appear wherever a file record or cell-content record may appear.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: PAD between elements
- **GIVEN** a RECTANGLE, a PAD, then a PROPERTY record
- **WHEN** they are decoded
- **THEN** the property is attached to the rectangle

### Requirement: Complete annotated example
A decoder conforming to this capability SHALL decode the following 419-byte file into one cell `top` containing one rectangle on layer 49, datatype 0, `[0,400] × [0,200]`, with unit 1000 (1 nm), the file properties `S_MAX_SIGNED_INTEGER_WIDTH = 4`, `S_MAX_UNSIGNED_INTEGER_WIDTH = 4` and `S_TOP_CELL = top`, the layer name `M1` for 49/0 (geometry and text), and a valid CRC32. The name tables follow the cell and are reached through forward refnums. The file is the output of capability `oasis-write` for its reference case.

Compatibility: native

```
offset  bytes                                            meaning
0       25 53 45 4D 49 2D 4F 41 53 49 53 0D 0A           magic "%SEMI-OASIS\r\n"
13      01 03 31 2E 30 00 E8 07 01                       START "1.0", unit 1000 (real type 0), offset-flag 1
22      1C 17 00 08 04                                   PROPERTY: UUUU=1 C N S; PROPNAME#0; unsigned 4
27      1C 17 01 08 04                                   PROPERTY: PROPNAME#1; unsigned 4
32      1C 17 02 0F 00                                   PROPERTY: PROPNAME#2; n-string PROPSTRING#0
37      0D 00                                            CELL by refnum 0 (modal state reset)
39      14 63 31 00 90 03 C8 01                          RECTANGLE info W H D L; layer 49; datatype 0;
                                                         w 400; h 200; x, y = modal 0
47      07 1A "S_MAX_SIGNED_INTEGER_WIDTH"               PROPNAME#0 (implicit)
75      07 1C "S_MAX_UNSIGNED_INTEGER_WIDTH"             PROPNAME#1
105     07 0A "S_TOP_CELL"                               PROPNAME#2
117     07 0D "S_CELL_OFFSET"                            PROPNAME#3
132     09 03 74 6F 70                                   PROPSTRING#0 "top"
137     0B 02 4D 31 03 31 03 00                          LAYERNAME geometry "M1", layer =49, datatype =0
145     0C 02 4D 31 03 31 03 00                          LAYERNAME text "M1", layer =49, datatype =0
153     03 03 74 6F 70                                   CELLNAME#0 "top"
158     1C 17 03 08 25                                   PROPERTY on CELLNAME: S_CELL_OFFSET = 37
163     02                                               END
164     01 99 01  01 00  01 2F  01 84 01  01 89 01  01 00
                                                         strict table offsets: CELLNAME 153, TEXTSTRING 0,
                                                         PROPNAME 47, PROPSTRING 132, LAYERNAME 137, XNAME 0
179     E9 01 + 233 x 00                                 padding b-string (length 233)
414     01                                               validation scheme 1 (CRC32)
415     D3 3B 7E EE                                      CRC32 0xEE7E3BD3, least significant byte first
```

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Reference file decodes
- **GIVEN** the 419-byte file above
- **WHEN** it is decoded
- **THEN** the result is one cell `top` with one rectangle 49/0 `[0,400] × [0,200]`, the END record spans bytes 163..418 (256 bytes), and validation succeeds
