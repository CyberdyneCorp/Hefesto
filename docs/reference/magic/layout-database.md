# Layout Database

## Purpose
Layout geometry is stored on corner-stitched tile planes (`tiles/`). Each cell definition (CellDef) has one plane per technology plane. Cell instances (CellUses) are kept on a binned "bplane" (`bplane/`), which allows overlapping elements and is faster than a tile plane for hierarchy; it was introduced in 8.3. The database also holds labels and ports, cell properties, timestamps, and an undo/redo log.

## Requirements

### Requirement: Corner-stitched tile planes
Each tile SHALL store its body (type), the stitches `lb`, `bl`, `tr` and `rt`, and its lower-left point. A tile's top and right coordinates SHALL be derived from its neighbors. Coordinates SHALL be bounded by `INFINITY = (1<<30)-4`. A user of `ti_client` SHALL restore it to `CLIENTDEFAULT` when finished. (src: tiles/tile.h:65-99,164-172)

#### Scenario: Painting splits and merges tiles
- **WHEN** a rectangle of a type is painted on a plane
- **THEN** the plane SHALL remain a maximal-horizontal-strip tiling, and every point SHALL be covered by exactly one tile

### Requirement: Tile type and plane limits
The system SHALL support:
- at most `TT_MAXTYPES = 256` tile types, of which 2 are reserved
- at most `MAXPLANES = 64` planes

Built-in types 0 to 8 SHALL be: space, checkpaint, checksubcell, error_p, error_s, error_ps, magnet, fence, rotate. Technology types SHALL start at 9.

Built-in planes 0 to 5 SHALL be: router, designRuleCheck, designRuleError, mhint, fhint, rhint. Technology planes SHALL start at 6. (src: database/database.h.in:46,85-137,191-200)

#### Scenario: Too many types
- **WHEN** a technology file declares more than 254 types
- **THEN** the loader SHALL report "Too many tile types (max=%d)" (src: database/DBtechtype.c:422-488)

### Requirement: Non-manhattan (split) tiles
A tile MAY be split along a diagonal. Such a tile SHALL encode the type of each half and the diagonal direction in its body bits (`TT_DIAGONAL`, `TT_SIDE`, `TT_DIRECTION`). (src: database/database.h.in:139-143, tiles/tile.h:239-254)

