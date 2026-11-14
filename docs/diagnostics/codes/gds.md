# Diagnostic codes: gds area

Codes `GDS-<TOPIC>-<NNN>` emitted by `src/core/src/gds/` (GDSII stream read and write: the stream layer, the reader with read-only vendor views and content-hash timestamps, the writer and the `gds`/`calma` command;
`openspec/changes/m5-mask-gds-cif/design.md` D1/D2). Conventions and the
renderings are described in [../codes.md](../codes.md).

The area's table is assembled by `src/core/src/gds/codes.cpp` (owner:
INTEGRATION) from one fragment per workstream, each owned by that
workstream. The rows are sorted while compiling
(`src/core/src/diag/sorted_codes.hpp`):

| Fragment | Topics | Owner |
|---|---|---|
| `src/core/src/gds/stream/codes.inc` | `GDS-STREAM-*` | GDSR |
| `src/core/src/gds/read/codes.inc` | `GDS-READ-*` | GDSR |
| `src/core/src/gds/write/codes.inc` | `GDS-WRITE-*` | GDSW |
| `src/core/src/gds/command/codes.inc` | `GDS-CMD-*` | GDSW |

While M5 is in progress, an option or function the scaffold declared before
its workstream implemented it fails with the core code `CORE-NOTIMPL-005`
([../codes.md](../codes.md)).

## STREAM

Codes of the byte sources and sinks (plain, gzip, memory), the record reader and writer and the value encodings (`src/core/src/gds/stream/`; capabilities gds-read, gds-write; design D10). Owner: GDSR.

- `GDS-STREAM-001`: a record's length runs past the end of the stream. End of file inside a record header is not an error: it ends the records (gds-read "Record framing and primitive data types").
- `GDS-STREAM-002`: the gzip data of a `.gds.gz` (or any file starting with the gzip magic number) is damaged or ends inside a member. Bytes after the last complete member are ignored, as zlib's `gzread` does.
- `GDS-STREAM-003`: a seek (post-order reading, read-only offsets) asks for a byte past the end of the decompressed stream.
- `GDS-STREAM-004`: the writer refuses a record whose payload exceeds 65531 bytes, the most a two-byte length holds. Magic writes a wrapped length instead.
- `GDS-STREAM-005`: a compression level outside 1-9, or a zlib failure while compressing the output (gds-write "Compressed output").

## READ

Codes of the GDS reader, read-only vendor views and content-hash timestamps (`src/core/src/gds/read/`; capability gds-read; design D11). Owner: GDSR.

