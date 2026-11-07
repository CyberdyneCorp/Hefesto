# Layout Properties

## Purpose

Specifies cell-definition properties: the typed key/value store attached to each cell definition, the value types and their coercions, how geometric property values follow scaling and origin moves, the property compat mode that controls how types are persisted, and the complete list of reserved property names recognised anywhere in Magic together with their side effects, so that PDK cells and generated layouts keep their meaning in Hefesto.

The `.mag` `<< properties >>` syntax belongs to capability `cell-file-format`, the `property` command to `cmd-labels-properties`, mask-hint semantics to `mask-hints`, and the consumers of individual reserved names to the capabilities cited in the table.

## Requirements

### Requirement: Typed property store

Each cell definition SHALL hold a map from property name (case-sensitive string) to one typed value, where the type is one of string, integer list, dimension list, double list (64-bit integers) or plane (a mask of rectangles).
Compatibility: behavioral

- string: arbitrary text (may contain spaces).
- integer: list of 32-bit signed integers, never scaled.
- dimension: list of 32-bit integers in internal units; consecutive pairs are treated as `(x, y)` points and scale with the database (see requirement "Geometric properties follow the geometry"); a trailing odd value is never scaled or moved.
- double: list of 64-bit signed integers (used for file offsets), never scaled.
- plane: a set of rectangles (only for names beginning `MASKHINTS_`).
- Putting a value replaces any previous value of that name; putting "no value" (an empty string through the command) removes the property.
- On a cell flagged no-edit (read-only), putting or removing a property is silently ignored.
- Removing all properties clears the fixed-bbox flag.
- Typed reads: a string read of a non-string property reports "not found"; a double read of a string property parses it as a decimal 64-bit integer.

(Magic ref: database/database.h.in:730-795, database/DBprop.c:58-125,187-271,338-363)

#### Scenario: Replacing a property
- **GIVEN** cell `a` with string property `note = "x"`
- **WHEN** integer property `note = 5` is put
- **THEN** `note` SHALL be an integer property with value `5`

#### Scenario: Read-only cell ignores properties
- **GIVEN** cell `ro` flagged no-edit
- **WHEN** property `foo = bar` is put
- **THEN** `ro` SHALL have no property `foo`

### Requirement: Reserved-name type coercion

When a property with a reserved name is set (by command or by reading a file), the system SHALL store it with the type required by that name regardless of the type requested, and SHALL fall back to a string when the value cannot be parsed.
Compatibility: exact

| Name | Forced type | Parse rule |
|---|---|---|
| `FIXED_BBOX`, `OBS_BBOX` | dimension, exactly 4 values | `xbot ybot xtop ytop`; on read, scaled by the file's scale factor |
| `GDS_START`, `GDS_BEGIN`, `GDS_END` | double | decimal 64-bit integer (prefix match on the first 9/9/7 characters when reading files) |
| `GDS_FILE` | string | |
| `MASKHINTS_<layer>` | plane | groups of 4 values `xbot ybot xtop ytop`, each group painted as a rectangle (see `mask-hints`) |

Other names keep the requested type (string by default).

(Magic ref: commands/CmdLQ.c:2545-2570, database/DBio.c:3300-3500)

#### Scenario: FIXED_BBOX stored as dimension
- **GIVEN** a `.mag` file with `string FIXED_BBOX 0 0 100 200`
- **WHEN** it is read
- **THEN** `FIXED_BBOX` SHALL be a dimension property `(0,0,100,200)` and the cell SHALL be flagged fixed-bbox

### Requirement: Geometric properties follow the geometry

When a cell's geometry is rescaled (internal-unit scale change) or translated (origin move), every dimension property and every plane property SHALL be transformed with it: dimension values pairwise as points, plane properties rectangle by rectangle; integer, double and string properties SHALL be left unchanged.
Compatibility: exact

(Magic ref: database/DBcellsrch.c:1822-1932,2083-2096,2283-2293)

#### Scenario: Scaling FIXED_BBOX
- **GIVEN** `FIXED_BBOX = (0,0,100,200)` and integer property `n = 7`
- **WHEN** the database is scaled by 2/1
- **THEN** `FIXED_BBOX` SHALL become `(0,0,200,400)` and `n` SHALL stay `7`

### Requirement: Property compat mode

The system SHALL maintain a global property compat flag, true by default, under which every property is written to `.mag` files with type keyword `string`; when compat is false each property SHALL be written with its own type keyword (`string`, `integer`, `dimension` for dimension and plane, `double`).
Compatibility: exact

- This flag is unrelated to Hefesto's `magic-compat` setting (capability `hefesto-magic-compat-mode`), which does not change it.
- Reading always converts reserved names to their forced types (requirement "Reserved-name type coercion") whatever keyword is present, so compat files round-trip.
- The value text written is the same in both modes (dimension and plane values divided by the output reducer; see capability `cell-file-format`).
- Properties are written in natural-sort order of their names (see capability `cell-file-format`, requirement "Natural ordering of names").
- The compat flag is queried and set through the `property compat` command (capability `cmd-labels-properties`); accepted values: `disable, no, false, off, 0` / `enable, yes, true, on, 1`.

(Magic ref: database/DBprop.c:37-44, database/DBio.c:4918-4942,5022-5100, commands/CmdLQ.c:2343-2541)

