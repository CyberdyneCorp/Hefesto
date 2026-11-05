# GDS and CIF Input/Output

## Purpose
Magic converts between its abstract layers and mask layers using the tech file's `cifoutput` and `cifinput` styles. These styles apply boolean and sizing operators to generate or interpret mask data. The `gds` (alias `calma`) command reads and writes GDSII stream format, optionally gzip-compressed. The `cif` command does the same for Caltech Intermediate Form. GDS files can be read as read-only "vendor" views: the original stream bytes are copied verbatim on output, and the cell timestamps are content hashes.

## Requirements

### Requirement: CIF output styles
A `cifoutput` style SHALL accept these declarations (src: cif/CIFtech.c:557-1611):
- `scalefactor v [nanometers|angstroms]`, `units angstroms`, `gridlimit n`
- `layer`, `templayer` and `labellayer` definitions
- `labels`
- `calma|gds <layer> <datatype>`, where both numbers are 0–255
- `options` with `calma-permissive-labels`, `set-minimum-grid`, `grow-euclidean`, `see-no-vendor`, `no-errors`, `string-limit`

Layers SHALL be built with these operators:
- boolean: `and`, `and-not`, `or`
- sizing: `grow`, `grow-min`, `grow-grid`, `shrink`, `close`
- bloating: `bloat-or`, `bloat-max`, `bloat-min`, `bloat-all`
- contact and slot generation: `squares`, `squares-grid`, `slots`
- boundaries and selection: `bbox`, `boundary`, `net`, `tagged`, `mask-hints`, `maxrect`
- shape cleanup: `orthogonal`, `not-square`, `bridge`, `bridge-lim`
- interaction filters: `overlapping`, `nonoverlapping`, `interacting`, `noninteracting`

#### Scenario: Contact cut generation
- **GIVEN** `squares 1 2 2` on a via layer
- **WHEN** GDS is written
- **THEN** each via area SHALL be filled with 2×2 cuts at 2 spacing and 1 border

#### Scenario: Bad grow
- **WHEN** a `grow 0` operator is parsed
- **THEN** the loader SHALL report "Grow/shrink distance must be greater than zero."

### Requirement: CIF input styles
A `cifinput` style SHALL map mask layers to magic types. It SHALL support:
- `layer <magictype> <ciflayers>`, `templayer`
- `calma <ciflayer> <layers> <datatypes>`, with comma lists and `*` wildcards
- `labels ... [text|sticky|port|cell]`
- `ignore`
- the operators `and`, `and-not`, `or`, `grow`, `grow-grid`, `shrink`, `copyup`, `boundary`, `not-square`, `mask-hints`, `tagged`

(src: cif/CIFrdtech.c:620-1058)

#### Scenario: Unknown GDS layer label
- **WHEN** a GDS text record is on an unmapped layer:purpose pair
- **THEN** the system SHALL warn "label ... is on unhandled layer:purpose pair L:D and will be discarded."

### Requirement: GDS read
`gds read <file>` SHALL try the extensions `""`, `.gds`, `.gds.gz`, `.gds2` and `.strm`, and SHALL read gzip input transparently. GDS reading is not undoable, and the read SHALL warn: "Warning: Calma reading is not undoable!  I hope that's OK."

`gds` SHALL accept these options (yes/no words `yes|no|true|false|on|off|1|0`):
- `flatten [N]`: limit 10
- `flatglob <pattern>`: default `*_CDNS_*`
- `ordering`, `noduplicates`, `unique`
- `polygon subcells`, `path subcells`, `savepaths`
- `maskhints`, `magscale`, `rescale`
- `warning default|none|align|limit|redirect`
- `datestamp`, `drccheck` (default yes), `readonly`, `library`

(src: commands/CmdCD.c:130-1082, calma/CalmaRead.c, calma/CalmaRdcl.c)

#### Scenario: Duplicate structure
- **WHEN** a structure name appears twice in one file
- **THEN** the second one SHALL be renamed `name_N`, with the message "Cell \"%s\" was already defined in this file."

#### Scenario: Out-of-order structures
- **WHEN** a structure is referenced before it is defined and `ordering` is off
- **THEN** the system SHALL rewind the input and suggest `gds ordering on`

### Requirement: Read-only GDS views
With `gds readonly yes`, each cell read SHALL be flagged `CDVENDORGDS` and SHALL get the properties `GDS_FILE`, `GDS_START` and `GDS_END`. The database unit SHALL be exactly 1e-9 (or 1e-10 with `units angstroms`); any other unit SHALL be rejected with "Cannot read this file in read-only mode.". (src: calma/CalmaRdcl.c:513-571, calma/CalmaRead.c:849-880)

#### Scenario: Write uses original bytes
- **GIVEN** a read-only GDS cell
- **WHEN** `gds write` is run
- **THEN** the cell's structure SHALL be copied verbatim from `GDS_FILE` between `GDS_START` and `GDS_END`

### Requirement: Content-hash timestamps for GDS
For read-only GDS cells, unless a nonzero `gds datestamp` is set, the timestamp SHALL be a hash computed as follows:
1. Take a 64-bit FNV-1a hash of the structure's records after STRNAME through ENDSTR. BGNSTR dates SHALL be excluded.
2. Mix in the stamps of referenced structures recursively.
3. Fold the result to a positive 31-bit value. A value of 0 SHALL become 1.

A nonzero `gds datestamp` SHALL set the timestamp to that value and mark it `CDFIXEDSTAMP`. (src: calma/CalmaRead.c:313-790, calma/CalmaRdcl.c:504-511)

#### Scenario: Reproducible .mag generation
- **GIVEN** the same unchanged GDS file
- **WHEN** it is read read-only in two sessions on different days and the cells are saved as .mag
- **THEN** the timestamps written SHALL be identical

### Requirement: GDS write
`gds write [file]` SHALL write the root cell (or the named file) using the current output style. It SHALL write a version-3 header with units of 1e-9 (or 1e-10).

These options SHALL control the output:
- `compress N`: range 0–9; above 0, writes `.gds.gz`
- `arrays yes`: write individual SREFs instead of AREFs
- `merge`
- `labels` (default yes)
- `lower` (default yes)
- `addendum`: skip read-only cells
- `abstract allow`
- `undefined allow`
- `library`: omit the top cell

Abstract (LEFview) cells SHALL be refused unless `abstract allow` is set. `gds contacts` is unimplemented: contacts SHALL always be written as individual cuts. (src: commands/CmdCD.c:351-410,1045-1081; calma/CalmaWrite.c:283-1927)

#### Scenario: No output style
- **WHEN** `gds write` runs with no output style
- **THEN** the system SHALL report "No CIF/GDS output style set!"

### Requirement: CIF read and write
`cif read <file>` and `cif write <file>` SHALL read and write `.cif` files without compression. The `cif` command SHALL also support:
- `ostyle`, `istyle`
- `see`, `coverage`, `statistics`
- `arealabels` (default off), `idcell` (default on)
- `prefix`, `rescale`, `limit` (1..100)
- `flat`, `hier`/`array` interaction output

(src: commands/CmdCD.c:1751-2271, cif/CIFwrite.c, cif/CIFrdutils.c)

#### Scenario: View generated layer
- **WHEN** `cif see <layer>` is run
- **THEN** the generated mask layer under the box SHALL be shown as feedback
