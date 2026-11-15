# gds-write

## Purpose

Defines how Hefesto serializes a layout hierarchy to a GDSII stream file so that the output is byte-identical to Magic VLSI 8.3.694 (commit `fc5daa9e`) for the same technology file, layout, and options, except for the documented time fields. It covers the `gds write` operation and every option that affects writing, the record-level byte encoding (record headers, integers, 8-byte reals, strings, dates), library and structure framing, structure ordering and naming, cell references (SREF/AREF, transforms, properties 61/99), boundary output for unmerged tiles and the merged-polygon algorithm with its 200-point limit, label TEXT records, verbatim copying of read-only (vendor) cells, addendum/abstract/undefined handling, and compressed output.

This capability does not define how the per-layer mask geometry is computed from the layout. Generation of mask geometry belongs to capabilities `mask-output-operators` and `mask-hierarchical-generation`. The `cifoutput` grammar (layers, `calma` numbers, `labels`, `units`, `options`) belongs to `mask-output-styles`. Reading GDS, including how `GDS_FILE`/`GDS_START`/`GDS_END` and content-hash timestamps are produced, belongs to `gds-read`. Keyword prefix matching of command options follows capability `ui-command-dispatch`.

Terminology used throughout:

- **Output style**: the current `cifoutput` style (see `mask-output-styles`). Its *scalefactor* `S` (output units per Magic internal unit), *expander* `E` (`S/E` is the number of centimicrons per internal unit), and *angstrom flag* (`units angstroms`) are used here.
- **Mask layer**: a non-temporary layer of the output style, in style order. Each has a GDS layer number `L` and datatype `D` (`-1` when no `calma`/`gds` statement was given).
- **Mask plane**: the generated geometry of one mask layer for one cell, expressed in output units (internal units × `S`) and stored as a corner-stitched tile decomposition (see Requirement "Tile decomposition and enumeration order").
- **Cell id number**: a positive integer assigned to each cell during a write (see Requirement "Structure ordering").

## Requirements

### Requirement: Write command and output file naming

Hefesto SHALL provide a "write GDS" operation that writes the hierarchy rooted at the root cell of the current layout view to a single file. The file name SHALL be resolved as follows:

1. With no file argument (Magic: bare `gds` or `calma`), the name is the root cell name after its last `/`.
2. With `write <file>`, the name is `<file>`.
3. If the name has no extension, one is appended: `.gds` when the compression level is 0, `.gds.gz` when it is 1–9. The test depends on the `magic-compat` setting (Q-MASK-210, resolution: compat): with it ON (the default) the name has no extension when it contains no `.` anywhere, including directory components, as in Magic; with it OFF only the final path component is tested.
4. The path is resolved relative to the current working directory, with no search path.

`write` with a number of arguments other than exactly one file name is an argument error and writes nothing. `calma` is an exact alias of `gds`. If the file cannot be created, nothing is written and an error is reported. If writing returns failure (see Requirement "Write result and error propagation"), Hefesto reports that the file may be incompletely written; the file is still closed and left on disk.

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:131, commands/CmdCD.c:213-228, commands/CmdCD.c:1010-1013, commands/CmdCD.c:1045-1081)

#### Scenario: Extension added when the name has no dot
- **GIVEN** compression level 0 and root cell `inv`
- **WHEN** the user runs `gds write out/inv`
- **THEN** the file `out/inv.gds` is created

#### Scenario: Compressed extension
- **GIVEN** `gds compress 6`
- **WHEN** the user runs `gds write chip`
- **THEN** the file `chip.gds.gz` is created and contains a gzip stream

#### Scenario: Dot in a directory name suppresses the extension
- **GIVEN** compression level 0 and `magic-compat` ON
- **WHEN** the user runs `gds write ./build/chip`
- **THEN** the file `./build/chip` is created with no extension, because the path contains a `.`
- **AND** with `magic-compat` OFF the file `./build/chip.gds` is created

#### Scenario: Bare command writes the root cell
- **GIVEN** the root cell of the window is `top`
- **WHEN** the user runs `gds` with no arguments
- **THEN** the file `top.gds` is written

### Requirement: Write option settings and defaults

Hefesto SHALL support the following write-affecting settings with these defaults and argument forms. The settings are global to the session and persist between writes. Boolean arguments are matched against `no|false|off|0|yes|true|on|1` (the first four mean false), and `allow|disallow` arguments against `disallow|0|allow|1`. Each option with no argument reports its current value and changes nothing. An unrecognized argument is an argument error and leaves the setting unchanged.

| Option | Argument | Default | Effect on writing |
|---|---|---|---|
| `labels` | bool | yes | Emit label TEXT records (Requirement "Label TEXT records") |
| `lower` | bool | yes | When no, every string record is upper-cased on output |
| `library` | bool | no | When yes, the root cell structure is not written |
| `addendum` | bool | no | When yes, read-only cells with content are not written |
| `arrays` | bool | no | When yes, arrays are written as individual SREFs |
| `merge` | bool | no | When yes, connected tiles are merged into polygons |
| `compress` | integer 0–9 | 0 | gzip level; 0 = plain GDS. Values outside 0–9 are rejected with an error and the setting is kept |
| `datestamp` | `yes`\|`no`\|integer | default | Creation date source (Requirement "Date records") |
| `nodatestamp` | bool | no | Backward-compatible alias: yes ≡ `datestamp no`, no ≡ `datestamp yes` |
| `undefined` | allow\|disallow | disallow | Allow references to cells that cannot be loaded |
| `abstract` | allow\|disallow | disallow | Allow writing abstract (LEF-derived) views |
| `contacts` | bool | (no-op) | Accepted for compatibility. `yes` prints an "unimplemented" error and changes nothing |

The output-style flags `units angstroms`, `options calma-permissive-labels`, and `options string-limit` (defined in `mask-output-styles`) also affect writing as specified below. The `cif` options that enable or disable hierarchical and array interaction generation (defined in `mask-hierarchical-generation`) apply to GDS writing.

Compatibility: behavioral

(Magic ref: commands/CmdCD.c:97-125, commands/CmdCD.c:139-145, commands/CmdCD.c:232-450, commands/CmdCD.c:614-700, commands/CmdCD.c:838-880, commands/CmdCD.c:962-1008, calma/CalmaWrite.c:76-89)

#### Scenario: Compression value out of range
- **GIVEN** `gds compress 3` is in effect
- **WHEN** the user runs `gds compress 12`
- **THEN** an error is reported and the compression level stays 3

#### Scenario: Contacts option is a no-op
- **WHEN** the user runs `gds contacts yes`
- **THEN** an "unimplemented" error is printed and the output is identical to the output without the command

#### Scenario: Lowercase disabled
- **GIVEN** `gds lower no`
- **WHEN** a cell named `inv` with a label `in` is written
- **THEN** the STRNAME record contains `INV` and the label STRING record contains `IN`

### Requirement: Record encoding

Every GDSII record written SHALL consist of a 4-byte header followed by the data bytes. The header is a 2-byte unsigned big-endian total record length (header included), a 1-byte record type, and a 1-byte data type. Integers are big-endian two's complement: 2 bytes for I2 (data type 2), 4 bytes for I4 (data type 3). A bit array (data type 1) is one big-endian 2-byte word. Records with no data (data type 0) have length 4. The record types and data types written are:

| Record | Type | Data type | Length |
|---|---|---|---|
| HEADER | 0x00 | 2 | 6 |
| BGNLIB | 0x01 | 2 | 28 |
| LIBNAME | 0x02 | 6 | 4 + padded string |
| UNITS | 0x03 | 5 | 20 |
| ENDLIB | 0x04 | 0 | 4 |
| BGNSTR | 0x05 | 2 | 28 |
| STRNAME | 0x06 | 6 | 4 + padded string |
| ENDSTR | 0x07 | 0 | 4 |
| BOUNDARY | 0x08 | 0 | 4 |
| SREF | 0x0A | 0 | 4 |
| AREF | 0x0B | 0 | 4 |
| TEXT | 0x0C | 0 | 4 |
| LAYER | 0x0D | 2 | 6 |
| DATATYPE | 0x0E | 2 | 6 |
| XY | 0x10 | 3 | 4 + 8 × points |
| ENDEL | 0x11 | 0 | 4 |
| SNAME | 0x12 | 6 | 4 + padded string |
| COLROW | 0x13 | 2 | 8 |
| TEXTTYPE | 0x16 | 2 | 6 |
| PRESENTATION | 0x17 | 1 | 6 |
| STRING | 0x19 | 6 | 4 + padded string |
| STRANS | 0x1A | 1 | 6 |
| MAG | 0x1B | 5 | 12 |
| ANGLE | 0x1C | 5 | 12 |
| PROPATTR | 0x2B | 2 | 6 |
| PROPVALUE | 0x2C | 6 | 4 + padded string |

Hefesto never writes PATH, BOX, NODE, WIDTH, PATHTYPE, REFLIBS, FONTS, GENERATIONS, or FORMAT records from its own data. They can appear only inside bytes copied verbatim from a vendor GDS file.

The output stream from Hefesto's own data SHALL follow this grammar (verbatim-copied structures are covered by Requirement "Verbatim copy of read-only cells", and transcoded OASIS cells by Requirement "Read-only OASIS vendor cells"):

