# LEF Write

## Purpose

Defines how Hefesto writes LEF (Library Exchange Format) abstract views of layout cells so that the result is byte-compatible with Magic VLSI 8.3.694. It covers the `lef write`, `lef writeall`, `lef nocheck` and `lef help` command surfaces, the file header and technology section (`-tech`), numeric formatting, macro construction (class, size, origin, pins, ports, antenna areas, obstructions), the abstract-generation options `-hide`, `-pinonly`, `-toplayer` and `-nomaster`, and cell properties that steer LEF output (`LEFclass`, `LEFsymmetry`, `LEFsite`, `LEFproperties`, `FIXED_BBOX`, `OBS_BBOX`).

Related capabilities, referenced by name and not repeated here:

- `lef-tech-section`: the tech-file `lef` section. It maps magic types to LEF layer names and classes (route, cut/contact, masterslice, overlap, bound, ignore, obstruction), and sets the canonical names and default route/via parameters.
- `lef-read`: reading LEF, the `lef read` and `lef datestamp` options, and how LEF reading creates or updates layer parameters and cell properties.
- `def-write`: DEF output. It shares the magic-type to LEF-layer lookup defined in this spec.

Conventions used throughout:

- "Internal units" are Magic database units.
- `oscale` is the number of micrometres per internal unit (see "Output scale and LEF database units").
- `B = (bxl, byl, bxh, byh)` is the macro boundary (see "Macro boundary determination").
- Rectangles are half-open `[xl,xh) x [yl,yh)` in internal units.
- Indentation strings are exact: `IN0` = 2 spaces, `IN1` = 4 spaces, `IN2` = 6 spaces, `IN3` = 8 spaces.

## Requirements

### Requirement: Command dispatch and target cell

The `lef` command SHALL take a sub-option as its first argument. The sub-option is matched case-insensitively against the keywords `read`, `write`, `writeall`, `nocheck`, `datestamp` and `help`. An exact match or an unambiguous prefix SHALL select that keyword. With no sub-option, the command SHALL behave as `help`. An ambiguous or unknown sub-option SHALL print `"<arg>" isn't a valid lef option.` and then the help text. For every sub-option except `help`, the target cell SHALL be chosen in this order:

1. The currently selected cell instance.
2. Otherwise, the root cell of the only layout window.
3. Otherwise, with no window, the cell under the box tool, through its parent use.
4. If none of these exist, the command SHALL print `Point to a window first` and do nothing.

`read` and `datestamp` are specified in capability `lef-read`.

Compatibility: behavioral

(Magic ref: lef/lefCmd.c:69-75, lef/lefCmd.c:200-235, utils/lookup.c:33-55)

#### Scenario: Prefix sub-option
- **GIVEN** a layout window showing cell `inv`, with nothing selected
- **WHEN** the user runs `lef wr`
- **THEN** the command SHALL fail as ambiguous, because `wr` is a prefix of both `write` and `writeall`, and SHALL print the help text
- **AND** `lef writea` SHALL select `writeall`

#### Scenario: Selected instance takes precedence
- **GIVEN** a window showing cell `top`, and a selected instance of cell `nand2`
- **WHEN** the user runs `lef write`
- **THEN** LEF SHALL be written for `nand2`, not for `top`

### Requirement: lef write argument grammar

`lef write` SHALL accept an optional output file name and any number of options, following the grammar below. The file name, if present, SHALL be the first argument after `write`. Any other argument that does not start with `-` and is not consumed as an option value SHALL be a usage error, which prints the help text and writes nothing.

An option matches when the argument *starts with* the option's literal text, comparing only that text's length:

| Option | Chars compared |
|---|---|
| `-tech` | 5 |
| `-hide` | 5 |
| `-pinonly` | 8 |
| `-toplayer` | 9 |
| `-nomaster` | 9 |
| `-allspec` | 8 (DEF only) |
| `-units` | 6 (DEF only) |
| `-anal` | 5 (DEF only) |

The DEF-only options `-allspec`, `-units` and `-anal` SHALL be ignored under `lef`, with the informative message `The "<opt>" option is only for def write`. Any other argument starting with `-` SHALL be a usage error. If no target cell exists, the command SHALL print `No cell selected` and write nothing. If no file name is given, the target cell's name SHALL be used as the file name. The macro is written with the library header only when the target cell is the current edit root cell (see "Single-cell file layout").

```ebnf
lef_write   = "lef" , "write" , [ filename ] , { option } ;
option      = "-tech"
            | "-hide" , [ distance ]
            | "-pinonly" , [ distance ]
            | "-toplayer"
            | "-nomaster" ;
distance    = numeric_token ;   (* see "Distance option values" *)
```

Option defaults:

| Option | Default | Meaning of the default |
|---|---|---|
| tech | false | |
| hide (setback) | -1 | disabled |
| pinonly | -1 | disabled |
| toplayer | false | |
| domaster | true | masterslice layers are written |

Compatibility: behavioral

(Magic ref: lef/lefCmd.c:330-436)

#### Scenario: File name must come first
- **WHEN** the user runs `lef write -hide out.lef`
- **THEN** `out.lef` SHALL NOT be taken as a distance, because it is not numeric
- **AND** it SHALL NOT be taken as a file name, because it is not the first argument
- **AND** the command SHALL print the help text and write nothing

#### Scenario: Option prefix match
- **WHEN** the user runs `lef write abs -technology`
- **THEN** the argument SHALL match `-tech`, and the technology section SHALL be written to file `abs.lef`

### Requirement: Distance option values

When `-hide` or `-pinonly` is followed by an argument whose leading characters parse as a floating-point number, that argument SHALL be consumed as the option's distance. The criterion is that a C `strtod` conversion consumes at least one character, so `5`, `-3`, `0.5um` and `2e1` all count. The distance SHALL be converted to internal units by the standard coordinate parser, honoring unit suffixes such as `um` and the current default distance unit, which is lambda unless the snap/units setting changes it. With no numeric argument, the option's value SHALL be 0. A resulting negative value SHALL disable that option.

Compatibility: behavioral

(Magic ref: lef/lefCmd.c:350-379, utils/strdup.c:154-166, commands/CmdSubrs.c:324-345)

#### Scenario: Hide with explicit distance
- **GIVEN** the default units are lambda, and 1 lambda = 1 internal unit
- **WHEN** the user runs `lef write -hide 10`
- **THEN** the setback SHALL be 10 internal units

#### Scenario: Negative distance disables hiding
- **WHEN** the user runs `lef write -hide -5`
- **THEN** `-5` SHALL be consumed as the distance
- **AND** the setback SHALL be -5, so no hiding is performed and the output SHALL equal plain `lef write`

### Requirement: lef writeall arguments and scope

`lef writeall` SHALL accept only options from the grammar below; any other argument, including a file name, SHALL be a usage error. Options are matched by leading-text comparison:

| Option | Chars compared |
|---|---|
| `-notop` | 6 |
| `-tech` | 5 |
| `-hide` | 5 |
| `-pinonly` | 8 |
| `-toplayer` | 9 |
| `-nomaster` | 9 |
| `-all` | 4 |

`-hide` and `-pinonly` take optional distances as in "Distance option values".

`lef writeall` SHALL write one file containing:

- the full library header (see "Library header"), always written, with the technology part only under `-tech`;
- one MACRO per cell in the write set, in the order given by "writeall macro order";
- `END LIBRARY`.

The write set SHALL contain:

- the target cell, unless `-notop` is given;
- every cell instantiated directly by the target cell;
- with `-all`, every cell reachable through the hierarchy below the target cell.