#### Scenario: Default compat output
- **GIVEN** compat true and integer property `n = 7`
- **WHEN** the cell is written
- **THEN** the properties section SHALL contain `string n 7`

#### Scenario: Typed output
- **GIVEN** compat false and integer property `n = 7`
- **WHEN** the cell is written
- **THEN** the properties section SHALL contain `integer n 7`

### Requirement: Reserved property names and side effects

The system SHALL recognise the following reserved property names with the stated effects; any other name SHALL be stored and persisted without side effects.
Compatibility: behavioral

| Name | Value | Effect (consumer capability) |
|---|---|---|
| `FIXED_BBOX` | 4 dims | sets/clears the fixed-bbox flag; used as the cell boundary by LEF/DEF writers, selection display and mask (CIF/GDS) generation; never changes the computed bounding box (`lef-write`, `def-write`, `mask-*`, `layout-hierarchy`) |
| `OBS_BBOX` | 4 dims | obstruction rectangle for LEF output (`lef-write`) |
| `GDS_FILE` | path | marks the cell as vendor (read-only) GDS; setting/clearing sets/clears the vendor-GDS flag; GDS output copies the structure verbatim from this file; renaming requires force and then removes it; timestamps are hashed from the GDS content (`gds-*`, `layout-hierarchy`) |
| `GDS_START` / `GDS_BEGIN` | offset | byte offset of the structure in `GDS_FILE` (`GDS_BEGIN` takes precedence when present); with `GDS_START` but no `GDS_END` the whole file is dumped and subcells are not written (`gds-*`) |
| `GDS_END` | offset | byte offset of the end of the structure (`gds-*`) |
| `GDS_MAG_SOURCE` | cell name | this cell is a magnified copy of the named structure; GDS output writes the original instead (`gds-*`) |
| `CIFhier` | any | GDS output re-enables hierarchical/array processing while writing this cell's subcells (`gds-*`) |
| `LEFview` | `TRUE` or `no_prefix` | abstract (LEF-derived) view: extraction writes `abstract`, ignores the substrate node and duplicate-label warnings; GDS output prefixes subcell names unless `no_prefix` (`lef-*`, `extract-*`, `gds-*`) |
| `LEFclass`, `LEFsite`, `LEFsymmetry`, `LEFproperties` | strings | round-tripped LEF macro CLASS, SITE, SYMMETRY and PROPERTY data (`lef-read`, `lef-write`) |
| `MASKHINTS_<layer>` | plane | mask hints for output layer `<layer>`; also DRC exception/exemption areas (`mask-hints`, `drc-*`) |
| `device` | `<class> <devname> ...` | cell is extracted as a device: local parasitics zeroed, the named device type added; value starting `primitive ` emits `parameters :<cell> <rest>` (`extract-*`) |
| `parameter` | string | extraction emits `parameters :<cell> <value>` for instances (`extract-*`) |
| `noextract` | any | cell is not extracted (only its substrate is prepared) (`extract-*`) |
| `flatten` | any | the "property-flatten" descend mode descends into this cell (`layout-hierarchy`) |
| `path`, `path_<n>` | dims | centre-line points of GDS paths recorded on read when path recording is enabled; a numeric suffix is added to keep names unique (`gds-read`) |

Script-level conventions without database side effects (Tcl device generators) use string properties `gencell`, `library`, `parameters`; Hefesto SHALL preserve them verbatim.

(Magic ref: database/DBprop.c:89-105, database/DBio.c:3315-3500, calma/CalmaWrite.c:528,738,957,1020-1150, calma/CalmaRdcl.c:884-1030, calma/CalmaRdpt.c:720-725, cif/CIFrdpt.c:240-300, extract/ExtBasic.c:288-360,1720-1745, extract/ExtCell.c:85-95,600-612, extract/ExtUnique.c:102-103, database/DBcellsubr.c:93-101, lef/lefRead.c:2199-2443, lef/lefWrite.c:331-346,1302-1384,1849-1863,2042-2150, drc/DRCtech.c:426-449, drc/DRCbasic.c:920-935, tcltk/toolkit.tcl:696-805)

#### Scenario: GDS_FILE toggles vendor status
- **GIVEN** a writable cell `pad`
- **WHEN** property `GDS_FILE = /lib/pads.gds` is set and later removed
- **THEN** `pad` SHALL be vendor-GDS while the property exists and not afterwards

#### Scenario: noextract
- **GIVEN** a cell with property `noextract` set to any value
- **WHEN** hierarchical extraction runs
- **THEN** no `.ext` file SHALL be produced for that cell

### Requirement: Property changes and modification/undo

Setting or removing a property through the user command SHALL mark the cell modified and needing a new timestamp; property changes SHALL NOT be recorded in the undo history.
Compatibility: behavioral

- Properties created as side effects of reading files (GDS, LEF, DEF, CIF) do not by themselves mark the cell modified beyond what the reader does.
- See Q-MODEL-14 for the absence of undo.

(Magic ref: commands/CmdLQ.c:2760-2768, database/DBprop.c:58-125)

#### Scenario: Property set is not undoable
- **GIVEN** a cell without property `x`
- **WHEN** `property x 1` is executed and then `undo` is executed
- **THEN** property `x` SHALL still exist
