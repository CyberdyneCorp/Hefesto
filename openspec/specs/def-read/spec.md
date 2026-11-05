# DEF Read

## Purpose

Defines how Hefesto reads a DEF (Design Exchange Format) file into a layout cell, compatibly with Magic's `def read` command. It covers the command and its options, the accepted DEF grammar, unit scaling and rounding, and how each DEF construct maps to the magic layout database. The constructs are design/die area, vias (rectangle-based and parameter-generated), components (subcell placement with the DEF lower-left convention), pins (port labels), regular and special net routing (widths, extensions, vias, non-default rules), and blockages. It also covers annotation mode and diagnostics.

Shared LEF/DEF machinery is specified in capability `lef-read` and is only referenced here. That machinery covers the tokenizer, the LEF-name-to-magic-type lookup, rectangle reading, via geometry storage and cut growing, and the shared non-default-rule table. Writing DEF is capability `def-write`. The tech-file `lef` section is capability `lef-tech-section`.

Notation used throughout:

- `oscale`: the DEF-database-units-per-internal-unit factor (float, see "Unit scaling").
- `round(v)`: round-half-away-from-zero of a single-precision float, then converted to integer.
- "Doubled units": an integer coordinate system at twice the internal-unit resolution, used for route centerlines and via geometry. Converting a doubled coordinate `d` back to internal units uses an arithmetic right shift (`floor(d / 2)`), except where noted.

## Requirements

### Requirement: def read command
The system SHALL provide the command `def read <filename> [option ...]`, where each option is recognised by a case-sensitive prefix comparison of the argument against a fixed prefix.
Compatibility: behavioral

- `-labels`: matched by any argument beginning with `-label`. Sets *dolabels*.
- `-annotate`: matched by any argument beginning with `-anno`. Sets *annotate*. *annotate* SHALL imply *dolabels*.
- `-noblockage`: matched by any argument beginning with `-noblock`. Sets *noblockage*.
- `-import`: matched by any argument beginning with `-import`. It SHALL be accepted and ignored for DEF.
- Options are examined only when at least one argument follows the file name. Arguments that begin with `-` and match none of the prefixes, and extra arguments that do not begin with `-`, SHALL be ignored silently.
- If no file name is given (`def read` alone), the command SHALL print the `def` option summary and do nothing else.
- The subcommand word itself (`read`) is matched by unique-prefix lookup against the table `read`, `write`, `writeall`, `nocheck`, `datestamp`, `help`. `def nocheck` and `def datestamp` SHALL print "The "nocheck" option is only for LEF writes." and "The "datestamp" option is only for LEF reads." respectively. `def writeall` is specified in `def-write`.
- On success, the cell that received the design SHALL be returned. If exactly one layout window exists, the design SHALL be loaded into it.

(Magic ref: lef/lefCmd.c:78-273, lef/defRead.c:2456-2537)

#### Scenario: Option prefixes
- **GIVEN** a DEF file `top.def`
- **WHEN** the command `def read top -lab -noblockageXYZ -foo` is run
- **THEN** *dolabels* SHALL remain unset, because `-lab` does not begin with `-label`
- **AND** *noblockage* SHALL be set, because `-noblockageXYZ` begins with `-noblock`
- **AND** `-foo` SHALL be ignored without a message

#### Scenario: Annotate implies labels
- **WHEN** `def read top -annotate` is run
- **THEN** the read SHALL behave as though both *annotate* and *dolabels* were given

### Requirement: Command context (informative)
Magic SHALL require a target window or box context before any non-help `def` subcommand, including `def read`.
Compatibility: informative

When no cell is selected, no layout window exists and no box tool cell exists, Magic prints "Point to a window first" and aborts. In the Tcl build, the read also restricts the event loop so that key and button events are not processed while it runs, and it prints progress through a periodic timer (about every 5 seconds: "  Processed N of M <items> (P%).  Est. time remaining: Ts"). Hefesto MAY instead run headless and report progress in any form. See Q-LEFDEF-86 and Q-LEFDEF-88.

(Magic ref: lef/lefCmd.c:200-219, lef/defRead.c:2537-2547, lef/lefRead.c:87-140)

#### Scenario: No window context
- **GIVEN** Magic with no layout window, no selection and no box
- **WHEN** `def read top` is run
- **THEN** Magic prints "Point to a window first" and reads nothing

### Requirement: DEF file lookup
The system SHALL resolve the DEF file name as follows, where the basename is the part after the last `/` and its extension is the part from the last `.` in the basename.
Compatibility: behavioral

1. If the basename has an extension other than `.def`, try the name exactly as given. If that fails, strip the extension and try `<stripped>.def`.
2. If the extension is `.def`, try the name as given.
3. If there is no extension, try `<name>.def` only. The bare name SHALL NOT be tried.

Each try searches the cell search path and then the cell library path (see capability `cell-library-resolution`). If no file opens, the system SHALL print "Cannot open input file <name> (<system error text>)." and return no cell. On success it SHALL print "Reading DEF data from file <resolved name>." and "This action cannot be undone.". Undo recording SHALL be disabled for the whole read and re-enabled afterwards. The read is not undoable.

(Magic ref: lef/lefWrite.c:150-250, lef/defRead.c:2518-2552, lef/defRead.c:2760)

#### Scenario: Unusual extension
- **GIVEN** files `chip.txt` and `chip.def` both exist
- **WHEN** `def read chip.txt` is run
- **THEN** `chip.txt` SHALL be read

#### Scenario: No extension
- **GIVEN** only a file named `chip` (no extension) exists
- **WHEN** `def read chip` is run
- **THEN** the open SHALL fail, because only `chip.def` is tried

### Requirement: Tokenization and keyword matching
The system SHALL tokenize DEF input with the LEF/DEF tokenizer of capability `lef-read`, and match every DEF keyword in a keyword table by full-length, case-insensitive comparison.
Compatibility: exact

The tokenizer rules from `lef-read` are summarised here:

- Tokens are separated by whitespace.
- A token that begins with `#` starts a comment that runs to end of line.
- A double-quoted string is one token, including its quotes. A quote preceded by a backslash does not terminate the string.
- Lines are read in chunks of at most 2048 characters.
- Parentheses and `;` are only recognised as separate tokens when surrounded by whitespace. For example, `(10` is one token that does not start with a lone `(`.

Keywords in tables (section names, `+` properties, orientations, pin classes and uses) match by full-length case-insensitive comparison. Prefixes are not accepted, and an exact case-sensitive match is preferred. The following tokens are compared case-sensitively instead:

- The route keywords `NEW` (also `new`), `RECT`, `POLYGON`, `VIRTUAL`, `TAPER`, `TAPERRULE`, `DEFAULT` (within TAPERRULE) and `PIN` (within net connections).
- Layer, via, rule, cell and net names. For layer lookup, see "Route layer resolution".

The input line counter SHALL start at 0 for each read. The total line count SHALL be reported at the end as "DEF read: Processed <n> lines.".

(Magic ref: lef/lefRead.c:160-247, utils/lookupfull.c:52-75, lef/defRead.c:2556, lef/defRead.c:2748)

#### Scenario: Case-insensitive section names
- **WHEN** the file contains `components 1 ;` instead of `COMPONENTS 1 ;`
- **THEN** the COMPONENTS section SHALL be recognised

#### Scenario: Case-sensitive NEW
- **WHEN** a route contains `New met2 ( 0 0 ) ( 0 100 )`
- **THEN** `New` SHALL NOT start a new route leg; it SHALL be treated as a via name (see "Route via placement")

### Requirement: Top-level DEF grammar and section dispatch
The system SHALL accept the following top-level DEF structure, processing statements in file order and stopping at `END DESIGN`.
Compatibility: exact