#### Scenario: Triangle paint
- **WHEN** a triangle is painted (the `tri` record in a .mag file, or the `splitpaint` command)
- **THEN** a split tile SHALL be created that stores the left type, the right type, and the direction (`/` or `\`)

### Requirement: Contacts span multiple planes
A contact type SHALL have an image on each plane of its residues. `DBPaint` SHALL paint every plane the type lives on and SHALL then resolve the contact images. Stacked contact types SHALL be generated automatically and named `<a>+<b>`. (src: database/databaseInt.h:113-167, database/DBtechtype.c:507-527, database/DBpaint2.c)

#### Scenario: Paint a via
- **WHEN** a via contact type is painted
- **THEN** its images SHALL appear on both connected metal planes

### Requirement: Paint and erase tables
For each plane, painting and erasing SHALL be governed by `DBPaintResultTbl[plane][paint][old]` and `DBEraseResultTbl`, which the technology's compose rules fill in.

Default behavior:
- Painting type t SHALL produce t on t's own plane, and SHALL leave the existing material on every other plane.
- Erasing t from t SHALL produce space.

(src: database/DBtpaint.c:38-247)

#### Scenario: Erase
- **GIVEN** metal1 is present
- **WHEN** metal1 is erased over the same area
- **THEN** the area SHALL become space on the metal1 plane

### Requirement: Cell definitions and uses
A CellDef SHALL hold:
- name and file path
- one tile plane per technology plane
- a bplane of CellUses
- labels
- a property table
- a timestamp
- flags, which SHALL include: `CDAVAILABLE`, `CDMODIFIED`, `CDNOTFOUND`, `CDINTERNAL`, `CDGETNEWSTAMP`, `CDFIXEDBBOX`, `CDNOEDIT`, `CDVENDORGDS`, `CDFIXEDSTAMP`, `CDDEREFERENCE`

A CellUse SHALL hold an instance id, a Manhattan transform, optional array parameters (xlo/xhi/ylo/yhi plus separations, applied before the transform), and a lock flag. (src: database/database.h.in:338-513)

#### Scenario: Default instance id
- **WHEN** a cell is placed without an id
- **THEN** the system SHALL generate `<cellname>_<n>`, and array elements SHALL be addressed as `id[x,y]` (src: database/DBcellname.c:383-450,2401-2450)

#### Scenario: Paint marks a cell modified
- **WHEN** any paint, label or instance change is made to a CellDef
- **THEN** `CDMODIFIED` and `CDGETNEWSTAMP` SHALL be set

### Requirement: Bounding box computation
A cell's bounding box SHALL include all paint except the DRC check plane, all subcells, and all label rectangles. An empty cell SHALL get the box (0,0)-(1,1). (src: database/DBcellbox.c:605-700)

#### Scenario: Empty cell
- **WHEN** a new cell has no content
- **THEN** its bounding box SHALL be (0,0)-(1,1)

### Requirement: Labels and ports
A label SHALL have:
- a layer type and rectangle
- a justification (center or one of 8 compass directions)
- an optional font, size, rotation and offset (in 1/8 database units)
- text

A label becomes a port when it is given a port index and direction bits. A port SHALL also carry a class (input, output, tristate, bidirectional, feedthrough), a use (signal, analog, power, ground, clock, reset, scan, tieoff), and a shape (abutment, ring, feedthrough). Labels MAY be `sticky`. (src: database/database.h.in:219-319)

#### Scenario: Non-sticky label follows material
- **GIVEN** a non-sticky label on metal1
- **WHEN** the metal1 under it is erased
- **THEN** the label SHALL be shrunk or moved, with the message "Moving label …", and sticky labels SHALL never be moved (src: database/DBlabel.c:649-745)

#### Scenario: Cell with ports is a subcircuit
- **WHEN** any label in a cell has port direction bits
- **THEN** the cell SHALL be treated as a subcircuit for extraction (src: database/DBlabel.c:71-79)

### Requirement: Cell properties
Each cell SHALL hold typed key/value properties. The supported types are string, integer, dimension (scaled with units), double, and plane.

Reserved names SHALL include:
- `GDS_FILE`, `GDS_START`, `GDS_END` (read-only GDS views)
- `FIXED_BBOX`, `OBS_BBOX`
- `LEFview`, `LEFclass`, `LEFsite`, `LEFsymmetry`, `LEFproperties`
- `MASKHINTS_<name>`
- `flatten`, `device`, `parameter`, `noextract`, `CIFhier`, `GDS_MAG_SOURCE`

Setting `FIXED_BBOX` SHALL set `CDFIXEDBBOX`. Setting `GDS_FILE` SHALL set `CDVENDORGDS`. A property put on a `CDNOEDIT` cell SHALL be ignored. (src: database/DBprop.c:37-123)

#### Scenario: Mark a read-only GDS view
- **WHEN** `property GDS_FILE /path/lib.gds` is set on a writable cell
- **THEN** the cell SHALL be flagged `CDVENDORGDS`

### Requirement: Timestamps
When a cell's timestamp is updated (`DBUpdateStamps`), a cell flagged `CDGETNEWSTAMP` SHALL be handled according to its flags (src: database/DBtimestmp.c:225-327):
- `CDNOEDIT`: the flag SHALL be cleared and the stamp kept.
- `CDFIXEDSTAMP`: the stamp SHALL be kept.
- `CDVENDORGDS`: the new stamp SHALL be a hash of the cell's GDS contents.
- otherwise: the new stamp SHALL be `time()`.

The parents of a restamped cell SHALL be marked `CDSTAMPSCHANGED`. A stamp mismatch found on read SHALL be queued; processing it re-reads the child and rechecks DRC in the parents.

#### Scenario: Read-only cell is not perpetually modified
- **GIVEN** a cell read from a read-only GDS view
- **WHEN** background DRC updates its error plane
- **THEN** the cell SHALL NOT be marked modified, and SHALL NOT request a new timestamp (src: drc/DRCcontin.c:761-771)

### Requirement: Undo and redo
Each command's database changes SHALL be grouped into one undo unit, delimited by `UndoNext`. The system SHALL keep up to 1000 commands (`MAXCOMMANDS`). Recording a new event after an undo SHALL discard the redo tail. Undo SHALL be disabled while a .mag file or GDS stream is read. (src: utils/undo.c:58-59,483-547,812-842; database/DBio.c:841)

#### Scenario: Undo then edit
- **GIVEN** two commands were undone
- **WHEN** a new paint command runs
- **THEN** the two undone commands SHALL no longer be redoable
