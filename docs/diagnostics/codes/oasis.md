# Diagnostic codes: oasis area

Codes `OASIS-<TOPIC>-<NNN>` emitted by `src/core/src/oasis/` (the OASIS codec, OASIS read with read-only vendor views and content-hash timestamps, OASIS write with repetition detection and transcoding, and the `oasis` command;
`openspec/changes/m6-lef-def-oasis/design.md` D1/D2). Conventions and the
renderings are described in [../codes.md](../codes.md).

The area's table is assembled by `src/core/src/oasis/codes.cpp` (owner:
INTEGRATION) from one fragment per workstream, each owned by that
workstream. The rows are sorted while compiling
(`src/core/src/diag/sorted_codes.hpp`):

| Fragment | Topics | Owner |
|---|---|---|
| `src/core/src/oasis/codec/codes.inc` | `OASIS-CODEC-*` | OASFMT |
| `src/core/src/oasis/read/codes.inc` | `OASIS-READ-*` | OASR |
| `src/core/src/oasis/write/codes.inc` | `OASIS-WRITE-*` | OASW |
| `src/core/src/oasis/command/codes.inc` | `OASIS-CMD-*` | OASCMD |

While M6 is in progress, an option or function the scaffold declared before
its workstream implemented it fails with the core code `CORE-NOTIMPL-006`
([../codes.md](../codes.md)).

## CODEC

Codes of the OASIS codec: primitives, records, modal state, repetitions, name tables, CBLOCK and validation (`src/core/src/oasis/codec/`; capability oasis-format; design D11). Owner: OASFMT.

The decoder (`oasis::Decoder`) returns an error that stops decoding from `next()`, and every later call returns it again. It keeps warnings, errors that drop one element, and the errors found once the whole file is read in `diagnostics()`. The text of each code is the oasis-format message, e.g. "invalid record type 35".

- `OASIS-CODEC-001`: "not an OASIS file": the first 13 bytes are not `%SEMI-OASIS\r\n` (oasis-format "File structure and magic bytes" rule 1).
- `OASIS-CODEC-002`: "unexpected end of file": the input ends inside a record or before END (rule 3). `oasis::validate` also uses it when the last 256 bytes are not an END record.
- `OASIS-CODEC-003`: "data after END record" (rule 4). It does not stop decoding.
- `OASIS-CODEC-004`: "invalid record type <n>", including a first record that is not START and a second START.
- `OASIS-CODEC-005`: "integer overflow": an integer with a set bit beyond bit 63, a field the format declares 32-bit above 2^32 − 1, or a repetition dimension n' + 2 past 2^64 − 1.
- `OASIS-CODEC-006`: "division by zero in real" or "invalid real type <t>".
- `OASIS-CODEC-007`: "illegal character in a-string/n-string" (also an empty n-string). The bytes are used as they are.
- `OASIS-CODEC-008`: "coordinate overflow": a delta component outside the signed 32-bit range, a repetition step outside it, or a position, length, accumulated vertex or repetition offset that does not fit in 64 bits. The element is dropped.
- `OASIS-CODEC-009`: "invalid point list": count 0 drops the element. A type above 5 stops decoding, since the record's length is then unknown.
- `OASIS-CODEC-010`: "invalid repetition type <r>" or "zero repetition grid".
- `OASIS-CODEC-011`: "invalid interval type <k>".
- `OASIS-CODEC-012`: an interval with a > b in a LAYERNAME record. The interval is empty.
- `OASIS-CODEC-013`: "unsupported OASIS version <v>" or "invalid unit" in START. The file is not read.
- `OASIS-CODEC-014`: "END record length <n>" (not 256 bytes) or "unknown validation scheme <s>" (validation skipped).
- `OASIS-CODEC-015`: "OASIS validation failed (expected <stored>, computed <computed>)". The content is still used (Q-OASIS-2).
- `OASIS-CODEC-016`: "implicit and explicit <TABLE> records mixed" or "duplicate <TABLE> refnum <n>".
- `OASIS-CODEC-017`: "duplicate cell name <s>". The later refnum is an alias.
- `OASIS-CODEC-018`: "undefined <TABLE> refnum <n>", reported at END for every refnum used but never defined.
- `OASIS-CODEC-019`: "cell <name> defined twice", reported at END. oasis-read decides what happens to the cell.
- `OASIS-CODEC-020`: "modal variable <name> undefined" (oasis-format "Modal variables and reset rules"), also for a repetition of type 0 with no modal repetition.
- `OASIS-CODEC-021`: an element that is invalid but readable is dropped: a magnification of 0 or less or a non-finite one, a non-finite angle, a polygon with fewer than 3 vertices, a path with fewer than 2, or a CTRAPEZOID type above 25.
- `OASIS-CODEC-022`: a warning about an irregular record: TEXT with info bit 7, RECTANGLE with S and H (the height is ignored), a type 0/1 polygon list with an odd count, or a trapezoid with fewer than 3 distinct vertices (dropped).
- `OASIS-CODEC-023`: CBLOCK errors, which stop decoding: "unsupported CBLOCK compression type <n>", "CBLOCK size mismatch", damaged DEFLATE data, an uncompressed size above the inflate cap (256 times the compressed size, and at least `DecoderOptions::inflate_limit`, 1 GiB by default; design D11, security review M6-S4), and a START, END or CBLOCK record inside a CBLOCK.
- `OASIS-CODEC-024`: "strict mode violation: <detail>", reported at END, where the table offsets of a strict file are (Q-OASIS-7). The decoded content is unchanged.
- `OASIS-CODEC-025`: "unknown standard property <name>". The property is kept as an ordinary one.
- `OASIS-CODEC-026`: the encoder cannot write an element or property, for example a value out of range, a repetition or point list its type cannot represent, an illegal name, a name given only by reference, or a call out of order.
- `OASIS-CODEC-027`: CBLOCK compression failed, or the level is not 1 to 9.
- `OASIS-CODEC-028`: "invalid property value type <n>" (above 15).

