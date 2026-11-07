# Cell File Format (.mag)

## Purpose

Defines Magic's native text cell format (`.mag`) for both reading and writing, so that Hefesto can read every `.mag` file Magic reads and write files that are byte-identical to the files Magic writes for the same in-memory cell. It covers the line model, the header, coordinate scaling on read and scale reduction on write, paint (`rect`/`tri`) sections including stacked-contact residues, `use` blocks, labels and ports, elements, properties, the end marker, the record ordering and number formatting of the writer, read-only gzip input, and the `.tcl` command-script save variant.

Out of scope, owned elsewhere:
- How a cell name or a `use` path is resolved to a file, and how instance paths are computed for writing: capability `cell-library-resolution`.
- Locking, read-only cells, temporary-file writes, size verification, crash backups and archives: capability `cell-io-safety`.
- Meaning of timestamps and mismatch processing: capability `cell-timestamps`.
- In-memory semantics of paint, tile decomposition, labels, instances, transforms, arrays and properties: the layout capabilities (`layout-*`).
- Layer names, aliases, stacked contact residues and the `scalegrid` rescaling of technology values: the technology capabilities (`tech-*`).

Reference build configuration: Magic compiled with zlib, file locking and the Tcl wrapper (the defaults). Terms used below:
- **internal unit**: the database grid unit.
- **λ ratio**: the pair `(L0, L1)` such that one lambda equals `L1/L0` internal units. Initially `(1, 1)`.
- **file unit**: the coordinate unit of a given `.mag` file.

## Requirements

### Requirement: Overall file grammar

A `.mag` file SHALL follow the grammar below, and Hefesto SHALL both accept it on read and produce it on write.
Compatibility: exact

Each line is terminated by LF. On read, CR characters are discarded wherever they occur. Section keywords are case-sensitive unless stated otherwise.

```ebnf
mag_file      = header , { section } , end_line , { any_line } ;
header        = magic_line , [ tech_line , [ scale_line ] , [ stamp_line ] ] ;
magic_line    = "magic" , { any_char } , NL ;            (* only the first 5 chars are checked *)
tech_line     = "tech" , SP , name , NL ;
scale_line    = ( "magscale" , SP , int , SP , int | "maxlabscale" , { any_char } ) , NL ;
stamp_line    = "timestamp" , SP , int , NL ;
section       = paint_section | use_block | label_section
              | element_section | property_section ;
paint_section = "<<" , SP , layer_name , SP , ">>" , NL , { paint_rec | comment } ;
paint_rec     = rect_rec | tri_rec ;
rect_rec      = "rect" , SP , int , SP , int , SP , int , SP , int , NL ;
tri_rec       = "tri" , SP , int , SP , int , SP , int , SP , int , SP , corner , NL ;
corner        = "nw" | "sw" | "se" | "ne" ;
use_block     = use_line , [ array_line ] , [ stamp_line ] , transform_line , box_line ;
use_line      = "use" , SP , cell_name , [ WS , [ "*" ] , use_id , [ WS , path ] ] , NL ;
array_line    = "array" , 6 * ( SP , int ) , NL ;        (* xlo xhi xsep ylo yhi ysep *)
transform_line= "transform" , 6 * ( SP , int ) , NL ;    (* a b c d e f *)
box_line      = "box" , 4 * ( SP , int ) , NL ;
label_section = "<<" , SP , "labels" , SP , ">>" , NL , { label_rec | port_rec } ;
element_section = "<<" , SP , "elements" , SP , ">>" , NL , { element_rec } ;
property_section = "<<" , SP , "properties" , SP , ">>" , NL , { property_rec } ;
end_line      = "<<" , SP , "end" , SP , ">>" , NL ;
comment       = "#" , { any_char } , NL ;                (* allowed anywhere a line is read *)
int           = [ "-" ] , digit , { digit } ;
```

The label, port, element and property records are defined in their own requirements. Sections MAY appear in any order and MAY repeat on read. The writer always emits them in the order header, paint, uses, labels, elements, properties, end.

Annotated example, as written by Magic for a cell `top` in technology `scmos` with λ ratio (1,1):

```
magic                                   <- identification line
tech scmos                              <- technology name
timestamp 1700000000                    <- cell timestamp (see cell-timestamps)
<< polysilicon >>                       <- paint section, one per type, in type order
rect 0 0 2 10
<< metal1 >>
rect 0 2 6 8
tri 6 2 10 8 nw                         <- right triangle, right angle at the NW corner
use inv  inv_0 ../lib                   <- two spaces = unlocked; path only on first instance
timestamp 1690000000                    <- child's timestamp as last seen by the parent
transform 1 0 20 0 1 0                  <- child -> parent transform a b c d e f
box 0 0 8 12                            <- child's bounding box, child coordinates
use inv *inv_1                          <- '*' = locked instance
array 0 3 10 0 0 0                      <- xlo xhi xsep ylo yhi ysep
timestamp 1690000000
transform -1 0 60 0 1 0
box 0 0 8 12
<< labels >>
rlabel metal1 s 0 2 6 8 0 VDD           <- sticky rectangular label, position 0 (center)
port 1 nsew power bidirectional         <- port attributes of the preceding label
flabel metal1 0 0 0 0 3 FreeSans 64 0 8 0 out   <- rendered-font label
<< properties >>
string FIXED_BBOX 0 0 60 20             <- properties sorted by key, property compat mode
<< end >>
```

(Magic ref: database/DBio.c:437-517, database/DBio.c:4678-4958)

#### Scenario: Minimal valid file
- **GIVEN** the current technology is `scmos`
- **WHEN** a file containing exactly the lines `magic`, `tech scmos`, `timestamp 5` and `<< end >>` is read for cell `a`
- **THEN** cell `a` SHALL be read successfully with no paint, no instances, no labels and timestamp 5

#### Scenario: Writer section order
- **GIVEN** a cell with paint, two instances, one label and one property
- **WHEN** the cell is written
- **THEN** the output SHALL contain, in order, the header lines, all paint sections, all use blocks, `<< labels >>` with its records, `<< properties >>` with its records, and `<< end >>`

### Requirement: Line reading model

When reading, Hefesto SHALL split the input into lines with the following rules.
Compatibility: exact

- Every CR byte (0x0D) is discarded. A line ends at LF or at end of file.
- A line whose first character is `#` is a comment and is skipped entirely. This applies to every line read through the general line reader: header lines, section headers, use-block lines, label, element and property records. Inside a paint section, a line starting with `#` is also skipped.
- The general line buffer holds at most 2047 characters including the LF. A longer physical line is delivered as several consecutive logical lines. The only exceptions are the `use` line, which is read in full whatever its length, and property values (see "Properties section records").
- In Magic, a blank line (a line that contains only LF) is **not** skipped: in every position where a section header, record or use line is expected, it causes a format error. Hefesto SHALL skip blank lines on read, and SHALL never write them (see Q-CELL-1).
- End of file before the `<< end >>` line is a format error. Any content after `<< end >>` is ignored. Exception: in backup and archive files, the next line is read as a container record (see capability `cell-io-safety`).

