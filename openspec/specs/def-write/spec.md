# DEF Write

## Purpose

Defines the `def write` command and the DEF 5.7 file that it writes for one cell. The file contains the die area, generated via definitions, placed components, pins, non-default rules, special and regular nets (taken from the cell's extracted netlist) and blockages. Hefesto must reproduce Magic's output byte for byte for every `exact` item. Related capabilities: `lef-tech-section` and `lef-read` populate the LEF layer table, `def-read` reads DEF, `lef-write` writes LEF. Reading and flattening the extracted netlist belong to capabilities `ext-flattening-read` and `ext-flattening-hierarchy`.

Terms used throughout:

- **internal unit**: one Magic database unit.
- **scale `s`**: the number of DEF database units per internal unit (see "Output scale and number formatting").
- **layer table**: the LEF layer table, keyed by name. Its records are created by the tech-file `lef` section, by `lef read` and `def read`, and temporarily by this capability (generated vias). Each record has a class (route, cut/via, masterslice, overlap, bound, ignore), a magic type, an optional obstruction type, a canonical name, a reference count and, for route layers, a default width.
- **%g10**: the C `printf` conversion `%.10g` applied to an IEEE-754 binary64 value.

## Requirements

### Requirement: def write command

The system SHALL provide `def write [filename] [-units <n>] [-allspecial] [-anal]`. It SHALL write the DEF description of the target cell. The target cell SHALL be the currently selected cell use if one exists, otherwise the root cell of the layout window, otherwise the cell under the box tool. `filename` SHALL be the output file name (see "Output file naming"). When it is omitted, the name SHALL be the target cell's name. The built-in help describes this argument as `[cell]`, which is wrong (see Q-LEFDEF-109). The options are:

| option | minimum prefix that matches | effect |
|---|---|---|
| `-units <n>` | `-units` | DEF database units per micron, given as a decimal integer (default 1000) |
| `-allspecial` | `-allspec` | write every port net and every special net under SPECIALNETS, and omit NETS |
| `-anal` | `-anal` | in each NONDEFAULTRULES rule, also list every route layer that the rule does not mention, at that layer's default width |

An option matches when the argument begins with the "minimum prefix" text. Any further characters are ignored, so `-allspecialXYZ` is accepted and `-alls` is not.

Compatibility: behavioral

(Magic ref: lef/lefCmd.c:78-445, lef/defWrite.c:2970-3208)

#### Scenario: Default write

- **GIVEN** a cell `top` loaded in the layout window and no selection
- **WHEN** `def write` is run
- **THEN** the file `top.def` SHALL be written with `UNITS DISTANCE MICRONS 1000`

#### Scenario: Units override

- **GIVEN** cell `top`
- **WHEN** `def write out -units 2000` is run
- **THEN** the file `out.def` SHALL be written with `UNITS DISTANCE MICRONS 2000`, and every coordinate SHALL be twice the value it has at the default units

### Requirement: def command argument validation

The command SHALL validate its arguments as follows:

1. A non-option argument SHALL be accepted only in position 2 (the first argument after `write`). Any other position, or an unrecognized option beginning with `-`, SHALL print the `def` help and write nothing.
2. Let `c` = (number of arguments, including `def` and `write`) − (number of options) − (number of option values consumed). If `c` is neither 2 nor 3, the help SHALL be printed and nothing written.
3. `-units` SHALL consume the next argument. If that argument is not an integer, the message `The "-units" option requires an argument.` SHALL be printed and the units SHALL stay unchanged. The value SHALL NOT be range-checked. See Q-LEFDEF-104.
4. The LEF-only write options `-tech`, `-hide`, `-pinonly`, `-toplayer` and `-nomaster` SHALL each print `The "<opt>" option is only for lef write` and SHALL otherwise be ignored. `-hide` and `-pinonly` SHALL NOT consume a numeric argument under `def`, so a number after one of them is a positional argument and triggers rule 1.
5. The subcommand SHALL be matched against the shared list `read`, `write`, `writeall`, `nocheck`, `datestamp`, `help`, accepting unique prefixes. `def nocheck` SHALL print `The "nocheck" option is only for LEF writes.`. `def datestamp` SHALL print `The "datestamp" option is only for LEF reads.`. Neither SHALL do anything else.
6. `def` with no subcommand, or with an unknown subcommand (which first reports `"<x>" isn't a valid def option.`), SHALL print one help line per entry of the DEF help table, each prefixed `    def `.

Compatibility: informative

(Magic ref: lef/lefCmd.c:154-205, lef/lefCmd.c:325-445, lef/lefCmd.c:446-481, lef/lefCmd.c:548-560)

#### Scenario: Misplaced number after -hide

- **WHEN** `def write -hide 5` is run
- **THEN** the message `The "-hide" option is only for lef write` SHALL be printed
- **AND** because `5` is a non-option argument in position 3, the help SHALL be printed and no file written

### Requirement: def writeall is not supported

`def writeall` SHALL write nothing. It SHALL report `Sorry, can't write hierarchical DEF at this time.` followed by `Try "def write"`. Hierarchical designs are written by first flattening them (keeping subcells as components) and then using `def write`.

Compatibility: behavioral

(Magic ref: lef/lefCmd.c:275-281)

#### Scenario: writeall rejected

- **WHEN** `def writeall` is run
- **THEN** no file SHALL be created and the two-line error SHALL be reported

### Requirement: Output file naming

The output file name SHALL be derived from the name `N` given on the command line (or the cell name) as follows:

1. Let `B` be the part of `N` after its last `/`. If `B` contains a `.` and the text from the last `.` is not `.def`, the system SHALL first try to open `N` exactly as given. If that succeeds, `N` SHALL be used unchanged. Otherwise, the extension SHALL be stripped and `.def` appended.
2. If `B` ends in `.def`, `N` SHALL be used as given.
3. If `B` has no `.`, `.def` SHALL be appended.

Paths are resolved relative to the current working directory. On iPadOS they are resolved relative to the document container (see Q-LEFDEF-90).

The nets sections SHALL be produced before NONDEFAULTRULES is complete but written after it. An implementation MAY buffer them in any way. Magic writes them to a temporary sibling file named `<N without extension>_part.def` and deletes it after appending. That temporary file is informative. Hefesto SHALL NOT leave it behind and SHALL NOT require write access beyond the output file.

Compatibility: behavioral

(Magic ref: lef/lefWrite.c:150-235, lef/defWrite.c:3015-3043, lef/defWrite.c:3093-3110, lef/defWrite.c:3174-3203)

#### Scenario: Foreign extension kept

- **WHEN** `def write result.txt` is run in a writable directory
- **THEN** the DEF SHALL be written to `result.txt`, not to `result.def`

#### Scenario: Extension appended

- **WHEN** `def write chip` is run
- **THEN** the DEF SHALL be written to `chip.def`

### Requirement: Preconditions and diagnostics

Before writing, the system SHALL check the following:

- If the target cell has the reserved unnamed-cell name `(UNNAMED)`, it SHALL report `Please name the cell before generating DEF.` and write nothing.
- If the output file cannot be opened, it SHALL report `Cannot open output file <name> (<reason>).` and write nothing.
- If the cell's extracted netlist file `<cellname>.ext` (with the hierarchy it references) cannot be read, it SHALL report `Warning:  Circuit has no .ext file;  no nets written.` and `Run extract on this circuit if you want nets in the output.`. It SHALL still write every other section, and SHALL omit SPECIALNETS and NETS.

The netlist SHALL be read from the existing `.ext` files. It SHALL NOT be re-extracted, and it SHALL NOT be checked for staleness against the layout (see Q-LEFDEF-110). The netlist SHALL be read in unscaled mode (node locations in internal units), with subcircuits kept and resistance information not used.

Progress messages such as `Diagnostic:  Write DEF header for cell <c>`, `Generating DEF output <file> for cell <c>:`, `Diagnostic:  Finding all components of cell <c>`, `Diagnostic:  Finding all pins of cell <c>`, `Diagnostic:  Finding all nets in cell <c>` and `(This can take a while!)` are informative.

Compatibility: behavioral

(Magic ref: lef/defWrite.c:2994-3012, lef/defWrite.c:251-293)

#### Scenario: No extraction available

- **GIVEN** cell `top` with no `top.ext` on the search path
- **WHEN** `def write` is run
- **THEN** `top.def` SHALL contain the header, VIAS, COMPONENTS and PINS sections, the BLOCKAGES section if non-electrical obstruction paint exists, and `END DESIGN`
- **AND** it SHALL contain no SPECIALNETS or NETS section

### Requirement: Output scale and number formatting

The scale SHALL be `s = f32( f32(10·S / (E·1000)) · f32(units) )`. Here `S` is the scale factor and `E` the expander of the current CIF/GDS output style (see the cif-output capabilities), and `f32()` means rounding to IEEE-754 binary32. If no output style is defined, `s = f32(1.0 · units)`.

All arithmetic described as "float" in this capability SHALL be done in binary32, in the order of operations given. The product of an integer and `s` SHALL be computed by converting the integer to binary32 and then multiplying in binary32. Where a formula says `/2.0`, that division SHALL be done in binary64 on the binary32 value widened to binary64. Every non-integer-valued number written to the file SHALL be the binary32 result widened to binary64 and formatted with %g10 (`%.10g`). Integer-valued results print without a decimal point (for example `1000`, `-130`). Values that are not exactly representable SHALL print with all 10 significant digits that binary32→binary64 widening produces (for example `0.3333333433`).

Compatibility: exact

(Magic ref: lef/defWrite.c:3005, cif/CIFmain.c:139-146)

#### Scenario: Ten-nanometre grid

- **GIVEN** an output style with 1 internal unit = 10 nm (S/E such that `10·S/(E·1000)` = 0.01)
- **WHEN** `def write` runs with default units
- **THEN** `s` SHALL be exactly 10.0 (because f32(0.01f · 1000) rounds to 10.0)
- **AND** an internal coordinate 37 SHALL be written as `370`

#### Scenario: Non-representable scale (verify against Magic)

- **GIVEN** an output style where `10·S/(E·1000)` = 1/3 and units 1
- **WHEN** internal coordinate 1 is written
- **THEN** the text SHALL be `0.3333333433`

### Requirement: DEF output file grammar and section order

The file SHALL follow this grammar exactly, including spaces and blank lines. Literal strings are quoted, `NL` is a single `\n`, and `num` is a number formatted as in "Output scale and number formatting".

```ebnf
def_file      = header , vias , components , pins ,
                [ nondefault ] , [ specialnets ] , [ nets ] , [ blockages ] ,
                "END DESIGN" , NL , NL ;
header        = "VERSION 5.7 ;" , NL ,
                "   NAMESCASESENSITIVE ON ;" , NL ,
                "   DIVIDERCHAR \"/\" ;" , NL ,
                "   BUSBITCHARS \"()\" ;" , NL ,
                "   DESIGN " , cellname , " ;" , NL ,
                "   TECHNOLOGY " , techname , " ;" , NL ,
                "   UNITS DISTANCE MICRONS " , int , " ;" , NL ,
                "   DIEAREA ( " , num , " " , num , " ) ( " , num , " " , num , " ) ;" , NL ,
                NL ;
vias          = "VIAS " , int , " ;" , NL , { via_def } , "END VIAS" , NL , NL ;
via_def       = "   - " , vianame , { NL , "      + RECT " , layer ,
                " ( " , num , " " , num , " ) ( " , num , " " , num , " )" } , " ;" , NL ;
components    = "COMPONENTS " , int , " ;" , NL , { component } , "END COMPONENTS" , NL , NL ;
component     = "   - " , instname , " " , macroname , NL ,
                "      + PLACED ( " , num , " " , num , " ) " , orient , " ;" , NL ;
pins          = "PINS " , int , " ;" , NL , { pin } , "END PINS" , NL , NL ;
pin           = "   - " , text , " + NET " , text , NL ,
                [ "     + DIRECTION " , [ direction ] , NL ] ,
                [ "     + USE " , [ use ] , NL ] ,
                "     + PORT" , NL ,
                [ "        + LAYER " , layer , " ( " , num , " " , num , " ) ( " ,
                  num , " " , num , " )" ] ,
                "        + PLACED ( " , num , " " , num , " ) N ;" , NL ;
nondefault    = "NONDEFAULTRULES " , int , " ;" , NL , { ndrule } ,
                "END NONDEFAULTRULES" , NL , NL ;
ndrule        = "  - " , rulename , { NL , "     + LAYER " , layer , " WIDTH " , num ,
                [ " WIREEXT " , num ] } , " ;" , NL ;
specialnets   = "SPECIALNETS " , int , " ;" , NL , { net } , "END SPECIALNETS" , NL , NL ;
nets          = "NETS " , int , " ;" , NL , { net } , "END NETS" , NL , NL ;
net           = "   - " , netname , [ " ( PIN " , netname , " )" ] ,
                { [ BREAK ] , " ( " , inst , " " , pinname , " )" } ,
                [ routing ] , " ;" , NL ;     (* routing: see net routing requirements *)
blockages     = "BLOCKAGES " , int , " ;" , NL , { blk } , "END BLOCKAGES" , NL , NL ;
blk           = "   - LAYER " , layer , { NL , "      RECT ( " , num , " " , num ,
                " ) ( " , num , " " , num , " )" } , " ;" , NL ;
BREAK         = NL , "      " ;
orient        = "N" | "S" | "E" | "W" | "FN" | "FS" | "FE" | "FW" ;
direction     = "INPUT" | "OUTPUT" | "INOUT" | "FEEDTHRU" ;
use           = "SIGNAL" | "ANALOG" | "POWER" | "GROUND" | "CLOCK" ;
```

The sections SHALL appear in the order shown. VIAS, COMPONENTS and PINS SHALL always be written, even when their count is 0. NONDEFAULTRULES, SPECIALNETS, NETS and BLOCKAGES SHALL be written only when their count is greater than 0. `techname` SHALL be the name declared in the technology file's `tech` section. `cellname` SHALL be the target cell's name.

Annotated example (scale 10, one via, one component, one pin, one regular net):

```
VERSION 5.7 ;
   NAMESCASESENSITIVE ON ;
   DIVIDERCHAR "/" ;
   BUSBITCHARS "()" ;
   DESIGN top ;
   TECHNOLOGY demo ;
   UNITS DISTANCE MICRONS 1000 ;
   DIEAREA ( 0 0 ) ( 1000 500 ) ;
                                                 <- blank line
VIAS 1 ;
   - via1_260_260                                <- <cutlayer>_<w>_<h> in DEF units
      + RECT metal1 ( -130 -130 ) ( 130 130 )    <- residue layers, centred on origin
      + RECT metal2 ( -130 -130 ) ( 130 130 )
      + RECT via1 ( -85 -85 ) ( 85 85 ) ;        <- generated cut squares
END VIAS

COMPONENTS 1 ;
   - u1 inv
      + PLACED ( 1000 0 ) N ;
END COMPONENTS

PINS 1 ;
   - A + NET A
     + DIRECTION INPUT
     + USE SIGNAL
     + PORT
        + LAYER metal1 ( -20 -20 ) ( 20 20 )        + PLACED ( 120 220 ) N ;
END PINS

NETS 1 ;
   - A ( PIN A ) ( u1 A )
      + ROUTED metal1  ( 10 115 ) ( 990 * ) ;
END NETS

END DESIGN
                                                 <- final blank line
```

(The `<-` annotations are not part of the file.)

Compatibility: exact

(Magic ref: lef/defWrite.c:105-182, lef/defWrite.c:3045-3205)

#### Scenario: Empty cell

- **GIVEN** cell `e` with no paint, no subcells and no labels, an empty bounding box at (0,0)-(1,1), and no `.ext` file
- **WHEN** `def write` is run with s = 10
- **THEN** after the header the file SHALL contain exactly `VIAS 0 ;\nEND VIAS\n\nCOMPONENTS 0 ;\nEND COMPONENTS\n\nPINS 0 ;\nEND PINS\n\nEND DESIGN\n\n`

### Requirement: DIEAREA

DIEAREA SHALL be the cell's `FIXED_BBOX` property when that property exists and holds exactly four integers `xl yl xh yh`. Otherwise it SHALL be the cell's bounding box. Each coordinate SHALL be written as float `f32(v)·s`.

Compatibility: exact

(Magic ref: lef/defWrite.c:137-179)

#### Scenario: Bounding box die area

- **GIVEN** a cell with bounding box (0,0)-(100,50), no FIXED_BBOX, and s = 10
- **THEN** the header SHALL contain `   DIEAREA ( 0 0 ) ( 1000 500 ) ;`

#### Scenario: Fixed bbox wins

- **GIVEN** property `FIXED_BBOX` = `-10 -10 110 60` and s = 10
- **THEN** the header SHALL contain `   DIEAREA ( -100 -100 ) ( 1100 600 ) ;`

### Requirement: Magic-to-LEF layer map

Before writing, the system SHALL build a map from every magic tile type `t` (user types and stacked contact types) to a LEF layer record and its name, using the magic-type-to-LEF-layer rule owned by capability `lef-write` (Requirement "Magic type to LEF layer name"), with candidates taken in this capability's table enumeration order (see "Table enumeration order"). If `t` has no LEF name, geometry of that type SHALL be skipped wherever this capability says so.

Compatibility: exact

(Magic ref: lef/defWrite.c:2041-2081, lef/defWrite.c:2910-2928)

#### Scenario: Obstruction type maps to route layer

- **GIVEN** the tech `lef` section line `obstruction obsm1 metal1` and `route metal1 metal1`
- **THEN** both magic types `metal1` and `obsm1` SHALL map to LEF layer `metal1`

### Requirement: Contact region identification

The VIAS and NETS writers SHALL identify one contact region per group of contact tiles, as follows:

1. Consider a tile of contact type `T` on plane `p`. If `T` is a stacked contact type, replace it with its first residue contact type (in type-number order) whose home plane is `p`. If no residue has home plane `p`, ignore the tile. If `T` is not stacked and `p` is not its home plane, ignore the tile.
2. If any tile directly adjacent along the tile's left edge, or along its bottom edge, has type `T` (or is a stacked type whose residues include `T`), ignore this tile. Only the bottom-most, left-most tile of a region is processed.
3. Otherwise, the region SHALL be the tile's rectangle. Grow it by including tiles stacked directly above, walking upward from the top-left neighbour, while each tile is `T` or a stacked type containing `T`. Then grow it by including tiles directly to the right, walking rightward from the right-top neighbour, under the same rule.

This bounding box SHALL be the contact rectangle `C = (xl, yl, xh, yh)`. The rule assumes rectangular contacts. See Q-LEFDEF-111.

Compatibility: exact

(Magic ref: lef/defWrite.c:1754-1858, lef/defWrite.c:971-1078)

#### Scenario: Two-tile contact

- **GIVEN** via1 paint covering (10,10)-(36,36), stored as two tiles (10,10)-(36,20) and (10,20)-(36,36) because of neighbouring geometry
- **THEN** one contact region (10,10)-(36,36) SHALL be identified, from the lower tile

### Requirement: Generated via definitions

For every contact region found by searching each plane (in plane order) for contact types whose home plane is that plane (including stacked types that have such a residue), the system SHALL derive a via name and geometry. A region whose contact type `T` has no LEF name SHALL be skipped. Let `C` be the contact rectangle, `w = xh − xl`, `h = yh − yl`.

1. **Half-unit area.** Let `A = (−w, −h, w, h)` in half-units. This is the area centered on the origin with all coordinates doubled.
2. **Directional surround.** Take the first two residue types of `T` in type-number order, `L` and `H`. For each, get the default surround `sL`/`sH` and the directional surround `dL`/`dH` of `T` by that layer, from the DRC rules (see the drc capabilities). Only `L` is used if `T` has a single residue (see Q-LEFDEF-106). If `dL > 0`, search the plane of `L` over the area `(xl − dL + sL, yl − sL, xh + dL + sL, yh + sL)` for any tile whose type does not connect to `L`. Space counts as such a tile. If one is found, `oL = 'v'`; otherwise `oL = 'h'`. The same applies to `H`. See Q-LEFDEF-105 for the asymmetric search area. If `dL = 0`, `oL = 'x'`.
3. **Name.** The name is `<lefname(T)>_<a>_<b>` when `dL = 0` and `dH = 0`, and `<lefname(T)>_<a>_<b>_<oL><oH>` otherwise. Here `a` = %g10 of float `f32(w)·s` and `b` = %g10 of float `f32(h)·s`.
4. **Registration.** If the layer table already has a live record with this name, reuse it and do not count it. Otherwise create a temporary via record (class via, magic type `T`, area `A`, reference count 0), insert it into the layer table under the name, and add 1 to the VIAS count. In the new record:
   - if `sL > 0` or `dL > 0`, attach a residue rectangle for `L` equal to `A` grown by `2·sL` on all sides, and then, if `dL > 0`, grown by a further `2·dL` in x when `oL = 'h'` or in y when `oL = 'v'`;
   - do the same for `H`.
5. **Position index.** Record the key `<planeShortName(home plane of T)>_<xl>_<yl>` (decimal integers) as pointing to the via record. Nets use this key.

The VIAS count SHALL be the number of records created in step 4.

Compatibility: exact

(Magic ref: lef/defWrite.c:1661-1715, lef/defWrite.c:1728-2023)

#### Scenario: Plain via naming

- **GIVEN** s = 10, a via1 contact region (10,10)-(36,36), LEF name `via1`, and no surround rules
- **THEN** the via name SHALL be `via1_260_260` and the VIAS count SHALL be 1

#### Scenario: Directional surround suffix

- **GIVEN** the same via, and a directional surround of via1 by metal1 = 2 with metal1 extending at least 2 units left and right of the contact
- **AND** no directional surround rule for metal2
- **THEN** the name SHALL be `via1_260_260_hx`

#### Scenario: Two identical vias share a definition

- **GIVEN** two via1 regions of 26×26 at different positions
- **THEN** one definition `via1_260_260` SHALL be written and the VIAS count SHALL be 1

### Requirement: VIAS section content

`VIAS <count> ;` SHALL be followed by one `via_def` for each layer-table record that meets all of these conditions, in table enumeration order:

- the record is live;
- its class is via;
- its name contains at least two `_` characters, with the first `_` at a different position from the last.

This filter selects the generated vias, but also any other via record whose name has two or more underscores. Such records are written but not counted (see Q-LEFDEF-91).

For each record of magic type `T`:

1. For each residue type `r` of `T`, in type-number order, write `+ RECT <lefname(r)> ( x1 y1 ) ( x2 y2 )`. The rectangle is the record's residue rectangle for `r` if there is one, otherwise the area `A`. Each coordinate is the float `f32(v)·s/2` (multiply first, then divide in binary32). A residue with no LEF name SHALL print the name as `(null)` (see Q-LEFDEF-115).
2. **Cuts.** If the CIF/GDS output style defines a squares, squares-grid or slots operator for `T`, with cut size `z`, spacing `p` and border `b` in CIF units, compute integers using C truncation toward zero. Here `c = f32(10·S/E)`, the CIF units per internal unit.
   - `z' = trunc( trunc(f32(z)·s) / c )`, and the same for `p'` and `b'`. Each step is done in binary32 and truncated to an integer.
   - `X1 = trunc(trunc(f32(A.x1)·s)/2)`, and the same for `Y1`, `X2` and `Y2` (integer division by 2 after truncating the product).
   - `pitch = z' + p'`
   - `nx = trunc((X2 − X1 + p' − 2b') / pitch)`
   - If `nx = 0`: `nx = 1` and `left = trunc((X1 + X2 − z')/2)`. If `left < X1`, the warning `Warning: via size is <z'> but area width is <X2−X1>!` is printed.
   - Otherwise: `left = trunc((X1 + X2 + p' − nx·pitch)/2)`.
   - Compute `ny` and `bottom` the same way from `Y1` and `Y2`. The y warning fires when `bottom >= Y1` (see Q-LEFDEF-107).
   - Write `nx·ny` cuts row by row from the bottom, left to right within each row: `+ RECT <lefname(T)> ( l b ) ( l+z' b+z' )`, starting at (`left`, `bottom`) and stepping by `pitch`. Values are integers printed with %g10.
3. If no cut operator exists, write one cut rectangle `+ RECT <lefname(T)>` equal to `A·s/2`.

Each RECT SHALL start on a new line with 6 spaces. The record SHALL end with ` ;` and a newline.

Compatibility: exact

(Magic ref: lef/defWrite.c:2099-2256, cif/CIFtech.c:2327-2381)

#### Scenario: Single centred cut

- **GIVEN** s = 10, via `via1_260_260` with area (−26,−26,26,26) half-units, and converted cut parameters z' = 170, p' = 170, b' = 50
- **THEN** X1 = −130 and X2 = 130, and nx = trunc((260+170−100)/340) = 0, so nx = 1 and left = trunc((−130+130−170)/2) = −85
- **AND** the record SHALL be:

```
   - via1_260_260
      + RECT metal1 ( -130 -130 ) ( 130 130 )
      + RECT metal2 ( -130 -130 ) ( 130 130 )
      + RECT via1 ( -85 -85 ) ( 85 85 ) ;
```

#### Scenario: Two-by-two cut array

- **GIVEN** a via area of 80×80 DEF units (X1 = −400, X2 = 400) and z' = 170, p' = 170, b' = 50
- **THEN** nx = trunc((800+170−100)/340) = 2 and left = trunc((0+170−680)/2) = −255
- **AND** the cuts SHALL be at x ranges (−255,−85) and (85,255) and the same y ranges: four RECT lines, bottom row first

### Requirement: Component orientation

The DEF orientation of a transform with matrix `(a b; d e)` (where `x' = a·x + b·y + c` and `y' = d·x + e·y + f`) SHALL be computed as follows:

- If `a = 0` and `e = 0` (rotated by 90°): `flip = (b·d > 0)` and `sw = (d > 0)`.
- Otherwise: `flip = (a·e < 0)` and `sw = (e ≤ 0)`.
- index = 4·flip + 2·[a = 0 and e = 0] + sw, looked up in `N S E W FN FS FE FW`.

| a b d e | DEF |
|---|---|
| 1 0 0 1 | N |
| −1 0 0 −1 | S |
| 0 1 −1 0 | E |
| 0 −1 1 0 | W |
| −1 0 0 1 | FN |
| 1 0 0 −1 | FS |
| 0 −1 −1 0 | FE |
| 0 1 1 0 | FW |

Compatibility: exact

(Magic ref: lef/defWrite.c:200-229)

#### Scenario: Mirror about the Y axis

- **GIVEN** a use with transform a = −1, b = 0, d = 0, e = 1
- **THEN** its orientation SHALL be written as `FN`

#### Scenario: 90-degree counter-clockwise

- **GIVEN** a transform with a = 0, b = −1, d = 1, e = 0 (x' = −y, y' = x)
- **THEN** its orientation SHALL be written as `W`

### Requirement: COMPONENTS section

The count SHALL be the sum over all child uses that have an instance id of `(xhi − xlo + 1)·(yhi − ylo + 1)`, using the use's array index bounds. See Q-LEFDEF-94 for reversed bounds. Uses SHALL be visited in the subcell enumeration order of the cell (see "Table enumeration order").

For a non-arrayed use:

- `instname` SHALL be the use id, and `macroname` SHALL be the child cell name after its last `/` (if any).
- Let `R` be the child's `FIXED_BBOX` (four integers) if the child cell is marked fixed-bbox and the property exists, else the child's bounding box `Bc`. Let `U` = the use's bounding box in parent coordinates (the transform applied to `Bc`).
- If `R` is the FIXED_BBOX, let `off = T(R).ll − T(Bc).ll`, where `T` is the use transform. Otherwise `off = (0,0)`.
- The placement SHALL be `x = f32(U.xl − R.xl + off.x)·s` and `y = f32(U.yl − R.yl + off.y)·s`. Note that `R.xl` is not transformed (see Q-LEFDEF-93).

For an arrayed use (`xlo ≠ xhi` or `ylo ≠ yhi`), one component SHALL be written per element, with `y` in the outer loop and `x` in the inner loop, both ascending over the index range:

- `instname` = `<id>[<idx>]`, where `<idx>` is `y,x` for a 2-D array, `y` for a column array, or `x` for a row array.
- `macroname` = the child cell name (no `/` stripping).
- placement = the lower-left of the element transform applied to `R`, times `s` (no subtraction of `R.xl`).

The orientation SHALL come from the use transform in both cases. Status SHALL always be `PLACED`.

Compatibility: exact

(Magic ref: lef/defWrite.c:2278-2310, lef/defWrite.c:2756-2901, database/DBcellsrch.c:1350-1380)

#### Scenario: Simple placement

- **GIVEN** child `inv` with bounding box (0,0)-(46,272), and use `u1` with transform N translated by (100,0), with s = 10
- **THEN** the output SHALL be `   - u1 inv\n      + PLACED ( 1000 0 ) N ;\n`

#### Scenario: Child bbox not at origin

- **GIVEN** child `c` with bounding box (−5,0)-(20,10), and use `u2` with identity orientation translated by (100,0), with s = 10
- **THEN** U.xl = 95 and x = (95 − (−5))·10 = 1000, so `+ PLACED ( 1000 0 ) N` SHALL be written

#### Scenario: Two-dimensional array naming

- **GIVEN** use `a` arrayed with x index 0..1 and y index 0..1
- **THEN** the components SHALL be written in the order `a[0,0]`, `a[0,1]`, `a[1,0]`, `a[1,1]` (y,x), and the count SHALL include 4 for this use

### Requirement: PINS section

The count SHALL be the number of labels in the target cell that are ports (labels with any port connection-direction flag set). Pins SHALL be written in the cell's label list order. For each port label with text `t`, rectangle `(xl,yl,xh,yh)` and layer `L`:

1. `   - t + NET t` (the text is not sanitized).
2. If the port class is not default: `     + DIRECTION ` followed by `INPUT` (input), `OUTPUT` (output), `INOUT` (tristate or bidirectional), `FEEDTHRU` (feedthrough), or nothing for other encodings. Then a newline.
3. If the port use is not default: `     + USE ` followed by `SIGNAL`, `ANALOG`, `POWER`, `GROUND` or `CLOCK`, or nothing for reset, scan and tieoff (see Q-LEFDEF-103). Then a newline.
4. `     + PORT` and a newline.
5. If `L` has a LEF name: `        + LAYER <name> ( −w′ −h′ ) ( w′ h′ )`, **with no newline**. Here `w′ = f32(s·f32(w))/2.0` and `h′` likewise, with `w = xh − xl` and `h = yh − yl`. The negative corner uses `f32(s·f32(−w))/2.0`. If `L` has no LEF name, an error is reported instead (informative) and nothing is written.
6. `        + PLACED ( cx cy ) N ;` and a newline, where `cx = f32(s·f32(xl+xh))/2.0` and `cy` likewise.

Compatibility: exact

(Magic ref: lef/defWrite.c:2329-2457)

#### Scenario: Input signal pin

- **GIVEN** s = 10 and port label `A` on metal1 at (10,20)-(14,24) with class input and use signal
- **THEN** the pin SHALL be written as:

```
   - A + NET A
     + DIRECTION INPUT
     + USE SIGNAL
     + PORT
        + LAYER metal1 ( -20 -20 ) ( 20 20 )        + PLACED ( 120 220 ) N ;
```

#### Scenario: Default class and use

- **GIVEN** a port label `B` with default class and default use
- **THEN** no DIRECTION or USE line SHALL be written

#### Scenario: Point label

- **GIVEN** a port label `C` on metal2 with zero-area rectangle (7,7)-(7,7) and s = 10
- **THEN** the LAYER fragment SHALL be `( 0 0 ) ( 0 0 )` and the placement `( 70 70 )`

### Requirement: Net classification

When the netlist is available, every flattened node SHALL be classified, skipping nodes whose type is space (substrate):

1. If the node's preferred name is a top-level name (no hierarchy prefix):
   - It is **special** if the name ends in `!`.
   - It is **special** if the name is an entry of `globalNets`, or equals the `vdd` or `gnd` field, of the project setting "global nets and power names" (capability `hefesto-settings`).
   - It is **special** if it is neither special nor a port, and its name does not end in `#`.
2. A node is a **port** node if any cell in the hierarchy declares it as a port (port flags are merged when nodes are flattened together).

In Magic, the second bullet of rule 1 comes from the Tcl array `globals` and the variables `$VDD` and `$GND`, and the second and third bullets apply only in builds with Tcl. Hefesto SHALL apply all three bullets as in the Tcl build, taking the second from the project setting, which a startup script seeds from those variables. The first bullet, the trailing-`!` rule, is kept as an explicit additional rule because Magic applies it in every build (Q-LEFDEF-102, decided by D-14).

Counts:

- Without `-allspecial`: special nets = special nodes; regular nets = port nodes that are not special; all other nodes are "blockage" nodes.
- With `-allspecial`: special nets = special or port nodes; regular nets = 0.

SPECIALNETS SHALL list the special nets (and, under `-allspecial`, the port nets). NETS SHALL list the regular nets. Both SHALL use the flattened-node enumeration order of the netlist (see capability `ext-flattening-hierarchy`).

Compatibility: exact

(Magic ref: lef/defWrite.c:251-394, lef/defWrite.c:680-700, extflat/EFbuild.c:1330-1358)

#### Scenario: Global power net

- **GIVEN** top-level nets `vdd!` (no port), `A` (top-level port) and an unlabeled internal net `a_12_n3#`
- **WHEN** `def write` is run
- **THEN** SPECIALNETS SHALL contain `vdd!`, NETS SHALL contain `A`, and `a_12_n3#` SHALL appear in neither

#### Scenario: Labeled non-port net becomes special

- **GIVEN** a top-level labeled net `clkint` that is not a port and does not end in `!`
- **THEN** it SHALL be written under SPECIALNETS

#### Scenario: All special

- **WHEN** `def write -allspecial` is run on the same design
- **THEN** SPECIALNETS SHALL list `vdd!`, `A` and `clkint`, and no NETS section SHALL be written

### Requirement: DEF net naming

The net name SHALL be built from the node's preferred hierarchical name `p1/p2/.../pk`. The prefix components `p1..p(k−1)` SHALL be copied verbatim, each followed by `/`. The last component `pk` SHALL be sanitized: every `#` is removed, and every `;`, `-`, `*` and `%` is replaced by `_`. Other characters, including `!`, `[`, `]` and `/` inside a component, SHALL be copied unchanged.

Names longer than 255 bytes are undefined in Magic (fixed buffer, see Q-LEFDEF-113). Hefesto SHALL write them in full.

Compatibility: exact

(Magic ref: lef/defWrite.c:400-503)

#### Scenario: Sanitized local name

- **GIVEN** node names `net-1`, `a_n5#` and `x1/sig*b`
- **THEN** the DEF names SHALL be `net_1`, `a_n5` and `x1/sig_b`

### Requirement: Net record header and connections

Each net record SHALL begin with `   - <name>`, and the output column SHALL be set to 5 + length(name). Then:

1. If the node is a port of the top cell, append ` ( PIN <name> )`. The column is not updated.
2. For each name of the node, in the node's name-list order, that has exactly two hierarchy levels (`inst/pin`) and whose `pin` does not end in `#`: apply the line-break rule with `addlen = 5 + len("inst pin")`, then append ` ( inst pin )`. Here `pin` is sanitized as in "DEF net naming" and `inst` is verbatim.

Line-break rule (used throughout the nets sections): `column += addlen`. If `column > 70`, write `\n      ` (newline plus 6 spaces) and set `column = 6 + addlen`. The break is written before the text it was checked for.

Compatibility: exact

(Magic ref: lef/defWrite.c:710-745, lef/defWrite.c:510-531)

#### Scenario: Connections wrap

- **GIVEN** net `n1` connecting to `( u10 A )`, `( u11 A )`, ... across many instances
- **THEN** the entries SHALL stay on the first line while the running column is ≤ 70, and each break SHALL be written as `\n      ` before the entry that would push the column past 70

### Requirement: Net geometry discovery

For each net, the routing geometry SHALL be found as follows:

1. For each location seed of the node (one 1×1 rectangle and layer per node record of the top-level netlist, including duplicate-name records at other locations), in seed-list order, skipping seeds at the substrate sentinel coordinate (≤ −(2^31−1)+2): run a connectivity search in the target cell's own paint (not subcells). Start at the seed rectangle, accept as a start any type that connects to the seed layer, follow connectivity through all planes, and call the segment writer on every tile reached, in traversal order.
2. If no tile has been passed to the segment writer for this net so far (after a seed's search), search the hierarchy at the node location for the first tile of a connecting type (in subcells too, transformed to top coordinates). If one is found, grow its rectangle by 1 on every side, add the contact's residue types to the mask if the seed layer is a contact, and run the connectivity search from that rectangle. If nothing is found and the seed layer is electrically active, report `Nothing of type <layer> found at node <name> location (floating label?)!` (informative).
3. After the last seed, if a segment is pending and it is not a via, write its end coordinate with the pending orientation. Then write ` ;` and a newline, and reset the column to 0.

The traversal order of the connectivity search (a LIFO stack; for each popped tile, push its neighbours along the left edge bottom→top, the bottom edge left→right, the right edge top→bottom and the top edge right→left, then one tile per other connected plane in plane order) determines the order of route segments. Hefesto SHALL reproduce this order, using the same tile decomposition (maximal horizontal strips) and the same traversal, so that route segments are byte-identical (decision X-1, capability `hefesto-output-determinism`; see Q-LEFDEF-108). Seeds whose components are connected in the top cell produce repeated geometry (Q-LEFDEF-112).

Compatibility: exact

(Magic ref: lef/defWrite.c:747-834, database/DBconnect.c:171-243, database/DBconnect.c:395-640)

#### Scenario: Net with no top-level paint

- **GIVEN** a regular net that exists only as a connection between two subcell pins, with no top-level wire
- **THEN** its record SHALL contain only the name and connection list followed by ` ;`, with no `+ ROUTED`

### Requirement: Wire segment derivation

The segment writer SHALL handle each tile of type `t` with rectangle `r`, skipping space tiles:

1. **Contacts.** If `t` is a contact, apply the contact region rule ("Contact region identification") to get `t` and `r`. Ignored tiles produce nothing.
2. If `t` has no LEF name, the tile produces nothing.
3. Let `w` and `h` be the width and height of `r`, and `orient = E`.
4. **Route width `W`.**
   - Special mode (SPECIALNETS, including `-allspecial`): `W = min(w,h)`. If the LEF record of `t` is a via (or `t` is a contact with no LEF record), `orient = CENTER`.
   - Regular mode: if the LEF record is a via (or contact with no record), `orient = CENTER`. Otherwise `W` is the LEF route width of the record, and if that is 0, the DRC default width of `t`.
5. If not a via:
   - If `h ≠ W` and (`w = W`, or `W = 0` and `h > w`), then `orient = N`.
   - **Non-default/sliver test.** It applies when (`h ≠ W` and `w ≠ W`), or (`h = W` and `w < W`), or (`w = W` and `h < W`):
     - If `w < W`, the tile produces nothing.
     - If `h < W`, grow the shape vertically. Search upward from `r.yh` in steps of `W`, over the tile's x span, for tiles that do not connect to `t`. The new top is the lowest bottom edge of such a tile, and the search stops at the first step that finds one. Grow downward symmetrically, taking the highest top edge. Extend `r` to these limits. If the new height is still `< W`, the tile produces nothing. If `r` grew, mark every not-yet-visited, non-contact tile of a type connecting to `t` that lies entirely inside `r` so that the segment writer skips it.
     - In regular mode with `h ≠ W` and `w ≠ W`, select the non-default rule named `<lefname>_width_<trunc(f32(min(w,h))·s)>`. If no rule with that name exists, create one: one layer entry for this layer, width `min(w,h)`, extension 0, spacing = DRC default spacing of `t` to `t`. Otherwise there is no rule.
     - `orient = N` if `h > w`, else `E`.
   - If the test does not apply, there is no rule.
6. **Endpoints** (binary32):
   - E: `y1 = y2 = f32(f32(yl+yh)·s)/2`, `x1 = f32(xl)·s`, `x2 = f32(xh)·s`. If `W = 0`, `W = h`. In regular mode, `x1 += f32(⌊q/2⌋)·s` and `x2 −= f32(⌊q/2⌋)·s`, where `q` is the rule width if a rule is selected, else `W`. The halving is integer division (see Q-LEFDEF-95).
   - N: the same with x and y exchanged, using `xl + xh` for the centerline.
   - CENTER (via): `x1 = x2 = f32(f32(xl+xh)·s)/2` and `y1 = y2 = f32(f32(yl+yh)·s)/2`.
   - Special-mode endpoints are the rectangle edges, with no inset.
7. For vias, choose the route-continuation types from the residues of `t` in type-number order. `rtype` is the first residue equal to the previous segment's layer type (or simply the first residue if this is the net's first tile). `r2type` is the last other residue.

Compatibility: exact

(Magic ref: lef/defWrite.c:935-1311)

#### Scenario: Regular horizontal wire

- **GIVEN** s = 10, regular net `A` (top port), metal1 route width W = 3, and a single metal1 tile (0,10)-(100,13)
- **THEN** orient = E and y = f32(23·10)/2 = 115, x1 = 0 + 1·10 = 10, x2 = 1000 − 10 = 990
- **AND** the record SHALL be `   - A ( PIN A )\n      + ROUTED metal1  ( 10 115 ) ( 990 * ) ;\n`

#### Scenario: Special wire

- **GIVEN** s = 10, special net `vdd!`, and a metal1 tile (0,0)-(200,10)
- **THEN** W = 10, y = 50, and the record SHALL be `   - vdd!\n      + ROUTED metal1 100  ( 0 50 ) ( 2000 * ) ;\n`

#### Scenario: Wide regular wire creates a non-default rule

- **GIVEN** s = 10, metal1 W = 3, and a regular-net metal1 tile (0,0)-(100,8)
- **THEN** a rule `metal1_width_80` with width 8 SHALL be created, the segment SHALL be written as `metal1 TAPERRULE metal1_width_80  ( 40 40 ) ( 960 * )`, and NONDEFAULTRULES SHALL list it

### Requirement: Route record assembly

The segment writer SHALL keep state across the tiles of one net: the previous tile and its rectangle, the previous layer type (initially "none"), the previous end point `(px,py)`, the previous orientation, and the current rule. Each tile (after the derivation above) SHALL be written as follows.

**Continuation test.** `same = false`. If there is a previous tile:

- Shrink the previous rectangle to its end strip: the last unit in x if its orientation was E, in y if N.
- Shrink the current rectangle to its start strip: the first unit in x if E, in y if N.
- If the two strips touch (share an edge or overlap):
  - previous E: `same` is true when the strips abut in x. In special mode it is false unless both have the same y extent.
  - previous N: `same` is true when they abut in y. In special mode it is false unless both have the same x extent.
  - previous CENTER: `same = true`.
- In special mode, a previous CENTER makes `same = false`.
- If `same` and a rule is selected, `same = false` and the remembered rule is cleared.
- If the selected rule differs from the remembered rule, `same = false` and the rule is remembered.

**Writing** (`write(x,y,o)` is the coordinate writer below; `vianame` is the via looked up by the position key of this contact region; if not found, report `Cannot find via name <key> in table!` and use `<lefname>_<%g10 f32(w)·s>_<%g10 f32(h)·s>`):

1. If `!same` or `t ≠` the previous layer type:
   - a. **Via continuing a route.** Applies if `same`, `t` ≠ previous type, the current tile is a via, and `rtype` = the previous type. Bring the route to the via center:
     - previous N with `x1 = px`: write(px, y1, N).
     - previous E with `y1 = py`: write(x1, py, E).
     - previous E otherwise: write(x1, py, E), then write(x1, y1, N).
     - previous N otherwise: write(px, y1, N), then write(x1, y1, E).

     Then apply the line-break rule with `len(vianame)+2` and write ` <vianame> `.
   - b. **Otherwise, start a new record.**
     - If there is a previous tile: if it was not a via, write(px, py, previous orient). Then write `\n      NEW ` and set the column to 10.
     - If there is no previous tile: write `\n      + ROUTED ` and set the column to 15.
     - If the current tile is a via: let `R` = the LEF name of `rtype` (or of `r2type` if `rtype` is none), looked up among route and masterslice layers and then via records. Apply the line-break rule with `len(R)+1` and write `R `. If a rule is selected, write `TAPERRULE <rule> `. In special mode, write the width (below). Then write(x1, y1, CENTER), apply the line-break rule with `len(vianame)+2`, and write ` <vianame> `.
     - If the current tile is a wire: apply the line-break rule with `len(lefname)+1` and write `<lefname> `. Write the optional `TAPERRULE <rule> ` and, in special mode, the width. Then write(x1, y1, CENTER).
2. Else (`same` and same type):
   - If orient = previous orient and `px ≠ x1` and `py ≠ x2` (dogleg; the test compares `py` with `x2`, see Q-LEFDEF-96): write(px, py, prev orient), then write(x1, y1, orient).
   - Else if the previous orient is E: if `x1 = px`, write(x1, py, E). Otherwise end and restart: write(px, py, E), write `\n      NEW <lefname>` and set the column to 10 + len. In special mode, write a space (column + 1) and the width. Then write(x1, y1, CENTER).
   - Else if the previous orient is N: if `y1 = py`, write(px, y1, N). Otherwise the same restart, except that the column is reset to 10 + len *after* the width is written (Q-LEFDEF-97).
   - Else (the previous tile was a via): if orient = N and `x1 ≠ px`, write(x1, py, E). If orient = E and `y1 ≠ py`, write(px, y1, N).
3. Update the state:
   - the previous layer type becomes `r2type` if this is a via with both `rtype` and `r2type` defined, none if this is any other via, and `t` otherwise;
   - `(px,py) = (x2,y2)`;
   - the previous orient becomes the current orient;
   - the previous tile becomes this tile.

**Width writer** (special mode): let `v` = %g10 of `f32(W)·s`. Apply the line-break rule with `len(v)+1` and write `v `.

**Coordinate writer** `write(x,y,o)`: if `column + 12 > 70`, write `\n      ` and set the column to 6. Write ` ( `. Write `* ` if `o` is N or S, else `%g10(x) ` (x is the binary32 value widened). Write `* ` if `o` is E or W, else `%g10(y) `. Write `)`. Add to the column 4 + the length of each field written including its trailing space. The `TAPERRULE` text does not advance the column.

Compatibility: exact

(Magic ref: lef/defWrite.c:1313-1641, lef/defWrite.c:533-623)

#### Scenario: Wire into a via

- **GIVEN** s = 10, regular net, metal1 W = 3, a metal1 tile (0,10)-(30,13) visited first, then a via1 region (30,9)-(36,14) with via `via1_60_50`, where metal1 and metal2 are its residues (in that type order)
- **THEN** the route SHALL read `+ ROUTED metal1  ( 10 115 ) ( 330 * ) via1_60_50 ` and continue on metal2 with the next tile

#### Scenario: Off-centre via adds a jog

- **GIVEN** the same wire, but the via region is (30,10)-(36,16) (centre y = 130)
- **THEN** the route SHALL read `( 10 115 ) ( 330 * ) ( * 130 ) via1_60_60 `

#### Scenario: Net starting on a via

- **GIVEN** a regular net whose first visited tile is via region (0,0)-(6,6) with residues metal1 then metal2, and s = 10
- **THEN** the record SHALL begin `\n      + ROUTED metal1  ( 30 30 ) via1_60_60 `

#### Scenario: Two disjoint wires

- **GIVEN** a net with metal1 tiles (0,10)-(100,13) and (0,50)-(100,53) that do not touch, visited in that order
- **THEN** the output SHALL be `+ ROUTED metal1  ( 10 115 ) ( 990 * )` followed by `\n      NEW metal1  ( 10 515 ) ( 990 * ) ;`

### Requirement: NONDEFAULTRULES section

After the nets are produced, the count SHALL be the number of entries in the non-default rule table. This table includes rules created by this write, rules created by earlier writes in the same session, and rules read by `lef read`/`def read` (see Q-LEFDEF-99). Rules SHALL be written in table enumeration order:

```
  - <rulename>[anal-lines]{\n     + LAYER <L1> WIDTH <%g10 f32(width1)·s>[ WIREEXT <%g10 f32(ext1)/2.0>]} ;\n
```

The braces repeat once per layer entry in the rule, but every repetition writes the *first* entry's layer, width and extension (Q-LEFDEF-98). WIREEXT is written only when the extension is greater than 0. It is not multiplied by `s`.

With `-anal`, before the rule's own layers, write `\n     + LAYER <canon> WIDTH <%g10 f32(routewidth)·s>` for every distinct route-class layer record, in layer-table enumeration order, that meets all of these conditions:

- it has not already been written for this rule (aliases of one record are written once);
- its magic type is defined;
- the rule has no entry with the same magic type.

Magic dereferences cleared table slots here (Q-LEFDEF-100). Hefesto SHALL skip cleared slots.

Compatibility: exact

(Magic ref: lef/defWrite.c:3063-3135)

#### Scenario: Generated rule

- **GIVEN** the rule `metal1_width_80` (width 8, extension 0) created while writing nets, and s = 10
- **THEN** the section SHALL be `NONDEFAULTRULES 1 ;\n  - metal1_width_80\n     + LAYER metal1 WIDTH 80 ;\nEND NONDEFAULTRULES\n\n`

#### Scenario: Anal mode lists other layers

- **GIVEN** route layers metal1 (width 3) and metal2 (width 4), the rule `metal1_width_80`, and `-anal`, with s = 10
- **THEN** the rule SHALL contain `+ LAYER metal2 WIDTH 40` before `+ LAYER metal1 WIDTH 80`

### Requirement: BLOCKAGES section

The system SHALL build one blockage bucket per live layer-table record of class route or via, in table enumeration order. This includes the temporary generated vias that exist at that moment.

- Bucket name: the canonical name for a route record, or the LEF name of the record's magic type for a via record.
- Bucket mask: the record's magic type, plus its obstruction type if it has one. An obstruction type that is not electrically active SHALL also be added to the "non-electrical obstruction" set.

Geometry SHALL be collected in two passes:

1. For every netlist node that is neither special nor a port (and not space-typed), run the connectivity search from the node's location over types connecting to its layer. For each tile reached, take the tile's type, or the residue on the visited plane for a stacked contact. Use the side's type for a split tile. If the type is non-space, prepend the tile's bounding rectangle to the first bucket whose mask contains the type.
2. For each plane in plane order, enumerate every tile whose type is in the non-electrical obstruction set, and prepend it in the same way.

Buckets are lists, so their output order is the reverse of the order in which rectangles were added. Contact tiles are reached on every plane they occupy, so the same rectangle can appear more than once.

If at least one bucket is non-empty, write `BLOCKAGES <number of non-empty buckets> ;`. Then, for each non-empty bucket in bucket order, write `   - LAYER <name>` followed by `\n      RECT ( x1 y1 ) ( x2 y2 )` per rectangle, with each coordinate `f32(v)·s` (product rounded to binary32), and then ` ;\n`. Finally write `END BLOCKAGES\n\n`.

In Magic, pass 1 passes its arguments to the per-tile collector in the wrong order (undefined behaviour, typically a crash, whenever such a node has geometry; Q-LEFDEF-101). Hefesto SHALL implement the behaviour described here.

Compatibility: exact

(Magic ref: lef/defWrite.c:2467-2747)

#### Scenario: Obstruction paint becomes a blockage

- **GIVEN** s = 10, the tech line `obstruction obsm1 metal1`, `obsm1` not electrically active, and one obsm1 rectangle (0,0)-(20,10)
- **THEN** the file SHALL contain `BLOCKAGES 1 ;\n   - LAYER metal1\n      RECT ( 0 0 ) ( 200 100 ) ;\nEND BLOCKAGES\n\n`

#### Scenario: No obstruction geometry

- **GIVEN** no obstruction paint and no floating nets
- **THEN** no BLOCKAGES section SHALL be written

### Requirement: Table enumeration order

Wherever this capability says "table enumeration order" (the layer table, the non-default rule table, the via position index), the order SHALL be the iteration order of a Magic string hash table as defined in capability `hefesto-output-determinism` (requirement "Magic string hash table"), which owns the hash, bucket, growth and iteration rules and the signed-byte interpretation of key bytes (see Q-LEFDEF-116). Under decision X-1 these orders depend only on names and insertion history, and the output SHALL be byte-identical to Magic's. This capability adds:

- **Size.** The layer table and the rule table each request 32 buckets.
- **Removal.** Removing a generated via clears its value but keeps the key and its position, so an emptied slot can be refilled in a later write. Enumeration skips empty slots.

The subcell enumeration order for COMPONENTS SHALL be the cell's subcell-plane enumeration order (see the database capabilities).

Compatibility: exact

(Magic ref: lef/lefTech.c:151-180, lef/defWrite.c:2052-2068)

#### Scenario: Hash of a short key

- **GIVEN** a layer-table key `"ab"` (bytes 97, 98) in a table of size 32 (shift = 25)
- **THEN** h = 98 + 97·65599 = 6363201, and the bucket SHALL be `((6363201·1103515245 + 12345) mod 2^64 >> 25) & 31` = 1

### Requirement: Session state after writing

After writing, the system SHALL remove the generated via records from the layer table and SHALL discard the via position index. Non-default rules created during the write SHALL remain in the rule table for the rest of the session (until the technology is reloaded), so later `def write` and `lef write` runs see them (see Q-LEFDEF-99). Writing SHALL NOT modify the layout and SHALL NOT be undoable.

Compatibility: behavioral

(Magic ref: lef/defWrite.c:3197-3207, lef/lefTech.c:151-180)

#### Scenario: Repeat write accumulates rules

- **GIVEN** a write that created rule `metal1_width_80`
- **WHEN** `def write` is run again on a different cell with no wide wires
- **THEN** that file's NONDEFAULTRULES SHALL still list `metal1_width_80`