Each cell SHALL appear at most once. Internal scratch cells SHALL be excluded. Before writing, the whole subtree under the target cell SHALL be loaded. If any cell cannot be read, the command SHALL print `Could not read entire subtree of the cell.` and `Failed on cell <name>.` and write nothing.

Compatibility: behavioral

(Magic ref: lef/lefCmd.c:282-329, lef/lefWrite.c:2211-2347)

#### Scenario: Library of children only
- **GIVEN** cell `top` that instantiates `inv` (twice) and `nand2`, and `nand2` instantiates `nmos4`
- **WHEN** the user runs `lef writeall -notop`
- **THEN** the file SHALL contain exactly two macros, `inv` and `nand2`
- **AND** with `-all` added, it SHALL also contain `nmos4`

#### Scenario: Def-style option rejected
- **WHEN** the user runs `lef writeall out.lef`
- **THEN** the command SHALL print the help text and write nothing

### Requirement: writeall macro order

Cells SHALL be collected by a depth-first, pre-order traversal:

1. The target cell is collected first, unless `-notop` is given.
2. The direct children of the target cell are visited in the database's subcell-enumeration order. Each child not yet collected is collected.
3. With `-all`, the traversal recurses into a child's children immediately after collecting that child.

Macros SHALL then be written in the **reverse** of collection order (last collected, first written). As a result, the target cell, when included, is written last. See Q-LEFDEF-41 for the dependence on subcell-enumeration order.

Compatibility: exact

(Magic ref: lef/lefWrite.c:2246-2250, lef/lefWrite.c:2286-2291, lef/lefWrite.c:2321-2339)

#### Scenario: Top written last
- **GIVEN** cell `top` with a single child `inv`
- **WHEN** the user runs `lef writeall`
- **THEN** the MACRO order SHALL be `inv` and then `top`

#### Scenario: Recursive order
- **GIVEN** `top` whose child enumeration yields `a` then `b`, and `a` has child `c`
- **WHEN** the user runs `lef writeall -notop -all`
- **THEN** the collection order SHALL be `a, c, b`
- **AND** the MACRO order SHALL be `b, c, a`

### Requirement: Output file naming

The output path SHALL be derived from a base name. For `lef write`, the base name is the given file name, or the cell name if none is given. For `lef writeall`, it is the target cell's file path if the cell was loaded from a file, otherwise its name. The derivation SHALL work as follows:

1. Take the extension to be the text after the last `.` in the final path component.
2. If the extension is exactly `.lef`, open the name as given.
3. If there is another extension, first try to open the name exactly as given. If that fails, strip the extension and append `.lef`.
4. If there is no extension, append `.lef`.
5. If opening still fails and the base name came from the cell's file path, retry with the cell name plus `.lef`.

A failure to open SHALL print `Cannot open output file <name> (<reason>).` and write nothing. On iPadOS, the file SHALL be created inside the app's document container (platform mechanism; see Q-LEFDEF-43).

Compatibility: behavioral

(Magic ref: lef/lefWrite.c:150-235, lef/lefWrite.c:2367-2382)

#### Scenario: Default name
- **WHEN** `lef write` is run on cell `inv`
- **THEN** the file `inv.lef` SHALL be written

#### Scenario: Unusual extension kept
- **WHEN** the user runs `lef write cells.tlef`
- **THEN** the file `cells.tlef` SHALL be written, without `.lef` appended

#### Scenario: writeall uses the cell's file
- **GIVEN** cell `top` loaded from `/work/top.mag`
- **WHEN** the user runs `lef writeall`
- **THEN** the output SHALL be `/work/top.lef`

### Requirement: lef nocheck net list

The system SHALL keep a session-global list of net names that are excluded from antenna-area computation. The list starts empty.

- `lef nocheck` with no further argument SHALL report the list. Magic returns the names as a list, or prints `There are no net names being ignored.` (Tcl result `none`) when the list is empty.
- `lef nocheck n1 n2 ...` SHALL process its arguments left to right. An argument equal to `none`, compared case-insensitively, SHALL clear the list. Any other argument SHALL be added to the front of the list; duplicates are not removed.
- A list entry `*` SHALL match every net.
- Under `def`, `nocheck` SHALL print `The "nocheck" option is only for LEF writes.` and do nothing.

Compatibility: behavioral

(Magic ref: lef/lefCmd.c:483-545, lef/lefWrite.c:1494-1505)

#### Scenario: Wildcard suppresses all antenna output
- **GIVEN** `lef nocheck *`
- **WHEN** a cell whose input pin `A` connects to transistor gates is written with `lef write`
- **THEN** no `ANTENNAGATEAREA` or `ANTENNADIFFAREA` line SHALL be written for any pin

#### Scenario: Reset
- **WHEN** the user runs `lef nocheck VPWR none VGND`
- **THEN** the list SHALL contain only `VGND`

### Requirement: Help text

`lef help`, `lef` with no sub-option, and every usage error SHALL print `The "lef" options are:` followed by one line per sub-option. Each line is formatted as `    lef <description>`, where the description lists that sub-option's arguments.

Compatibility: informative

(Magic ref: lef/lefCmd.c:155-171, lef/lefCmd.c:547-557)

#### Scenario: Help
- **WHEN** the user runs `lef help`
- **THEN** a usage summary covering read, write, writeall, nocheck, datestamp and help SHALL be printed

### Requirement: Output scale and LEF database units

The system SHALL compute `oscale`, the number of micrometres per internal unit, from the current CIF/GDS output style (see the capability that specifies the `cifoutput` tech section):

`oscale = float32(10 * scaleFactor) / float32(expander * 1000)`

The result is a 32-bit float. `scaleFactor` is the style's centimicrons per internal unit, multiplied by `expander`. `expander` is 1 normally, 10 for a style declared in nanometres, and 100 for angstroms. With no output style, `oscale = 1.0`.

The LEF database-unit value `U` SHALL be computed whenever a library header is written:

1. Start with `U = 1000`.
2. If the output style declares angstrom units, set `U = 10000`.
3. If the style's grid limit is 1, 5 or 10, divide `U` by it, using integer division.

So `U` is one of 100, 200, 1000, 2000 or 10000. `U` persists for the session and is used by number formatting until the next header is written. Before any header is written, `U = 1000`. See Q-LEFDEF-34.

Compatibility: exact

(Magic ref: lef/lefWrite.c:66, lef/lefWrite.c:274-292, cif/CIFmain.c:139-146)

#### Scenario: Nanometre style with grid limit 5
- **GIVEN** an output style in nanometres with grid limit 5
- **WHEN** a header is written
- **THEN** `U` SHALL be 200

#### Scenario: Angstrom style
- **GIVEN** an output style in angstroms with no grid limit
- **WHEN** a header is written
- **THEN** `U` SHALL be 10000

### Requirement: Distance number formatting

Every distance value written in a macro, and every SITE `SIZE` value, SHALL be computed in 32-bit float arithmetic as `v = oscale * float32(n)`, where `n` is an integer in internal units. Values written as `0.0 - x` are computed in double precision and then narrowed to float32. Each value SHALL be formatted as follows:

```
r = (v < 0) ? -0.5 : 0.5                       (float32)
q = truncf(v * float32(U) + r) / float32(U)    (float32)
text = printf(fmt, (double) q)
```

The format `fmt` depends on `U`:

| `U` | `fmt` |
|---|---|
| 100 | `%.2f` |
| 200 | `%.3f` |
| 1000 | `%.3f` |
| 2000 | `%.4f` |
| 10000 | `%.4f` |
| 20000 | `%.5f` |
| any other value | `%.5f`, with `U` replaced by 100000 |

This is round-half-away-from-zero to the `1/U` grid, applied in float32. Values that are exactly 0 SHALL print without a minus sign; the ORIGIN line computes `0.0 - x`, which yields `+0.0` for `x = 0`. A value in `(-1/(2U), 0)` would print as `-0.000`; see Q-LEFDEF-45.