```ebnf
stream      = header bgnlib libname units { structure } endlib ;
header      = rec(0x00,2, i2(3)) ;
bgnlib      = rec(0x01,2, date date) ;
libname     = rec(0x02,6, string) ;
units       = rec(0x03,5, real8 real8) ;
structure   = rec(0x05,2, date date) rec(0x06,6, string)
              { cellref } { paintelem } { labelelem } rec(0x07,0) ;
cellref     = ( rec(0x0A,0) | rec(0x0B,0) ) rec(0x12,6, string)
              rec(0x1A,1, i2) [ rec(0x1C,5, real8) ]
              [ rec(0x13,2, i2 i2) ] rec(0x10,3, i4 i4 { i4 i4 })
              { rec(0x2B,2, i2) rec(0x2C,6, string) } rec(0x11,0) ;
paintelem   = boundary | labellayertext ;
boundary    = rec(0x08,0) rec(0x0D,2, i2) rec(0x0E,2, i2)
              rec(0x10,3, i4 i4 { i4 i4 }) rec(0x11,0) ;
labellayertext = rec(0x0C,0) rec(0x0D,2, i2) rec(0x16,2, i2)
              rec(0x10,3, i4 i4) rec(0x19,6, string) rec(0x11,0) ;
labelelem   = rec(0x0C,0) rec(0x0D,2, i2) rec(0x16,2, i2)
              [ rec(0x17,1, i2) rec(0x1A,1, i2) rec(0x1B,5, real8)
                [ rec(0x1C,5, real8) ] ]
              rec(0x10,3, i4 i4) rec(0x19,6, string) rec(0x11,0)
              [ boundary ] ;
endlib      = rec(0x04,0) ;
date        = i2 i2 i2 i2 i2 i2 ;     (* year-1900, month, day, hour, min, sec *)
rec(t,d,data) = u2(4 + len(data)) u1(t) u1(d) data ;
```

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:148-191, calma/calmaInt.h:29-104)

#### Scenario: ENDEL encoding
- **WHEN** an ENDEL record is written
- **THEN** the bytes are `00 04 11 00`

#### Scenario: LAYER record for layer 49
- **WHEN** a LAYER record with value 49 is written
- **THEN** the bytes are `00 06 0D 02 00 31`

#### Scenario: Negative I4 coordinate
- **WHEN** the coordinate -200 is written as I4
- **THEN** the bytes are `FF FF FF 38`

### Requirement: Eight-byte real encoding

Every R8 value (UNITS, MAG, ANGLE) SHALL be written in GDSII excess-64 base-16 floating point format: byte 0 is `sign << 7 | exponent`, and bytes 1–7 are the 56 most significant bits of the mantissa. For value `d`:

1. If `d == 0.0`, all 8 bytes are zero.
2. Otherwise `sign = 1` if `d < 0` and `d := |d|`. Start with `exponent = 64`. While `d >= 1.0`, set `d := d/16` and `exponent += 1`. While `d < 0.0625`, set `d := d*16` and `exponent -= 1`.
3. The mantissa is the binary expansion of `d` (so `1/16 <= d < 1`), taken bit by bit from 2⁻¹, truncated after 56 bits (no rounding).

All operations are on IEEE-754 binary64 values. Scaling by 16 and subtracting 0.5 are exact, so the result is the exact base-16 representation of the binary64 input. A binary64 value always fits in 56 mantissa bits after normalization, so truncation never drops a non-zero bit.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:3260-3320)

#### Scenario: Standard values
- **WHEN** the following values are encoded
- **THEN** the bytes are:
  `0.001` → `3E 41 89 37 4B C6 A7 F0`;
  `1e-9` → `39 44 B8 2F A0 9B 5A 54`;
  `0.0001` → `3D 68 DB 8B AC 71 0C B4`;
  `1e-10` → `38 6D F3 7F 67 5E F6 EC`;
  `1.0` → `41 10 00 00 00 00 00 00`;
  `0.5` → `40 80 00 00 00 00 00 00`;
  `90` → `42 5A 00 00 00 00 00 00`;
  `180` → `42 B4 00 00 00 00 00 00`;
  `270` → `43 10 E0 00 00 00 00 00`;
  `-1.0` → `C1 10 00 00 00 00 00 00`;
  `0.1` → `40 19 99 99 99 99 99 9A`

### Requirement: String records and character mapping

Every string record (LIBNAME, STRNAME, SNAME, STRING, PROPVALUE) SHALL be encoded as follows. Let `s` be the source string and `n = strlen(s)`:

1. If `n` is odd, `n := n + 1`. The extra byte is the string terminator and is written as `0x00`.
2. If the output style has the `string-limit` flag and the string is longer than 32, only 32 bytes are written, chosen by the `magic-compat` setting (Q-MASK-207, resolution: compat). With it ON (the default), as in Magic, if `n > 32` the last 32 bytes of the (padded) string are written: start at offset `n - 32` and set `n := 32`; for odd original lengths this keeps the last 31 characters plus the `0x00` pad. With it OFF, if the original length `m = strlen(s)` exceeds 32, the last 32 characters are written (offset `m - 32`, `n := 32`, no pad). The truncation warning is the same in both settings.
3. The record length is `n + 4`, the record type is the given type, and the data type is 6.
4. Each byte `c` of the `n` bytes is mapped as follows:
   - `c == 0` (the pad) → `0x00`.
   - `c > 127` → `X`.
   - Otherwise `c := table[c]`, where `table` is the *strict* table, or the *permissive* table when the output style has `calma-permissive-labels`.
   - Then, if the `lower` setting is no and `c` is a lowercase ASCII letter, it is written upper-cased.
5. Strict table: `A–Z`, `a–z`, `0–9`, `$` and `_` map to themselves. Every other printable character (0x20–0x7E) maps to `_`. Control characters 0x00–0x1F and DEL (0x7F) map to `0x00`.
6. Permissive table: every printable character 0x20–0x7E maps to itself, except that with `magic-compat` ON (the default) `%` (0x25) maps to `&` and `&` (0x26) maps to `%`, as in Magic, with Magic's "characters changed" warning (Q-MASK-200, resolution: compat). With it OFF there is no exception. Control characters and DEL map to `0x00`. The same table, per setting, is used for STRING, PROPVALUE and STRNAME records and for the name-legality rule of "Structure naming". Layout data is never changed by the mapping.

A control character in a label or use name therefore produces a `0x00` byte inside the string (see Q-MASK-208). No other length limit is applied. Strings longer than 65531 bytes are not guarded against.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:197-235, calma/CalmaWrite.c:3158-3244, calma/calmaInt.h:129)

#### Scenario: Odd-length string padding
- **WHEN** the STRNAME `top` is written with the strict table
- **THEN** the bytes are `00 08 06 06 74 6F 70 00`

#### Scenario: Strict mapping of punctuation in a label
- **GIVEN** the strict table
- **WHEN** a label with text `A<0>` is written
- **THEN** the STRING record data is `A_0_` (`41 5F 30 5F`) with length field 8

#### Scenario: Permissive mapping swaps percent and ampersand
- **GIVEN** the output style has `options calma-permissive-labels`
- **WHEN** a label with text `a%b` is written
- **THEN** the STRING record data is `61 26 62 00` (`a&b` plus pad)
- **AND** with `magic-compat` OFF it is `61 25 62 00` (`a%b` plus pad)

#### Scenario: String limit keeps the tail
- **GIVEN** the output style has `options string-limit`
- **WHEN** a 36-character label `abcdefghijklmnopqrstuvwxyz0123456789` is written
- **THEN** the STRING record has length 36 and contains `efghijklmnopqrstuvwxyz0123456789`

#### Scenario: String limit with an odd length
- **GIVEN** the output style has `options string-limit`
- **WHEN** a 35-character label `bcdefghijklmnopqrstuvwxyz0123456789` is written
- **THEN** with `magic-compat` ON the STRING data is the 31 characters `fghijklmnopqrstuvwxyz0123456789` followed by `0x00`
- **AND** with `magic-compat` OFF it is the 32 characters `efghijklmnopqrstuvwxyz0123456789`

### Requirement: Date records

Each BGNLIB and BGNSTR record SHALL contain two dates: a *creation* date followed by a *modification* date. Each date is six I2 values from the local-time broken-down representation of a POSIX time value `t`: `year - 1900`, `month (1–12)`, `day`, `hour`, `minute`, `second`.

- The creation date is the session date stamp if one is set, otherwise the timestamp of the cell being written (the root cell for BGNLIB). For a verbatim-copied structure it is the vendor cell's timestamp. For every structure produced by a full dump it depends on the `magic-compat` setting (Q-MASK-215, resolution: compat): with it ON (the default) it is the session date stamp if set, otherwise the timestamp of the vendor cell that triggered the dump, as in Magic; with it OFF it is the session date stamp if set, otherwise the creation date copied from that structure's BGNSTR in the source file.
- The modification date depends on the `magic-compat` setting (Q-MASK-202, resolution: compat): with it ON (the default) it is always the current time at the moment the record is written, as in Magic, and is the only non-reproducible field; with it OFF it is time 0, encoded as `datestamp no` encodes the creation date, as Magic's documentation states. BGNLIB, generated and verbatim-copied structures, full-dump structures and vendor cells all follow the same rule.
- Date stamp setting: `datestamp yes` (and the initial state) means "no session stamp; use the cell timestamp". `datestamp no` and `nodatestamp yes` set the session stamp to 0. `datestamp <integer>` sets the session stamp to that integer. `nodatestamp no` clears it. Any other argument is an error. Magic's help text says `datestamp yes` uses the current time, but it uses the cell timestamp (see Q-MASK-224).

Conversion uses the host local time zone in both `magic-compat` settings (the device zone on iPadOS; Q-MASK-203, resolution: replicate); golden cases and CI run with `TZ=UTC`, and one test pins a non-UTC zone. Under `TZ=UTC`, `t = 0` encodes as `00 46 00 01 00 01 00 00 00 00 00 00`.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:1438-1444, calma/CalmaWrite.c:3072-3078, calma/CalmaWrite.c:3123-3134, calma/CalmaWrite.c:446-451, commands/CmdCD.c:803-837, commands/CmdCD.c:917-961)

#### Scenario: Fixed date stamp
- **GIVEN** `TZ=UTC` and `gds datestamp 1700000000`
- **WHEN** a library is written
- **THEN** the first 12 data bytes of BGNLIB and of every BGNSTR encode 2023-11-14 22:13:20 as `00 7B 00 0B 00 0E 00 16 00 0D 00 14`
- **AND** the last 12 data bytes encode the current time with `magic-compat` ON, and `00 46 00 01 00 01 00 00 00 00 00 00` (t = 0) with it OFF