```ebnf
def_file      = { statement } , [ "END" , "DESIGN" ] ;
statement     = skip_stmt | skip_section | units_stmt | design_stmt
              | tech_stmt | diearea_stmt | components | vias | pins
              | specialnets | nets | nondefaultrules | blockages | unknown ;
skip_stmt     = ( "VERSION" | "NAMESCASESENSITIVE" | "ROW" | "TRACKS"
              | "GCELLGRID" | "DIVIDERCHAR" | "BUSBITCHARS" | "HISTORY" ) ,
                { token } , ";" ;
skip_section  = ( "REGIONS" | "PROPERTYDEFINITIONS" | "DEFAULTCAP"
              | "PINPROPERTIES" | "IOTIMINGS" | "SCANCHAINS" | "CONSTRAINTS"
              | "GROUPS" ) , { token } , "END" , same_section_name
              | "BEGINEXT" , { token } , ( "ENDEXT" | "END" , "BEGINEXT" ) ;
units_stmt    = "UNITS" , token , token , integer , { token } , ";" ;
                (* normally: UNITS DISTANCE MICRONS n ; the two words are not checked *)
design_stmt   = "DESIGN" , name , { token } , ";" ;
tech_stmt     = "TECHNOLOGY" , name , { token } , ";" ;
diearea_stmt  = "DIEAREA" , point , point , { token } , ";" ;
point         = "(" , number , number , ")" | number , number ;
unknown       = token , { token } , ";" ;
```

The section bodies `components`, `vias`, `pins`, `specialnets`, `nets`, `nondefaultrules` and `blockages` are defined in their own requirements. Each of them begins with `<SECTION> <count> {token} ;` and ends with `END <SECTION>`.

- A skipped statement SHALL be consumed up to and including the next `;` token.
- A skipped section SHALL be consumed up to the matching `END <name>`. An `END` followed by another `END` SHALL cause the search to continue. If the input ends first, the system SHALL report the error "Section <name> has no END record!".
- A section count that is not an integer SHALL be taken as 0.
- An unknown top-level keyword SHALL produce the message "Unknown keyword "<tok>" in DEF file; ignoring." and the statement SHALL be skipped to the next `;`. This applies to `FILLS`, `SLOTS`, `STYLES` and other unsupported sections; see Q-LEFDEF-84.
- `END` followed by anything other than `DESIGN` SHALL produce the error "END statement out of context." and reading SHALL continue.
- Content after `END DESIGN` SHALL be ignored.
- The output-related semantics of `VERSION`, `BUSBITCHARS` and `DIVIDERCHAR` SHALL be ignored. In particular, bus-bit and divider characters do not change name handling.

(Magic ref: lef/defRead.c:2462-2747, lef/lefRead.c:401-505)

#### Scenario: Skipped sections
- **GIVEN** a DEF file containing `ROW ROW_0 core 0 0 N DO 10 BY 1 STEP 460 0 ;` and a `GROUPS 1 ; - g1 u1 ; END GROUPS`
- **WHEN** it is read
- **THEN** neither construct SHALL change the layout, and no messages SHALL be produced for them

#### Scenario: Unsupported FILLS section (verify against Magic)
- **GIVEN** `FILLS 1 ; - LAYER met1 RECT ( 0 0 ) ( 10 10 ) ; END FILLS`
- **WHEN** it is read
- **THEN** the message `Unknown keyword "FILLS" in DEF file; ignoring.` SHALL be produced, followed by `Unknown keyword "-" ...` for the fill statement
- **AND** `END FILLS` SHALL produce the error "END statement out of context.", after which reading continues

### Requirement: Unit scaling
The system SHALL convert every DEF distance `v` (in DEF database units) to internal units as `round(v / oscale)`, where `oscale` is a single-precision float.
Compatibility: exact

`oscale` is computed as follows:

1. At the start of the read, `oscale = s0`, where `s0 = (10 · scalefactor) / (expander · 1000)` of the current CIF/GDS output style (microns per internal unit). If no output style exists, `s0 = 1.0`.
2. On each `UNITS` statement, the third token after `UNITS` is parsed as an integer `n`, and `oscale` SHALL be multiplied by `n`. The two intermediate words are not checked. If the value is not an integer, the system SHALL report the error "Invalid syntax for UNITS statement.", then the message "Assuming default value of 100", and use `n = 100`.

Other rules:

- Each further `UNITS` statement multiplies again; see Q-LEFDEF-79.
- If no `UNITS` statement precedes a geometric construct, DEF numbers are interpreted as microns.
- DEF reading SHALL NOT change the internal grid or rescale the database. Values that are not integral in internal units are rounded, never trimmed or grid-scaled.
- Point coordinates, rectangle corners, wire widths, placement origins and non-default-rule widths and spacings use `round(v / oscale)`.
- Route centerline points and extensions use doubled units, `round(2·v / oscale)`.
- Via rectangle coordinates are stored in doubled units, `round(v / (oscale/2))`.

(Magic ref: lef/defRead.c:2555, lef/defRead.c:2604-2618, cif/CIFmain.c:138-146, lef/lefRead.c:868-931)

#### Scenario: Nanometre internal grid
- **GIVEN** an output style with 1 internal unit = 1 nm (`s0 = 0.001`) and `UNITS DISTANCE MICRONS 1000 ;`
- **WHEN** the coordinate `1235` is converted
- **THEN** `oscale` SHALL be `1.0` and the result SHALL be `1235`

#### Scenario: Rounding half away from zero
- **GIVEN** `s0 = 0.01` (10 nm per internal unit) and `UNITS DISTANCE MICRONS 1000 ;`, so `oscale = 10`
- **WHEN** the coordinates `1235` and `-1235` are converted
- **THEN** the results SHALL be `124` and `-124`

#### Scenario: No UNITS statement
- **GIVEN** `s0 = 0.001` and no UNITS statement
- **WHEN** the DIEAREA `( 0 0 ) ( 10 20 )` is read
- **THEN** the die SHALL span `0..10000` by `0..20000` internal units, with the numbers treated as microns

### Requirement: Design cell selection
The system SHALL paint all DEF content into a single design cell, chosen as described below.
Compatibility: behavioral

- `DESIGN <name> ;`: if no design cell has been chosen yet, look up a cell named `<name>`. If none exists, create a new, empty, available cell with that name. If a design cell was already chosen (because content came before `DESIGN`), rename it to `<name>`.
- When DIEAREA, COMPONENTS, PINS, SPECIALNETS, NETS, NONDEFAULTRULES or BLOCKAGES is reached before any `DESIGN` statement, the design cell SHALL be looked up or created under the name exactly as passed to `def read` (including any directory or extension text). See Q-LEFDEF-78.
- An existing cell SHALL NOT be cleared. DEF content SHALL be added to whatever it already contains.
- `TECHNOLOGY <name>`: if `<name>` differs (case-sensitive) from the current technology name, the system SHALL report the warning "DEF technology name "<name>" does not match current magic technology name "<tech>"" and continue.
- If the file never selects a design cell, the system SHALL print "DEF read: Design has no contents." and return no cell.

(Magic ref: lef/defRead.c:2410-2430, lef/defRead.c:2577-2603, lef/defRead.c:2752-2759)

#### Scenario: Design name
- **GIVEN** no cell named `picorv` is loaded
- **WHEN** a DEF file with `DESIGN picorv ;` is read
- **THEN** a new cell `picorv` SHALL receive all geometry

#### Scenario: Re-reading into an existing cell
- **GIVEN** cell `picorv` already contains a met1 rectangle
- **WHEN** a DEF with `DESIGN picorv ;` is read
- **THEN** the existing rectangle SHALL remain, and the DEF geometry SHALL be added

### Requirement: Die area
The system SHALL read `DIEAREA` as a rectangle and store it on the design cell as the property `FIXED_BBOX`, a dimension-typed property of four integers `xbot ybot xtop ytop` in internal units.
Compatibility: exact

- The rectangle uses the first two points read; the corners are canonicalised so that `xbot ≤ xtop` and `ybot ≤ ytop`.
- Points MAY be written with or without parentheses.
- Setting the property SHALL mark the cell as having a fixed bounding box (see capability `layout-properties`).
- Any further points (a polygonal die) SHALL be skipped up to `;`. See Q-LEFDEF-77.
- A malformed point SHALL report the error "Bad port geometry: RECT requires 4 values.". See Q-LEFDEF-68 for Magic's subsequent crash.