Antenna areas, technology LAYER values and CUT AREA are NOT passed through this rounding. They SHALL be printed with `%f`, which gives six decimals.

Compatibility: exact

(Magic ref: lef/lefWrite.c:90-127)

#### Scenario: Three decimals
- **GIVEN** `U = 1000` and `oscale = 0.005`
- **WHEN** coordinate 23 is written
- **THEN** the text SHALL be `0.115`

#### Scenario: Two decimals
- **GIVEN** `U = 100` and `oscale = 0.01`
- **WHEN** coordinate 123 is written
- **THEN** the text SHALL be `1.23`

#### Scenario: Half-unit rounding (verify against Magic)
- **GIVEN** `U = 1000` and `oscale = 0.0005`
- **WHEN** coordinates 3 and -3 are written
- **THEN** the results SHALL follow the float32 formula exactly
- **AND** the expected texts are `0.002` and `-0.002` if `0.0005f*3` rounds up to `0.0015`, otherwise `0.001` and `-0.001`

### Requirement: LEF file grammar

LEF output SHALL follow the grammar below. Every line ends with `\n`. Indentation is literal. `F` is a formatted distance (see "Distance number formatting"). `R` is a `%f` real.

```ebnf
lef_file      = [ header ] , { macro } , "END LIBRARY\n" , "\n" ;
header        = "VERSION 5.7 ;\n"
              , "  NOWIREEXTENSIONATPIN ON ;\n"
              , "  DIVIDERCHAR \"/\" ;\n"
              , "  BUSBITCHARS \"[]\" ;\n"
              , [ units ] , [ propdefs ] , { site } , [ tech_layers ] ;
units         = "UNITS\n" , "  DATABASE MICRONS " , int , " ;\n" , "END UNITS\n" , "\n" ;
propdefs      = "PROPERTYDEFINITIONS\n"
              , { "  MACRO " , key , " STRING ;\n" }-
              , "END PROPERTYDEFINITIONS\n" , "\n" ;
site          = "SITE " , name , "\n"
              , "  SYMMETRY " , text , " ;\n"
              , "  CLASS " , text , " ;\n"
              , "  SIZE " , F , " BY " , F , " ;\n"
              , "END " , name , "\n" , "\n" ;
tech_layers   = { tech_layer } ;
tech_layer    = "LAYER " , lname , "\n" , layer_body , "END " , lname , "\n" , "\n" ;
layer_body    = cut_body | route_body | "  TYPE MASTERSLICE ;\n" | "  TYPE OVERLAP ;\n" ;
cut_body      = "  TYPE CUT ;\n" , [ "  CUT AREA " , R , " ;\n" ] ;
route_body    = "  TYPE ROUTING ;\n"
              , [ "  PITCH " , R , " ;\n" ] , [ "  WIDTH " , R , " ;\n" ]
              , [ "  SPACING " , R , " ;\n" ]
              , [ "  DIRECTION " , ( "HORIZONTAL" | "VERTICAL" ) , " ;\n" ] ;
macro         = "MACRO " , cell , "\n"
              , "  CLASS " , text , " ;\n"
              , "  FOREIGN " , cell , " ;\n"
              , "  ORIGIN " , F , " " , F , " ;\n"
              , "  SIZE " , F , " BY " , F , " ;\n"
              , [ "  SYMMETRY " , text , " ;\n" ]
              , [ "  SITE " , text , " ;\n" ]
              , { pin } , [ obs ] , { "  PROPERTY " , prop , " ;\n" }
              , "END " , cell , "\n" ;
pin           = "  PIN " , pname , "\n"
              , [ "    DIRECTION " , dir , " ;\n" ]
              , [ "    USE " , use , " ;\n" ] , [ "    USE GROUND ;\n" ]
              , [ "    SHAPE " , shape , " ;\n" ]
              , [ "    ANTENNAGATEAREA " , R , " ;\n" ]
              , [ "    ANTENNADIFFAREA " , R , " ;\n" ]
              , { port }-
              , "  END " , pname , "\n" ;
port          = "    PORT\n" , geom_items , "    END\n" ;
obs           = "  OBS\n" , geom_items , "  END\n" ;
geom_items    = { [ "      LAYER " , lname , " ;\n" ] , shape_rec }- ;
shape_rec     = "        RECT " , F , " " , F , " " , F , " " , F , " ;\n"
              | "        POLYGON " , F , " " , F , " " , F , " " , F , " " , F , " " , F , " ;\n" ;
dir           = "INPUT" | "OUTPUT" | "OUTPUT TRISTATE" | "INOUT" | "FEEDTHRU" | "" ;
use           = "SIGNAL" | "ANALOG" | "POWER" | "GROUND" | "CLOCK" | "" ;
shape         = "ABUTMENT" | "RING" | "FEEDTHRU" ;
```

Notes on the grammar:

- No blank line separates consecutive macros, or the header from the first macro, unless a header block that ends in a blank line comes before it.
- `NAMESCASESENSITIVE` SHALL NOT be written.
- The empty alternatives of `dir` and `use` are explained in Q-LEFDEF-36.

Annotated example. The setup is: `oscale = 0.01`, `U = 1000`, the edit-root cell `inv`, no `-tech`, an extraction style with no gate connection to the pins, and these properties:

| Property | Value |
|---|---|
| `LEFclass` | `CORE` |
| `LEFsymmetry` | `X Y` |
| `LEFsite` | `unit` |

The cell has no SITE cell named `unit`. Its bounding box is (0,0)-(100,200). The metal1 LEF name is `met1`. Port `A` (index 0, INPUT, SIGNAL) is a metal1 rectangle (10,50)-(30,150). Port `Y` (index 1, OUTPUT) is a metal1 rectangle (70,50)-(90,150). The only other routing paint is a metal1 rectangle (40,0)-(60,20).

```
VERSION 5.7 ;                              <- header (cell is edit root)
  NOWIREEXTENSIONATPIN ON ;
  DIVIDERCHAR "/" ;
  BUSBITCHARS "[]" ;
MACRO inv                                  <- no UNITS block without -tech
  CLASS CORE ;                             <- LEFclass
  FOREIGN inv ;
  ORIGIN 0.000 0.000 ;                     <- negated boundary lower-left
  SIZE 1.000 BY 2.000 ;
  SYMMETRY X Y ;                           <- LEFsymmetry
  SITE unit ;                              <- LEFsite
  PIN A                                    <- port index 0
    DIRECTION INPUT ;
    USE SIGNAL ;
    PORT
      LAYER met1 ;
        RECT 0.100 0.500 0.300 1.500 ;
    END
  END A
  PIN Y
    DIRECTION OUTPUT ;
    PORT
      LAYER met1 ;
        RECT 0.700 0.500 0.900 1.500 ;
    END
  END Y
  OBS                                      <- remaining routing geometry
      LAYER met1 ;
        RECT 0.400 0.000 0.600 0.200 ;
  END
END inv
END LIBRARY
                                           <- final empty line
```

Compatibility: exact

(Magic ref: lef/lefWrite.c:252-497, lef/lefWrite.c:816-958, lef/lefWrite.c:1028-1129, lef/lefWrite.c:1294-1387, lef/lefWrite.c:2032-2084, lef/lefWrite.c:2294)

#### Scenario: Example output
- **GIVEN** the cell described in the annotated example
- **WHEN** the user runs `lef write`
- **THEN** `inv.lef` SHALL be byte-identical to the example text, without the `<-` annotations and the whitespace before them

### Requirement: Single-cell file layout