(Magic ref: database/DBio.c:4187-4214, database/DBio.c:986-999, database/DBio.c:3117-3140)

#### Scenario: Comment lines are ignored
- **GIVEN** a paint section `<< metal1 >>` followed by `# note` and then `rect 0 0 4 4`
- **WHEN** the file is read
- **THEN** metal1 SHALL be painted at (0,0)-(4,4) and no error SHALL be reported

#### Scenario: CRLF files
- **GIVEN** a file whose lines end in CR LF
- **WHEN** it is read
- **THEN** it SHALL be read identically to the same file with LF line endings

#### Scenario: Blank line is skipped
- **GIVEN** a file with an empty line between two paint sections
- **WHEN** it is read
- **THEN** the empty line SHALL be skipped without an error
- **AND** the paint of both sections SHALL be in the cell

### Requirement: Header parsing

Hefesto SHALL parse the header as follows.
Compatibility: exact

1. **Line 1.** It MUST begin with the five characters `magic`. Otherwise the read fails with the message `First line in file must be "magic"; instead saw: <line>` and nothing is read.
2. **Line 2.**
   - If it begins with `<` or is empty, there is no technology, scale or timestamp line. The file is read in the current technology with timestamp 0, and line 2 is processed as the first body line.
   - Otherwise it MUST match `tech <name>`. The name is the first whitespace-delimited token, truncated to 49 characters. If the line does not match, the read fails with `Malformed "tech" line: <line>`.
   - Technology comparison and automatic technology loading are described in "Technology check on read" below.
3. **Optional scale line.** If the next line starts with `m`:
   - `magscale <n> <d>` sets the file scale ratio. If two integers cannot be parsed, the message `Expected two arguments to magscale;  ignoring` is printed and n = d = 1.
   - A line starting with `maxlabscale` is accepted and ignored, with the warning `Deprecated keyword "maxlabscale" in input file.`
   - Any other line starting with `m` produces `Expected magscale but got: <line>` and is skipped.
4. **Optional timestamp line.** If the next line starts with `t`, it is parsed as `timestamp <int>`. If the parse fails, the message `Expected timestamp but got: <line>` is printed, the timestamp stays 0, and the line is consumed.
5. If no timestamp line is present, the cell timestamp is 0. For the handling of a zero timestamp, see capability `cell-timestamps`.

The header lines MUST appear in this order. A `magscale` after `timestamp` is not recognized as a header line: it is processed as a body line, which produces a format error.

(Magic ref: database/DBio.c:565-784)

#### Scenario: Bad first line
- **WHEN** a file whose first line is `MAGIC` is read
- **THEN** the read SHALL fail with `First line in file must be "magic"; instead saw: MAGIC`
- **AND** the cell SHALL contain nothing from the file

#### Scenario: File without tech line
- **GIVEN** the current technology is `sky130A`
- **WHEN** a file whose second line is `<< metal1 >>` is read
- **THEN** the file SHALL be read in technology `sky130A` without any technology check, and the cell timestamp SHALL be 0

#### Scenario: Header with magscale and timestamp
- **WHEN** a file begins with `magic`, `tech scmos`, `magscale 1 2`, `timestamp 1234`
- **THEN** the file scale ratio SHALL be 1/2 and the cell timestamp SHALL be 1234

### Requirement: Technology check on read

When the `tech` line names a technology different from the current one, Hefesto SHALL behave as follows.
Compatibility: behavioral

1. Report `Cell <cell> has technology "<file tech>", but current technology is "<current tech>"`.
2. If the read was requested with "ignore technology" (as for `load -force`, expansion of subcells, and backup or archive reads), print `Will attempt to read cell anyway.` and continue. Layer names are then resolved against the current technology.
3. Otherwise, if no non-internal cell in memory contains any paint, attempt automatic technology discovery and loading (see capability `cell-library-resolution`).
4. If the current technology still differs afterwards, the read fails without reading any content. Magic prints the following advice:
   - `Use command "tech load" if you want to switch technologies, or use`
   - `"cellname delete <cell>" and "load <cell> -force" to force the cell to load as technology <current>`

(Magic ref: database/DBio.c:583-751, commands/CmdTZ.c:85-129)

#### Scenario: Mismatch with paint present
- **GIVEN** technology `scmos` is loaded and some loaded cell contains paint
- **WHEN** a cell whose file says `tech sky130A` is loaded without the force option
- **THEN** the read SHALL fail and the cell SHALL contain no content from that file

#### Scenario: Forced read in a different technology
- **GIVEN** technology `scmos` is loaded
- **WHEN** a file with `tech other` and a section `<< metal1 >>` is read with the ignore-technology option
- **THEN** a warning SHALL be printed, and metal1 paint SHALL be read using scmos's `metal1`

### Requirement: Coordinate scaling on read

Hefesto SHALL reconcile the file's scale with the database scale before reading the body, using integer arithmetic exactly as follows.
Compatibility: exact

Let the file's ratio be `(nf, df)` from `magscale` (default `(1, 1)`). Let the current λ ratio be `(L0, L1)`.

1. Compute `n = nf × L1` and `d = df × L0`. Reduce n/d by their greatest common factor (always positive).
2. If `d > 1` and the grid-limit test below allows refinement:
   - Rescale the entire database and the technology-dependent values by the factor `d`, as for the `scalegrid` command (see capability `tech-loading-and-scaling`, requirement "scalegrid command").
   - Set `L1 = L1 × d` and then `d = 1`.
   - At full verbosity, print `Input cell <cell> scales magic internal geometry by factor of <d>`.
3. **Grid-limit test.** Let the current output style have scale factor `s`, expander `e` and grid limit `g` (with `g = 0` treated as 1). Refinement is refused when `(s × n × 10) / (g × e × d)` is 0 in integer division, or when `(s × n × 10)` is not divisible by `(g × e × d)`. If there is no current output style, refinement is always allowed. This is the grid-limit check of capability `tech-loading-and-scaling` (requirement "Grid limit check") applied to the factor (n, d); the style values are defined by the `mask-*` capabilities.
4. Every coordinate-like value read afterwards is converted as `v' = v × n` (if n > 1), then `v' = v' / d` (if d > 1). The division truncates toward zero.
   - This applies to rect and tri coordinates, use-block translation and box values, array separations, label rectangles, flabel size and offsets, element coordinates, `dimension` property values, `FIXED_BBOX`/`OBS_BBOX` values given as strings, and `MASKHINTS_*` values.
   - It does not apply to array indices, transform matrix coefficients a, b, d, e, `integer` or `double` property values, or label position and rotation.
5. If after step 2, `n > 1`, Magic prints `Scaled magic input cell <cell> geometry by factor of <n>`. If also `d > 1`, it appends `/ <d>` and warns `Warning:  Geometry may be lost because internal grid cannot be reduced.`

(Magic ref: database/DBio.c:786-833, cif/CIFtech.c:466-488, utils/fraction.c:54-109)

