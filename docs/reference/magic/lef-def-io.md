# LEF and DEF Input/Output

## Purpose
LEF and DEF exchange abstract views and placed-and-routed designs with digital place-and-route flows. One command implementation serves both `lef` and `def`; it is built unless `--disable-lef` is given. The tech file's `lef` section maps LEF layer names to magic types.

## Requirements

### Requirement: Tech lef section
The `lef` section SHALL accept lines of the form `<keyword> <magictype> <lefname>...`. The keywords SHALL be `obstruction`, `layer`, `route`, `routing`, `masterslice`, `cut`, `contact`, `overlap`, `bound` and `ignore`. A `layer` line SHALL map to `cut` when the magic type is a contact and to `route` otherwise.

An `obstruction` line MAY name a second via-obstruction type. Route width and spacing defaults SHALL come from the DRC rules. (src: lef/lefTech.c:216-504)

#### Scenario: Unknown keyword
- **WHEN** an unknown keyword is used
- **THEN** the loader SHALL report "Unknown LEF section keyword: %s.  Line ignored."

### Requirement: lef read
`lef read <file> [-import] [-annotate]` SHALL behave as follows (src: lef/lefRead.c:2070-3196):
- It SHALL read the LEF file, trying the name as given and then with `.lef` appended.
- Undo SHALL be disabled during the read, and the read SHALL print "This action cannot be undone.".
- Each MACRO and each SITE SHALL become a cell, unless a cell with that name already exists; an existing cell SHALL be annotated instead.

Each new macro cell SHALL get:
- the properties `LEFclass`, `LEFsymmetry`, `LEFsite`, `LEFproperties`, `FIXED_BBOX` (from SIZE/ORIGIN), and `LEFview=TRUE`
- sticky port labels for each PIN PORT rectangle, carrying direction, use and shape
- paint for OBS geometry, on the obstruction types

#### Scenario: Annotate existing layout
- **GIVEN** cell `inv` already exists with labels A and Y
- **WHEN** a LEF file with MACRO inv is read
- **THEN** the existing labels SHALL become ports with the LEF pin attributes, OBS SHALL NOT be painted, and `LEFview` SHALL NOT be set

#### Scenario: Error limit
- **WHEN** more than 100 errors occur
- **THEN** the system SHALL report "Further errors will not be reported." and a total at the end

### Requirement: lef write
`lef write [file]` SHALL accept the options `-tech`, `-hide [d]`, `-pinonly [d]`, `-toplayer` and `-nomaster`. It SHALL write the selected cell as a LEF 5.7 macro:
- `CLASS` from `LEFclass`, defaulting to BLOCK
- `FOREIGN`, `ORIGIN`, `SIZE` (from FIXED_BBOX or the bounding box), `SYMMETRY` and `SITE`
- PINs in port-index order, with DIRECTION, USE, SHAPE, ANTENNAGATEAREA and ANTENNADIFFAREA
- OBS geometry

`-hide` SHALL replace interior geometry with obstruction cover. `lef writeall [-notop] [-all]` SHALL write the children (or the whole hierarchy) to `<root>.lef`. (src: lef/lefCmd.c:275-445, lef/lefWrite.c:252-2399)

#### Scenario: Abstract with hidden interior
- **WHEN** `lef write -hide` is run
- **THEN** each routing layer SHALL be written as an obstruction covering the cell, with the pin areas plus spacing cut out

### Requirement: def read
`def read <file> [-labels] [-annotate] [-noblockage]` SHALL create the design cell (named by DESIGN) and SHALL read these sections:
- DIEAREA, as `FIXED_BBOX`
- COMPONENTS, as cell uses with orientations N/S/E/W/FN/FS/FE/FW, loading `.mag` masters if they are not already defined
- VIAS, both RECT-based and generated
- PINS, as sticky port labels
- NETS and SPECIALNETS, painting ROUTED/FIXED/COVER wires and vias
- NONDEFAULTRULES
- BLOCKAGES, unless `-noblockage` is given

(src: lef/defRead.c:1048-2762)

#### Scenario: Missing master
- **WHEN** a component's macro is unknown and no `.mag` file exists for it
- **THEN** the system SHALL report "Cell %s is not defined.  Maybe you have not read the corresponding LEF file?"

#### Scenario: Count mismatch
- **WHEN** the number of nets read differs from the declared count
- **THEN** the system SHALL report "Number of nets read (%d) does not match the number declared (%d)."

### Requirement: def write
`def write [file] [-units n] [-allspecial] [-anal]` SHALL write DEF 5.7 with `BUSBITCHARS "()"`, using units 1000 unless `-units` is given. The output SHALL contain:
- `DIEAREA`
- `VIAS`
- `COMPONENTS` (array elements written as `name[y,x]`)
- `PINS`
- `NONDEFAULTRULES`
- `SPECIALNETS` and `NETS`, which require an extracted `.ext` file
- `BLOCKAGES`

Hierarchical DEF (`def writeall`) is not supported. (src: lef/defWrite.c:104-3208, lef/lefCmd.c:278-279)

#### Scenario: No extraction
- **WHEN** `def write` runs on a cell that has no `.ext` file
- **THEN** the system SHALL warn "Circuit has no .ext file;  no nets written."

#### Scenario: Power nets as special nets
- **WHEN** a net name ends in `!`, or matches `$VDD`/`$GND`/`$globals`
- **THEN** the net SHALL be written under SPECIALNETS

### Requirement: Date stamps for LEF macros
`lef datestamp <value>` SHALL give every macro read afterwards that timestamp and the `CDFIXEDSTAMP` flag. `lef datestamp default` SHALL restore normal stamping. (src: lef/lefCmd.c:446-482, lef/lefRead.c:2160-2164)

#### Scenario: Fixed stamp
- **WHEN** `lef datestamp 1` is set and a LEF file is read
- **THEN** all macro cells SHALL have timestamp 1