`lef write` SHALL write a library header only when the target cell is the current edit root cell. If it is not, for example when a sub-instance is selected, the file SHALL contain only the MACRO and the `END LIBRARY` trailer, with no `VERSION` line. In that case `U` is not recomputed. For a single-cell header, PROPERTYDEFINITIONS and SITE blocks SHALL be derived from that one cell only. See Q-LEFDEF-34.

Compatibility: exact

(Magic ref: lef/lefWrite.c:2384-2399, lef/lefCmd.c:430-433)

#### Scenario: Selected subcell
- **GIVEN** edit root `top`, and a selected instance of `inv`
- **WHEN** the user runs `lef write`
- **THEN** `inv.lef` SHALL begin with `MACRO inv` and end with `END LIBRARY` followed by an empty line

### Requirement: Library header

The header SHALL be written in this order:

1. The four fixed lines: `VERSION 5.7 ;`, then `NOWIREEXTENSIONATPIN ON ;`, `DIVIDERCHAR "/" ;` and `BUSBITCHARS "[]" ;`, each indented with 2 spaces.
2. Only under `-tech`: the UNITS block `UNITS` / `  DATABASE MICRONS <U> ;` / `END UNITS`, followed by an empty line. `<U>` is printed with `%d`.
3. The PROPERTYDEFINITIONS block, if any property key is collected. It contains one `  MACRO <key> STRING ;` line per distinct key, then `END PROPERTYDEFINITIONS` and an empty line.
4. The SITE blocks (see "SITE blocks").
5. Only under `-tech`: the technology LAYER blocks (see "Technology layers").

Property keys SHALL be collected from the `LEFproperties` string of every cell in the write set:

1. Each key is the first space-delimited word of a key/value pair.
2. The value is either a double-quoted string, which may contain spaces, or the next word.
3. Pairs are separated by one or more spaces.
4. Collection stops at a key with no value.

All property types are declared as `STRING`. The order of keys SHALL be the iteration order of the collection's Magic string hash table (capability `hefesto-output-determinism`, requirement "Magic string hash table"), reproduced exactly under decision X-1; see Q-LEFDEF-30.

Compatibility: exact

(Magic ref: lef/lefWrite.c:252-314, lef/lefWrite.c:2139-2200)

#### Scenario: Units with tech
- **GIVEN** `U = 1000`
- **WHEN** the user runs `lef write -tech` on the edit root cell
- **THEN** lines 5-8 of the file SHALL be `UNITS`, `  DATABASE MICRONS 1000 ;`, `END UNITS` and an empty line

#### Scenario: Property definitions
- **GIVEN** cell `inv` with `LEFproperties` = `CDL "inv x" MASK 1`
- **WHEN** the user runs `lef write`
- **THEN** the header SHALL contain `PROPERTYDEFINITIONS`, then the lines `  MACRO CDL STRING ;` and `  MACRO MASK STRING ;` in hash order, then `END PROPERTYDEFINITIONS` and an empty line

### Requirement: SITE blocks

For each distinct `LEFsite` property value among the cells in the write set, the system SHALL look for a cell with exactly that name. If such a cell exists, a SITE block SHALL be written:

- `SITE <name>`
- `  SYMMETRY <LEFsymmetry of the site cell, default Y> ;`
- `  CLASS <LEFclass of the site cell, default CORE> ;`
- `  SIZE <w> BY <h> ;`, where `w` and `h` are the site cell's bounding-box width and height, formatted as distances
- `END <name>`
- an empty line

If the site cell has the fixed-bounding-box flag, the box SHALL instead come from the `FIXED_BBOX` property of the cell *being written*, not the site cell, provided that property exists as four integers. See Q-LEFDEF-33. Values with no cell of that name SHALL be skipped silently. SITE blocks SHALL appear in the iteration order of the site Magic string hash table (capability `hefesto-output-determinism`), reproduced exactly under decision X-1.

Compatibility: exact

(Magic ref: lef/lefWrite.c:315-370, lef/lefWrite.c:2109-2125)

#### Scenario: Site from LEF-read site cell
- **GIVEN** a site cell `unit` of size 46x272 internal units with no properties, `oscale = 0.005`, and `U = 1000`
- **AND** cell `inv` with `LEFsite = unit`
- **WHEN** `inv` is written as the edit root
- **THEN** the header SHALL contain `SITE unit`, `  SYMMETRY Y ;`, `  CLASS CORE ;`, `  SIZE 0.230 BY 1.360 ;`, `END unit` and an empty line

### Requirement: Technology layers

Under `-tech`, the system SHALL write one LAYER block for each layer record of the tech `lef` section (see capability `lef-tech-section`). Each record is written once even when several LEF names alias it, always under its canonical name. These records SHALL be skipped:

- records with no magic type (obstruction-only and `ignore` records);
- records of class bound;
- via records that hold a fixed via geometry cell (vias created by LEF/DEF reading).

The remaining records SHALL be written as follows:

| Record class | Lines written |
|---|---|
| cut/contact | `  TYPE CUT ;`, plus `  CUT AREA %f ;` when the stored cut rectangle area `A` (internal units squared) is positive. The value is `float(A) * oscale * oscale`. |
| route | `  TYPE ROUTING ;`, then `  PITCH %f ;` if pitch > 0, `  WIDTH %f ;` if width > 0, `  SPACING %f ;` if spacing > 0 (each value times `oscale`), then `  DIRECTION HORIZONTAL ;` or `  DIRECTION VERTICAL ;` if width > 0 |
| masterslice | `  TYPE MASTERSLICE ;` |
| overlap | `  TYPE OVERLAP ;` |

Pitch, width, spacing, direction and the cut rectangle are the values held in the layer record, initialized by the tech file and possibly updated by `lef read` (see capabilities `lef-tech-section` and `lef-read`). The block order SHALL be the layer-table enumeration order (Requirement "Table enumeration order" of capability `def-write`), reproduced exactly under decision X-1; see Q-LEFDEF-30. With default initialization, the written WIDTH equals the DRC default spacing and no SPACING line appears; see Q-LEFDEF-31. The CUT AREA default is discussed in Q-LEFDEF-32.

Compatibility: exact

(Magic ref: lef/lefWrite.c:374-497, lef/lefTech.c:325-370, lef/lefTech.c:478-516)

#### Scenario: Route layer block (verify against Magic)
- **GIVEN** route record `met1`, with width 14, spacing -1, pitch 0, horizontal direction, and `oscale = 0.01`
- **WHEN** `-tech` output is written
- **THEN** the block SHALL be `LAYER met1`, `  TYPE ROUTING ;`, `  WIDTH 0.140000 ;`, `  DIRECTION HORIZONTAL ;`, `END met1` and an empty line

#### Scenario: Obstruction-only name skipped
- **GIVEN** a tech line `obstruction m1obs met1obs` with no route mapping for `met1obs`
- **WHEN** `-tech` output is written
- **THEN** no `LAYER met1obs` block SHALL be written

### Requirement: Magic type to LEF layer name

For each magic type `t` (user types and stacked contact types), the system SHALL determine its LEF layer name. The name is the canonical name of the first layer record, in layer-table enumeration order (Requirement "Table enumeration order" of capability `def-write`, a Magic string hash table as defined in capability `hefesto-output-determinism`), for which both of these hold:

1. The record's class is cut/via when `t` is a contact type (including via definitions that `lef read` created), and route or masterslice when `t` is not a contact.
2. The record's magic type, or its obstruction type, equals `t`.

If no record matches, `t` SHALL have no LEF name. Geometry of a type with no LEF name is never written. This capability owns the mapping; capability `def-write` uses it. When several records match, the enumeration order decides the winner, so Hefesto SHALL reproduce Magic's enumeration order exactly (decision X-1); see Q-LEFDEF-30.