#### Scenario: Finer file refines the database
- **GIVEN** λ ratio (1,1) and an output style that allows refinement
- **AND** an already-loaded cell with metal1 at (0,0)-(3,5)
- **WHEN** a file with `magscale 1 2` and `rect 1 1 3 3` is read
- **THEN** the already-loaded rectangle SHALL become (0,0)-(6,10) and the λ ratio SHALL become (1,2)
- **AND** the new rectangle SHALL be stored as (1,1)-(3,3)

#### Scenario: Coarser file is multiplied
- **GIVEN** λ ratio (1,2)
- **WHEN** a file without `magscale` containing `rect 0 0 3 5` is read
- **THEN** n/d SHALL be 2/1 and the rectangle SHALL be stored as (0,0)-(6,10)

#### Scenario: Refinement refused truncates
- **GIVEN** λ ratio (1,1) and an output style whose grid limit forbids refinement by 2
- **WHEN** a file with `magscale 1 2` containing `rect -3 0 3 5` is read
- **THEN** the rectangle SHALL be stored as (-1,0)-(1,2), with each value divided by 2 and truncated toward zero
- **AND** the "Geometry may be lost" warning SHALL be printed, although Magic omits it because n = 1 (see Q-CELL-2)

### Requirement: Body section dispatch

When reading the body, Hefesto SHALL classify each logical line as follows.
Compatibility: exact

**Section headers.** A line is a section header when it starts with the two characters `<<` and is followed by optional whitespace and a token. The token is the maximal run of non-whitespace characters. Text after the token is not checked.

**Header tokens.** The token is handled in this order:
1. **A tile type name** in the current technology (exact name or alias denoting a single type, as defined by the technology capabilities). This begins a paint section of that type.
   - The built-in type names `checkpaint`, `checksubcell`, `error_p`, `error_s`, `error_ps`, `magnet`, `fence` and `rotate` are valid type names. They read into their dedicated planes (DRC check, DRC error, and router hint planes).
2. **`labels`, `elements` or `properties`**. Each begins the corresponding record section.
3. **`end`**. The read finishes successfully.
4. **Any other token.** It is resolved as a type list by the technology's noisy name-to-mask rule. Rectangles in this section are painted once for each user type in the resulting mask, in increasing type order. If the name resolves to nothing, an error is reported by the name resolver and the section's rectangles are parsed but not painted.

**Stacked contact types.** When the header names a stacked contact type, each rectangle is painted once for each of the type's residue types, in increasing type order. The paint rules of the technology combine the residues back into the stacked type.

**Non-header lines.** A line that is not a section header MUST be a `use` line, which starts a use block. Any other line causes the message `Expected "use" line but saw: <line>` and a format error.

**Section termination.**
- A paint section ends at the first line that does not start with `r`, `t` or `#`.
- The label, element and property sections end as defined in their own requirements.

(Magic ref: database/DBio.c:844-1000, database/DBtechtype.c:86-93)

#### Scenario: Stacked contact header
- **GIVEN** a technology in which `m3c2` is a stacked type with residues `m2c` and `m3c`
- **WHEN** the section `<< m3c2 >>` with `rect 0 0 4 4` is read
- **THEN** the rectangle SHALL be painted with `m2c` and then with `m3c`, producing the stacked type `m3c2` by the paint rules

#### Scenario: DRC check area persisted in a file
- **WHEN** a file contains `<< checkpaint >>` followed by `rect 0 0 10 10`
- **THEN** the area (0,0)-(10,10) SHALL be recorded as pending DRC re-check in the cell (see the DRC capabilities)

#### Scenario: Unknown body line
- **WHEN** a body line reads `foo 1 2 3`
- **THEN** the message `Expected "use" line but saw: foo 1 2 3` SHALL be printed and the read SHALL end with a format error

### Requirement: Rectangle and triangle records

Hefesto SHALL parse paint records with the following character-level rules, which reproduce Magic's fast scanner.
Compatibility: exact

**Line selection and prefix skipping.**
- A record line is selected by its first character only. `r` means rectangle and `t` means triangle.
- For `r`, the next 4 characters are skipped unconditionally (normally `ect `). For `t`, the next 3 are skipped (normally `ri `).

**Reading the four integers.**
- Each integer is an optional `-` followed by decimal digits. Zero digits yield the value 0.
- After each of the first three integers the next character MUST be whitespace. Otherwise the record is a format error.
- Before the second, third and fourth integers, any run of whitespace is skipped, and that run may include LF (see Q-CELL-3). As a result, `rect  1 2 3 4` (two spaces after `rect`) is read as (0,1,2,3). With `magic-compat` OFF, a `rect` or `tri` record SHALL be parsed as whitespace-separated tokens on its own line, and a short or malformed record SHALL be a format error.
- No overflow checking is performed.

**Remainder of the line.**
- After the fourth integer, the remaining characters up to LF are scanned.
- For a triangle, the corner is decoded from the presence of the lowercase letters `s` and `e` anywhere in that remainder:

| `s` present | `e` present | corner |
|---|---|---|
| no | no | `nw` |
| yes | no | `sw` |
| yes | yes | `se` |
| no | yes | `ne` |

  Uppercase letters are ignored. For example, `SE` decodes as `nw`.
- For a rectangle, the remainder is ignored.

**Meaning and painting.**
- `rect x1 y1 x2 y2` denotes the rectangle [x1,x2]×[y1,y2].
- `tri x1 y1 x2 y2 C` denotes the right triangle inside that rectangle whose right-angle vertex is corner C of the rectangle. In terms of a split tile: `nw` and `sw` are the left side of the diagonal, `se` and `ne` are the right side. For `nw` and `se` the diagonal runs from lower-left to upper-right; for `sw` and `ne` it runs from upper-left to lower-right.
- After scaling (see "Coordinate scaling on read"), a record whose rectangle has x1 ≥ x2 or y1 ≥ y2 is silently ignored.
- Each record is painted into the cell with the technology's paint rules, in file order (see capability `layout-paint-semantics`).
- At full verbosity, every 10000th record prints `<cell>: <count> rects`.

(Magic ref: database/DBio.c:916-980, utils/getrect.c:57-145, tiles/tile.h:205-250)

#### Scenario: Triangle corners
- **WHEN** the record `tri 0 0 10 10 se` is read in a metal1 section
- **THEN** metal1 SHALL occupy the triangle with vertices (0,0), (10,0), (10,10)

#### Scenario: Degenerate rectangle ignored
- **WHEN** `rect 5 0 5 10` is read
- **THEN** nothing SHALL be painted and no error SHALL be reported

#### Scenario: Non-numeric coordinate
- **WHEN** `rect 1.5 0 2 2` is read
- **THEN** the read SHALL end with a format error

#### Scenario: Double space quirk (verify against Magic)
- **WHEN** `rect  1 2 3 4` (two spaces after `rect`) is read in a metal1 section
- **THEN** metal1 SHALL be painted at (0,1)-(2,3)

### Requirement: Use block parsing

Hefesto SHALL parse a use block as follows.
Compatibility: exact