(Magic ref: lef/defRead.c:2637-2650, lef/lefRead.c:868-931, database/DBprop.c:89-95)

#### Scenario: Die area property
- **GIVEN** `oscale = 1`
- **WHEN** `DIEAREA ( 0 0 ) ( 200000 150000 ) ;` is read
- **THEN** the design cell SHALL have `FIXED_BBOX` = `0 0 200000 150000`

#### Scenario: Reversed corners
- **WHEN** `DIEAREA ( 100 100 ) ( 0 0 ) ;` is read with `oscale = 1`
- **THEN** `FIXED_BBOX` SHALL be `0 0 100 100`

### Requirement: VIAS section grammar
The system SHALL read the VIAS section with the following grammar and record each via definition in the shared LEF/DEF via table of capability `lef-read`, keyed by via name.
Compatibility: exact

```ebnf
vias        = "VIAS" , count , { token } , ";" , { via_def } , "END" , "VIAS" ;
via_def     = "-" , vianame , { via_prop } , ";" ;
via_prop    = "+" , ( "RECT" , layer , point , point
                    | "VIARULE" , rulename
                    | "CUTSIZE" , int , int
                    | "LAYERS" , layer , layer , layer
                    | "CUTSPACING" , int , int
                    | "ENCLOSURE" , int , int , int , int
                    | "ROWCOL" , int , int
                    | other_keyword , { token } )
            | non_plus_token ;   (* tokens not introduced by "+" are ignored *)
```

- `VIARULE <name>` SHALL be read and ignored. Missing generation parameters SHALL NOT be filled from any rule.
- `CUTSIZE`, `CUTSPACING`, `ENCLOSURE` and `ROWCOL` values SHALL be parsed as integers in DEF database units. A non-integer token SHALL report the error "Invalid syntax for CUTSIZE." (respectively CUTSPACING, ENCLOSURE or ROWCOL). For ROWCOL, the failing value SHALL be set to 1.
- `ORIGIN`, `OFFSET`, `PATTERN`, `POLYGON`, `MASK` and any other `+` keyword SHALL report the message "Unknown via property "<kw>" in VIAS definition; ignoring.". Its arguments are skipped until the next `+` or `;`. See Q-LEFDEF-62.
- A statement that does not start with `-` or `END` SHALL report the message "Unknown keyword "<tok>" in VIAS definition; ignoring." and be skipped to `;`.
- At the start of each via definition, ROWCOL SHALL default to 1 row and 1 column.

(Magic ref: lef/defRead.c:1902-2170)

#### Scenario: Unsupported via property
- **WHEN** a via contains `+ ORIGIN 10 10`
- **THEN** the message `Unknown via property "ORIGIN" in VIAS definition; ignoring.` SHALL be produced, and the via SHALL be built as if ORIGIN were absent

### Requirement: Rectangle-based DEF vias
For each `+ RECT <layer> <pt> <pt>` in a via definition, the system SHALL resolve the layer as a non-obstruction layer (see `lef-read` layer lookup) and add the rectangle to the via's geometry exactly as a LEF VIA `RECT` is added (capability `lef-read`).
Compatibility: exact

The geometry is built as follows:

- The rectangle is converted to doubled units with `round(v / (oscale/2))` and canonicalised.
- If the layer is a contact type, the cut-growing rule of `lef-read` is applied.
- The first rectangle becomes the via's primary area and primary type.
- Each later rectangle is added to the via's secondary rectangle list. If a later rectangle's layer is a contact and the current primary type is not, the two are swapped, so that the contact becomes the primary type.
- A rectangle on an unknown layer SHALL produce the `lef-read` layer error and SHALL add no geometry.
- Coordinates are relative to the via's placement point.

(Magic ref: lef/defRead.c:2017-2022, lef/lefRead.c:2646-2685)

#### Scenario: Three-rectangle via
- **GIVEN** `oscale = 1`, LEF names `met1`, `via1` and `met2` mapped to magic types `metal1`, `m2contact` and `metal2`, and cut growing does not apply
- **WHEN** `- V12 + RECT met1 ( -100 -100 ) ( 100 100 ) + RECT via1 ( -50 -50 ) ( 50 50 ) + RECT met2 ( -120 -80 ) ( 120 80 ) ;` is read
- **THEN** the primary type SHALL be `m2contact` with doubled area `(-100,-100)-(100,100)`
- **AND** the secondary list SHALL hold `metal1` `(-200,-200)-(200,200)` and `metal2` `(-240,-160)-(240,160)`, all in doubled units

### Requirement: Generated DEF vias
When a via definition has used any of `CUTSIZE`, `LAYERS`, `CUTSPACING`, `ENCLOSURE` or `ROWCOL`, the system SHALL generate the via geometry at the terminating `;` from the parameters, as defined below. All parameters are integers in DEF units.
Compatibility: exact

Parameters:

- `sx, sy` (cut size)
- `px, py` (cut spacing)
- `ebx, eby, etx, ety` (bottom and top enclosure)
- `R, C` (rows, columns)
- layers `B, K, T` (bottom, cut, top)

Layer order: if the middle layer `K` of `LAYERS` is not a contact type, the system SHALL report the warning "Improper layer order for VIARULE.". It SHALL then swap `K` with `T` if `T` is a contact, otherwise swap `K` with `B` if `B` is a contact. If neither is a contact, it SHALL report the error "No cut layer specified in VIARULE.".

Geometry (doubled units):

```
Wc = sx*C + px*(C-1)        Hc = sy*R + py*(R-1)       (cut-array size, DEF units)
top    : x in [-round((Wc+2*etx)/oscale), +round((Wc+2*etx)/oscale)],
         y in [-round((Hc+2*ety)/oscale), +round((Hc+2*ety)/oscale)]   -> primary area, primary type = T
bottom : same with ebx, eby                                            -> secondary rect, type B
cuts   : x0 = trunc(-Wc/2), y0 = trunc(-Hc/2)   (C integer division, toward zero)
         for i in 0..C-1 (columns, x), for j in 0..R-1 (rows, y):
            x = x0 + i*(sx+px);  y = y0 + j*(sy+py)
            xbot = round(x/(oscale/2)); ybot = round(y/(oscale/2))
            xtop = xbot + round(sx/(oscale/2)); ytop = ybot + round(sy/(oscale/2))
            apply lef-read cut growing for type K; add as secondary rect, type K
```

Because a full extent `E/oscale` in internal units equals the half-extent in doubled units, the top and bottom rectangles are centred on the via origin.

- The primary type of a generated via SHALL be the top layer `T`, not the cut. See Q-LEFDEF-63.
- Secondary rectangles are prepended to the list, so the list order is the last cut generated first, then earlier cuts, then the bottom rectangle.
- Parameters not given in the definition SHALL keep whatever value they last had in this read. In Magic, a parameter never set in the read is undefined; see Q-LEFDEF-61.
- The "generated" condition SHALL persist for all later via definitions in the same VIAS section, including purely rectangle-based ones. See Q-LEFDEF-60.

(Magic ref: lef/defRead.c:1946-2160, lef/lefRead.c:2545-2632)

#### Scenario: 1x2 generated via
- **GIVEN** `oscale = 1`, cut growing does not apply, and `- VG + VIARULE M1M2 + CUTSIZE 100 100 + LAYERS met1 via1 met2 + CUTSPACING 100 100 + ENCLOSURE 50 20 20 50 + ROWCOL 1 2 ;`
- **WHEN** the via is generated
- **THEN** the primary area SHALL be `metal2`, doubled `(-340,-200)-(340,200)`, which is internal `(-170,-100)-(170,100)`
- **AND** the bottom rectangle SHALL be `metal1`, doubled `(-400,-140)-(400,140)`
- **AND** the two cuts SHALL be doubled `(-300,-100)-(-100,100)` and `(100,-100)-(300,100)`, which is internal `(-150,-50)-(-50,50)` and `(50,-50)-(150,50)`