## READ

Codes of the OASIS reader, read-only vendor views and content-hash timestamps (`src/core/src/oasis/read/`; capability oasis-read; design D12). Owner: OASR.

`oasis read` turns every OASIS element instance into its GDS-equivalent element and reads the result with the GDS reader, so most read messages are `GDS-READ-*` codes (codes/gds.md) with the same text as for `gds read`. Their byte position is the position of the OASIS record in the decompressed stream, or `<CBLOCK offset>+<offset inside its data>` for a record inside a CBLOCK. The decoder's own messages are `OASIS-CODEC-*` codes.

- `OASIS-READ-001`: "Nothing in "cifinput" section of tech file." The technology has no input style; nothing is read.
- `OASIS-READ-002`: "Cannot open <path> to read OASIS input.", followed by the file system's error.
- `OASIS-READ-003`: a fatal decoding error (an `OASIS-CODEC-*` text, such as "unexpected end of file"), prefixed with the cell and byte position. The cells complete before it are read; when there is none, nothing is read.
- `OASIS-READ-004`: "coordinate overflow; element instance dropped": a coordinate, path width or extension, or a placement or array point of an element instance is outside 32 bits. Reported once per element.
- `OASIS-READ-005`: "XGEOMETRY not supported: skipping." (or XELEMENT), once per kind and cell; "XNAME not supported: skipping." once per file.
- `OASIS-READ-006`: an element that does not fit its GDS-equivalent record: a polygon, trapezoid, circle or path with more than 8191 points is dropped; a text string or cell name above 65530 bytes is cut.
- `OASIS-READ-007`: a resource cap of the read (design D21): more than 2^28 element instances and uses, or a GDS-equivalent stream over 2 GiB. The rest of the file is not read.

## WRITE

Codes of the OASIS writer, repetition detection and the transcoders (`src/core/src/oasis/write/`; capability oasis-write; design D13). The codes of transcoding read-only OASIS cells into GDS are `GDS-WRITE-*` codes (codes/gds.md). Owner: OASW.

`oasis write` translates the element stream that `gds write` produces for the same layout, so every message of the prepare phase, the generation, the labels, the references and the vendor copies is the `GDS-WRITE-*` code `gds write` reports, with the same text.