**1. The `use` line.**
- The line starts with `use` and is read in full regardless of length.
- It is split into up to three whitespace-delimited tokens after `use`: the cell name, the instance id and the path. Further tokens are ignored. If there is no cell name, the message is `Malformed "use" line: <line>` and a format error follows.
- An id that starts with `*` marks the instance as locked. The id is the remainder after the `*`.
- A missing id means "no id"; one is generated later (see "Instance identifiers on read").
- A path of 1024 characters or more is ignored, with the message `Path for cell <cell> in "use" line is too long and will be ignored.`
- Path handling (variable expansion, joining with the parent directory, conflict resolution) is defined in capability `cell-library-resolution`.

**2. Optional `array xlo xhi xsep ylo yhi ysep` line.**
- It MUST have six integers. Otherwise the message is `Malformed "array" line: <line>` and a format error follows.
- xsep and ysep are scaled. The indices are not.
- If the line is absent, all six values are 0.

**3. Optional `timestamp <int>` line.**
- On a parse failure the message is `Malformed "timestamp" line: <line>` and a format error follows.
- If the line is absent, the child timestamp is 0.

**4. Mandatory `transform a b c d e f` line.**
- It MUST have six integers forming a Manhattan orientation. Either a = e = 0 with |b| = |d| = 1, or b = d = 0 with |a| = |e| = 1.
- Otherwise the message is `Malformed or illegal "transform" line: <line>` and a format error follows.
- c and f are translations and are scaled.
- The mapping from child to parent coordinates is x' = a·x + b·y + c and y' = d·x + e·y + f.

**5. Mandatory `box x1 y1 x2 y2` line.**
- On failure the message is `Malformed "box" line: <line>` and a format error follows.
- The values are scaled. They are the child's bounding box in child coordinates, a guess used until the child is read. The Magic manual page describes this box differently (see Q-CELL-38).

**Creating the definition.** If no cell definition with that name exists, an unread placeholder definition is created with the following properties:
- Its timestamp is the child timestamp.
- Its bounding box is the box.
- A degenerate box is widened so that x2 = x1 + 1 when x2 ≤ x1, and y2 = y1 + 1 when y2 ≤ y1. On the first use of that name in the file, Magic prints `Subcell has degenerate bounding box: ...` and `Adjusting bounding box of subcell <child> of <parent> to ...`.

**Placing the instance.** The instance is created with:
- the id;
- the locked flag;
- the array parameters, applied as array(xlo, ylo, xhi, yhi, xsep, ysep) under the identity transform;
- the transform.

It is then placed in the parent. Timestamp comparison follows capability `cell-timestamps`.

**Circular use.** If the named definition already exists and is the parent itself or an ancestor of the parent, the instance is not created and the block is skipped. On the first use of that name in the file, Magic prints `Subcells are used circularly!` and `Ignoring subcell <child> of <parent>.`

(Magic ref: database/DBio.c:2385-2583, database/DBio.c:3018-3064, database/DBio.c:3091-3171, database/DBcellsrch.c:1068-1088)

#### Scenario: Locked and unlocked instances
- **WHEN** the lines `use inv  inv_0`, `timestamp 0`, `transform 1 0 0 0 1 0`, `box 0 0 4 4` are read
- **THEN** an unlocked instance `inv_0` of `inv` SHALL be created at the identity transform
- **AND** for `use inv *inv_1` the instance `inv_1` SHALL be created locked

#### Scenario: Illegal transform
- **WHEN** a use block contains `transform 2 0 0 0 1 0`
- **THEN** the message `Malformed or illegal "transform" line: transform 2 0 0 0 1 0` SHALL be printed and the read SHALL end with a format error

#### Scenario: Self use is ignored
- **GIVEN** the file being read for cell `a`
- **WHEN** it contains `use a a_0` with a valid block
- **THEN** no instance SHALL be created and `Subcells are used circularly!` SHALL be printed

#### Scenario: Array line
- **WHEN** a block contains `array 0 3 10 0 1 20` and `transform 1 0 5 0 1 0`
- **THEN** the instance SHALL be a 4×2 array with x separation 10 and y separation 20, translated by (5,0)

### Requirement: Instance identifiers on read

After the whole file has been read, Hefesto SHALL make the instance ids in the cell unique.
Compatibility: behavioral

- When an instance has the same id as an instance already seen, Magic prints `Duplicate instance-id for cell <child> (<id>) will be renamed`, and that instance's id is cleared.
- Every instance without an id then receives `<childname>_<k>`. Here k is the smallest non-negative integer, at or above a per-child-definition counter starting at 0, such that the name is not used in the cell. Magic prints `Setting instance-id of cell <child> to <id>`.
- Magic visits instances in its subcell-plane enumeration order. Hefesto SHALL visit them in file order, so the later of two instances with the same id is the one renamed (see Q-CELL-4).
- The naming rule itself belongs to the layout hierarchy capabilities.

(Magic ref: database/DBio.c:1013, database/DBcellname.c:2605-2772)

#### Scenario: Missing ids are generated
- **WHEN** a cell file contains two use blocks of cell `inv` with no ids
- **THEN** the instances SHALL receive the ids `inv_0` and `inv_1`

### Requirement: Label records

Within a `<< labels >>` section, Hefesto SHALL parse records selected by their first character.
Compatibility: exact

**Section termination.** The section ends at the first line whose first character is not `r`, `f`, `p` or `l`.

**`rlabel` (first character `r`).** The token after the layer is examined:
- If it begins with `s`, the record is `rlabel <layer> s <x1> <y1> <x2> <y2> <pos> <text>` and the label is sticky.
- Otherwise the record is `rlabel <layer> <x1> <y1> <x2> <y2> <pos> <text>`.

**`flabel` (first character `f`).**
- Sticky form: `flabel <layer> [s] <x1> <y1> <x2> <y2> <pos> <font> <size> <rot> <ox> <oy> <text>`, with the same sticky rule as `rlabel`.
- `<font>` is a font name of at most 255 characters. An unknown font name selects the default font, for which `rlabel` semantics apply.
- `<size>`, `<ox>` and `<oy>` are in eighths of a file unit and are scaled.
- `<rot>` is in degrees.

**Obsolete point label (first character `l`).** `label <layer> <x> <y> <pos> <text>` gives a point label at (x,y) with the default font.

**Field rules.**
- `<layer>` has at most 49 characters.
- `<pos>` is the integer position code: 0 = center, 1 = n, 2 = ne, 3 = e, 4 = se, 5 = s, 6 = sw, 7 = w, 8 = nw.
- `<text>` is everything after the separating whitespace up to the end of the line, including inner and trailing spaces. Magic truncates it to its first 99 characters. Hefesto SHALL keep the full text (see Q-CELL-5).

**Malformed records.** A record that does not match prints `Skipping bad "rlabel" line: <line>` (or `flabel` or `label`) and is skipped. A malformed `rlabel` whose sticky token cannot be read prints `Skipping bad "flabel" line`. Skipping a bad record is not a format error.