Compatibility: exact

(Magic ref: lef/defWrite.c:2041-2097, lef/defWrite.c:2910-2930)

#### Scenario: Obstruction type maps to route name
- **GIVEN** tech lines `route metal1 met1` and `obstruction obsm1 met1`
- **WHEN** paint of type `obsm1` is written
- **THEN** it SHALL be written under `LAYER met1 ;`

#### Scenario: Via definition captures a cut type (verify against Magic)
- **GIVEN** a LEF file read with `VIA M1M2_PR` whose cut is magic type `via1`, and a tech `cut via1 via1` entry
- **WHEN** `M1M2_PR` comes earlier in layer-table enumeration order than `via1`
- **THEN** type `via1` SHALL map to the name `M1M2_PR` (see Q-LEFDEF-92)

### Requirement: Macro source geometry and layer masks

Before writing a macro, the system SHALL flatten the cell. All paint from the cell and every level of its subcell hierarchy, within the cell's bounding box, SHALL be merged into one flat copy using normal paint-combination rules. Space and DRC-marker types SHALL be excluded. Labels SHALL be taken only from the cell itself, not from subcells.

The system SHALL then build the following type sets from the tech `lef` section:

| Set | Contents |
|---|---|
| routing set R | (a) the type of every port label on the cell; (b) for every record of class route or cut/via, plus masterslice when `domaster`: its magic type, all residues if that type is a contact, and its obstruction type if any. Space SHALL be removed from R before pin processing. |
| hide set H | the magic types of route records that have both a magic type and an obstruction type |
| well set W | if `domaster` and an extraction style exists: the magic types of masterslice records that are not substrate types of that style |
| masterslice planes P | if `domaster`: the planes of the masterslice records' types |
| boundary set | the magic types of `bound` records |
| gate set G | types that are the device type of a MOSFET, FET, asymmetric or subcircuit-MOSFET device in the current extraction style |
| diffusion set D | the antenna tie-down types of the current extraction style |

If no extraction style exists, G and D are undefined in Magic; see Q-LEFDEF-40. Hefesto SHALL treat them as empty.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1176-1288, extract/ExtTech.c:514-562)

#### Scenario: Subcell paint appears in macro
- **GIVEN** cell `buf` containing instances of `inv`, and no paint of its own except labels
- **WHEN** `lef write` is run on `buf`
- **THEN** the routing geometry of the `inv` instances SHALL appear in the PIN/OBS records of `buf`

#### Scenario: Nomaster
- **GIVEN** a masterslice record `nwell` mapped to type `nwell`
- **WHEN** the user runs `lef write -nomaster`
- **THEN** no geometry on LEF layer `nwell` SHALL be written, unless a port label sits on type `nwell`

### Requirement: Macro boundary determination

The macro boundary `B` SHALL be determined as follows:

1. If the boundary set is non-empty, `B` is the bounding box of all paint of boundary types in the flat copy, taken over all planes.
2. Otherwise, `B` is the cell's bounding box.
3. If the cell has the fixed-bounding-box flag and a `FIXED_BBOX` property of exactly four integers `xl yl xh yh`, `B` is that box, overriding steps 1 and 2.

If the boundary set is non-empty but contains no paint, Magic's result is undefined; see Q-LEFDEF-53. Geometry is never shifted: all coordinates are written relative to the cell origin, and `ORIGIN` records the offset.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1316-1353)

#### Scenario: FIXED_BBOX wins
- **GIVEN** a cell with paint bbox (-5,0)-(105,200), and `FIXED_BBOX` = `0 0 100 200` with the fixed-bbox flag set
- **AND** `oscale = 0.01`
- **WHEN** it is written
- **THEN** `ORIGIN 0.000 0.000 ;` and `SIZE 1.000 BY 2.000 ;` SHALL be written

### Requirement: Macro header lines

After `MACRO <cellname>`, the system SHALL write, in order:

1. `  CLASS <LEFclass> ;`, or `  CLASS BLOCK ;` when the cell has no `LEFclass` property.
2. `  FOREIGN <cellname> ;`.
3. `  ORIGIN <-bxl*oscale> <-byl*oscale> ;`, each value computed as `0.0 - oscale*bxl`.
4. `  SIZE <(bxh-bxl)*oscale> BY <(byh-byl)*oscale> ;`.
5. `  SYMMETRY <LEFsymmetry> ;`, only if that property exists.
6. `  SITE <LEFsite> ;`, only if that property exists.

Property strings SHALL be written verbatim. The macro name SHALL be the cell name, verbatim.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1294-1387)

#### Scenario: Offset origin
- **GIVEN** `B` = (-20,-10)-(80,190) and `oscale = 0.01`
- **WHEN** the macro is written
- **THEN** `  ORIGIN 0.200 0.100 ;` and `  SIZE 1.000 BY 2.000 ;` SHALL be written

#### Scenario: Default class
- **WHEN** a cell without `LEFclass` is written
- **THEN** `  CLASS BLOCK ;` SHALL be written

### Requirement: Pin enumeration and ordering

A port label is a label of the cell that has at least one port direction bit set. Pins SHALL be generated per port index `i`, for `i = 0 .. maxIndex`, where `maxIndex` is the largest port index among port labels. For each `i`:

1. Process each not-yet-visited port label with index `i`, in the cell's label-list order. Each processed label is marked visited and contributes zero or one PORT block.
2. All labels with index `i` SHALL belong to one PIN, whatever their text. The PIN header uses the text of the first label that produces geometry.
3. The closing line SHALL be `  END <text of the first label with index i>`.
4. If a later label with index `i` has a different text, the system SHALL print `Index <i> is used for ports "<first>" and "<other>"` once for each such label. No separate PIN is written for it; see Q-LEFDEF-50.
5. Indices with no label SHALL be skipped.
6. A port index whose labels produce no writable geometry SHALL produce no PIN record at all; see Q-LEFDEF-46.

A label with index `i` that is processed after another label of the same index SHALL produce no PORT if routing geometry already yanked for this pin covers that label's rectangle on the label's plane. Coverage is checked against the label's type, plus every contact type having it as a residue when the label is sticky. Under `-toplayer`, coverage is also checked against the selected net.

Visited marks SHALL be cleared after the macro is written.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1400-1440, lef/lefWrite.c:1738-1820)

#### Scenario: Port order by index
- **GIVEN** labels in list order `Y` (index 1), `A` (index 0) and `VDD` (index 2)
- **WHEN** the macro is written
- **THEN** PINs SHALL appear in the order `A`, `Y`, `VDD`

#### Scenario: Two labels of one port
- **GIVEN** two labels `VDD`, both with index 2, on two disconnected metal1 rails
- **WHEN** the macro is written
- **THEN** one `PIN VDD` SHALL contain two `PORT` blocks, one per rail

#### Scenario: Shorted labels
- **GIVEN** two labels `A`, both with index 0, on the same connected metal1 shape
- **WHEN** the macro is written
- **THEN** one `PIN A` SHALL contain exactly one `PORT` block

### Requirement: Pin header records

The PIN header SHALL be written immediately before the first geometry record of the pin. Its lines SHALL be, in order:

- `  PIN <label text>`, verbatim.
- `    DIRECTION <d> ;`, if the label's port class is not default:

  | Port class | `<d>` |
  |---|---|
  | input | `INPUT` |
  | output | `OUTPUT` |
  | tristate | `OUTPUT TRISTATE` |
  | bidirectional | `INOUT` |
  | feedthrough | `FEEDTHRU` |