- `GDS-READ-001`: Magic's "Unexpected record type in input: Expected <record> record but got <record>." (calmaUnexpected) as one line from the library-header functions. gds::read prints it as Magic does: a counted GDS-READ-006 "Unexpected record type in input: " and the unprefixed line "    Expected <record> record but got <record>.". In the library header it aborts the read (gds-read "Library grammar and header processing").
- `GDS-READ-002`: Magic's "Unexpected EOF.": the stream ends, or a record is too short for its values, where the grammar expects a record.
- `GDS-READ-003`: `gds readonly yes` with UNITS other than 1 nm (or 1 Angstrom for an angstrom output style): "Incompatible scale factor of <mpd>, must be 1e-9." Nothing is read (gds-read "Read-only vendor views").
- `GDS-READ-004`: the informative "Library written using GDS-II Release <version>" line of the HEADER record.
- `GDS-READ-005`: Magic's "Unknown timestamp format;  setting timestamp to zero." for a BGNSTR whose data is not 24 bytes (gds-read "Structure timestamps").
- `GDS-READ-006`: a read error or warning that Magic reports through CalmaReadError, printed as "Error while reading cell "<cell>" (byte position <offset>): <text>" and counted for `gds warning limit`; under `gds warning redirect <file>` the text goes to the file. The same code carries Magic's CIFReadError ("CIF file read error: polygon with fewer than 4 points.") and its TxError lines (path direction reversal, mitre limit, a malformed array property) (gds-read "Read diagnostics").
- `GDS-READ-007`: `gds warning limit`: the 100th error prints "Error limit set:  Remaining errors will not be reported." (and the 100th CIF warning "Warning limit set: ...") and later ones are not printed.
- `GDS-READ-008`: the informative lines of a read: "Warning: Calma reading is not undoable!  I hope that's OK.", "Library name: <name>", `Reading "<name>".`, "Saving contents of cell <name>", "Cell <s> is magnified by <m>:  reading magnified copy <name>.", the cells already in memory, instance-name character changes and label adjustments.
- `GDS-READ-009`: `gds read` without a cifinput style: "Don't know how to read GDS-II: Nothing in "cifinput" section of tech file." Nothing is read.
- `GDS-READ-010`: the file cannot be opened ("Cannot open <path> to read GDS-II stream input."), followed by the file system's error.
- `GDS-READ-011`: a resource cap of the reader (design D21, R9): look-ahead or magnified-copy recursion deeper than 512, magnified copies nested deeper than 32 or beyond the read-scale range, an AREF of more than 2^20 elements copied from a flattened cell, or flatten copies that would take the read past 2^26 tile paints (copies times the flattened cell's solid tiles). The reference is placed without the look-ahead or magnification, or the copy is skipped.
- `GDS-READ-012`: native notes of magic-compat OFF behaviors with no Magic message: a round path end read as a half-width extension (Q-MASK-113), a negative path width read as its absolute value (Q-MASK-115), a malformed structure's geometry discarded (Q-MASK-110).

The read engine's grid and unit messages (`MASK-INPUT-010`..`012`) are printed as Magic's CIFReadWarning ("CIF file read warning: ...", the grid message once per session), and `MASK-INPUT-013` as a read error.

## WRITE

Codes of the GDS writer, verbatim copy and full dump (`src/core/src/gds/write/`; capability gds-write; design D12). Owner: GDSW. GDS-WRITE-001 to 019 belong to the writer core, 020 to 039 to the merged-polygon and label emitters, 040 on to verbatim copy, full dump, addendum and abstract views (`src/core/src/gds/write/codes.hpp`).

- `GDS-WRITE-004`: a value written to an integer record field does not fit it: an XY coordinate outside the signed 32-bit range after scaling (Q-MASK-222, a fix in both modes; Magic overflows in 32 bits). The message names the cell and the value, and the write fails.
- `GDS-WRITE-006`: Magic's warnings for a changed name or string: "Warning: character 'c' changed to 'd' in name <name>", "Warning: string in output unprintable; changed to 'XXXXX<n>'", "Warning: characters changed in string '<text>'; modified string is '<mapped>'", "Warning: Unprintable character changed to 'X' in label." and the `string-limit` truncation warning. The layout keeps the original text (Q-MASK-208).

## CMD

Codes of the `gds`/`calma` command and its settings (`src/core/src/gds/command/`; design D15). Owner: GDSW.

No codes yet.

## Table

The table below is generated from the fragments:

```sh
hefesto-cli --diagnostic-codes=gds
```

The unit test `each area's code table matches its document and prefix`
fails when the two disagree.

<!-- BEGIN DIAGNOSTIC CODES (generated by `hefesto-cli --diagnostic-codes=gds`) -->
| Code | Default severity | Meaning |
|---|---|---|
| `GDS-READ-001` | error | Unexpected record type in input: the GDS record found is not the one the grammar expects. |
| `GDS-READ-002` | error | Unexpected EOF: the GDS stream ends before the record the grammar expects. |
| `GDS-READ-003` | error | Incompatible scale factor for a read-only (`gds readonly yes`) read; nothing is read. |
| `GDS-READ-004` | info | `Library written using GDS-II Release <version>`. |
| `GDS-READ-005` | error | Unknown BGNSTR timestamp format; the cell timestamp is set to zero. |
| `GDS-READ-006` | error | GDS read error (Magic's CalmaReadError text, prefixed with the cell and byte position). |
| `GDS-READ-007` | warning | `gds warning limit`: the 100th read error; later ones are not reported. |
| `GDS-READ-008` | info | GDS read progress (`Reading "<name>".`, the library name, saved flatten cells, magnified copies, cells already in memory). |
| `GDS-READ-009` | error | No cifinput style: nothing in the "cifinput" section of the technology file; nothing is read. |
| `GDS-READ-010` | error | The GDS file cannot be opened or read. |
| `GDS-READ-011` | warning | A GDS read resource cap was reached (look-ahead or magnification depth, array copies, scale range); the element is read without it. |
| `GDS-READ-012` | warning | magic-compat OFF read behavior note (round path ends, negative path widths, discarded structure geometry). |
| `GDS-STREAM-001` | error | The GDS stream ends inside a record's data bytes. |
| `GDS-STREAM-002` | error | The gzip-compressed GDS stream is corrupt or cut off. |
| `GDS-STREAM-003` | error | A seek moved past the end of the (decompressed) GDS stream. |
| `GDS-STREAM-004` | error | A GDS record payload exceeds 65531 bytes and cannot be written. |
| `GDS-STREAM-005` | error | Gzip compression of the GDS output failed or the level is not 1 to 9. |
| `GDS-WRITE-004` | error | A GDS output value does not fit its integer record field (Q-MASK-222); the write fails. |
| `GDS-WRITE-006` | warning | A name or string was changed for GDS output (character table, unprintable character, string-limit truncation). |
<!-- END DIAGNOSTIC CODES -->