**Layer resolution.**
1. The layer is resolved as a type name. If that fails, it is resolved as an alias (the lowest type of the alias).
2. If it is still unknown, Magic prints `Warning: label "<text>" attached to unknown type "<layer>"` and the label is attached to space.
3. If the type is a stacked contact type, the label is attached to its highest-numbered residue type.

**Order.** Labels are appended to the cell in file order.

(Magic ref: database/DBio.c:3868-4167)

#### Scenario: Sticky rectangular label
- **WHEN** `rlabel metal1 s 0 2 6 8 3 VDD` is read
- **THEN** a sticky label `VDD` on metal1 SHALL be created with rectangle (0,2)-(6,8) and position east

#### Scenario: Text with spaces
- **WHEN** `rlabel metal1 0 0 0 0 0 a b  c` is read
- **THEN** the label text SHALL be `a b  c`

#### Scenario: Long label text kept
- **WHEN** an rlabel record has a 120-character text
- **THEN** the stored label text SHALL be all 120 characters, where Magic keeps only the first 99

### Requirement: Port records

A `port` record SHALL attach port attributes to the label read immediately before it in the same cell.
Compatibility: exact

Grammar: `port <index> <dirs> [<use> <class> [<shape>]]`.

- **Token count.** The record MUST have 2, 3, 4 or 5 tokens after `port`. Magic rejects 3 tokens. Hefesto SHALL accept them, reading the third token as `<class>` and leaving the use unset, because that is the line Magic writes for the uses reset, scan and tieoff (see Q-CELL-6).
- **`<dirs>`.** At most 4 characters from `n`, `s`, `e`, `w`, each setting the corresponding connection direction. Other letters are ignored.
- **`<use>`.** The full words `reset`, `scan` and `tieoff` select those uses (Hefesto only, Q-CELL-6). Otherwise selected by its first letter: `s` signal, `a` analog, `p` power, `g` ground, `c` clock, `d` default. Any other letter prints `Ignoring unknown "port" use: <use>` and leaves use unset.
- **`<class>`.** Selected by its first letter: `i` input, `o` output, `t` tristate, `b` bidirectional, `f` feedthrough, `d` default. Any other letter prints `Ignoring unknown "port" use: <use>`. The message names the use token (see Q-CELL-6).
- **`<shape>`.** Selected by its first letter: `a` abutment, `r` ring, `f` feedthrough, `d` default. Any other letter prints `Ignoring unknown "port" shape: <shape>`.
- **Rejection.** The record is skipped with `Skipping bad "port" line: <line>` when there is no preceding label, when the preceding label already has any direction set, or when the token count is wrong.

(Magic ref: database/DBio.c:3978-4085, database/database.h.in:276-309)

#### Scenario: Port with use and class
- **WHEN** `rlabel metal1 0 0 1 1 0 A` is followed by `port 2 ew signal input`
- **THEN** label `A` SHALL be port 2 with directions east and west, use signal and class input

#### Scenario: Three-token port accepted
- **WHEN** a label is followed by `port 1 nsew input`
- **THEN** the label SHALL be port 1 with all four directions, class input and the use unset, where Magic skips the line

### Requirement: Element records

Within a `<< elements >>` section, Hefesto SHALL parse persistent display elements.
Compatibility: exact

**Section termination.** The section ends at the first line whose first character is not `r`, `l` or `t`.

**Record forms.**
- `rectangle <name> <styles> <x1> <y1> <x2> <y2> [<flags>]`
- `line <name> <styles> <x1> <y1> <x2> <y2> [<flags>]`
- `text <name> <styles> <x> <y> "<text>" [<flags>]`

**Field rules.**
- `<name>` has at most 127 characters.
- `<styles>` is a comma-separated list of display-style long names (at most 1023 characters).
- `<flags>` is a comma-separated list, after optional leading whitespace, of at most 99 characters.
- The text of a `text` element lies between the first and the last `"` on the line.
- Coordinates are scaled.

**Malformed records.** A record that does not match prints `Skipping bad "<kind>" element line: <line>` and is skipped.

**Creating the element.** Each element is created as persistent and attached to the cell being read. Its styles are applied in list order, after which the temporary initial style 0 is removed. Its flags are then parsed by the element flag parser (see the display/element capabilities).

(Magic ref: database/DBio.c:3694-3846, dbwind/DBWelement.c:178-272)

#### Scenario: Text element
- **WHEN** `text note1 white 10 20 "hello world" NORTH` is read
- **THEN** a persistent text element `note1` SHALL be created at (10,20) with text `hello world`, style `white` and position north

### Requirement: Properties section records

Within a `<< properties >>` section, Hefesto SHALL parse records of the form `<type> <key> <value>`.
Compatibility: exact

**Tokenizing and termination.**
- `<type>` is the first token (at most 31 characters).
- `<key>` is the second token (at most 127 characters).
- `<value>` is the rest of the line after the separating whitespace, without the LF.
- `<type>` is matched against `string`, `integer`, `dimension`, `double` by case-insensitive unique-prefix lookup. A token that matches none, or is ambiguous (for example `d`), ends the section and is processed as a body line.
- A matching record with fewer than three fields prints `Skipping bad property line: <line>` and is skipped. This means a property with an empty value cannot be read (see Q-CELL-7).

**Long values.** If the value fills the line buffer (2047 characters), further 2047-character chunks are appended until a chunk ends before its buffer is full. End of file during a chunk ends the section and discards the partial property.

**Typed values.**
- `integer`: whitespace-separated integers (base auto-detected, as in C `strtol` with base 0, when validating; stored using decimal `%d` parsing). Stored unscaled.
- `double`: 64-bit integers, same rules, stored unscaled.
- `dimension`: integers scaled by the read scale.
- If any token of an `integer`, `double` or `dimension` value fails to parse as an integer, Magic stores the raw value as a string property and then also overwrites it with a numeric property built from the tokens. This is a memory-safety defect (see Q-CELL-8). Hefesto SHALL store the raw value as a string property and stop.

**`string` values with reserved keys.**
- `GDS_FILE`: stored as a string, and the cell is marked as a vendor (read-only GDS) cell (see capability `gds-read`).
- `FIXED_BBOX` or `OBS_BBOX`: four integers are parsed and scaled and stored as a 4-element dimension. `FIXED_BBOX` additionally marks the cell's bounding box as fixed. If parsing fails, Magic prints `Cannot read bounding box values in <key> property` and stores the value as a string.
- Keys starting with `MASKHINTS_`: the value is parsed as groups of four scaled integers. Each group (x1 y1 x2 y2) is painted as a rectangle into a hint plane. A non-integer token prints `Mask-hint "<rest>" has non-integer values!` and drops the property. A count not divisible by four prints `Mask-hint property number of values is not divisible by four.  Truncated.`
- Keys starting with `GDS_START`, `GDS_BEGIN` or `GDS_END`: one 64-bit integer, stored as a double-type property. On failure Magic prints `Cannot read file offset value in <key> property` and stores the value as a string.

**Other `string` values.** Stored verbatim, including trailing spaces.