- `    USE <u> ;`, if the label's port use is not default:

  | Port use | `<u>` |
  |---|---|
  | signal | `SIGNAL` |
  | analog | `ANALOG` |
  | power | `POWER` |
  | ground | `GROUND` |
  | clock | `CLOCK` |

  For the uses reset, scan and tie-off, the line is `    USE  ;`; see Q-LEFDEF-36.
- If the use is default, the power-name rule from "Power and ground pin detection" applies instead.
- `    SHAPE <s> ;`, if the port shape is not default:

  | Port shape | `<s>` |
  |---|---|
  | abutment | `ABUTMENT` |
  | ring | `RING` |
  | feedthrough | `FEEDTHRU` |

- The antenna lines from "Antenna areas", unless the pin is a power rail.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1028-1129, lef/lefWrite.c:1693-1709)

#### Scenario: Tristate output clock
- **GIVEN** a label `Z` with class tristate, use clock and shape ring
- **WHEN** it is written
- **THEN** the lines SHALL be `  PIN Z`, `    DIRECTION OUTPUT TRISTATE ;`, `    USE CLOCK ;` and `    SHAPE RING ;`

#### Scenario: Reset use
- **GIVEN** a label `RN` with use reset
- **WHEN** it is written
- **THEN** the line `    USE  ;` SHALL be written, with two spaces before the semicolon

### Requirement: Power and ground pin detection

A pin SHALL be a power rail if its use is power or ground. A pin with default use SHALL also be a power rail if its label text exactly equals the configured power-net name or the configured ground-net name. These are the `vdd` and `gnd` fields of the project setting "global nets and power names" (capability `hefesto-settings`; decision D-14), seeded from Magic's `$VDD` and `$GND`. An unset field matches nothing. For such a pin, the system SHALL write `    USE POWER ;` and/or `    USE GROUND ;`, in that order. Both lines are written if the text matches both names. Power rails SHALL get no antenna lines.

Compatibility: behavioral

(Magic ref: lef/lefWrite.c:1092-1112)

#### Scenario: Name-matched ground
- **GIVEN** the configured ground name is `VSS`, and a label `VSS` with default use
- **WHEN** it is written
- **THEN** `    USE GROUND ;` SHALL be written and no antenna lines SHALL be written

### Requirement: Power-name source in Magic

Hefesto SHALL treat this description of where Magic takes the power and ground names from as reference material. In Magic they are the global Tcl variables `VDD` and `GND`, and Magic's non-Tcl build does no such matching. Hefesto takes them from the `vdd` and `gnd` fields of the project setting "global nets and power names" of capability `hefesto-settings`, which a startup script seeds by setting `VDD` and `GND` (Q-LEFDEF-44, decided by D-14).

Compatibility: informative

(Magic ref: lef/lefWrite.c:1092-1112)

#### Scenario: Tcl variable
- **GIVEN** Magic with `set VDD vccd1`
- **WHEN** a pin `vccd1` with default use is written
- **THEN** `    USE POWER ;` is written

### Requirement: Antenna areas

For each processed port label that is not ignored, the system SHALL compute two antenna areas: a gate area `AG` and a diffusion area `AD`. A label is ignored if its use is power or ground, or if its text equals an entry of the nocheck list, or the list contains `*`. The areas are computed as follows:

1. Expand the label rectangle. A rectangle of zero width is grown by 1 unit on the left and on the right. A rectangle of zero height is grown by 1 unit at the bottom and at the top.
2. Select the electrically connected net that starts from the label's type within that rectangle, in the flat copy, using the tech connectivity rules.
3. `AG` is computed plane by plane, in increasing plane order. On each plane, sum the areas of the selected tiles whose type is in G, in internal units squared. A split (diagonal) tile counts half its rectangle area, using integer division. Stop at the first plane whose sum is greater than 0.
4. `AD` is computed in the same way, using D instead of G.

`    ANTENNAGATEAREA %f ;` SHALL be written if `AG > 0`, and `    ANTENNADIFFAREA %f ;` if `AD > 0`. The values are `oscale * oscale * float(A)`, computed in float32. These lines come from the label whose geometry triggers the PIN header.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1468-1537, lef/lefWrite.c:602-620, lef/lefWrite.c:1693-1709)

#### Scenario: Gate area
- **GIVEN** `oscale = 0.01`, and input pin `A` whose net touches gate-type tiles of total area 30 internal units squared on the first plane with gates
- **WHEN** it is written
- **THEN** `    ANTENNAGATEAREA 0.003000 ;` SHALL be written

#### Scenario: Ignored net
- **GIVEN** `lef nocheck A`
- **WHEN** the same cell is written
- **THEN** pin `A` SHALL have no antenna lines

### Requirement: Default pin geometry

When neither `-hide` nor `-pinonly` is active, each processed port label's pin geometry SHALL be the entire connected net selected as in "Antenna areas". The selection is made even for ignored labels. All of that net's material SHALL then be removed from the flat copy, on every plane written for the pin, so that it is not repeated as obstruction.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1506-1508, lef/lefWrite.c:1632-1648, lef/lefWrite.c:1712-1714)

#### Scenario: Whole net as pin
- **GIVEN** label `A` on a metal1 strip that connects through a via to a metal2 strip
- **WHEN** the user runs `lef write`
- **THEN** PIN `A`'s PORT SHALL contain the metal1 strip, the metal2 strip and the via cut
- **AND** none of these SHALL appear in OBS

### Requirement: Pin-only pin geometry

When `-pinonly p` is active (`p >= 0`) and `-hide` is not, the system SHALL:

1. Select the whole net.
2. Remove from the selection all material inside `B` shrunk by `p` on every side. With `p = 0`, that is all material inside `B`.
3. Paint the label's type over the label rectangle (zero-size dimensions grown as in "Antenna areas"), so that the label area always remains part of the pin.

The resulting selection is the pin geometry and is removed from the flat copy. The net material that was removed from the selection stays in the flat copy and becomes obstruction.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1631-1648)

#### Scenario: Label-only pin
- **GIVEN** `B` = (0,0)-(100,200), and label `A` (10,50)-(30,150) on a metal1 strip (10,20)-(30,180)
- **WHEN** the user runs `lef write -pinonly`
- **THEN** PIN `A` SHALL contain the metal1 RECT (10,50)-(30,150), scaled
- **AND** OBS SHALL contain the metal1 pieces (10,20)-(30,50) and (10,150)-(30,180)

#### Scenario: Pin extends to edge distance
- **GIVEN** the same cell, with metal1 (10,0)-(30,180) touching the bottom edge
- **WHEN** the user runs `lef write -pinonly 10`
- **THEN** the pin SHALL contain metal1 (10,0)-(30,10) plus the label area (10,50)-(30,150)
- **AND** OBS SHALL contain (10,10)-(30,50) and (10,150)-(30,180)

### Requirement: Hidden-detail pin geometry with zero setback

When `-hide` is active with setback 0, each processed port label's pin geometry SHALL be one rectangle `C` of the label's type. `C` is determined as follows:

- With `-pinonly 0`, `C` is the label rectangle (grown as in "Antenna areas").
- Otherwise, `C` is the largest rectangular chunk of homogeneous material of the label's type that contains the label rectangle in the flat copy.
- If no such chunk exists, `C` is the largest such chunk of the first type, in type-number order, that connects to the label's type. In that case the chunk is also selected on its own type, minus the area inside `B` shrunk by `p` when `p > 0`.
- If no chunk exists on any connected type, `C` is the label rectangle.

`C` SHALL be painted with the label's type into the pin selection, and remembered for the obstruction cut-out. The net is still selected for antenna computation unless the label is ignored. Only the pin geometry is removed from the flat copy. See Q-LEFDEF-51.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1539-1602)