#### Scenario: Default uses the cell timestamp
- **GIVEN** no date stamp is set and cell `inv` has timestamp 1600000000
- **WHEN** `inv` is written under `TZ=UTC`
- **THEN** its BGNSTR creation date is `00 78 00 09 00 0D 00 0C 00 1A 00 28` (2020-09-13 12:26:40)

### Requirement: Library header and trailer

The stream SHALL start with these records in order and end with ENDLIB:

1. HEADER with version 3: `00 06 00 02 00 03`.
2. BGNLIB with the creation date (session stamp or root cell timestamp) and the current time.
3. LIBNAME holding the root cell name, sanitized exactly as a structure name (Requirement "Structure naming"), using cell id number 1.
4. UNITS with two R8 values: user units per database unit and meters per database unit. These are `0.001` and `1e-9` for nanometer units, and `0.0001` and `1e-10` when the output style has `units angstroms`.
5. The structures (Requirement "Structure ordering").
6. ENDLIB `00 04 04 00`.

The UNITS values SHALL be derived from the current output style's `units angstroms` flag on every write, in both `magic-compat` settings (Q-MASK-201, resolution: fix). Magic keeps them in static storage, so once a write with an angstrom style has occurred in a session every later nanometer write declares 1e-10 m per database unit while its coordinates are scaled for nanometers, a file 10× too small; this is a fixed deviation (X-2 tier 1). Golden cases use one write per session, so none shows the difference.

When compiled with gzip support, Magic flushes the stream with a sync flush after ENDLIB. Hefesto does not otherwise pad the output.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:3062-3103, calma/CalmaWrite.c:357-388)

#### Scenario: Header bytes for a nanometer style
- **GIVEN** a nanometer output style, root cell `top`, session stamp 0, `TZ=UTC`
- **WHEN** the library is written
- **THEN** the file starts with `00 06 00 02 00 03`, then `00 1C 01 02 00 46 00 01 00 01 00 00 00 00 00 00` followed by 12 bytes of current time, then `00 08 02 06 74 6F 70 00`, then `00 14 03 05 3E 41 89 37 4B C6 A7 F0 39 44 B8 2F A0 9B 5A 54`

#### Scenario: Angstrom units
- **GIVEN** an output style with `units angstroms`
- **WHEN** the library is written
- **THEN** the UNITS record is `00 14 03 05 3D 68 DB 8B AC 71 0C B4 38 6D F3 7F 67 5E F6 EC`

#### Scenario: Nanometer write after an angstrom write
- **GIVEN** a session that has written a library with an angstrom style
- **WHEN** a library is then written with a nanometer style
- **THEN** its UNITS record is `00 14 03 05 3E 41 89 37 4B C6 A7 F0 39 44 B8 2F A0 9B 5A 54` (0.001, 1e-9)

### Requirement: Write preconditions and subtree loading

Before writing anything to the stream, Hefesto SHALL:

1. Fail with "no output style" if no output style is selected. This check is made before the output file is opened, so the output file is not created or modified, in both `magic-compat` settings (Q-MASK-219, resolution: fix; Magic opens the file first and truncates an existing file of the same name).
2. Load every cell in the hierarchy under the root, within the root's bounding box, by the recursive area search over uses described in Requirement "Structure ordering" (which can rebuild a cell's use index with `magic-compat` ON). If a cell cannot be loaded and `undefined` is disallow, fail without writing any records. With `undefined allow`, unloadable cells are tolerated.
3. Resolve pending timestamp mismatches between parents and children (see capability `layout-geometry-model`).
4. Reset all cell id numbers to 0 and give the root id number 1. The next number assigned is 2.

Compatibility: behavioral

(Magic ref: calma/CalmaWrite.c:284-345, calma/CalmaWrite.c:824-830)

#### Scenario: Missing subcell with undefined disallowed
- **GIVEN** root `top` uses cell `missing` whose file cannot be found, and `undefined` is disallow
- **WHEN** GDS is written
- **THEN** the write fails, reporting that it failed on cell `missing`, and the file contains no records

#### Scenario: Missing subcell with undefined allowed
- **GIVEN** the same layout and `gds undefined allow`
- **WHEN** GDS is written
- **THEN** the write succeeds, `top` contains an SREF with SNAME `missing`, and no structure named `missing` is written

### Requirement: Structure ordering

Structures SHALL be written in a depth-first post-order traversal from the root: every child structure is written before the first structure that references it, and each cell is written at most once. The traversal of a cell C is:

1. If C already has a positive id number, stop (already written or in progress).
2. If C's id number is 0, assign the next number (2, 3, …). Then make the number positive, which marks C as "in progress". Ids are therefore assigned in DFS pre-order of first visit, and the root is 1.
3. If C is not loaded, try to load it. If that fails, stop without writing C. Parent references to C remain (undefined reference).
4. Apply the read-only, abstract, and addendum rules (Requirements "Verbatim copy of read-only cells", "Read-only OASIS vendor cells", "Abstract views", "Addendum mode").
5. Unless C is a full-dump vendor cell, traverse each child use of C in *use enumeration order* (below). If any child traversal returns failure, C fails and is not written.
6. Write C's structure (generated or copied). Under `library yes`, the root's own structure is skipped, but its descendants are written.

After the root traversal completes, every cell that was referenced by a forward reference in a full-dumped vendor library, never defined there, and exists in the database is traversed in the same way and appended (Requirement "Full dump of a vendor library").

*Use enumeration order* is the order in which the uses of a cell are visited. It depends on the `magic-compat` setting (Q-MASK-204, resolution: compat). The same order is used for the cell references inside a structure, and by `cif-format-io` for CIF symbol numbering and calls (Q-MASK-323).

With `magic-compat` OFF it is the *canonical order*: the reverse of insertion order, i.e. the reverse of the `.mag` file's `use` order for a cell read from `.mag`, or of the SREF/AREF order for a cell read from GDS, independent of session history.

With `magic-compat` ON (the default) it is the order in which Magic's cell-use spatial index (bplane) returns the uses at the moment the writer enumerates them, which Hefesto's cell-use index SHALL reproduce exactly:

- *Insertion.* A use that is placed (read from a file, created, moved, or re-placed) is pushed at the head of the index's unbinned list (LIFO) unless bins exist and the use's bounding box lies inside the bin area, in which case it is pushed at the head of the list of its bin (oversized uses, as wide or as tall as a bin, go to the array's oversized list).
- *Re-placement.* When a child's bounding box is recomputed, every use of that child is removed from its parent's index and placed again, in the order of the child's parent-use list (Magic DBReComputeBbox), which moves those uses to the head of the unbinned list.
- *Rebuild.* An overlap search over an area A (Magic's `BPE_OVERLAP`, used by every area search over uses) first shrinks A by 1 on each side and tests whether the shrunk area surrounds the index's bounding box, touching allowed. If it does not (a *non-surrounding search*), the bins are rebuilt when the unbinned list holds 10 or more uses: existing bins are dissolved into the list, and a bin array is built over the list's bounding box (doubled when bins already existed) with Magic's bin sizing and recursive sub-binning (minimum population 10, average population 1.0); sub-bins are also built on the fly when a bin yields 10 or more rejected uses during a search. Because of the shrink, a search over a cell's own bounding box is non-surrounding whenever one of its uses touches that box's edge. An exact-match lookup (Magic's `BPE_EQUAL`, as used to find a duplicate use) and a search over the whole plane (as made by each writer's enumeration of a cell's uses) never rebuild.
- *Walk.* An enumeration visits the bins of the root array in row-major order from the bottom-left bin (only the bins the search area reaches, for a non-surrounding search), descending into sub-arrays where a bin is sub-binned, each array's oversized list after its bins, each list from head to tail, and finally the unbinned list from head to tail.

The bin geometry, sizing arithmetic and walk SHALL follow Magic's bplane exactly (bplane/bpBins.c, bplane/bpEnum.h, bplane/bpEnum.c). Each writer makes the same searches as Magic, at the same points, so it sees the same index state:

1. Subtree loading (Requirement "Write preconditions and subtree loading", and its CIF counterpart) is a recursive overlap search: the root's uses are searched over the root's bounding box, each use's child over the search area transformed into the child's coordinates (each array element in turn), and the elements of an array after the first are skipped once the transformed area surrounds the child's bounding box. Each of these searches can rebuild the searched cell's bins. In a fresh session the root, and any cell whose transformed search area does not surround its uses after the shrink, is therefore binned before any structure is written if it has 10 or more uses.
2. The timestamp-mismatch fix-up that follows re-places the uses of each child whose bounding box changed.
3. The GDS writer then enumerates SREFs before the cell's own mask generation (CIFGenSubcells); the CIF writer numbers children before CIFGenSubcells and writes `C` calls after it (capability `cif-format-io`).

The first write of a fresh session therefore sees unbinned uses only in cells with fewer than 10 uses, or whose subtree-loading search surrounded their uses. The golden harness runs each case as one write in a fresh session with no DRC or extraction before it, unless the case tests history on purpose; cases with 10 or more uses and an interaction verify both CIF and GDS. The `.mag` writer lists uses alphabetically, independent of this order.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:316-325, cif/CIFwrite.c:137-145, database/DBexpand.c:276-340, database/DBcellsrch.c:1131-1222, bplane/bpEnum.c:75-120, bplane/bpBins.c:645-651, calma/CalmaWrite.c:345-366, calma/CalmaWrite.c:920-996, calma/CalmaWrite.c:1047-1083, calma/CalmaWrite.c:1349-1370, calma/CalmaWrite.c:1486, calma/CalmaWrite.c:1510-1514, database/DBcellsrch.c:1263-1281, database/DBcellsrch.c:86-109, database/DBcellbox.c:728-764, bplane/bpMain.c:114-170, bplane/bpBins.c:256-719, bplane/bpEnum.c:80-140, bplane/bpEnum.h:190-400, cif/CIFwrite.c:333, cif/CIFwrite.c:467)

#### Scenario: Post-order with shared child
- **GIVEN** `top.mag` lists `use a a_0` then `use b b_0`, `a` uses `c`, and `b` uses `c`
- **WHEN** GDS is written with default options
- **AND** the uses of `top` enumerate in reverse file order (`b_0` first)
- **THEN** the structure order is `c`, `b`, `a`, `top`
- **AND** `c` appears exactly once
- **AND** the cell id numbers are `top`=1, `b`=2, `c`=3, `a`=4

#### Scenario: Canonical order with magic-compat OFF
- **GIVEN** `magic-compat` OFF, `top.mag` lists 12 uses `u1` … `u12`, and a DRC check of part of `top` ran before the write
- **WHEN** GDS is written
- **THEN** the SREFs of `top` are written in the order `u12`, `u11`, …, `u1`, the same as in a fresh session

#### Scenario: Binned order with magic-compat ON
- **GIVEN** `magic-compat` ON, the same `top`, and an area search that does not surround `top`'s uses ran before the write
- **WHEN** GDS is written
- **THEN** the SREFs of `top` are written in the bin-walk order of the rebuilt index, which matches Magic's output for the same session history

#### Scenario: Fresh-session write of a cell whose uses touch its bounding box
- **GIVEN** `magic-compat` ON, a fresh session, and `top` with 12 uses read from `top.mag`, one of which touches the edge of `top`'s bounding box
- **WHEN** GDS is written once, with no other command before it
- **THEN** the subtree-loading search over `top`'s bounding box has rebuilt `top`'s bins, and the SREFs of `top` are written in the bin-walk order, as in Magic, not in the unbinned order

#### Scenario: Library mode omits the root
- **GIVEN** `gds library yes` and `top` uses `inv`
- **WHEN** GDS is written
- **THEN** the stream contains structure `inv` and no structure `top`
- **AND** LIBNAME is still `top`

### Requirement: Structure naming

The STRNAME of a structure, the SNAME of a reference to it, and LIBNAME SHALL be derived from the cell name as follows:

1. The name is *legal* if every byte is ≤ 127 and maps to a non-zero value in the active character table (strict or permissive), and, when the output style has `string-limit`, its length is ≤ 32.
2. A legal name is written through the string encoding (Requirement "String records and character mapping"). Characters that the table changes, such as `-` → `_` in the strict table, are changed in the output, with a warning. If the mapped name equals a name already given to a different cell in this write, the later cell in write order is named `XXXXX<n>` as in item 3; if that name is also taken in this write, `_k` is appended with the smallest free `k ≥ 1`. Its SNAMEs use the same name, and a warning names both cells. This applies in both `magic-compat` settings (Q-MASK-209, resolution: fix; a collision would lose one cell's geometry in every reader), and names without a collision are as in Magic.
3. An illegal name is replaced by `XXXXX<n>`, where `<n>` is the decimal absolute value of the cell's id number at the time of writing.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:1877-1934)

#### Scenario: Dash mapped to underscore
- **GIVEN** the strict table
- **WHEN** cell `nand-2` is written
- **THEN** its STRNAME data is `nand_2`

#### Scenario: Non-ASCII name replaced
- **GIVEN** cell `célula` is the third cell visited (id 3)
- **WHEN** it is written
- **THEN** its STRNAME is `XXXXX3` and every SNAME referencing it is `XXXXX3`

#### Scenario: Long name under string-limit
- **GIVEN** `options string-limit` and a 40-character cell name with id 2
- **WHEN** the cell is written
- **THEN** its STRNAME is `XXXXX2`

#### Scenario: Colliding mapped names
- **GIVEN** the strict table and cells `a-b` (id 2) and `a_b` (id 3), with `a-b` written first
- **WHEN** the library is written
- **THEN** `a-b` is written as `a_b`, `a_b` is written as `XXXXX3` with a warning, and every SNAME referencing `a_b` is `XXXXX3`

### Requirement: Output scale factors

For each structure written from Hefesto's own data, two integer scale factors SHALL be computed from the output style. Let `U = 100` if the style has `units angstroms`, else `U = 10`.

- If `U mod E == 0`: `Wscale = S * U / E` (multiplies internal units: cell reference positions, array pitches, label positions, label rectangles), and `Pscale = U / E` (multiplies mask-plane coordinates, which are already in output units).
- Otherwise the behaviour depends on the `magic-compat` setting (Q-MASK-225, resolution: compat): with it ON (the default) an error is reported, `Wscale = S` and `Pscale = 1`, and the output is written anyway, mis-scaled, as in Magic; with it OFF the write is refused before the output file is opened, with an error suggesting `units angstroms` or a coarser `scalegrid`, and the command fails.

All products, including the label-midpoint sum, are computed exactly in 64-bit integer arithmetic; a value outside the signed 32-bit (I4) range fails the write with an error naming the cell and the value, in both `magic-compat` settings (Q-MASK-222, resolution: fix; Magic overflows in 32 bits, which is undefined behaviour). The same rule covers an AREF's COLROW counts: a count above 32767 fails the write (Magic writes the low 16 bits).

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:1450-1478)

#### Scenario: Centimicron style
- **GIVEN** `scalefactor 10` with expander 1, nanometer units
- **WHEN** scales are computed
- **THEN** `Wscale = 100` and `Pscale = 10`

#### Scenario: Nanometer style
- **GIVEN** `scalefactor 5 nanometers` (expander 10)
- **WHEN** scales are computed
- **THEN** `Wscale = 5` and `Pscale = 1`

#### Scenario: Angstrom style
- **GIVEN** `scalefactor 25 angstroms` (expander 100) with `units angstroms`
- **WHEN** scales are computed
- **THEN** `U = 100`, `Wscale = 25`, and `Pscale = 1`

#### Scenario: Angstrom scale without angstrom units
- **GIVEN** `scalefactor 25 angstroms` (expander 100) without `units angstroms`
- **WHEN** scales are computed
- **THEN** an error is reported advising `units angstroms`, `Wscale = 25`, `Pscale = 1`, and the output is still written

### Requirement: Structure body order

A structure written from Hefesto's own data SHALL contain, in this order:

1. BGNSTR (28 bytes, two dates) and STRNAME.
2. One cell reference element per child use, in use enumeration order (or several per arrayed use under `arrays yes`).
3. For each mask layer in output-style order, skipping temporary layers and layers whose GDS layer number is outside 0–255 (including the unset value -1): if the layer is a `labellayer`, one TEXT element per tile (Requirement "Label layers"); otherwise the boundary elements of its mask plane (Requirements "Unmerged boundary output" and "Merged polygon output").
4. If `labels` is yes: the non-port labels in the cell's label-list order, then the port labels sorted by port index (Requirement "Label TEXT records").
5. ENDSTR `00 04 07 00`.

Datatype values are not range-checked on output.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:1419-1610)

#### Scenario: References precede paint
- **GIVEN** cell `top` with one use of `inv` and metal1 paint
- **WHEN** `top` is written
- **THEN** the SREF element appears before any BOUNDARY element in `top`

#### Scenario: Layer without GDS number is skipped
- **GIVEN** a non-temporary output layer `XL` with no `calma` statement
- **WHEN** a cell with geometry on `XL` is written
- **THEN** no element for `XL` appears

### Requirement: Mask geometry for each structure

For each structure written from Hefesto's own data, the mask planes SHALL be generated as defined in `mask-hierarchical-generation`, over the generation area `A`:

1. `A` is the cell bounding box expanded on every side by the output style's interaction radius (see `mask-output-styles`).
2. If the cell has a `FIXED_BBOX` property with exactly four integer values (xlo ylo xhi yhi), `A` is enlarged to include that rectangle.
3. Flat generation of the cell's own paint over `A` is followed by subcell interaction generation and then array interaction generation. Each of these two is skipped when disabled by the `cif` hierarchy/array write options. If the cell has a `CIFhier` property, both are force-enabled for that cell, and for every structure written while its children are traversed, and the previous settings are restored afterwards.
4. Cells marked as vendor GDS (set during this write, see Requirement "Verbatim copy of read-only cells") are treated by interaction generation as defined in `mask-hierarchical-generation`.

No clipping is applied to tiles on output.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:1480-1512, calma/CalmaWrite.c:1059-1083, calma/CalmaWrite.c:1349-1370, cif/CIFhier.c:392)

#### Scenario: Fixed bounding box enlarges the area
- **GIVEN** a cell with bounding box (0,0)-(10,10), interaction radius 2, and `FIXED_BBOX` (−20 −20 30 30)
- **WHEN** its mask geometry is generated for GDS
- **THEN** the generation area is (−20,−20)-(30,30)

### Requirement: Tile decomposition and enumeration order

A mask plane SHALL be a corner-stitched decomposition of the plane into *maximal horizontal strips*, which is canonical for Manhattan geometry: no two horizontally adjacent tiles have the same type, and no two vertically adjacent tiles of the same type share both left and right x-coordinates. The plane holds two types, empty and solid. Non-Manhattan geometry uses split (triangular) tiles as produced by mask generation (see Q-MASK-218).

Solid tiles SHALL be visited in the order of the corner-stitched area enumeration over the whole plane:

- Build a forest where each tile's *parent* is the neighbor touching the tile's left edge at its bottom-left corner (the left-bottom stitch). Tiles touching the left boundary of the search area are roots, ordered top to bottom.
- Visit the forest in depth-first pre-order. The children of a tile are visited in top-to-bottom order.
- A split tile is reported once for each side whose type is solid. The left (TT_SIDE clear) side is reported before the right side.

This order determines element order in the output.

Compatibility: exact

(Magic ref: database/DBtiles.c:521-618, tiles/tile.h:205-250)

#### Scenario: L-shape order
- **GIVEN** a mask plane with solid tiles A = (0,20)-(20,40) and B = (0,0)-(40,20) in output units
- **WHEN** solid tiles are enumerated
- **THEN** A is visited before B

### Requirement: Unmerged boundary output

When `merge` is no, each solid tile (or each solid side of a split tile) SHALL be written as one BOUNDARY element: BOUNDARY, LAYER `L`, DATATYPE `D`, XY, ENDEL. With tile rectangle `(x0,y0)-(x1,y1)` in mask-plane units multiplied by `Pscale`:

- Rectangle: XY length 44 (5 points): `(x0,y0) (x1,y0) (x1,y1) (x0,y1) (x0,y0)`.
- Triangle: XY length 36 (4 points), selected by `k = 2*side + dir`, where `dir = 1` for a diagonal from top-left to bottom-right (`\`), `dir = 0` for bottom-left to top-right (`/`), and `side = 1` for the right-hand triangle:
  - k=0 (`/`, left/upper-left): `(x0,y0) (x0,y1) (x1,y1) (x0,y0)`
  - k=1 (`\`, left/lower-left): `(x0,y1) (x0,y0) (x1,y0) (x0,y1)`
  - k=2 (`/`, right/lower-right): `(x0,y0) (x1,y0) (x1,y1) (x0,y0)`
  - k=3 (`\`, right/upper-right): `(x0,y1) (x1,y1) (x1,y0) (x0,y1)`

BOX records are never used for rectangles.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:2723-2810)

#### Scenario: Single rectangle bytes
- **GIVEN** `scalefactor 10` (Pscale 10), layer `M1` with `calma 49 0`, and a metal1 rectangle (0,0)-(4,2) in internal units, giving mask tile (0,0)-(40,20)
- **WHEN** the cell is written with `merge no`
- **THEN** the element bytes are `00 04 08 00`, `00 06 0D 02 00 31`, `00 06 0E 02 00 00`, `00 2C 10 03 00000000 00000000 00000190 00000000 00000190 000000C8 00000000 000000C8 00000000 00000000`, `00 04 11 00`

#### Scenario: L-shape unmerged
- **GIVEN** the L-shape tiles A = (0,20)-(20,40) and B = (0,0)-(40,20), Pscale 10
- **WHEN** written with `merge no`
- **THEN** two BOUNDARY elements are written in the order A then B, with XY `(0,200) (200,200) (200,400) (0,400) (0,200)` and `(0,0) (400,0) (400,200) (0,200) (0,0)`

#### Scenario: Lower-left triangle
- **GIVEN** a split tile (0,0)-(10,10) with `\` diagonal, solid on the left side, Pscale 1
- **WHEN** written with `merge no`
- **THEN** the XY record has length 36 and points `(0,10) (0,0) (10,0) (0,10)`

### Requirement: Merged polygon output

When `merge` is yes, each connected group of solid tiles on a mask plane SHALL be written as one or more BOUNDARY polygons by the following algorithm. Hefesto MUST reproduce it step for step because point order and polygon split points are observable. Coordinates are in mask-plane units until output.

Definitions: a *ring* is a cyclic list of nodes. Each node has a start point and a kind ∈ {INIT, EXT, INT}, and represents the segment from its start point to the next node's start point. A *bound* is a ring plus a point count. The bounds list is ordered with the newest bound first.

```
for each solid tile T in enumeration order:            # Requirement "Tile decomposition..."
    if T is marked PROCESSED: continue
    bounds := []
    stack := [T]; mark T PENDING
    while stack not empty:
        t := pop(stack)                                  # LIFO
        if t not PENDING: continue
        mark t PROCESSED
        ring, np := init_ring(t)                          # corners, all INIT
        intedges := 0
        for each boundary segment (p1 -> p2, neighbor n) of t, in the order below:
            ext := (type of n facing t) != (type of t facing n)
            intedges += add_segment(ring, ext, p1, p2)    # kind of the node left current: EXT 0, INT 1, INIT 2; -1 if none matches
            np += 1    (not for the diagonal segment)
            if not ext and n is unmarked: mark n PENDING; push(stack, n)
        if intedges == 0:                                 # isolated tile
            write t as in "Unmerged boundary output"; continue outer loop with next T
        merge_segments(ring, bounds, np)
    remove_degenerate(bounds); remove_colinear(bounds)
    for each bound B in bounds order (newest first): write B
```

**init_ring and segment order.** For a rectangle `(llx,lly)-(urx,ury)`, the ring is `(urx,ury) → (llx,ury) → (llx,lly) → (urx,lly)` with `np := 0`. The segments are generated counterclockwise:

- top: neighbors above from right to left, each segment `(min(urx, R(n)), ury) → (max(llx, L(n)), ury)`
- left: neighbors to the left from bottom to top, `(llx, min(ury, T(n))) → (llx, max(lly, B(n)))`
- bottom: neighbors below from left to right, `(max(llx, L(n)), lly) → (min(urx, R(n)), lly)`
- right: neighbors to the right from top to bottom, `(urx, max(lly, B(n))) → (urx, min(ury, T(n)))`

Only neighbors that share a segment of positive length are considered. For a split tile, `np := 1` and only the sides adjacent to the solid triangle are searched, plus the diagonal as an EXT segment (which does not increment `np`). With k as in "Unmerged boundary output":

- k=0: ring `(urx,ury),(llx,ury),(llx,lly)`; order top, left, diagonal `(llx,lly)→(urx,ury)`.
- k=1: ring `(llx,ury),(llx,lly),(urx,lly)`; order left, bottom, diagonal `(urx,lly)→(llx,ury)`.
- k=2: ring `(urx,ury),(llx,lly),(urx,lly)`; order diagonal `(urx,ury)→(llx,lly)`, bottom, right.
- k=3: ring `(urx,ury),(llx,ury),(urx,lly)`; order top, diagonal `(llx,ury)→(urx,lly)`, right.

**add_segment(ring, ext, p1, p2)** starts at the node where the previous call finished (initially the ring head) and scans the ring once for the first INIT node X whose start equals `p1` and/or whose successor's start equals `p2`:

- both match: X.kind := EXT/INT.
- only the start matches: insert a node Y after X with `Y.start = p2` and `Y.kind = X.kind` (INIT). Then set X.kind := EXT/INT, and continue from Y.
- only the end matches: insert Y after X with `Y.start = p1` and `Y.kind = EXT/INT`, and continue from Y.

It returns the kind of the node it leaves current (EXT 0, INT 1; INIT 2 after a start-only match), or -1 when no node matches, as calmaAddSegment does. So a tile with no internal neighbour but several neighbours along its top or bottom side (for example a split tile whose space side touches it) has a nonzero `intedges` and goes through `merge_segments`: it is written as a merged bound starting at its top-right corner, not as the plain rectangle.

**merge_segments(edge, bounds, np)**, where `edge` is the original ring head (for a rectangle, the `(urx,ury)` node). For each node `c` of `edge` that is not EXT (INT or INIT), taken in ring order starting from `edge.next` and ending with `edge` itself, with predecessor `p`: for each bound B in list order with `B.points + np <= 201`, and for each node `q` of B that is not EXT (from `B.first.next` to `B.first`, with predecessor `qp`) such that `q.start == c.next.start` and `q.next.start == c.start`, splice: `p.next := q.next`, `qp.next := c.next`, delete `q` and `c`, set `B.first := qp` if `q` was `B.first`, `B.points += np - 2`, and return. Only one splice is made per tile. If no splice is made, prepend a new bound `{first = edge, points = np}`.

**remove_degenerate.** For each bound, repeat until no change: scan from `first`. If node `c = p.next` satisfies `c.start == c.next.next.start`, remove `c` and `c.next`, set `first := p`, `points -= 2`, and restart.

**remove_colinear.** For each bound, make one pass with `p` starting at `first` and stopping when `p` returns to `first`. `first` is updated when the node removed is `first`. Let `c = p.next` and `n = c.next`. Remove `c` (`points -= 1`, keep `p`) when `p.x == c.x == n.x` or `p.y == c.y == n.y`, or when both `p→c` and `c→n` are non-axis-parallel and their direction vectors are equal after each is divided by its greatest common factor (a vector is reduced only when the two are not already equal). Otherwise advance `p`.

**write bound.** BOUNDARY, LAYER, DATATYPE, then XY with length `4 + 8*(points+1)`: every node's start point from `first` around the ring, multiplied by `Pscale`, followed by the first point again. Then ENDEL.

Consequences: a polygon never has more than 200 XY points, including the closing point. A component that would exceed this is split into several abutting bounds. Holes and multiply-connected joins appear as zero-width slits. Tiles of one component joined to two bounds do not merge those bounds. The merge and collinear passes are ported exactly in both `magic-compat` settings (Q-MASK-217, resolution: replicate).

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:1954-2025, calma/CalmaWrite.c:2032-2075, calma/CalmaWrite.c:2089-2172, calma/CalmaWrite.c:2187-2257, calma/CalmaWrite.c:2273-2340, calma/CalmaWrite.c:2394-2709)

#### Scenario: L-shape merged into one polygon
- **GIVEN** the L-shape tiles A = (0,20)-(20,40) and B = (0,0)-(40,20) on layer 49/0, Pscale 10
- **WHEN** written with `merge yes`
- **THEN** exactly one BOUNDARY is written with XY length 60 and points `(200,400) (0,400) (0,0) (400,0) (400,200) (200,200) (200,400)`

#### Scenario: Isolated rectangle under merge
- **GIVEN** a single isolated tile (0,0)-(40,20), Pscale 10
- **WHEN** written with `merge yes`
- **THEN** the output is identical to the unmerged rectangle `(0,0) (400,0) (400,200) (0,200) (0,0)`

#### Scenario: Point limit splits a long comb (verify against Magic)
- **GIVEN** a connected comb shape whose tile decomposition would need more than 199 polygon points
- **WHEN** written with `merge yes`
- **THEN** it is written as two or more BOUNDARY elements, each with at most 200 XY points, sharing edges

### Requirement: Label layers

For a mask layer declared with `labellayer`, each solid non-split tile of its mask plane SHALL be written as a TEXT element, independent of the `labels` setting: TEXT, LAYER `L`, TEXTTYPE `D`, XY with one point `((x0+x1)*Pscale/2, (y0+y1)*Pscale/2)` using C integer division (truncation toward zero), STRING holding the mask layer name, ENDEL. Split tiles are ignored. No PRESENTATION, STRANS, or MAG records are written.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:1514-1534, calma/CalmaWrite.c:3000-3048, cif/CIFtech.c:879-880)

#### Scenario: Label layer tile
- **GIVEN** `labellayer BND` with `calma 63 5`, and a tile (0,0)-(15,10), Pscale 1
- **WHEN** the cell is written
- **THEN** a TEXT element is written with LAYER 63, TEXTTYPE 5, XY `(7,5)`, and STRING `BND`

### Requirement: Label TEXT records

When `labels` is yes, each cell label SHALL be written as follows. A *port label* is a label with any port-direction flag set. All other labels are *plain labels*.

- Plain labels are written in label-list order. The text layer is the mask layer that the output style assigns to labels of the label's type (`labels <type>` or `labels <type> noport`). The rectangle layer is the same layer. A label whose type has no text layer is not written.
- Port labels are written after all plain labels, sorted ascending by port index. The text layer is the style's port-text layer for the type, and the rectangle layer is the style's port layer for the type (`labels <type> port`). A port label is written only if its type has a port layer. A type that has only `labels <type>` with no `port` declaration has a port-text layer but no port layer: with `magic-compat` ON (the default) its port labels are not written, as in Magic; with it OFF they are written with the port-text layer as both text and rectangle layer; types with neither layer are skipped. In both settings Hefesto warns once per type and write about port labels written without a port layer or skipped (Q-MASK-206, resolution: compat). Labels with equal port index are written in reverse label-list order (a stable sort of the reversed label list), in both settings and independent of the host C library, matching the Linux/glibc reference Magic (Q-MASK-205, resolution: replicate).

For a written label with text layer `Lt/Dt`, the label is skipped if `Lt` is outside 0–255. Otherwise the element is TEXT, LAYER `Lt`, TEXTTYPE `Dt`, then, only if the label has an explicit font (font index ≥ 0):

- PRESENTATION = `((font & 3) << 4) | j`, where `j` is given by the label's justification (the side of the anchor the text extends to): center 0x5, N 0x9, NE 0x8, E 0x4, SE 0x0, S 0x1, SW 0x2, W 0x6, NW 0xA.
- STRANS = 0.
- MAG = R8 of `((double)size / 800) * (double)S / (double)E`, evaluated left to right in binary64, where `size` is the label size in eighths of an internal unit.
- ANGLE = R8 of the rotation in degrees, only if the rotation is non-zero.

Then XY with one point `((xbot+xtop)*Wscale/2, (ybot+ytop)*Wscale/2)` from the label rectangle, using C integer division; STRING with the label text; ENDEL. The text-magnification read setting (`gds magscale`) does not affect writing with `magic-compat` ON (the default), as in Magic; with it OFF the MAG value is divided by `magscale`, so a read and a write with the same `magscale` keep the text size (Q-MASK-221, resolution: compat). The settings agree for the default `magscale` 1.0.

After the TEXT element, if the rectangle layer exists with GDS layer `Lr` in 0–255, that layer has no generation operators (a labels-only layer), and the label rectangle has positive width and height, a BOUNDARY is also written on `Lr/Dr` with the rectangle scaled by `Wscale`, in the 5-point order of "Unmerged boundary output".

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:1536-1606, calma/CalmaWrite.c:2829-2984, calma/CalmaWrite.c:1387-1400, cif/CIFtech.c:903-958)

#### Scenario: Default-font point label
- **GIVEN** Wscale 100, a plain label `VDD` of type metal1 with no font at point (5,5), and metal1 labels mapped to layer 49/0
- **WHEN** the cell is written
- **THEN** the element is `00 04 0C 00`, `00 06 0D 02 00 31`, `00 06 16 02 00 00`, `00 0C 10 03 00 00 01 F4 00 00 01 F4`, `00 08 19 06 56 44 44 00`, `00 04 11 00`

#### Scenario: Rendered label with font
- **GIVEN** `scalefactor 10`, expander 1, a label with font 0, size 80, justification N, rotation 90 at point (0,0)
- **WHEN** it is written
- **THEN** PRESENTATION is `00 06 17 01 00 09`, STRANS is `00 06 1A 01 00 00`, MAG is `00 0C 1B 05 41 10 00 00 00 00 00 00`, and ANGLE is `00 0C 1C 05 42 5A 00 00 00 00 00 00`

#### Scenario: Port labels sorted by index
- **GIVEN** the style declares `labels metal1 port` on layer M1PIN, and a cell with port labels `B` (index 2) and `A` (index 1) and a plain label `n1`
- **WHEN** the cell is written
- **THEN** the TEXT elements appear in the order `n1`, `A`, `B`

#### Scenario: Port label dropped without port layer
- **GIVEN** the style declares only `labels metal1` (no `port` variant) and a cell with port label `A` on metal1
- **WHEN** the cell is written
- **THEN** no TEXT element for `A` is written

#### Scenario: Label rectangle on a labels-only layer
- **GIVEN** a layer `M1LBL` with no operators, `labels metal1`, `calma 49 10`, Wscale 100, and a plain label rectangle (0,0)-(2,1)
- **WHEN** the cell is written
- **THEN** a TEXT at `(100,50)` is followed by a BOUNDARY on 49/10 with XY `(0,0) (200,0) (200,100) (0,100) (0,0)`

#### Scenario: Labels disabled
- **GIVEN** `gds labels no`
- **WHEN** a cell with labels and a `labellayer` is written
- **THEN** no label TEXT elements are written, but `labellayer` TEXT elements are still written

### Requirement: Cell reference elements

Each child use SHALL be written as one reference element. Let the use transform be `x' = a·x + b·y + c`, `y' = d·x + e·y + f`, with `a,b,d,e ∈ {−1,0,1}` and no scaling. The reflection flag and angle are:

- mirror := `a != e` or (`a == 0` and `b == d`). STRANS = `0x8000` if mirror, else `0x0000`.
- angle: start with 180 if `a == −1`, else none. If mirror and `a == 0`: 90 if `b == 1`, else 270. If not mirror and `a == 0`: 90 if `b == −1`, else 270.

| Transform (a b d e) | STRANS | ANGLE |
|---|---|---|
| 1 0 0 1 (identity) | 0x0000 | — |
| 0 −1 1 0 | 0x0000 | 90 |
| −1 0 0 −1 | 0x0000 | 180 |
| 0 1 −1 0 | 0x0000 | 270 |
| 1 0 0 −1 | 0x8000 | — |
| −1 0 0 1 | 0x8000 | 180 |
| 0 1 1 0 | 0x8000 | 90 |
| 0 −1 −1 0 | 0x8000 | 270 |

ANGLE records hold the literal bytes `42 5A 00…` (90), `42 B4 00…` (180), `43 10 E0 00…` (270). Let the array index ranges be `xlo..xhi` and `ylo..yhi`, with `nx = |xhi − xlo| + 1`, `ny = |yhi − ylo| + 1`, and pitches `xsep`, `ysep` (internal units).

When `arrays` is no, the use is an AREF if `nx > 1` or `ny > 1`, otherwise an SREF:

1. SREF or AREF; SNAME (Requirement "Structure naming"); STRANS; ANGLE if any.
2. AREF only: COLROW with `cols = nx`, `rows = ny`.
3. XY: SREF has 1 point `(c·Wscale, f·Wscale)`. AREF has 3 points (length 28): origin `(c·Wscale, f·Wscale)`, column point `T(xsep·nx, 0)·Wscale`, row point `T(0, ysep·ny)·Wscale`, where `T` is the full use transform including translation.
4. PROPATTR 61 with PROPVALUE holding the use's instance name, always written.
5. If `xlo != 0` or `ylo != 0`: PROPATTR 99 with PROPVALUE `"%d_%d_%d_%d"` of `xlo, xhi, ylo, yhi`.
6. ENDEL.

MAG is never written for references.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:1662-1863, calma/calmaInt.h:102-108, utils/geometry.c:95-107)

#### Scenario: Rotated SREF bytes
- **GIVEN** Wscale 100 and a use `sub_0` of `sub` with transform a=0 b=1 d=−1 e=0 c=10 f=20
- **WHEN** it is written
- **THEN** the bytes are `00 04 0A 00`, `00 08 12 06 73 75 62 00`, `00 06 1A 01 00 00`, `00 0C 1C 05 43 10 E0 00 00 00 00 00`, `00 0C 10 03 00 00 03 E8 00 00 07 D0`, `00 06 2B 02 00 3D`, `00 0A 2C 06 73 75 62 5F 30 00`, `00 04 11 00`

#### Scenario: AREF geometry
- **GIVEN** Wscale 100 and an identity-transform use `a1` of `sub` at the origin, with x indices 0..2, y indices 0..1, xsep 10, and ysep 20
- **WHEN** it is written
- **THEN** COLROW is `00 08 13 02 00 03 00 02` and XY (length 28) is `(0,0) (3000,0) (0,4000)`
- **AND** no PROPATTR 99 is written

#### Scenario: Non-zero array origin index
- **GIVEN** the same array with x indices 1..3
- **WHEN** it is written
- **THEN** after the PROPATTR 61 pair, `00 06 2B 02 00 63` is followed by PROPVALUE `1_3_0_1`

#### Scenario: Mirrored reference
- **GIVEN** a use with transform a=1 b=0 d=0 e=−1
- **WHEN** it is written
- **THEN** STRANS is `00 06 1A 01 80 00` and no ANGLE record is written

### Requirement: Array flattening on output

When `arrays` is yes, each use SHALL be written as `nx·ny` SREF elements, for `i = 0..nx−1` (outer loop) and `j = 0..ny−1` (inner loop). Each element is SREF, SNAME, STRANS, ANGLE if any, XY `((c + a·xsep·i + b·ysep·j)·Wscale, (f + d·xsep·i + e·ysep·j)·Wscale)`, ENDEL. No PROPATTR records are written in this mode, not even for non-arrayed uses.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:1754-1791)

#### Scenario: Flattened 2×1 array
- **GIVEN** `gds arrays yes`, Wscale 100, an identity use at (0,0) with x indices 0..1 and xsep 10
- **WHEN** it is written
- **THEN** two SREFs are written with XY `(0,0)` and `(1000,0)`, and neither has PROPATTR records

### Requirement: Verbatim copy of read-only cells

A cell that has a `GDS_FILE` string property, a `GDS_START` property, and a `GDS_END` property, and is not an abstract view, is a *read-only cell*. Its structure SHALL be copied from the vendor GDS file instead of being generated:

1. Children are traversed first (Requirement "Structure ordering"). A child that is a magnified copy (it has a `GDS_MAG_SOURCE` property) is replaced by the cell named in that property, if that cell exists in memory. Otherwise an error is reported and the copy itself is traversed.
2. Open the file named by `GDS_FILE`: first through the cell search path, then with `.gz` appended, then (for relative names not starting with `/`, `~`, `$`) relative to the directory of the cell's own `.mag` file, with and without `.gz`.
3. Let `start = GDS_START` and `end = GDS_END` (byte offsets in the uncompressed stream). If an old-style `GDS_BEGIN` property exists, it is used as `start` and the copy is *old style*.
4. Sanity checks. Any failure reports an error and falls back to writing the cell from Hefesto's own data (Requirement "Structure body order"):
   - `end >= start + 4`.
   - New style: among the `min(start, 65535)` bytes before `start`, find the shortest even record length `r ≥ 6` such that the 4 bytes at `start − r` are `r>>8, r&0xFF, 0x06, 0x06` (a STRNAME header). Not found, or the bytes cannot be read, means failure. If the found name differs from the cell name, only a warning is given.
   - Old style: the 4 bytes at `start` must have type 0x05 and data type 0x02 (BGNSTR).
   - The `end − start` bytes must be readable, and the last 4 must be `00 04 07 00` (ENDSTR).
5. On success, new style writes BGNSTR (creation date per Requirement "Date records", current time) and STRNAME from the cell name (Requirement "Structure naming"), followed by bytes `[start, end)` verbatim. Old style writes only bytes `[start, end)`. Nothing is written before all checks pass. A short write after that point is a fatal failure for this cell.
6. After the copy, the cell is marked as vendor GDS for the rest of the session. That mark affects interaction generation of its parents (see `mask-hierarchical-generation`).

SNAME records inside copied bytes depend on the `magic-compat` setting (Q-MASK-216, resolution: compat): with it ON (the default) they are copied verbatim, as in Magic, and Hefesto warns when a referenced child's current output name differs from the SNAME; with it OFF the copied records are parsed and each SNAME is rewritten to the child's current output name, with record lengths adjusted (Hefesto keeps each vendor cell's original GDS name for this).

If the file cannot be opened: when the cell's first parent also has `GDS_FILE`, the cell is silently skipped and marked vendor GDS. Otherwise an error is reported, and the write fails unless `undefined` is allow, in which case the cell is skipped.

A cell with `GDS_FILE` but no `GDS_START` is a *reference-only* cell. Its children are traversed, but no structure is written for it.

A cell named `polygon<integer>` (as created by `gds polygon subcells`) is not written when its first parent has `GDS_FILE`.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:851-905, calma/CalmaWrite.c:948-970, calma/CalmaWrite.c:1021-1056, calma/CalmaWrite.c:1085-1335, calma/CalmaWrite.c:1337-1346)

#### Scenario: New-style vendor copy
- **GIVEN** cell `sc_inv` with `GDS_FILE lib.gds`, `GDS_START 1000`, `GDS_END 1500`, where bytes 990–999 of `lib.gds` are the STRNAME record `00 0A 06 06 73 63 5F 69 6E 76` and bytes 1496–1499 are `00 04 07 00`
- **WHEN** a hierarchy using `sc_inv` is written
- **THEN** the output contains a fresh BGNSTR, STRNAME `sc_inv`, then exactly the 500 bytes from offsets 1000–1499 of `lib.gds`

#### Scenario: Bad end marker falls back
- **GIVEN** a read-only cell whose `GDS_END` points 2 bytes past an ENDSTR record
- **WHEN** GDS is written
- **THEN** an error is reported and the cell's structure is generated from its layout instead

#### Scenario: Missing vendor file
- **GIVEN** a read-only cell whose `GDS_FILE` cannot be found, a parent without `GDS_FILE`, and `undefined` disallow
- **WHEN** GDS is written
- **THEN** the write reports failure

### Requirement: Full dump of a vendor library

A read-only cell whose view is abstract (`LEFview` property set) or that has `GDS_START` but no `GDS_END` SHALL be written by dumping the entire vendor GDS library once per write (keyed by the resolved file name). Its children are not traversed. The dump:

1. Reads and discards HEADER, BGNLIB, LIBNAME and optional header records. UNITS must be compatible: meters per database unit must round to 1e-9 (nanometer style) or 1e-10 (angstrom style), whatever the current `gds readonly` setting, in both `magic-compat` settings (Q-MASK-212, resolution: fix; Magic checks only when `gds readonly` is yes and otherwise copies a library at the wrong scale). On a mismatch the dump is aborted with Magic's error, the library is recorded so the error is reported once per library per write, and the write's status follows the `undefined` rule.
2. Chooses a library prefix: two characters (an uppercase letter, then an uppercase letter or digit) followed by `_`, unique within this write. The source depends on the `magic-compat` setting (Q-MASK-211, resolution: compat): with it ON (the default) the characters are drawn from Hefesto's own implementation of the glibc `random()` algorithm (TYPE_3 additive feedback, default seed 1), one generator per project interpreter shared with the `random` command and reseeded by `random seed`, so the sequence does not depend on the host C library; with it OFF they are derived from a hash of the resolved library file name, with collisions resolved deterministically, so the prefix does not depend on session history. If the cell's `LEFview` property is the string `no_prefix`, the prefix is empty.
3. For each structure in the file: writes a new BGNSTR (creation date per Requirement "Date records", using the triggering vendor cell, and the current time), then STRNAME:
   - the original name if it equals the triggering cell's name;
   - the original name if a database cell of that name is an abstract read-only view whose `GDS_FILE` (after the path substitution of `gds-read`, including its `~` rule, Q-MASK-128) names this file;
   - otherwise `prefix + name`.
   A structure defined twice reports an error and reuses the first name.
4. Copies every following record verbatim up to the next BGNSTR or ENDLIB, except SNAME, which is replaced by the name assigned to that structure. A reference to a not-yet-defined structure becomes `prefix + name`, or `prefix + name + "[[0]]"` if a database cell with the prefixed name exists. Such forward references are remembered. Their final naming depends on the `magic-compat` setting (Q-MASK-214, resolution: compat): with it ON (the default) Magic's renaming is reproduced exactly, including a structure appended at the end of the write under its unprefixed name, and Hefesto warns that the SREF to the recorded name is left dangling; with it OFF a structure appended at the end of the write for an unresolved forward reference is written under the name recorded for that reference (`prefix + name` or the `[[0]]` form), so every SREF resolves.
5. Forward references still undefined at the end of the dump are recorded. A later dump of a library that defines them reuses the recorded prefix. At the end of the write, those that name database cells are written as described in Requirement "Structure ordering".
6. If the file starts with no structure, the next record's header is copied to the output with data type 2 (see Q-MASK-213).

The triggering cell is marked as vendor GDS.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:400-660, calma/CalmaWrite.c:679-805, calma/CalmaWrite.c:1112-1128, calma/CalmaWrite.c:345-366, calma/CalmaRead.c:824-880)

#### Scenario: Prefixed subcell names
- **GIVEN** an abstract read-only cell `pad` with `GDS_START` set, whose file defines `pad` and `via_x`, with `pad` referencing `via_x`, and a chosen prefix `QK_`
- **WHEN** GDS is written
- **THEN** the output contains structures `QK_via_x` and `pad`, and the SNAME in `pad` reads `QK_via_x`

#### Scenario: No-prefix option
- **GIVEN** the same cell with `LEFview` property `no_prefix`
- **WHEN** GDS is written
- **THEN** the structures are named `via_x` and `pad`

### Requirement: Read-only OASIS vendor cells

A read-only cell with `OASIS_FILE` and `OASIS_CELL` (capability `oasis-read` "Read-only OASIS vendor views") has only representational contents, the layers `cifinput` maps. Its structure SHALL NOT be generated from those contents. It SHALL instead be *transcoded*: the cell is decoded from its OASIS file and its elements are written as GDS elements, in both `magic-compat` settings (Q-OASIS-35, resolution: fix; Magic has no OASIS input, so there is no Magic output to match). Generating it from its layout would drop every vendor layer that `cifinput` does not map.

1. Children are traversed first, as in "Verbatim copy of read-only cells" rule 1, including the replacement of a magnified-copy child by its `GDS_MAG_SOURCE` cell.
2. The file named by `OASIS_FILE` is looked up as `GDS_FILE` is (rule 2 of "Verbatim copy of read-only cells"), and the cell named `OASIS_CELL` is decoded (capability `oasis-format`). If the file cannot be opened, or does not define that cell, the rules for an unopenable vendor GDS file apply, with `OASIS_FILE` in place of `GDS_FILE`.
3. If the file's unit does not give 1e-9 m per DB unit (1e-10 m for ångström output styles), an error is reported and the cell is generated from its layout, as for a failed sanity check of a GDS copy.
4. The structure is BGNSTR (creation date per Requirement "Date records", current time), STRNAME from the cell name (Requirement "Structure naming"), the transcoded elements in OASIS record order, and ENDSTR. Coordinates are copied in DB units, unscaled.
5. After the transcoding, the cell is marked as vendor GDS for the rest of the session, as in rule 6 of "Verbatim copy of read-only cells".

Each OASIS element, after its repetition is resolved, SHALL be written as follows:

| OASIS element | GDS element |
|---|---|
| RECTANGLE | BOUNDARY with the 4 corners and the closing point, on LAYER = layer and DATATYPE = datatype |
| POLYGON | BOUNDARY with the point list and the closing point |
| TRAPEZOID, CTRAPEZOID | BOUNDARY with the decoded vertices (capability `oasis-format`, including the merging of coincident vertices) and the closing point; an element dropped by the decoder is not written |
| CIRCLE | BOUNDARY with the vertices of `oasis-read` "CIRCLE elements" (current `circlepoints`); radius 0 writes nothing |
| PATH | PATH with WIDTH = 2 × half-width and the centerline as XY; PATHTYPE 0 when both extensions are 0, PATHTYPE 2 when both equal the half-width (half-width > 0), otherwise PATHTYPE 4 with BGNEXTN and ENDEXTN, as in `oasis-read` "PATH elements" |
| TEXT | TEXT with LAYER = textlayer, TEXTTYPE = texttype, the position as XY and the string mapped as in "String records and character mapping"; no PRESENTATION, STRANS, MAG or ANGLE |
| PLACEMENT | SREF (or AREF, below); STRANS 0x8000 when the flip bit is set; ANGLE = `AA × 90` (record 17) or the decoded angle (record 18), written only when nonzero; MAG written only when not 1. The SNAME is the output name ("Structure naming") of the database cell whose `OASIS_FILE` and `OASIS_CELL` name the referenced cell, or, when there is none, the referenced name mapped as in "String records and character mapping" |
| `S_GDS_PROPERTY` on a PLACEMENT | PROPATTR/PROPVALUE on the SREF or AREF, in PROPERTY order; on every SREF when the placement is expanded. Other properties are not written |
| XELEMENT, XGEOMETRY | not written, with one warning per cell (GDS has no counterpart) |

Repetitions: a PLACEMENT whose repetition `oasis-read` "Placement repetitions" reads as one Magic array (its rules 1 and 2) SHALL be written as one AREF with COLROW `(na, nb)` and the points `P0` = the placement position, `P1 = P0 + na·a` and `P2 = P0 + nb·b`, when both counts are at most 32767. Every other repetition, of a placement or of a shape, is expanded into one element per displacement, in displacement order.

An element that GDS cannot hold SHALL make the write fail rather than be written altered. This is the case for a layer, datatype, textlayer or texttype above 32767 (GDS LAYER, DATATYPE and TEXTTYPE are signed I2 values), for a BOUNDARY or PATH with more than 8191 XY points including any closing point (the XY record limit), and for a coordinate outside the signed 32-bit (I4) range. An error naming the cell and the element (layer/datatype, or the point count) is reported, nothing of the cell's structure is written, and the cell fails, so its parents fail too (Requirement "Structure ordering").

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: calma/CalmaWrite.c:972-1361)

#### Scenario: Vendor OASIS cell transcoded
- **GIVEN** a read-only cell `sc_inv` read from OASIS, whose source holds a RECTANGLE on 68/20 (a layer `cifinput` does not map), a CIRCLE of radius 50 and a TEXT `A`
- **WHEN** a hierarchy using `sc_inv` is written to GDS
- **THEN** structure `sc_inv` holds a 5-point BOUNDARY on 68/20, a BOUNDARY with the 64 circle vertices plus the closing point, and a TEXT `A`, in that order

#### Scenario: Regular placement repetition becomes an AREF
- **GIVEN** a read-only OASIS cell with a PLACEMENT of `A` at (0,0), no rotation, repetition type 1 with `nx = 3`, `ny = 2`, `sx = 100`, `sy = 200`
- **WHEN** it is transcoded to GDS
- **THEN** the output is one AREF with COLROW (3, 2) and XY (0,0), (300,0), (0,400)

#### Scenario: Largest GDS layer number
- **GIVEN** a read-only OASIS cell whose source holds a RECTANGLE on layer 32767, datatype 0
- **WHEN** a hierarchy using it is written to GDS
- **THEN** the BOUNDARY is written with the LAYER record `00 06 0D 02 7F FF`

#### Scenario: Layer number GDS cannot hold
- **GIVEN** a read-only OASIS cell `vendor` whose source holds a RECTANGLE on layer 32768, datatype 0
- **WHEN** a hierarchy using it is written to GDS
- **THEN** an error naming `vendor` and layer 32768 is reported, no structure is written for `vendor` or its parents, and the write reports failure

#### Scenario: Polygon above the XY limit
- **GIVEN** a read-only OASIS cell whose source holds a POLYGON of 8191 vertices, which needs 8192 XY points with the closing point
- **WHEN** it is written to GDS
- **THEN** an error naming the cell and the point count is reported and the write reports failure

### Requirement: Addendum mode

When `addendum` is yes, read-only cells that have a `GDS_START` property SHALL produce no structure and are not traversed. References to them are still written, so the output depends on the vendor library.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:1031-1035, commands/CmdCD.c:270-287)

#### Scenario: Addendum omits vendor structures
- **GIVEN** `gds addendum yes` and `top` using read-only cell `sc_inv`
- **WHEN** GDS is written
- **THEN** the output contains `top` with an SREF to `sc_inv` and no structure `sc_inv`

### Requirement: Abstract views

A cell that is an abstract view (`LEFview` property present) and not read-only SHALL fail the write unless `abstract` is allow. On failure the behaviour depends on the `magic-compat` setting (Q-MASK-226, resolution: compat). With it ON (the default), as in Magic: an error is reported, every cell not yet written is skipped, except the undefined-reference cells written at the end of the write, and the write returns failure; structures already written stay in the file, and ENDLIB is still appended. With it OFF the whole tree is checked for disallowed abstract views before the output file is opened, and on failure the error is reported, the file is not created or modified, and the write returns failure. With `abstract allow`, a warning is reported and the abstract view is written from its layout like any other cell.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:1037-1049)

#### Scenario: Abstract disallowed
- **GIVEN** `top` uses abstract cell `macro` (from LEF) and `abstract` is disallow
- **WHEN** GDS is written
- **THEN** the write fails, and neither `macro` nor `top` is written
- **AND** with `magic-compat` OFF no output file is created

#### Scenario: Abstract allowed
- **GIVEN** `gds abstract allow`
- **WHEN** GDS is written
- **THEN** `macro` is written from its abstract layout, followed by `top`

### Requirement: Compressed output

When the compression level `z` is 1–9, the entire stream SHALL be written as a single gzip member at deflate level `z`. When `z` is 0, the stream is written uncompressed with no gzip framing. The decompressed bytes MUST be identical for every `z`. The exact compressed byte sequence (gzip header OS byte, deflate block choices, the sync-flush marker before the end) depends on the compression library and is informative only: any conforming single-member gzip writer is accepted, and conformance compares decompressed bytes (Q-MASK-223, resolution: platform).

Compatibility: exact

(Magic ref: commands/CmdCD.c:1045-1068, calma/CalmaWrite.c:369-376, utils/path.c:832-935)

#### Scenario: Compressed and plain outputs agree
- **GIVEN** the same layout and a fixed date stamp
- **WHEN** GDS is written once with `compress 0` and once with `compress 9`
- **THEN** gunzip of the second file equals the first file, except for the modification-date fields

### Requirement: Write result and error propagation

The write operation SHALL return failure if the root traversal or any appended undefined-cell traversal returns failure (including a read-only OASIS cell holding an element GDS cannot express, Requirement "Read-only OASIS vendor cells"), or if a stream I/O error occurs. Even on failure, ENDLIB is appended and the file is closed. Problems flagged during mask generation are counted and reported as feedback, and do not cause failure.

Compatibility: behavioral

(Magic ref: calma/CalmaWrite.c:345-388)

#### Scenario: Failure still terminates the stream
- **GIVEN** a hierarchy containing a disallowed abstract cell
- **WHEN** GDS is written
- **THEN** the operation reports failure and the file ends with `00 04 04 00`

### Requirement: Magic write-time side effects

Magic's string encoder rewrites the source string in place with the mapped characters, including `0x00` for control characters. Label texts and use instance names in memory are therefore changed by a write (cell names are copied first and are not changed). Hefesto SHALL NOT mutate layout data during a write. This is recorded as a deviation candidate in Q-MASK-208. Magic also prints warnings for every changed name or character, and prints "Generating output for cell X" / "Copying output for cell X from F" per structure. Message wording is not normative.

Compatibility: informative

(Magic ref: calma/CalmaWrite.c:3218-3243, calma/CalmaWrite.c:1093, calma/CalmaWrite.c:1336)

#### Scenario: Label text not altered by write
- **GIVEN** a label `a<1>` and the strict table
- **WHEN** Hefesto writes GDS
- **THEN** the STRING record contains `a_1_` and the label text in the layout remains `a<1>`

### Requirement: Complete annotated example

Hefesto SHALL produce exactly the following stream for the reference case (time fields marked `NN`): output style `scalefactor 10` (expander 1, nanometer units), layer `M1` = `or metal1` with `calma 49 0`, root cell `top` containing only metal1 (0,0)-(4,2) and no labels, `gds datestamp 0`, `TZ=UTC`, and `merge no`.

```
00 06 00 02 00 03                                  HEADER  version 3
00 1C 01 02 0046 0001 0001 0000 0000 0000 NN×12    BGNLIB  created 1970-01-01, modified now
00 08 02 06 74 6F 70 00                            LIBNAME "top"
00 14 03 05 3E41 8937 4BC6 A7F0 3944 B82F A09B 5A54 UNITS  0.001, 1e-9
00 1C 05 02 0046 0001 0001 0000 0000 0000 NN×12    BGNSTR
00 08 06 06 74 6F 70 00                            STRNAME "top"
00 04 08 00                                        BOUNDARY
00 06 0D 02 00 31                                  LAYER 49
00 06 0E 02 00 00                                  DATATYPE 0
00 2C 10 03 (0,0) (400,0) (400,200) (0,200) (0,0)  XY, 5 points as I4 pairs
00 04 11 00                                        ENDEL
00 04 07 00                                        ENDSTR
00 04 04 00                                        ENDLIB
```

The total file size is 6+28+8+20+28+8+4+6+6+44+4+4+4 = 170 bytes.

Compatibility: exact

(Magic ref: calma/CalmaWrite.c:284-388, calma/CalmaWrite.c:1419-1610, calma/CalmaWrite.c:2723-2810, calma/CalmaWrite.c:3062-3103)

#### Scenario: Reference file size
- **GIVEN** the reference case above
- **WHEN** GDS is written
- **THEN** the file is 170 bytes long and matches the listing byte for byte, except for the 24 `NN` bytes