**Read-only cells.** Properties are stored even when the cell is read-only.

(Magic ref: database/DBio.c:3193-3672, utils/lookup.c:58-160)

#### Scenario: Fixed bounding box as string
- **GIVEN** a read scale of n/d = 2/1
- **WHEN** `string FIXED_BBOX 0 0 10 20` is read
- **THEN** the cell SHALL have dimension property FIXED_BBOX = (0,0,20,40) and a fixed bounding box

#### Scenario: Typed dimension property
- **WHEN** `dimension PITCH 5 7` is read at scale 1/1
- **THEN** the cell SHALL have a dimension property PITCH with values (5,7)

#### Scenario: Unknown type ends the section
- **WHEN** a properties section is followed by `<< end >>`
- **THEN** the token `<<` SHALL end the properties section and the end marker SHALL be processed

### Requirement: Format errors and partial reads

When a format error occurs, Hefesto SHALL keep everything read so far and report the error.
Compatibility: behavioral

**Effects of a format error.**
- Magic prints `File <cell> contained format error`.
- The whole cell is queued for a full DRC re-check.
- The read reports failure.

**Content that remains in the cell.** All paint, instances, labels, elements and properties read before the error remain in the cell. The cell is marked available, so it is not re-read automatically.

**Post-read processing.** The steps that run after a successful read also run after a failed read:
- the modified and box-changed flags are cleared;
- instance ids are made unique;
- timestamps are processed (see capability `cell-timestamps`).

**Errors that are not format errors.** The following print a message and continue:
- a malformed label, port, element or property record;
- an unknown port use, class or shape;
- an unknown label layer.

**Interrupts.** User interrupts are deferred while a cell is being read, so a read is never partially applied because of an interrupt.

(Magic ref: database/DBio.c:1002-1062, database/DBio.c:557)

#### Scenario: Missing end marker
- **WHEN** a file ends right after its last label record, without `<< end >>`
- **THEN** `File <cell> contained format error` SHALL be printed
- **AND** all paint, uses and labels in the file SHALL be present in the cell

### Requirement: Writer header and scale reduction

When writing a cell, Hefesto SHALL first compute a reducer r and then write the header.
Compatibility: exact

**Computing r.** Let the λ ratio be (L0, L1).
- If L1 ≤ L0, then r = 1.
- Otherwise start with g = L1 and fold in, using g ← gcd(|v|, g) whenever v mod g ≠ 0, every value v in this list:
  - the four coordinates of every tile, on its home plane, of every technology type (built-in types excluded);
  - the four coordinates of every label rectangle;
  - every value of every dimension property;
  - the four coordinates of every tile of every plane-type property;
  - the translation c and f of every instance;
  - the four coordinates of every instance's child-definition bounding box;
  - for arrayed instances only, xsep and ysep.

  Stop early when g reaches 1. Then r = g.

**Values not folded into r.** Built-in-type tiles (DRC check and error tiles, router hints), flabel size and offsets, and element coordinates are not part of r. They are later divided by r with truncation toward zero (see Q-CELL-9). With `magic-compat` OFF, these values SHALL also be folded into r.

**Header lines.**
1. `magic`
2. `tech <technology name>`
3. If L0 ≠ L1 / r (integer division): `magscale <L0> <L1/r>`. The fraction is not reduced further.
4. `timestamp <cell timestamp>`, with the timestamp as a signed 32-bit decimal.

**Pending timestamp.** If the cell still has a pending new timestamp when it is written, Magic prints `Magic error: writing out-of-date timestamp for <cell>.`. The current value is written anyway.

Every coordinate written in the body is the internal value divided by r, using C integer division (truncation toward zero). The written file therefore uses file units of r internal units.

(Magic ref: database/DBio.c:4232-4435, database/DBio.c:4716-4735)

#### Scenario: Reducible geometry hides magscale
- **GIVEN** λ ratio (1,2) and a cell whose only content is metal1 at (0,0)-(4,6)
- **WHEN** the cell is written
- **THEN** the header SHALL be `magic`, `tech <t>`, `timestamp <s>` with no magscale line
- **AND** the paint record SHALL be `rect 0 0 2 3`

#### Scenario: Irreducible geometry writes magscale
- **GIVEN** λ ratio (1,2) and metal1 at (0,0)-(3,6)
- **WHEN** the cell is written
- **THEN** the header SHALL contain `magscale 1 2` and the record SHALL be `rect 0 0 3 6`

### Requirement: Writer paint sections

Hefesto SHALL write paint as one section per tile type, using this exact algorithm.
Compatibility: exact

**Types visited.** For each type T in increasing type number, from the first non-space built-in type up to the last user (non-stacked) type, skipping types without a home plane:
1. Let M = {T} ∪ {every stacked contact type whose residues include T}.
2. Enumerate the tiles of T's home plane whose type is in M, in plane enumeration order (defined below).
3. For each such tile, emit a record for T. A split tile is emitted when its left or right side has a type in M.
4. Before the first record of T, emit `<< <long name of T> >>`. A type with no records gets no header.

**Records.**
- A non-split tile gives `rect <l> <b> <r> <t>` (each value divided by r).
- A split tile gives `tri <l> <b> <r> <t> <corner>`. The corner is `nw` if T is on the left side and the diagonal direction bit is 0, `sw` if left and the bit is 1, `se` if right and the bit is 0, and `ne` if right and the bit is 1.
- A split tile is reported once for each side whose type is in M. Each report writes the left-side record if the left side's type is in M, and the right-side record otherwise. As a result, when both sides match (for example a stacked contact on one side and one of its residues on the other), the left-side record is written twice (see Q-CELL-15). With `magic-compat` OFF, each side SHALL be written once, with its own record.

**Stacked contacts.** A stacked contact tile is written once under each of its residue types, so stacked types never appear as section names.

**Built-in types.** Built-in types are written like any other type: `checkpaint`, `checksubcell`, `error_p`, `error_s`, `error_ps`, `magnet`, `fence`, `rotate`, in that order, before the technology types.

**Plane enumeration order.** This is the tile enumeration order of capability `layout-geometry-model` (requirement "Tile enumeration order"), which owns it, applied to the whole plane. Restated for the whole-plane case, it is a depth-first pre-order traversal of a forest:
- The parent of a tile X is the tile containing the point (left(X) − 1, bottom(X)).
- Tiles touching the plane's left boundary are roots, visited from top to bottom.
- The children of a tile are visited from top to bottom.

Byte-identical output therefore requires the same tile decomposition as Magic (see Q-CELL-10).

(Magic ref: database/DBio.c:4746-4774, database/DBio.c:5939-6006, database/DBtiles.c:521-639)

#### Scenario: Residue decomposition of stacked contacts
- **GIVEN** a stacked contact tile of type `m3c2` (residues `m2c`, `m3c`) at (0,0)-(4,4) and no other paint
- **WHEN** the cell is written
- **THEN** the output SHALL contain `<< m2c >>` followed by `rect 0 0 4 4`, and later `<< m3c >>` followed by `rect 0 0 4 4`