#### Scenario: Swapped LAYERS order
- **WHEN** `+ LAYERS met1 met2 via1` is given
- **THEN** the warning "Improper layer order for VIARULE." SHALL be reported, and the cut SHALL be `via1` with top `met2`

### Requirement: Via redefinition
When a VIAS definition names a via that already exists in the shared via table (from LEF or an earlier DEF), the system SHALL report the warning "Composite via "<name>" redefined." and discard the previous geometry before reading the new one.
Compatibility: behavioral

If several names share the previous record, only the redefined name SHALL get a fresh record; the others SHALL keep the old geometry (capability `lef-read`, redefinition rule).

(Magic ref: lef/defRead.c:1977-1995, lef/lefRead.c:611-690)

#### Scenario: DEF overrides LEF via
- **GIVEN** LEF defined via `V12`
- **WHEN** a DEF VIAS section defines `V12` again
- **THEN** the warning SHALL be issued, and routes SHALL use the DEF geometry

### Requirement: COMPONENTS section
The system SHALL read COMPONENTS with the grammar below and create one subcell use in the design cell per component whose master cell can be found.
Compatibility: behavioral

```ebnf
components = "COMPONENTS" , count , { token } , ";" , { comp } , "END" , "COMPONENTS" ;
comp       = "-" , instname , mastername , { comp_prop | non_plus_token } , ";" ;
comp_prop  = "+" , ( ( "PLACED" | "FIXED" | "COVER" ) , "(" , num , num , ")" , orient
                   | "UNPLACED"
                   | ( "SOURCE" | "WEIGHT" | "FOREIGN" | "REGION" | "GENERATE"
                     | "PROPERTY" | "EEQMASTER" ) , token
                   | other_keyword ) ;
orient     = "N" | "S" | "E" | "W" | "FN" | "FS" | "FE" | "FW" ;
```

Instance names:

- Every `/` or `,` in the instance name SHALL be replaced by `_`, and the warning "Character in instance name converted to underscore." SHALL be reported once per replaced character.
- An empty instance name SHALL report the error "Bad component statement:  Need use and macro names" and skip the statement.

Masters:

- The master SHALL be the loaded cell with exactly that name, for example one created by `lef read`.
- If no such cell is loaded, the system SHALL try to read `<mastername>.mag` from the cell search path, ignoring tech mismatches, and recompute its bounding box.
- If that also fails, it SHALL report the error "Cell <master> is not defined.  Maybe you have not read the corresponding LEF file?", discard the placeholder cell, skip the rest of the statement and create no use.

Properties and placement:

- `+ SOURCE`, `+ WEIGHT`, `+ FOREIGN`, `+ REGION`, `+ GENERATE`, `+ PROPERTY` and `+ EEQMASTER` SHALL consume exactly one following token and be ignored.
- An unknown `+` keyword SHALL produce the message "Unknown component property "<kw>" in COMPONENT definition; ignoring.".
- After the statement, the use SHALL be placed into the design cell with the transform computed by "Component placement transform".
- If the instance name equals an existing use in the design cell, Magic's behavior is undefined; see Q-LEFDEF-83.
- In annotation mode, the whole COMPONENTS section SHALL be skipped.

(Magic ref: lef/defRead.c:2184-2404, lef/defRead.c:2657-2669, database/DBcellname.c:2401-2415)

#### Scenario: Slash in instance name
- **WHEN** `- core/u1 sky130_fd_sc_hd__inv_1 + PLACED ( 0 0 ) N ;` is read
- **THEN** a use named `core_u1` SHALL be created, and the warning SHALL be reported once

#### Scenario: Missing master
- **GIVEN** no cell `FOO` is loaded and no `FOO.mag` exists
- **WHEN** `- u1 FOO + PLACED ( 0 0 ) N ;` is read
- **THEN** the error "Cell FOO is not defined.  Maybe you have not read the corresponding LEF file?" SHALL be reported, and no use SHALL be created

### Requirement: Component placement transform
For a placement `( X Y ) <orient>`, the system SHALL compute the subcell transform so that the lower-left corner of the transformed master bounding box lands exactly on `(round(X/oscale), round(Y/oscale))` (the DEF lower-left convention).
Compatibility: exact

Magic transforms are written as `(a b c / d e f)`, mapping `x' = a·x + b·y + c` and `y' = d·x + e·y + f`. The base orientation matrices are:

| DEF | x' | y' | (a b d e) |
|---|---|---|---|
| N  | x  | y  | 1 0 0 1 |
| S  | −x | −y | −1 0 0 −1 |
| E  | y  | −x | 0 1 −1 0 |
| W  | −y | x  | 0 −1 1 0 |
| FN | −x | y  | −1 0 0 1 |
| FS | x  | −y | 1 0 0 −1 |
| FE | −y | −x | 0 −1 −1 0 |
| FW | y  | x  | 0 1 1 0 |

Algorithm:

1. Let `bb` be the master's `FIXED_BBOX` property when the master has a fixed bounding box and the property holds four dimension values. Otherwise `bb` is the master's computed bounding box.
2. Let `O` be the orientation matrix and `tr = O(bb)`, canonicalised.
3. The transform is `O`, followed by a translation of `(−tr.xbot, −tr.ybot)`, followed by a translation of `(round(X/oscale), round(Y/oscale))`.

Other rules:

- An orientation token not in the table SHALL report the error "Unknown macro orientation "<tok>"." and leave the use at its default (identity) transform.
- A malformed location SHALL report the error "Cannot parse location: must be ( X Y ) orient" and leave the use at its default transform.
- If a statement has more than one placement keyword, the last one wins.

(Magic ref: lef/defRead.c:1306-1421, utils/geometry.c:39-51)

#### Scenario: South orientation
- **GIVEN** `oscale = 1` and master bounding box `(0,0)-(100,200)`
- **WHEN** `+ PLACED ( 1000 2000 ) S` is read
- **THEN** the transform SHALL be `(-1 0 1100 / 0 -1 2200)`, and the placed bounding box SHALL be `(1000,2000)-(1100,2200)`

#### Scenario: East orientation
- **GIVEN** `oscale = 1` and master bounding box `(0,0)-(100,200)`
- **WHEN** `+ FIXED ( 1000 2000 ) E` is read
- **THEN** the transform SHALL be `(0 1 1000 / -1 0 2100)`, and the placed bounding box SHALL be `(1000,2000)-(1200,2100)`

#### Scenario: FIXED_BBOX takes precedence
- **GIVEN** master `inv` with computed bounding box `(-10,-5)-(110,205)` and `FIXED_BBOX 0 0 100 200`
- **WHEN** `+ PLACED ( 0 0 ) N` is read with `oscale = 1`
- **THEN** the transform SHALL be the identity, so that the master's `(0,0)` lands at `(0,0)`

### Requirement: Unplaced components
`+ UNPLACED` SHALL place the component at the origin with orientation N, applying the lower-left convention, and SHALL report the warning "Unplaced component "<instname>" will be put at origin.".
Compatibility: exact

A component statement with no placement keyword at all SHALL be placed with the identity transform, without lower-left correction. See Q-LEFDEF-85.

(Magic ref: lef/defRead.c:1324-1331, lef/defRead.c:2344-2349)

#### Scenario: Unplaced
- **GIVEN** master bounding box `(-10,-10)-(90,190)`
- **WHEN** `- u7 inv + UNPLACED ;` is read
- **THEN** the use transform SHALL translate by `(10,10)`, so that its bounding box is `(0,0)-(100,200)`, and the warning SHALL be reported

### Requirement: PINS section grammar
The system SHALL read the PINS section with the following grammar.
Compatibility: exact