#### Scenario: Chunk pin
- **GIVEN** label `A` (10,50)-(30,60) on metal1 that forms an L shape: (10,20)-(30,180) united with (30,20)-(80,40)
- **WHEN** the user runs `lef write -hide`
- **THEN** the pin SHALL be the single metal1 rectangle (10,20)-(30,180), the largest rectangle containing the label

### Requirement: Hidden-detail pin geometry with positive setback

When `-hide d` is active with `d > 0`, the system SHALL:

1. Select the whole net.
2. Remove from the selection all material inside `B` shrunk by `d`.
3. If `p > d`, also remove all material inside `B` shrunk by `p`. This has no further effect; see Q-LEFDEF-37.
4. Paint the label's type over the label rectangle.

The result is the pin geometry. No cut-out areas are recorded for this mode.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1603-1630)

#### Scenario: Edge pin with setback
- **GIVEN** `B` = (0,0)-(100,200), `d = 10`, and label `A` (40,0)-(60,10) on metal1 (40,0)-(60,100)
- **WHEN** the user runs `lef write -hide 10`
- **THEN** the pin SHALL be metal1 (40,0)-(60,10)

### Requirement: Degenerate setback

If `d > 0` and either `bxh - bxl < 2d` or `byh - byl < 2d`, the setback SHALL be treated as -1. No hiding is performed, and `-hide` has no effect for that macro. See Q-LEFDEF-47.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1355-1362)

#### Scenario: Setback too large
- **GIVEN** `B` = (0,0)-(100,200)
- **WHEN** the user runs `lef write -hide 60`
- **THEN** the output SHALL be identical to `lef write`

### Requirement: Top-layer pins

When `-toplayer` is active, for each processed label the system SHALL find `pTop`, the highest-numbered tech plane that contains any material of the pin selection. Pin geometry SHALL be written only for plane `pTop`, plus, if `domaster` and the masterslice plane set P is non-empty, for every plane in P. Material of the pin on all other planes SHALL NOT be written in the PORT and SHALL NOT be removed from the flat copy, so it becomes obstruction. Plane numbering follows the tech `planes` order, which is assumed to be bottom-to-top.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1650-1690)

#### Scenario: Metal2 pin over metal1
- **GIVEN** label `A` on metal1, connected by a via to metal2, with metal2 the highest plane used by the net
- **WHEN** the user runs `lef write -toplayer`
- **THEN** PIN `A`'s PORT SHALL contain only metal2 geometry
- **AND** the metal1 and via geometry SHALL appear in OBS

### Requirement: Geometry record emission

Geometry SHALL be written plane by plane, in increasing plane order. For a pin, the planes are those selected by "Top-layer pins". For OBS, all planes are written. On each plane, writing proceeds in two rounds:

1. **Routing round.**
   1. Each selected tile is copied into a scratch plane. A non-contact tile is copied if its type is in R and has a LEF name. A contact tile contributes every residue type that is in R, has a LEF name and lives on this plane.
   2. The scratch plane is merged by normal paint rules into Magic's canonical maximal-horizontal-strip tile decomposition.
   3. Every resulting tile whose type is in R SHALL be written, in the database's area-enumeration order: start at the top-left, then scan left to right and top to bottom as defined by the tile-plane search order.
2. **Cut round.** See "Contact cut records".

For each tile written:

- If the tile's type differs from the previously written type, write `      LAYER <name> ;` first. The previous type is reset at the start of each PORT and of the OBS block.
- A rectangular tile SHALL be written as `        RECT <xl> <yl> <xh> <yh> ;`.
- A diagonal (split) tile half SHALL be written as `        POLYGON` with three vertices, chosen by the right-angle corner of the triangle. Here L, R, B and T are the tile's left, right, bottom and top edges:

  | Triangle corner | Vertices |
  |---|---|
  | upper-right | (L,T) (R,T) (R,B) |
  | lower-right | (R,T) (R,B) (L,B) |
  | lower-left | (L,T) (R,B) (L,B) |
  | upper-left | (L,T) (R,T) (L,B) |

- If both halves of one split tile qualify, only the first half enumerated (the left half) SHALL be written; see Q-LEFDEF-39.
- Coordinates SHALL be written as `oscale * coordinate`, relative to the cell origin, formatted per "Distance number formatting".

Compatibility: exact

(Magic ref: lef/lefWrite.c:655-745, lef/lefWrite.c:816-958, lef/lefWrite.c:1672-1711)

#### Scenario: L-shape decomposition
- **GIVEN** pin metal1 forming the union of (0,0)-(30,10) and (0,10)-(10,40), with `oscale = 0.01`
- **WHEN** it is written
- **THEN** the PORT SHALL contain `        RECT 0.000 0.100 0.100 0.400 ;` followed by `        RECT 0.000 0.000 0.300 0.100 ;`
- **AND** exactly one `LAYER` line SHALL be written before them

#### Scenario: Triangle
- **GIVEN** a metal1 split tile (0,0)-(10,10) whose metal1 half has its right angle at the lower-right, with `oscale = 0.01`
- **WHEN** it is written
- **THEN** the record SHALL be `        POLYGON 0.100 0.100 0.100 0.000 0.000 0.000 ;`

### Requirement: Contact cut records

After the routing round on plane `p`, the system SHALL write the cut round:

1. Each selected non-split contact tile whose contact type has its home plane on `p` and has a LEF name is copied into the scratch plane. For a stacked contact type, the component contact whose home plane is `p` is copied instead.
2. Contact tiles of the scratch plane SHALL then be written as RECT (with LAYER changes) under the contact's LEF cut-layer name.

Routing tiles left over from the routing round SHALL NOT be written again. Each record covers the whole magic contact area, not individual cut squares; see Q-LEFDEF-52.

Compatibility: exact

(Magic ref: lef/lefWrite.c:747-814, lef/lefWrite.c:1716-1729, lef/lefWrite.c:853-860)

#### Scenario: Via in a pin
- **GIVEN** a pin containing m1/m2 contact type `via1`, area (0,0)-(20,20), whose cut LEF name is `via1`, on a metal1 strip (0,0)-(20,100), with `oscale = 0.01`
- **WHEN** it is written
- **THEN** on the via's home plane, after the metal RECTs, the PORT SHALL contain `      LAYER via1 ;` and `        RECT 0.000 0.000 0.200 0.200 ;`

### Requirement: PORT blocks

Each processed port label whose pin geometry yields at least one written record SHALL produce exactly one PORT block, starting at `    PORT` and ending at `    END`, containing all its records across all planes. The scratch geometry SHALL be cleared after each label. The pin closing line `  END <name>` SHALL be written only if a PIN header was written.

Compatibility: exact

(Magic ref: lef/lefWrite.c:860-868, lef/lefWrite.c:1773-1790)

#### Scenario: Pin without LEF geometry
- **GIVEN** port label `X` on a poly type that has no LEF layer name and connects to nothing with a LEF name
- **WHEN** the macro is written
- **THEN** no `PIN X` record SHALL appear

### Requirement: Obstructions without hiding

When `-hide` is not active, after all pins are written, all material remaining in the flat copy SHALL be written as obstructions, plane by plane. On each plane, the routing round runs first and then the cut round, as in "Geometry record emission" and "Contact cut records". The block SHALL begin with `  OBS` before its first record and end with `  END`. If no record is written, no OBS block SHALL appear.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1958-1989, lef/lefWrite.c:2032-2033)

#### Scenario: Internal wiring as OBS
- **GIVEN** a cell with an unlabeled metal2 strip (0,50)-(100,60), with `oscale = 0.01`
- **WHEN** the user runs `lef write`
- **THEN** OBS SHALL contain the metal2 LEF layer line and `        RECT 0.000 0.500 1.000 0.600 ;`