#### Scenario: Enumeration order is not sorted by y (verify against Magic)
- **GIVEN** metal1 rectangles A = (0,0)-(10,10) and B = (20,5)-(30,15) and no other metal1
- **WHEN** the cell is written
- **THEN** the metal1 section SHALL list `rect 0 0 10 10` before `rect 20 5 30 15`

#### Scenario: Triangle output
- **GIVEN** a split tile (0,0)-(10,10) with metal1 on the right side and diagonal direction bit 0
- **WHEN** the cell is written
- **THEN** the metal1 section SHALL contain `tri 0 0 10 10 se`

### Requirement: Writer use blocks

Hefesto SHALL write the instances of the cell sorted by instance id with the natural comparator (see "Natural ordering of names").
Compatibility: exact

**Use line.** For each instance, in that order, the use line is:

```
"use " + childname + " " + ("*" if locked else " ") + id + [" " + path] + "\n"
```

An unlocked instance therefore has two spaces between the child name and the id. Exactly when a path is written, and how it is computed, is defined in capability `cell-library-resolution`. A path is written at most once per child definition per file, on the first instance in sorted order.

**Following lines:**
1. If xlo ≠ xhi or ylo ≠ yhi: `array <xlo> <xhi> <xsep/r> <ylo> <yhi> <ysep/r>`.
2. `timestamp <child definition timestamp>`.
3. `transform <a> <b> <c/r> <d> <e> <f/r>`.
4. `box <x1/r> <y1/r> <x2/r> <y2/r>`, giving the child definition's current bounding box in child coordinates.

(Magic ref: database/DBio.c:4776-4798, database/DBio.c:6130-6287)

#### Scenario: Use block output
- **GIVEN** an unlocked 1×1 instance `inv_0` of `inv` (timestamp 99, bounding box (0,0)-(8,12)) with transform (1,0,20,0,1,0), in the same directory as its parent, and reducer 1
- **WHEN** the parent is written
- **THEN** the output SHALL be the four lines `use inv  inv_0`, `timestamp 99`, `transform 1 0 20 0 1 0` and `box 0 0 8 12`

### Requirement: Writer labels and ports

If the cell has at least one label, Hefesto SHALL write `<< labels >>` followed by one record per label, in the cell's label list order. Labels with empty text are skipped.
Compatibility: exact

**Label records.**
- Default-font label: `rlabel <type long name> [s ]<x1> <y1> <x2> <y2> <pos> <text>`. The coordinates are divided by r. `s ` is present only for sticky labels.
- Rendered-font label: `flabel <type long name> [s ]<x1> <y1> <x2> <y2> <pos> <font name> <size/r> <rotation> <ox/r> <oy/r> <text>`.

**Port records.** If the label has any connection direction, a port record follows its label: `port <index> <dirs>`, where `<dirs>` concatenates `n`, `s`, `e`, `w` in that order for the directions that are set. If the label's use, class or shape is non-default, the record continues with:
- the use, written as ` signal`, ` analog`, ` power`, ` ground`, ` clock` or ` default`. Magic writes nothing for the uses reset, scan and tieoff. Hefesto SHALL write them as ` reset`, ` scan` and ` tieoff` (see Q-CELL-6).
- the class, written as ` input`, ` output`, ` tristate`, ` bidirectional`, ` feedthrough` or ` default`.
- the shape, written as ` abutment`, ` ring` or ` feedthrough`. A default shape writes nothing.

(Magic ref: database/DBio.c:4800-4907)

#### Scenario: Port with only a use
- **GIVEN** label `VDD` on metal1 with directions n and s, port index 1, use power, default class and default shape
- **WHEN** the cell is written
- **THEN** the port record SHALL be `port 1 ns power default`

### Requirement: Writer elements

If the cell has persistent display elements, Hefesto SHALL write `<< elements >>` followed by one record per element.
Compatibility: exact

**Record format.**
- `rectangle <name> <styles> <x1/r> <y1/r> <x2/r> <y2/r>`
- `line <name> <styles> <x1/r> <y1/r> <x2/r> <y2/r>[ <flags>]`. The line flags are `halfx`, `halfy`, `arrowleft` and `arrowright`, in that order.
- `text <name> <styles> <x/r> <y/r> "<text>"[ <flags>]`. The text flags are the position name in uppercase (for example `NORTH`), written when the position is not center, and then the size (`small`, `large`, `xlarge`, `default`), written when the size is not medium.
- `<styles>` is the comma-joined list of the element's display-style long names.
- Flags are joined with `,` and preceded by one space.

**Order.** Magic writes elements in its element hash-table enumeration order (see Q-CELL-11). That order never affects any result, because the reader does not depend on it, so it is an exception to the rule that name-keyed hash order is reproduced (capability `hefesto-output-determinism`). Hefesto SHALL write them in a deterministic order, sorted by element name with the natural comparator (see "Natural ordering of names"), and golden comparison SHALL canonicalize element order before comparing (decision X-1, `docs/decisions/cross-cutting.md#X-1`).

(Magic ref: database/DBio.c:4909-4916, dbwind/DBWelement.c:178-272)

#### Scenario: Line element with flags
- **GIVEN** a persistent line element `l1` with style `white` from (0,0) to (10,0), flagged halfx and arrowright
- **WHEN** the cell is written with r = 1
- **THEN** the record SHALL be `line l1 white 0 0 10 0 halfx,arrowright`

### Requirement: Writer properties

If the cell has properties, Hefesto SHALL write `<< properties >>` followed by one record per property, sorted by key with the natural comparator.
Compatibility: exact

**Record format.** Each record is `<typeword> <key><values>`.
- **`<typeword>`.** In property compat mode (the default, toggled by `property compat`; see capability `layout-properties`) it is always `string`. Otherwise it is `string`, `integer`, `dimension` (also used for plane properties) or `double`, according to the property's type.
- **`<values>` by property type:**
  - string: one space followed by the value verbatim;
  - integer: ` %d` for each value;
  - dimension: ` %d` of value / r for each value;
  - double: ` %lld` for each value;
  - plane: ` %d %d %d %d` of each non-space tile's coordinates divided by r, in plane enumeration order.

**Round-trip consequence.** In property compat mode, integer, double and dimension properties are re-read as strings, unless their key is one of the reserved keys listed in "Properties section records" (see Q-CELL-7). With `magic-compat` OFF, properties SHALL be written with their own type, and a property with an empty value SHALL be written and read back as empty.

(Magic ref: database/DBio.c:4918-4944, database/DBio.c:4981-5110, database/DBprop.c:37, commands/CmdLQ.c:2525-2541)

#### Scenario: Property compat mode output
- **GIVEN** property compat mode is on and the cell has dimension property `FIXED_BBOX` = (0,0,60,20) and integer property `NFING` = (4)
- **WHEN** the cell is written with r = 1
- **THEN** the properties section SHALL be `string FIXED_BBOX 0 0 60 20` followed by `string NFING 4`