```ebnf
pins      = "PINS" , count , { token } , ";" , { pin } , "END" , "PINS" ;
pin       = "-" , pinname , { pin_prop | non_plus_token } , ";" ;
pin_prop  = "+" , ( "NET" , netname
                  | "SPECIAL"
                  | "DIRECTION" , pin_dir
                  | "USE" , pin_use
                  | "LAYER" , layer , rect_points
                  | ( "PLACED" | "FIXED" | "COVER" ) , "(" , num , num , ")" , orient
                  | "PORT"
                  | other_keyword ) ;
rect_points = point , point ;
pin_dir   = "DEFAULT" | "INPUT" | "OUTPUT" | "INOUT" | "FEEDTHRU" ;
pin_use   = "DEFAULT" | "SIGNAL" | "POWER" | "GROUND" | "CLOCK" | "RESET"
          | "ANALOG" | "SCAN" | "TIEOFF" ;
```

- The pin name is read as at most 2047 characters.
- `NET` SHALL consume the net name and ignore it. The label text is always the pin name.
- `SPECIAL` SHALL be ignored.
- `LAYER <name>` SHALL resolve the layer as a non-obstruction layer (`lef-read` layer lookup) and then read a rectangle. Anything between the layer name and the first point (for example `MASK n`, `SPACING d` or `DESIGNRULEWIDTH d`) SHALL cause a rectangle parse error.
- `POLYGON`, `VIA` and other `+` keywords SHALL report the message "Unknown pin property "<kw>" in PINS definition; ignoring.". Their arguments are skipped up to the next `+`. See Q-LEFDEF-66.
- An unknown direction SHALL report the error "Unknown pin class "<tok>"". An unknown use SHALL report the error "Unknown pin use "<tok>"". The token `OUTPUT TRISTATE` exists in the class table but can never match a single token.
- Each `+ PORT` after the first in a pin SHALL increment the processed-pin counter, so that a multi-port pin counts as one pin per port.

(Magic ref: lef/defRead.c:1451-1775)

#### Scenario: Multi-port pin count
- **GIVEN** `PINS 2 ;` followed by one pin `- vdd + NET vdd + PORT + LAYER met1 ( 0 0 ) ( 10 10 ) + FIXED ( 0 0 ) N + PORT + LAYER met1 ( 0 0 ) ( 10 10 ) + FIXED ( 100 0 ) N ;`
- **WHEN** it is read
- **THEN** the processed count SHALL be 2, and no count-mismatch warning SHALL be reported

### Requirement: Pin geometry placement
The system SHALL accumulate pin LAYER rectangles in a pending list and paint them into the design cell when a placement is known.
Compatibility: exact

Each painted rectangle is the stored rectangle transformed by the pin placement transform. That transform is the orientation matrix of "Component placement transform" followed by a translation to `(round(X/oscale), round(Y/oscale))`, with no lower-left correction, because the reference box is the empty box at the origin.

The order of events is as follows:

- On `+ LAYER`, the rectangle is pushed onto the front of the pending list. If a placement was already seen for this pin and is waiting for a layer, the whole pending list SHALL be painted and labelled immediately with that placement.
- On `+ PLACED`, `+ FIXED` or `+ COVER`, the placement transform is computed. If no LAYER has been seen yet in this pin, the placement is marked as waiting. Otherwise the whole pending list SHALL be painted and labelled now.
- Each painting operation empties the pending list and increments the pin port index by 1. The index starts at 0 for the read, counts across all pins, and is never reset.
- A pin that ends while a placement is still waiting for a layer SHALL report the error "Pin specified without layer, was not placed." when the next pin starts.
- The pending list and the "layer seen" state SHALL NOT be cleared between pins in the way the DEF standard implies. Rectangles left unpainted at the end of a pin SHALL be painted with the next pin's placement and name. See Q-LEFDEF-65.
- Pin geometry SHALL be painted in annotation mode too. See Q-LEFDEF-80.

(Magic ref: lef/defRead.c:1625-1730)

#### Scenario: Pin with orientation
- **GIVEN** `oscale = 1`
- **WHEN** `- a + NET a + DIRECTION INPUT + USE SIGNAL + LAYER met2 ( -70 0 ) ( 70 140 ) + PLACED ( 100 100 ) S ;` is read
- **THEN** metal2 SHALL be painted at `(30,-40)-(170,100)`, which is the rectangle rotated 180° about the origin and translated by `(100,100)`

#### Scenario: Placement before layer
- **WHEN** `- b + PLACED ( 500 0 ) N + LAYER met1 ( 0 0 ) ( 20 20 ) ;` is read with `oscale = 1`
- **THEN** metal1 SHALL be painted at `(500,0)-(520,20)` with a port label `b`

### Requirement: Pin port labels
For each painted pin rectangle `R` on type `t`, the system SHALL create a label on `R` with text equal to the pin name, as described below.
Compatibility: exact

Label attributes:

- Type `t`, center justification, and port index equal to the current pin port index.
- Flags: all four port-direction bits; the sticky flag (only when the layer resolved to a known type); the class bits from DIRECTION; and the use bits from USE (see capability `layout-labels-ports` for the bit values).

Rendered-font attributes. These apply only when at least one rendering font is loaded; otherwise font is −1, size 0, rotation 0 and offset (0,0):

- font 0, offset (0,0)
- rotation: 90 if `height > 2·width`, otherwise 0. When rotated, width and height are swapped for the size computation.
- size: `s = default width of t from the DRC rules`; while `2·s < height`, `s = 2·s`; finally `size = 8·s`.

DIRECTION mapping: DEFAULT → default; INPUT → input; OUTPUT → output; INOUT → bidirectional; FEEDTHRU → feedthrough.

USE mapping: each value maps to the same-named port use.

The DIRECTION and USE values persist from one pin to the next until changed, so a pin without DIRECTION inherits the previous pin's direction. See Q-LEFDEF-64.

In annotation mode, every existing label in the design cell whose rectangle equals `R` exactly and whose text equals the pin name SHALL be erased before the new label is added.

If the pin layer is unknown, the label is non-sticky and its rectangle is degenerate at `(0,0)`. See Q-LEFDEF-67. If the type has no default width rule and the rectangle has non-zero height, Magic loops forever. See Q-LEFDEF-89.

(Magic ref: lef/defRead.c:1555-1590, lef/defRead.c:1640-1729, database/DBlabel.c:135-219, database/DBlabel.c:431-461, drc/DRCtech.c:4620-4665)

#### Scenario: Label size and rotation
- **GIVEN** a font is loaded, metal2 has DRC default width 140, and the painted rectangle is `(0,0)-(140,1000)`
- **WHEN** the port label is created
- **THEN** its rotation SHALL be 90, the post-swap height SHALL be 140, `s` SHALL stay 140 (because `2·140 ≥ 140`), and the size SHALL be `8·140 = 1120`

#### Scenario: Square pin label
- **GIVEN** a font is loaded, metal1 has default width 140, and the rectangle is `(0,0)-(600,600)`
- **WHEN** the label is created
- **THEN** rotation SHALL be 0, `s` SHALL go `140 → 280 → 560` (stopping because `1120 ≥ 600`), and the size SHALL be 4480

### Requirement: NETS and SPECIALNETS grammar
The system SHALL read NETS and SPECIALNETS with the following grammar.
Compatibility: exact