- `OASIS-WRITE-001`: "Cannot open <path> to write OASIS output": the temporary file next to the target cannot be written or renamed onto it. The target is untouched.
- `OASIS-WRITE-002`: "<n> font label(s) written to OASIS without font, size, rotation or justification": once per write when at least one label with a font was written (OASIS TEXT has no presentation, Q-OASIS-32). Native only.
- `OASIS-WRITE-003`: "cell name "<name>" is already used in this OASIS file; the cell is written as "XXXXX<n>"" (Q-OASIS-61). A name that is empty or holds a byte outside 0x21-0x7E (a space under the permissive table) also becomes XXXXX<n>, without the message; `n` is the name's position in order of first need. Native only.
- `OASIS-WRITE-004`: "cell <cell> has a path of odd width <w> on <layer>/<datatype>; paths with odd width cannot be written to OASIS files": a read-only GDS cell's PATH whose width is odd (Q-OASIS-63). The path is not written and the write fails; the rest of the file is written.
- `OASIS-WRITE-005`: an element of a read-only GDS cell that OASIS gets changed or not at all: a BOUNDARY with fewer than 3 distinct vertices, a PATH with fewer than 2 points, a negative PATH width (written as its magnitude), a PATH type other than 0, 1, 2 and 4 (written with flush ends), an SREF, AREF, TEXT or BOX without points, and NODE elements (once per cell). Native only.
- `OASIS-WRITE-006`: "read-only cell <cell> carries XELEMENT or XGEOMETRY records whose payloads may refer to XNAME records of its source file, which are not written" (Q-OASIS-40). Native only.
- `OASIS-WRITE-007`: a read-only OASIS cell cannot be copied from its source file: the file cannot be decoded, or its unit is not the output unit (1000, or 10000 for `units angstroms`). The cell is generated from its layout. A file or cell that cannot be found follows gds-write's rule for an unopenable vendor file (`GDS-WRITE-041`).
- `OASIS-WRITE-008`: an output layer with a negative datatype gets no LAYERNAME record.
- `OASIS-WRITE-009`: "an element of cell <cell> cannot be written to OASIS: <reason>": the encoder refused an element (a value outside its ranges). The element is not written and the write fails.
- `OASIS-WRITE-010`: an AREF of a read-only GDS cell whose steps are not whole multiples (a type 10 repetition) with more than 2^24 instances; it is not written and the write fails (a proposed resource cap, design D21).
- `OASIS-WRITE-011`: "OASIS write cancelled."; no output file is left.
- `OASIS-WRITE-012`: the GDS element stream being translated is malformed (a read-only GDS cell's bytes end inside an element). The cells before it are written.

## CMD

Codes of the `oasis` command, its settings and format recognition on open (`src/core/src/oasis/command/`; capability oasis-command; design D15). Owner: OASCMD.

No codes yet.

## Table

The table below is generated from the fragments:

```sh
hefesto-cli --diagnostic-codes=oasis
```

The unit test `each area's code table matches its document and prefix`
fails when the two disagree.

<!-- BEGIN DIAGNOSTIC CODES (generated by `hefesto-cli --diagnostic-codes=oasis`) -->
| Code | Default severity | Meaning |
|---|---|---|
| `OASIS-CODEC-001` | error | Not an OASIS file: the first 13 bytes are not the OASIS magic bytes; nothing is read. |
| `OASIS-CODEC-002` | error | Unexpected end of file: the stream ends before the END record is complete, or no END record is in the last 256 bytes. |
| `OASIS-CODEC-003` | error | Data after the END record; the decoded content is still used. |
| `OASIS-CODEC-004` | error | Invalid record type, or a START record missing or repeated; decoding stops. |
| `OASIS-CODEC-005` | error | Integer overflow: an integer does not fit in 64 bits, or a 32-bit field exceeds 2^32 - 1; decoding stops. |
| `OASIS-CODEC-006` | error | Invalid real: an unknown real type or a division by zero; decoding stops. |
| `OASIS-CODEC-007` | warning | Illegal character in an a-string or n-string, or an empty n-string; the bytes are used as they are. |
| `OASIS-CODEC-008` | error | Coordinate overflow: a delta outside 32 bits or a position, offset or length outside 64 bits; the element is dropped. |
| `OASIS-CODEC-009` | error | Invalid point list: a count of 0 drops the element, a type above 5 stops decoding. |
| `OASIS-CODEC-010` | error | Invalid repetition: a type above 11 or a zero grid; decoding stops. |
| `OASIS-CODEC-011` | error | Invalid interval type; decoding stops. |
| `OASIS-CODEC-012` | warning | Empty interval (lower bound above the upper bound) in a LAYERNAME record. |
| `OASIS-CODEC-013` | error | Unsupported OASIS version or invalid unit in the START record; the file is not read. |
| `OASIS-CODEC-014` | warning | END record not 256 bytes long, or an unknown validation scheme (validation is skipped). |
| `OASIS-CODEC-015` | error | OASIS validation failed: the stored signature differs from the computed one; the content is still used. |
| `OASIS-CODEC-016` | error | Name table error: implicit and explicit records mixed, or a reference number defined twice; decoding stops. |
| `OASIS-CODEC-017` | warning | Duplicate cell name in CELLNAME records; the later reference number is an alias. |
| `OASIS-CODEC-018` | error | Undefined reference number once the whole file has been read. |
| `OASIS-CODEC-019` | error | A cell is defined twice (two CELL records for one name). |
| `OASIS-CODEC-020` | error | A record relies on an undefined modal variable; decoding stops. |
| `OASIS-CODEC-021` | error | Invalid element (magnification or angle, polygon or path vertex count, CTRAPEZOID type); the element is dropped. |
| `OASIS-CODEC-022` | warning | Irregular record: TEXT info bit 7, RECTANGLE with S and H, odd type 0/1 polygon count, or a degenerate trapezoid (dropped). |
| `OASIS-CODEC-023` | error | CBLOCK error: unsupported compression type, size mismatch, damaged data, inflate cap, or a START, END or CBLOCK record inside; decoding stops. |
| `OASIS-CODEC-024` | warning | Strict mode violation; the decoded content is unchanged. |
| `OASIS-CODEC-025` | warning | Unknown standard property; it is kept as an ordinary property. |
| `OASIS-CODEC-026` | error | The encoder cannot write a value: out of range, not representable in the requested form, unresolved, or a call out of order. |
| `OASIS-CODEC-027` | error | CBLOCK compression failed (zlib) or the compression level is not 1 to 9. |
| `OASIS-CODEC-028` | error | Invalid PROPERTY value type (above 15); decoding stops. |
| `OASIS-READ-001` | error | No cifinput style: nothing in the "cifinput" section of the technology file; nothing is read. |
| `OASIS-READ-002` | error | The OASIS file cannot be opened or read. |
| `OASIS-READ-003` | error | Decoding stopped (a fatal oasis-format error); only the cells complete before it are read. |
| `OASIS-READ-004` | error | Coordinate overflow: an element instance has a coordinate, width or displacement outside 32 bits; it is dropped. |
| `OASIS-READ-005` | warning | XNAME, XELEMENT or XGEOMETRY not supported: the record is skipped (reported once per kind and cell). |
| `OASIS-READ-006` | error | An element does not fit its GDS-equivalent record (a polygon or path above 8191 points, a string above 65530 bytes); it is dropped or cut. |
| `OASIS-READ-007` | error | A read resource cap was reached (the size of the GDS-equivalent stream); the rest of the file is not read. |
| `OASIS-WRITE-001` | error | The OASIS output file cannot be created or written; nothing is written. |
| `OASIS-WRITE-002` | warning | Font labels were written as OASIS TEXT without font, size, rotation or justification (once per write, with the count; Q-OASIS-32). |
| `OASIS-WRITE-003` | warning | A cell name is already used in the OASIS file; the later cell is written as XXXXX<n> (Q-OASIS-61). |
| `OASIS-WRITE-004` | error | A path of odd width in a read-only GDS cell cannot be written to OASIS; the write fails (Q-OASIS-63). |
| `OASIS-WRITE-005` | warning | An element of a read-only cell is dropped or changed for OASIS (degenerate boundary, single-point path, negative width, unknown path type, NODE). |
| `OASIS-WRITE-006` | warning | A read-only OASIS cell carries XELEMENT or XGEOMETRY records whose payloads may refer to XNAMEs that are not written (Q-OASIS-40). |
| `OASIS-WRITE-007` | error | A read-only OASIS cell cannot be copied from its source file (undecodable file, different unit); it is generated from its layout. |
| `OASIS-WRITE-008` | error | An output layer with a negative datatype gets no LAYERNAME record. |
| `OASIS-WRITE-009` | error | An element cannot be encoded in OASIS (a value out of range); it is not written and the write fails. |
| `OASIS-WRITE-010` | error | A read-only GDS array has inexact steps and more instances than an explicit repetition may list; it is not written and the write fails. |
| `OASIS-WRITE-011` | error | The OASIS write was cancelled; no output file is left. |
| `OASIS-WRITE-012` | error | The GDS element stream being translated is malformed or truncated; the cells before it are written. |
<!-- END DIAGNOSTIC CODES -->