#### Scenario: Typed mode output
- **GIVEN** property compat mode is off
- **WHEN** the same cell is written
- **THEN** the records SHALL be `dimension FIXED_BBOX 0 0 60 20` and `integer NFING 4`

### Requirement: Natural ordering of names

Hefesto SHALL sort instance ids and property keys with Magic's natural comparator cmp(s1, s2), defined by this loop.
Compatibility: exact

1. If s2 is exhausted, return 0 when s1 is exhausted and 1 otherwise.
2. Else, if s1 is exhausted, return 1.
3. Else, if the current characters are not both decimal digits, compare them case-insensitively. If they differ, return the difference of their lowercase codes. Otherwise advance both strings by one character.
4. Else, read the maximal digit runs of both strings as unsigned decimal numbers and compare them numerically. If they differ, return the sign. Otherwise advance both strings past their runs.

This comparator is not a strict weak order: a proper prefix compares greater than its extension in both directions, and differences in case or in leading zeros compare equal. The resulting order for such inputs depends on the sorting algorithm, so Magic's own output differs between hosts (see Q-CELL-12). Hefesto SHALL instead sort with the following total order, on every platform. It equals Magic's order whenever cmp is consistent for the inputs (decision X-1, `docs/decisions/cross-cutting.md#X-1`; tie-break decided by D-9, `docs/decisions/audit/DECISIONS.md#D-9`):

1. Run the loop above, but when exactly one string is exhausted at step 1 or 2, the exhausted string (a *natural prefix* of the other, such as `x` of `x2`, or `a01` of `a1b`) sorts first.
2. If the loop ends with both strings exhausted and no difference found (they differ only in letter case or in leading zeros of digit runs, or are identical), compare the two complete strings byte by byte as unsigned 8-bit values; at the first differing byte the smaller value sorts first, and a string that is a byte prefix of the other sorts first.
3. Two strings that are equal byte for byte compare equal; within one cell, instance ids and property keys are unique, so this never decides an order.

(Magic ref: database/DBcellname.c:676-707, database/DBio.c:4454-4467, database/DBio.c:4556-4567)

#### Scenario: Numeric runs compare numerically
- **WHEN** the instance ids `a10`, `B1`, `a2` are sorted for writing
- **THEN** the write order SHALL be `a2`, `a10`, `B1`

#### Scenario: A natural prefix sorts first
- **WHEN** the instance ids `x2` and `x` are sorted for writing, in either input order
- **THEN** the write order SHALL be `x`, `x2`

#### Scenario: Case and leading-zero ties are broken by bytes
- **WHEN** the property keys `a1`, `A1` and `a01` are sorted for writing
- **THEN** the write order SHALL be `A1`, `a01`, `a1`

### Requirement: Compressed cell input

Hefesto SHALL read cell files compressed with gzip, under the names `<name>.mag.gz`, using the same grammar as uncompressed files. Any file opened through the cell reader MAY be either compressed or plain, and its content decides which. Hefesto SHALL NOT write compressed cell files: every save writes a plain `.mag` file.
Compatibility: behavioral

The order in which `.mag` and `.mag.gz` candidates are tried, and the names recorded afterwards, are defined in capability `cell-library-resolution`.

(Magic ref: database/DBio.c:98-102, database/DBio.c:1498-1517, utils/path.c:74-96)

#### Scenario: Reading a gzip cell
- **GIVEN** only `inv.mag.gz` exists, containing a valid gzip-compressed cell
- **WHEN** cell `inv` is loaded
- **THEN** its content SHALL be read as if from an uncompressed `.mag` file

### Requirement: Command-script save variant

When a save is given an explicit target file name that ends in `.tcl` (and is longer than 4 characters), Hefesto SHALL write the cell as a script of editor commands instead of a `.mag` file.
Compatibility: behavioral

- The cell's modified, timestamp and file-path state is not changed.
- The cell is not renamed.
- No read-only check, locking or temporary file is used.
- On success, Magic prints `Saved cell <cell> as a sequence of magic commands (file <file>).`.
- If the file cannot be created, Magic continues with a normal `.mag` save to `<file>.tcl.mag`. Hefesto SHALL instead report the failure and write nothing (see Q-CELL-13).

(Magic ref: database/DBio.c:5629-5646, commands/CmdSubrs.c:754-769)

#### Scenario: Script save leaves the cell modified
- **GIVEN** a modified cell `top`
- **WHEN** it is saved as `top.tcl`
- **THEN** the file `top.tcl` SHALL be written and `top` SHALL remain marked modified

### Requirement: Command-script content

The command script SHALL consist of the following lines. All coordinates are in internal units, with no reduction.
Compatibility: informative

**1. Prologue.**
```
# Command script for generating cell <name>
<empty line>
suspendall
tech unlock *
set curunits [units]
units internal
load <name> -silent
box values 0 0 0 0
```
`<name>` is the cell name with a trailing `.tcl` removed.

**2. Paint.** Types are visited as for the `.mag` writer. Error types are skipped.
- A non-split tile gives `box values l b r t` followed by `paint <name of the tile's actual type>`.
- A split tile gives `box values ...` followed by one `splitpaint <dir> <type>` line for each non-space side. `<dir>` is taken from the table `nw, se, sw, ne`, indexed by 2·direction + side, and the same `<dir>` is used for both sides (see Q-CELL-14).

**3. Instances.** Instances are written in enumeration order, as `box position <llx> <lly>` of the instance bounding box, `getcell <child>` and `identify <id>`. Transform, array, lock and path are not written. Hefesto SHALL NOT replicate the defects of this requirement: it SHALL write each split side with its own direction, the instance transform, array, lock and path, label offsets converted like sizes, quoted texts, the port uses, and dimension properties in internal units (see Q-CELL-14).

**4. Labels.** For each label:
- `box values ...`.
- `label <text> <c|n|ne|e|se|s|sw|w|nw> <layer>` for a default-font label, or `label <text> <font> <size/8> <rot> <ox> <oy> <pos> [-]<layer>` for a rendered-font label, where `-` marks a sticky label.
- For ports:
  - `select area label`, if the label is not sticky;
  - `port make <index>`;
  - `port connections` followed by ` n`, ` s`, ` e` and ` w` for the directions that are set, then a newline;
  - optional `port <index> use <use>`, `port <index> class <class>` and `port <index> shape <shape>`.

**5. Properties.**
- `property string <key> "<value with \" escaped>"`
- `property integer <key> <v> ... `
- `property dimension <key> <v in current display units> ... `
- `property <key> <plane coordinates> `
- `property double <key> <v> ... `

Elements are not written.

**6. Epilogue.** `select clear`, `view`, `tech revert`, `units {*}$curunits`, `resumeall`.

(Magic ref: database/DBio.c:5131-5568)

#### Scenario: Script for a single rectangle
- **GIVEN** cell `a` containing only metal1 at (0,0)-(4,4)
- **WHEN** it is saved as `a.tcl`
- **THEN** the script SHALL contain `box values 0 0 4 4` followed by `paint metal1`, between the prologue and the epilogue