```ebnf
nets        = ( "NETS" | "SPECIALNETS" ) , count , { token } , ";" ,
              { net } , "END" , ( "NETS" | "SPECIALNETS" ) ;
net         = "-" , netname , { connection | token } , { net_prop } , ";" ;
connection  = "(" , ( compname , pinname | "PIN" , pinname ) , { token } , ")" ;
net_prop    = "+" , ( ( "ROUTED" | "FIXED" | "COVER" | "NOSHIELD" ) , routing
                    | "NONDEFAULTRULE" , rulename
                    | ( "SOURCE" | "USE" | "SHIELDNET" | "SUBNET" | "XTALK"
                      | "FREQUENCY" | "ORIGINAL" | "PATTERN" | "ESTCAP"
                      | "WEIGHT" | "VPIN" ) , token
                    | "PROPERTY" , token , token
                    | "FIXEDBUMP"
                    | other_keyword ) ;
(* regular-net routing: *)
routing     = leg , { "NEW" , leg } ;                  (* ends before "+" or ";" *)
leg         = layer , { route_elem } ;
(* special-net routing: *)
s_routing   = s_leg , { ( "NEW" , s_leg ) | s_plus } ;  (* ends only at ";" *)
s_leg       = layer , width , { route_elem } ;
s_plus      = "+" , ( "SHAPE" , shape | "STYLE" , int | "USE" , token
                    | "VOLTAGE" , token | "ORIGINAL" , token | "PATTERN" , token
                    | "ESTCAP" , token | "WEIGHT" , token | "PROPERTY" , token , token
                    | "FIXEDBUMP" | ( "ROUTED" | "FIXED" | "COVER" ) , s_leg
                    | "SHIELD" , netname , s_leg ) ;
shape       = "RING" | "PADRING" | "BLOCKRING" | "STRIPE" | "FOLLOWPIN" | "IOWIRE"
            | "COREWIRE" | "BLOCKWIRE" | "BLOCKAGEWIRE" | "FILLWIRE"
            | "FILLWIREOPC" | "DRCFILL" ;
route_elem  = "(" , coord , coord , [ number ] , ")"     (* point, optional extension *)
            | vianame
            | "RECT" , "(" , num , num , num , num , ")"
            | "VIRTUAL" | "TAPER" | "TAPERRULE" , rulename
            | "POLYGON" , token ;
coord       = number | "*" ;
```

- Connection lists SHALL be skipped, except in annotation mode (see "Net annotation mode").
- `SHAPE`, `STYLE`, `USE` and the other special-net keywords before the first routing statement are handled at the net level. Of these, `SHAPE`, `STYLE` and `VOLTAGE` SHALL report the message "Unknown net property "<kw>" in NET definition; ignoring.". After the first routing statement, they are handled inside special routing as `s_plus`.
- An unknown `s_plus` keyword or SHAPE value, or a non-numeric STYLE, SHALL report a message and skip input up to and including the next `;`. Routing reading then continues past that `;`. As a result, the following net statement is consumed as route tokens until its own `;`. See Q-LEFDEF-69.
- `VPIN` SHALL consume only one token. Its remaining fields are processed as ordinary tokens.
- A net-level `NONDEFAULTRULE <name>` applies to the next routing statement only. The rule SHALL be cleared at the start of each net and after each routing statement. See Q-LEFDEF-74. An unknown rule name SHALL report the error "Unknown nondefault rule "<name>"".
- The net count increments once per `-` statement.

(Magic ref: lef/defRead.c:62-300, lef/defRead.c:1049-1300)

#### Scenario: Routing then NONDEFAULTRULE
- **GIVEN** a nondefault rule `W2` that gives met1 width 280
- **WHEN** a net is written `- n1 ( u1 A ) + ROUTED met1 ( 0 0 ) ( 1000 0 ) + NONDEFAULTRULE W2 ;`
- **THEN** the met1 wire SHALL use the default met1 width, because the rule appears after the routing statement

### Requirement: Route layer resolution and width selection
At the start of each route leg (the first leg, or after `NEW`/`new`, or after `+ ROUTED|FIXED|COVER|SHIELD` in special nets), the system SHALL resolve the layer token and set the paint width `W` (internal units, full width) and default extension `Xd` (half-width extension, in doubled units) as described below.
Compatibility: exact

Layer resolution:

1. If the token names a record in the LEF/DEF layer table, the route type SHALL be that record's magic type. If the name exists in the table but has no record, the route type SHALL stay as it was before; see Q-LEFDEF-75.
2. Otherwise the route type SHALL be the magic type whose name equals the token converted to lower case.
3. If neither applies, the system SHALL report the error "Unknown layer type "<tok>" for NEW route" and continue with the next token.

The default width `Wdef` is the LEF layer record's route width (capability `lef-read` / `lef-tech-section`). If there is no record, `Wdef = (3 · λ1) / λ0` with integer arithmetic, where `λ1/λ0` is the database lambda ratio.

Width per net kind:

- Special nets: the token after the layer is a float `w`. Then `W = round(w / oscale)` if `w ≠ 0`, else `W = Wdef`, and `Xd = 0`. A non-numeric width SHALL report the error "Bad width in special net" and continue.
- Regular nets: if the current non-default rule set has an entry for this layer record, `W = rule.width`. Otherwise `W = Wdef`. In both cases `Xd = W`.
- `TAPER` SHALL set `W = Wdef`, and `Xd = W` for regular nets (0 for special nets).
- `TAPERRULE <name>`: if `<name>` is a known rule set with an entry for the current layer, `W = Xd = rule.width`, which also applies to special nets. If `<name>` is `DEFAULT`, the effect is the same as `TAPER`. Otherwise the system SHALL report the error "Unknown nondefault rule "<name>"".

(Magic ref: lef/defRead.c:146-219, lef/defRead.c:363-398, lef/lefTech.c:350-360)

#### Scenario: Special net width
- **GIVEN** `oscale = 1`
- **WHEN** `NEW met1 200 ( 0 0 ) ( 1000 0 )` is read in SPECIALNETS
- **THEN** `W = 200` and the extension SHALL be 0

#### Scenario: Zero special width
- **GIVEN** met1 has route width 140
- **WHEN** `+ ROUTED met1 0 ( 0 0 ) ( 0 500 )` is read
- **THEN** `W` SHALL be 140

### Requirement: Route wire segment geometry
The system SHALL convert consecutive route points into rectangles on the current route type, using the algorithm below in doubled units. The default extension `Xd` and width `W` are as defined in "Route layer resolution and width selection".
Compatibility: exact

```
on point "( x y [e] )":
   prev = P ; prevExt = ext                      # previous point/extension
   P.x = (x == "*") ? P.x : round(2*x/oscale)    # doubled units
   P.y = (y == "*") ? P.y : round(2*y/oscale)
   ext = (e given) ? round(2*e/oscale) : Xd
   if no valid previous point in this leg: mark valid; emit nothing
   else if prev.x != P.x and prev.y != P.y:
        error "Can't deal with nonmanhattan geometry in route."; emit nothing
        (the new point becomes the reference)
   else:
        (lo, hi) = canonical(prev, P)
        (extLo, extHi) = (P < prev in x or y) ? (ext, prevExt) : (prevExt, ext)
        if lo.x == hi.x: xbot = lo.x - W ; xtop = hi.x + W
        else:            xbot = lo.x - extLo ; xtop = hi.x + extHi
        if lo.y == hi.y: ybot = lo.y - W ; ytop = hi.y + W
        else:            ybot = lo.y - extLo ; ytop = hi.y + extHi
        emit rect (floor(xbot/2), floor(ybot/2), floor(xtop/2), floor(ytop/2))
```

- `W` is the full width in internal units, which equals the half-width in doubled units.
- A zero-length segment (a repeated point) SHALL emit a `W × W` square centred on the point. Extensions are ignored for it.
- `*` before any valid point in the leg SHALL report the error "No reference point for "*" wildcard". The point is then skipped.
- An unparsable coordinate SHALL report the error "Cannot parse X coordinate." (or Y), and the tokens are skipped up to `)`.
- An unparsable extension SHALL report the error "Can't parse route extension value." and the default extension is kept.
- A layer change via `NEW` invalidates the reference point.
- After a via, the reference point stays valid. The next segment starts at the via point, on the new layer, with the width and extension of the new layer.

(Magic ref: lef/defRead.c:529-675)

#### Scenario: Regular wire with default extension
- **GIVEN** `oscale = 1` and met1 width 140 in a regular net
- **WHEN** `met1 ( 0 0 ) ( 1000 0 )` is read
- **THEN** metal1 SHALL be painted at `(-70,-70)-(1070,70)`