### Requirement: Obstructions with zero setback

When `-hide` is active with setback 0, the obstruction SHALL be generated as follows:

1. Start from an empty scratch cell.
2. For each type `t` in the hide set H or the well set W, in type-number order:
   1. Let `Lb` be the bounding box of all material, of any type, remaining in the flat copy on every plane that contains `t`.
   2. If `OBS_BBOX` is defined as four integers, and the original, unflattened cell has any paint on such a plane, enlarge `Lb` to include `OBS_BBOX`. A null `Lb` becomes `OBS_BBOX`.
   3. Clip `Lb` to `B`. If the result is non-empty, paint `t` over it in the scratch cell.
3. For each pin rectangle `C` recorded under "Hidden-detail pin geometry with zero setback":
   1. Compute the spacing `s` for the label's type. `s` is the spacing of the widest wide-spacing rule for that type to itself; with no wide-spacing rule, `s` is the type's default minimum spacing to itself (see the DRC capability).
   2. Erase the label's type over `C` grown by `s` on all four sides.
4. Copy into the scratch cell, as in the routing round, every remaining flat-copy tile that overlaps any part of the area outside `B`. This means tiles intersecting one of these four strips, where `bb` is the cell's bounding box:
   - `[bb.xl, bb.xh) x [bb.yl, byl)`
   - `[bb.xl, bb.xh) x [byh, bb.yh)`
   - `[bb.xl, bxl) x [byl, byh)`
   - `[bxh, bb.xh) x [byl, byh)`
5. Write the scratch cell's routing round as OBS, plane by plane.

No cut records SHALL be written in this mode.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1822-1957, lef/lefWrite.c:1968-1977, drc/DRCtech.c:4686-4722, drc/DRCtech.c:4871-4908)

#### Scenario: Abstract cover with cut-out
- **GIVEN** `B` = (0,0)-(100,200), with metal1 in H and metal1 spacing 3
- **AND** label `A` on an isolated metal1 rectangle (10,50)-(30,150), plus unlabeled metal1 rectangles (5,5)-(95,20) and (40,180)-(95,195)
- **AND** `oscale = 0.01`
- **WHEN** the user runs `lef write -hide`
- **THEN** the pin SHALL be (10,50)-(30,150)
- **AND** OBS metal1 SHALL be the region (5,5)-(95,195) minus (7,47)-(33,153), written as its maximal-horizontal-strip RECTs

#### Scenario: OBS_BBOX enlarges cover
- **GIVEN** the same cell with `OBS_BBOX` = `0 0 100 200`
- **WHEN** the user runs `lef write -hide`
- **THEN** the metal1 cover SHALL span (0,0)-(100,200) before the cut-out

#### Scenario: Malformed OBS_BBOX
- **GIVEN** an `OBS_BBOX` property that is not four integers
- **WHEN** the user runs `lef write -hide`
- **THEN** the error `Improper values for obstruction bounding box OBS_BBOX property` SHALL be printed and the property ignored

### Requirement: Obstructions with positive setback

When `-hide d` is active with `d > 0` after the degenerate check, the obstruction SHALL be generated as follows:

1. Build the cover as in "Obstructions with zero setback" step 2, but clip `Lb` to `B` shrunk by `d`. There are no cut-outs.
2. Copy into the scratch cell every remaining flat-copy tile in the routing set that overlaps one of the four setback strips. The whole tile is copied, unclipped; see Q-LEFDEF-38. The strips, where `bb` is the cell's bounding box, are:
   - `[bb.xl, bb.xh) x [bb.yl, byl+d)`
   - `[bb.xl, bb.xh) x [byh-d, bb.yh)`
   - `[bb.xl, bxl+d) x [byl+d, byh-d)`
   - `[bxh-d, bb.xh) x [byl+d, byh-d)`
3. For each plane, write the routing round of the scratch cell.
4. Then, for each of the four strips in the order listed, copy the flat copy's contact tiles that overlap the strip, and write the contact tiles of the scratch plane that overlap the strip, as cut records.

Compatibility: exact

(Magic ref: lef/lefWrite.c:1874-1957, lef/lefWrite.c:1990-2030)

#### Scenario: Periphery detail kept
- **GIVEN** `B` = (0,0)-(100,200), `d = 10`, and a metal1 rectangle (2,2)-(8,8) with no label
- **WHEN** the user runs `lef write -hide 10`
- **THEN** OBS SHALL contain the metal1 rectangle (2,2)-(8,8)
- **AND** the metal1 cover SHALL lie within (10,10)-(90,190)

### Requirement: Macro PROPERTY lines

After the OBS block, if the cell has a `LEFproperties` string, the system SHALL write one `  PROPERTY <key> <value> ;` line per key/value pair. Each pair is written verbatim, quotes included, exactly as it appears in the string. Pairs are parsed as in "Library header". Parsing SHALL stop, without writing, at a key that has no value or an unterminated quote. Then `END <cellname>` SHALL be written. See Q-LEFDEF-42.

Compatibility: exact

(Magic ref: lef/lefWrite.c:2040-2084)

#### Scenario: Two properties
- **GIVEN** `LEFproperties` = `CDL "inv x" MASK 1`
- **WHEN** the macro is written
- **THEN** the lines `  PROPERTY CDL "inv x" ;` and `  PROPERTY MASK 1 ;` SHALL be written before `END inv`

#### Scenario: Dangling key
- **GIVEN** `LEFproperties` = `A 1 B`
- **WHEN** the macro is written
- **THEN** only `  PROPERTY A 1 ;` SHALL be written

### Requirement: Names written verbatim

Macro names, pin names, FOREIGN names, LEF layer names and property strings SHALL be written exactly as stored, with no escaping or character substitution. Characters such as `#`, `;`, spaces, `-`, `*` and `$` are not replaced. See Q-LEFDEF-35.

Compatibility: exact

(Magic ref: lef/lefWrite.c:976-1018, lef/lefWrite.c:1785-1788, lef/lefWrite.c:1031)

#### Scenario: Hash in pin name
- **GIVEN** a port label `a#1`
- **WHEN** the macro is written
- **THEN** the lines `  PIN a#1` and `  END a#1` SHALL be written

### Requirement: Write side effects

LEF writing SHALL NOT modify the cell being written. It SHALL NOT be recorded as an undoable action, and it SHALL leave no new user-visible cells. Magic's internal scratch cells are flagged internal. Any current selection SHALL be cleared afterwards. If the user interrupts during `lef writeall`, the remaining macros SHALL be skipped, but `END LIBRARY` SHALL still be written.

Compatibility: behavioral

(Magic ref: lef/lefWrite.c:1174-1185, lef/lefWrite.c:2086-2096, lef/lefWrite.c:2286-2294)

#### Scenario: Selection cleared
- **GIVEN** a selected rectangle in the layout
- **WHEN** the user runs `lef write`
- **THEN** the selection SHALL be empty afterwards and the cell SHALL be unmodified

### Requirement: Progress messages

The system SHALL report progress while writing; Magic prints these messages:

- `Generating LEF output <file> for cell <name>:`
- `Generating LEF output <file> for hierarchy rooted at cell <name>:`
- `Diagnostic:  Write LEF header for cell <name>`
- `Diagnostic:  Writing LEF output for cell <name>`
- `Diagnostic:  Scale value is <oscale %f>`

Compatibility: informative

(Magic ref: lef/lefWrite.c:265, lef/lefWrite.c:1175, lef/lefWrite.c:1225, lef/lefWrite.c:2257, lef/lefWrite.c:2369)

#### Scenario: Messages
- **WHEN** the user runs `lef write` on `inv`
- **THEN** Magic prints `Generating LEF output inv.lef for cell inv:` before writing