#### Scenario: Explicit zero extension on first point
- **WHEN** `met1 ( 0 0 0 ) ( 1000 0 )` is read with the same setup
- **THEN** metal1 SHALL be painted at `(0,-70)-(1070,70)`

#### Scenario: Reversed vertical segment
- **WHEN** `met1 ( 0 1000 0 ) ( 0 0 )` is read with the same setup
- **THEN** metal1 SHALL be painted at `(-70,-70)-(70,1000)`, because the lower end takes the second point's default extension of 70

#### Scenario: Odd special width rounds asymmetrically
- **GIVEN** `oscale = 1`
- **WHEN** the SPECIALNETS leg `met1 3 ( 0 0 ) ( 10 0 )` is read
- **THEN** metal1 SHALL be painted at `(0,-2)-(10,1)`, because doubled y spans −3..3 and floor-halving gives −2..1

#### Scenario: Wildcard coordinate
- **WHEN** `met1 ( 0 0 ) ( 1000 * ) ( * 500 )` is read in a regular net with `oscale = 1` and width 140
- **THEN** two rectangles SHALL be emitted: `(-70,-70)-(1070,70)` and `(930,-70)-(1070,570)`

### Requirement: Route via placement
A route token that is not `(`, `+`, `;` or a recognised route keyword SHALL be treated as a via name placed at the current reference point.
Compatibility: exact

Error cases:

- If there is no valid reference point, the system SHALL report the error "Route has via name "<tok>" but no points!" and ignore the token.
- If the name is not in the LEF/DEF via/layer table, the system SHALL report the error "Via name "<tok>" unknown in route." and ignore the token.
- If the name exists in the table without a record but its lower-cased form names a magic type `t`, the system SHALL report the error "Error: Via "<tok>" named but undefined." and emit a square of side `W` on `t`, centred on the reference point (doubled `±W`, then floor-halved).

Geometry for a defined via, with the reference point `P` in doubled units:

- Each secondary rectangle `r` of the via SHALL be emitted, in list order, as `floor((r + P)/2)` with its own type.
- Then the primary area SHALL be emitted the same way, with the primary type.

Layer switch after a via, where `p` is the primary type of the via:

- If `p` is a contact type, or the via is defined, look at the residue (constituent) types of `p` in increasing type order. The route type SHALL become the first residue type that differs from the current route type.
- For a non-contact `p`, the residue set is `{p}`, so the route type changes only if `p` differs from the current route type. See Q-LEFDEF-63.
- When the route type changes, the system SHALL look up the LEF layer record for the new type (see Q-LEFDEF-82) and recompute `W`:
  - special nets: `W` is the width saved at the start of the leg, unchanged
  - regular nets: the non-default rule set's width for that layer, else that layer's default width
- `Xd` SHALL be `W` for regular nets and 0 for special nets.

Unsupported tokens:

- A via orientation (`N`, `FS`, ...) or `MASK n` / `MASK <bits>` is processed as via names and reports "Via name ... unknown in route." for each token. Via orientation is not supported. See Q-LEFDEF-72.

(Magic ref: lef/defRead.c:398-527)

#### Scenario: Via between legs
- **GIVEN** `oscale = 1`, via `VG` from the generated-via scenario (primary metal2 `(-340,-200)-(340,200)` doubled), and met1 width 140
- **WHEN** the regular route `met1 ( 0 500 ) ( 1000 500 ) VG` is read
- **THEN** metal1 `(-70,430)-(1070,570)` SHALL be emitted, and then at doubled `P = (2000,1000)`: the cuts, the metal1 bottom rectangle `(800,430)-(1200,570)` and metal2 `(830,400)-(1170,600)`
- **AND** the route type afterwards SHALL be metal2

#### Scenario: Unknown via name
- **WHEN** a route contains `( 0 0 ) NOSUCHVIA`
- **THEN** the error `Via name "NOSUCHVIA" unknown in route.` SHALL be reported, and no geometry SHALL be emitted for it

### Requirement: Other route elements
The system SHALL treat the route elements `RECT`, `POLYGON` and `VIRTUAL` as follows.
Compatibility: exact

- `RECT ( dx1 dy1 dx2 dy2 )`: the rectangle relative to the reference point. Each corner is `trunc(P/2) + round(d/oscale)`, where `P` is the doubled reference coordinate and the division truncates toward zero. The corners are canonicalised and emitted on the current paint type. `RECT` is accepted in both regular and special nets. An unparsable value SHALL report the error "Cannot parse X coordinate in RECT." (or Y), and a missing `)` SHALL report "Bad coordinates in RECT.". See Q-LEFDEF-73.
- `POLYGON`: the system SHALL report the error "Route has POLYGON entries, this is not handled!". It then consumes tokens up to the first `)`. Later vertex points are processed as ordinary route points. See Q-LEFDEF-70.
- `VIRTUAL`: the keyword SHALL be ignored. The following point is processed as an ordinary point, so a wire is painted to it. See Q-LEFDEF-71.

(Magic ref: lef/defRead.c:285-357)

#### Scenario: Relative RECT
- **GIVEN** `oscale = 1`
- **WHEN** a route has the reference point `(1000,500)` (doubled `(2000,1000)`) and then `RECT ( -50 -20 50 20 )`
- **THEN** `(950,480)-(1050,520)` SHALL be emitted on the current paint type

### Requirement: Route painting and net labels
After a routing statement has been parsed, the system SHALL paint every emitted rectangle into the design cell in emission order, unless annotation mode is on.
Compatibility: exact

When *dolabels* is set and annotation mode is off, the first emitted rectangle of each routing statement SHALL receive a label, as follows:

- text: the net name
- position: a point at `((xbot+xtop)/2, (ybot+ytop)/2)`, with integer division truncating toward zero
- type: the rectangle's type
- justification: center
- flags: none
- port index: 0
- the label is non-sticky

For regular nets, each `+ ROUTED|FIXED|COVER|NOSHIELD` statement is a separate routing statement and gets its own label. A special net's routing runs to `;` and gets one label.

After the whole read, non-sticky labels are re-attached to the layer under them (capability `layout-labels-ports`).

(Magic ref: lef/defRead.c:700-720, lef/defRead.c:1207-1216, lef/defRead.c:2753-2759)

#### Scenario: Net label
- **GIVEN** `def read top -labels`, `oscale = 1` and met1 width 140
- **WHEN** `- n1 ( u1 Y ) ( u2 A ) + ROUTED met1 ( 0 0 ) ( 1000 0 ) ;` is read
- **THEN** a metal1 label `n1` SHALL be placed at point `(500,0)`

### Requirement: Net annotation mode
In annotation mode, the system SHALL paint no route geometry and SHALL instead place one label per net, chosen as described below.
Compatibility: behavioral

1. Scan the net's connection list in order. If a connection is `( PIN <name> )`, stop looking and place no label for this net. For `( <comp> <pin> )`, look up the terminal `<comp>/<pin>` in the current edit cell (see Q-LEFDEF-81). If it is found on type `t` with area `A`, search the top-level design cell's plane for `t` within `A` for any tile of a type that connects to `t`. On the first tile found, place the net-name label at the tile centre on type `t`, and stop looking.
2. If no label has been placed yet when a routing statement is read, place the label on the first emitted route rectangle whose lower-left corner lies in a tile of the same type in the design cell, at that rectangle's centre.
3. If no rectangle qualifies, report the warning "Warning:  Label "<net>" did not land on any existing net geometry." and place the label at the centre of the last emitted rectangle.

In annotation mode, COMPONENTS and BLOCKAGES SHALL be skipped. VIAS and NONDEFAULTRULES SHALL still be read. PINS SHALL still paint geometry and add labels, after erasing matching labels (see "Pin port labels").

(Magic ref: lef/defRead.c:1131-1190, lef/defRead.c:722-756, lef/defRead.c:2660-2730)

#### Scenario: Annotate a net through a terminal
- **GIVEN** an edit cell with subcell `u1` whose terminal `Y` is metal1 at `(10,10)-(20,20)`, and top-level metal1 covering `(0,0)-(100,30)`
- **WHEN** `def read top -annotate` reads `- n1 ( u1 Y ) + ROUTED met1 ( 0 15 ) ( 100 15 ) ;`
- **THEN** a label `n1` SHALL be placed at `(50,15)` on metal1, and no paint SHALL be added

### Requirement: NONDEFAULTRULES section
The system SHALL read DEF NONDEFAULTRULES into the shared non-default rule table of capability `lef-read`, keyed by rule name. A name that is redefined replaces the earlier rule set.
Compatibility: exact

```ebnf
ndrs     = "NONDEFAULTRULES" , count , { token } , ";" , { ndr } , "END" , "NONDEFAULTRULES" ;
ndr      = "-" , rulename , { ndr_prop } , ";" ;
ndr_prop = "+" , ( "HARDSPACING"
                 | "LAYER" , layername , { ( "WIDTH" | "DIAGWIDTH" | "SPACING" | "WIREEXT" ) , num }
                 | ( "VIA" | "VIARULE" ) , name
                 | ( "MINCUTS" | "PROPERTY" ) , token , token ) ;
```

- `LAYER`, `VIA` and `VIARULE` each create a rule entry for the named layer record (width 0, spacing 0). Looking up an unknown name SHALL create an empty table entry for that name; see Q-LEFDEF-75.
- `WIDTH v` sets `rule.width = round(v/oscale)` (full width, internal units).
- `SPACING v` sets `rule.spacing = round(v/oscale)`.
- `WIREEXT v` SHALL report the warning "Non-default extension at via not implemented." and store `round(2v/oscale)`. The stored value is not used when painting.
- `DIAGWIDTH` SHALL be ignored.
- WIDTH, SPACING or WIREEXT before any LAYER SHALL report "<KW> specified without layer." (for example "WIDTH specified without layer.").
- An unknown `+` keyword SHALL report "Unknown non-default rule property "<kw>" in NONDEFAULTRULE definition; ignoring.".

(Magic ref: lef/defRead.c:790-990)

#### Scenario: Rule widths
- **GIVEN** `oscale = 1`
- **WHEN** `- W2 + LAYER met1 WIDTH 280 SPACING 280 + LAYER met2 WIDTH 300 ;` is read, followed by a net `- n + NONDEFAULTRULE W2 + ROUTED met1 ( 0 0 ) ( 1000 0 ) ;`
- **THEN** metal1 SHALL be painted at `(-140,-140)-(1140,140)`

### Requirement: BLOCKAGES section
Unless *noblockage* or annotation mode is set, the system SHALL read layer blockages and paint them on the obstruction type of the named layer, using the grammar below.
Compatibility: exact

```ebnf
blockages = "BLOCKAGES" , count , { token } , ";" , { blk } , "END" , "BLOCKAGES" ;
blk       = "-" , { "LAYER" , layername | "RECT" , point , point | other_token } , ";" ;
```

- `LAYER <name>` SHALL resolve the name as an obstruction layer (`lef-read` layer lookup with the obstruction flag): the obstruction type if one is defined, otherwise the layer's own type. Layers of class `ignore` resolve to no type, silently.
- Each `RECT` SHALL be painted immediately on the current layer at the rectangle `round(v/oscale)`.
- Every other token, including `+`, `COMPONENT`, `SPACING`, `PUSHDOWN`, `PLACEMENT` and `POLYGON`, SHALL report the message "Unknown blockage property "<tok>" in BLOCKAGES definition; ignoring." and is skipped.
- A placement blockage (no LAYER) paints its RECT on whatever layer was current from a previous blockage in this section. See Q-LEFDEF-76.
- With *noblockage* or annotation mode, the section SHALL be skipped.
- The final message is "  Processed N blockage." for N ≤ 1, otherwise "  Processed N blockages.".

(Magic ref: lef/defRead.c:1786-1880, lef/defRead.c:2725-2737, lef/lefRead.c:697-783)

#### Scenario: Layer blockage
- **GIVEN** tech LEF mapping `met1` with obstruction type `obsm1`, and `oscale = 1`
- **WHEN** `BLOCKAGES 1 ; - LAYER met1 RECT ( 0 0 ) ( 500 500 ) ; END BLOCKAGES` is read
- **THEN** `obsm1` SHALL be painted at `(0,0)-(500,500)`

#### Scenario: No blockage option
- **WHEN** the same file is read with `-noblockage`
- **THEN** nothing SHALL be painted for the section

### Requirement: Section count checks
For COMPONENTS, VIAS, PINS, NETS, SPECIALNETS, NONDEFAULTRULES and BLOCKAGES, the system SHALL compare the number of `-` statements processed with the declared count, and report the result as follows.
Compatibility: informative

Each statement counts once. For PINS, extra ports also count (see "PINS section grammar").

- Equal counts SHALL print:
  - "  Processed N subcell instances total."
  - "  Processed N vias total."
  - "  Processed N pins total."
  - "  Processed N nets total." or "  Processed N special nets total."
  - "  Processed N non-default rules total."
- Different counts SHALL report a warning:
  - "Number of subcells read (N) does not match the number declared (M)."
  - The same pattern for vias, pins, nets and blockages.
  - NONDEFAULTRULES uses the noun "non-default rules".
- A section that lacks its `END <SECTION>` SHALL report an error: "Component END statement missing.", "Vias END statement missing.", "Pins END statement missing.", "Net END statement missing.", "Non-default rule END statement missing." or "Blockage END statement missing.".
- An unknown statement keyword inside a section SHALL report "Unknown keyword "<tok>" in <SECTION> definition; ignoring.", where SECTION is one of COMPONENT, VIAS, PINS, NET, SPECIALNET, NONDEFAULTRULES or BLOCKAGES. The statement is skipped to `;`.

(Magic ref: lef/defRead.c:1290-1297, lef/defRead.c:1765-1772, lef/defRead.c:2162-2168, lef/defRead.c:2392-2398)

#### Scenario: Count mismatch
- **WHEN** `NETS 3 ;` is followed by only two net statements
- **THEN** the warning "Number of nets read (2) does not match the number declared (3)." SHALL be reported

### Requirement: Diagnostics and error limits
The system SHALL classify every diagnostic as an error, a warning or a message, and SHALL keep separate counters for each.
Compatibility: informative

Magic's message formats are:

- error: "DEF read, Line <n> (Error): <text>", on stderr
- warning: "DEF read, Line <n> (Warning): <text>", on stderr
- message: "DEF read, Line <n> (Message): <text>", on stdout

Diagnostics from shared LEF helpers use the prefix "LEF read" instead; these include rectangle and layer parse errors and cut-size warnings.

Counter limits: only the first 100 of each kind are printed. On reaching 100, Magic prints "DEF Read:  Further errors will not be reported." (for warnings: "DEF read:  Further warnings will not be reported.", and similarly for messages).

At the end of the read, Magic prints "DEF Read: encountered N error(s) total." and "DEF Read: encountered N warning(s) total.", each only when the count is non-zero, and then resets the counters. Hefesto SHALL report the same diagnostic conditions but MAY use different wording.

(Magic ref: lef/lefRead.c:249-398, lef/defRead.c:2749)

#### Scenario: Error summary
- **WHEN** a DEF read produces 2 errors and 0 warnings
- **THEN** the summary "DEF Read: encountered 2 errors total." SHALL be printed, and no warning summary SHALL be printed

### Requirement: Read completion
After the last statement, if a design cell exists, the system SHALL complete the read as follows.
Compatibility: behavioral

1. Re-attach non-sticky labels in the whole cell to the paint under them.
2. Recompute the cell bounding box.
3. Mark the whole cell area for redisplay.
4. Mark the cell modified.
5. Re-enable undo.

The read SHALL never create undo records.

(Magic ref: lef/defRead.c:2748-2761)

#### Scenario: Modified flag
- **WHEN** a DEF read completes with a design cell
- **THEN** that cell SHALL be marked modified, and `undo` SHALL NOT revert any of the read
