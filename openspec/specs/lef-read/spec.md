# LEF Read

## Purpose

Defines how a LEF file is read into the layout database by `lef read`: the shared LEF/DEF tokenizer and error reporting, unit conversion, LEF layer name resolution, the technology sections that add to the LEF layer table (LAYER, VIA, VIARULE, NONDEFAULTRULE), via-definition geometry (including generated vias and cut growing, which `def read` also uses), and how MACRO and SITE sections become cells with properties, port labels and obstruction paint, both for new cells and for annotating existing ones. The LEF layer table is defined in capability `lef-tech-section`. This capability also covers the `lef read` and `lef datestamp` command options; command dispatch, target-cell selection and the other `lef` sub-options are in capability `lef-write`. DEF-specific reading is in capability `def-read`.

## Requirements

### Requirement: LEF/DEF tokenizer

The LEF and DEF readers SHALL split input into tokens as follows:
1. Input is read one physical line at a time, with at most 2048 characters per read; a longer line is processed as consecutive pieces of 2048 characters, and a token can be split at a piece boundary.
2. Lines that are empty, contain only whitespace, or whose first non-whitespace character is `#` are skipped.
3. Tokens are maximal runs of non-whitespace characters. When a `#` begins a token position (it follows whitespace or starts the line), the rest of the line is a comment. A `#` inside a token is part of the token.
4. A token that begins with `"` extends to the next `"` not immediately preceded by `\`, including whitespace and newlines; the returned token includes both quote characters.
5. The statement terminator `;` is recognized only as a separate token. A `;` attached to another token (for example `1.0;`) is part of that token.
6. In end-of-line-sensitive mode (used only for END detection, see "END statement matching"), reading a new line yields a distinguished end-of-line token before the first token of that line.

Numbers SHALL be parsed as IEEE-754 binary32 values from the longest valid leading prefix of the token (C `%f` semantics; `1.5abc` → 1.5); a token with no numeric prefix is a parse failure. Keywords SHALL be matched case-insensitively and in full (no prefix matching). Statements are skipped by consuming tokens up to and including the next `;` token.
Compatibility: exact

(Magic ref: lef/lefRead.c:160-232, 1826-1835, utils/lookupfull.c:52-75)

#### Scenario: Attached semicolon swallows the next statement
- **GIVEN** a macro containing `SIZE 1.0 BY 2.0;` on one line and `SYMMETRY X Y ;` on the next
- **WHEN** the LEF file is read
- **THEN** SIZE is parsed as 1.0 by 2.0, and the statement skip after SIZE consumes `SYMMETRY X Y ;`, so no `LEFsymmetry` property is set

#### Scenario: Comment after tokens
- **WHEN** the line `RECT 0 0 1 1 ; # pin area` is tokenized
- **THEN** the tokens are `RECT`, `0`, `0`, `1`, `1`, `;`

#### Scenario: Quoted token
- **WHEN** the line `PROPERTY note "a b" ;` is tokenized
- **THEN** the tokens are `PROPERTY`, `note`, `"a b"` (with the quotes), `;`

### Requirement: END statement matching

After an `END` keyword, the reader SHALL check what follows:
- When a section name N is expected, the next token SHALL match N case-insensitively for the section to be closed. If it is instead another `END` (or `ENDEXT`), that counts as "END END", which callers that require a match treat as closed. Any other token means the END does not match.
- When no name is expected (PORT, OBS and a skipped PORT or OBS), the END SHALL be closed only if it is the last token on its line.
Skipping a named section SHALL consume tokens until a matching `END N`. For the extension section `BEGINEXT`, the first `ENDEXT` token ends it. If the input ends first, the error `Section <N> has no END record!` is reported.
Compatibility: exact

(Magic ref: lef/lefRead.c:401-505)

#### Scenario: Mismatched macro END
- **GIVEN** `MACRO INV` ending with `END INVX`
- **WHEN** the file is read
- **THEN** the error `Macro END statement missing.` is reported and reading continues to treat later statements as part of macro `INV`

### Requirement: LEF/DEF message reporting

The readers SHALL report errors, warnings and informational messages tagged with the input line number. The number is the count of physical line reads from the start of the file (skipped comment and blank lines count; continuation reads inside a multi-line quoted token do not). Errors, warnings and messages SHALL be counted separately. Each kind SHALL print at most 100 messages, followed by one "further ... will not be reported" notice. At the end of a read, nonzero error and warning totals SHALL be summarized and all counters reset. Hefesto MAY change the message texts and destinations.

Magic's formats are `LEF read, Line <n> (Error): <msg>`, `(Warning)` and `(Message)`, or `LEF read (Error): ` when no line is current. The notices are `LEF Read:  Further errors will not be reported.` and `LEF read:  Further warnings will not be reported.`, and the summaries are `LEF Read: encountered <k> error[s] total.` and `LEF Read: encountered <k> warning[s] total.`. DEF reads use the prefix `DEF` instead of `LEF`. Errors and warnings go to the error stream and messages go to the output stream.
Compatibility: informative

(Magic ref: lef/lefRead.c:249-375, lef/lefInt.h:17-19, 183-192)

#### Scenario: Error cap
- **GIVEN** a LEF file that produces 150 errors
- **WHEN** it is read
- **THEN** 100 error messages are printed, then one "further errors" notice, and the summary reports 150 errors

### Requirement: LEF unit conversion

LEF distances SHALL be interpreted as microns regardless of any `UNITS` section. Let s be the number of microns per magic internal unit, computed in binary32 as (10 · F) / (E · 1000), where F is the current output style's scale factor (centimicrons per internal unit, before the expander) and E is its expander. If there is no current output style, s = 1.0. A LEF coordinate or length v SHALL convert to the internal integer round(v / s), where the division is binary32 and round is half away from zero. Via geometry SHALL use half internal units: round(v / (s/2)).
Compatibility: exact

(Magic ref: lef/lefRead.c:3015, 2797, 868-936, 2656, cif/CIFmain.c:139-146)

#### Scenario: Nanometer grid
- **GIVEN** an output style where one internal unit is 5 nm (s = 0.005)
- **WHEN** a LEF RECT coordinate 0.0125 is converted
- **THEN** the internal coordinate is round(2.5) = 3

#### Scenario: No output style
- **GIVEN** no output style is defined
- **WHEN** `SIZE 2.4 BY 5` is read
- **THEN** the size is 2 by 5 internal units

### Requirement: LEF layer name resolution

When a geometry statement names a layer (`LAYER <name>` in PORT, OBS or a VIA definition, and the layer names used by `def read`), the name SHALL be resolved as follows:
1. If the name is in the LEF layer table with an entry E:
   - For pin, route and via geometry ("normal" lookup), the type SHALL be E's normal type, or none when E is of class IGNORE.
   - For obstruction geometry ("obstruction" lookup), the type SHALL be E's obstruction type if it has one. Otherwise it SHALL be E's normal type, unless E is of class IGNORE, in which case it is none. If E is of class VIA and has an obstruction type, the secondary obstruction type SHALL be set to E's secondary via-obstruction type, which may be none.
   - The contact area SHALL be E's contact area if E is of class VIA, and null otherwise.
2. Otherwise the name SHALL be matched against magic type names and aliases, exactly as written and then in all-lowercase. An alias SHALL count only if it denotes a single type.
3. If no type results and the name is not an IGNORE entry, two errors SHALL be reported: `Don't know how to parse layer "<name>"` and `Try adding this name to the LEF techfile section`.
4. A `LAYER` statement whose name token is `;` reports `Bad Layer statement` and yields no type.

Within one PORT or OBS block, the secondary obstruction type SHALL be set only as described in step 1. Otherwise it keeps its previous value from an earlier LAYER statement in the same block, so a later layer can inherit it. The contact area SHALL also be left unchanged when step 2 resolves the name. See Q-LEFDEF-3. A table name that has no entry (see `lef-tech-section`, lifecycle) is treated as absent. See Q-LEFDEF-4.
Compatibility: exact

(Magic ref: lef/lefRead.c:697-765, 579-591, database/DBtechname.c:64-104)

#### Scenario: Fallback to magic type name
- **GIVEN** no `lef` section entry for `METAL1` and a magic type named `metal1`
- **WHEN** `LAYER METAL1 ;` is read in a PORT
- **THEN** the layer resolves to type `metal1` through the lowercase match

#### Scenario: Obstruction lookup with second type
- **GIVEN** the tech line `obstruction obsv1,m1 via1` (primary `m1`, secondary `obsv1`), with `via1` also defined by `cut m2c via1`
- **WHEN** an OBS has `LAYER via1 ;`
- **THEN** OBS RECTs on that layer paint `m1` and also `obsv1`

#### Scenario: Unknown layer
- **WHEN** `LAYER poly2 ;` is read and no table entry or magic type matches
- **THEN** both "Don't know how to parse layer" errors are reported and RECTs that follow it in that block are ignored

### Requirement: Supported LEF file structure

`lef read` SHALL accept the following top-level grammar and handle each statement as annotated:

```ebnf
lef_file      = { top_stmt } , [ "END" , "LIBRARY" ] ;      (* reading stops at END LIBRARY *)
top_stmt      = ignored_stmt | skipped_sect | layer_sect | via_sect | viarule_sect
              | ndr_sect | site_sect | macro_sect | unknown_stmt ;
ignored_stmt  = ( "VERSION" | "BUSBITCHARS" | "DIVIDERCHAR" | "MANUFACTURINGGRID"
              | "USEMINSPACING" | "CLEARANCEMEASURE" | "NAMESCASESENSITIVE"
              | "NOWIREEXTENSIONATPIN" ) , { token } , ";" ;
skipped_sect  = ( "UNITS" | "PROPERTYDEFINITIONS" | "SPACING" | "NOISETABLE"
              | "CORRECTIONTABLE" | "IRDROP" | "ARRAY" | "TIMING" ) as K , { token } , "END" , K
              | "BEGINEXT" , { token } , "ENDEXT" ;
layer_sect    = "LAYER" , name , { layer_stmt } , "END" , name ;
via_sect      = "VIA" , name , { layer_stmt } , "END" , name ;
viarule_sect  = "VIARULE" , name , { layer_stmt } , "END" , name ;
ndr_sect      = "NONDEFAULTRULE" , name , { ndr_stmt } , "END" , name ;
site_sect     = "SITE" , name , { macro_stmt } , "END" , name ;
macro_sect    = "MACRO" , name , { macro_stmt } , "END" , name ;
unknown_stmt  = token , { token } , ";" ;   (* message "Unknown keyword "<t>" in LEF file; ignoring." *)
```

Names SHALL be truncated to their first 127 characters. All lengths are in microns ("LEF unit conversion"). `NAMESCASESENSITIVE` SHALL have no effect: table lookups are always case-sensitive with the lowercase fallback described in "LEF layer name resolution". Before reading, if the LEF layer table was never initialized, it SHALL be initialized empty.
Compatibility: exact

(Magic ref: lef/lefRead.c:2956-3196)

#### Scenario: Content after END LIBRARY
- **GIVEN** a file with `END LIBRARY` followed by another `MACRO B ... END B`
- **WHEN** it is read
- **THEN** macro `B` is not created

#### Scenario: UNITS ignored
- **GIVEN** `UNITS DATABASE MICRONS 2000 ; END UNITS` and `SIZE 1 BY 1` with s = 0.01
- **WHEN** the file is read
- **THEN** the size is 100 by 100 internal units

### Requirement: LEF LAYER section

A `LAYER <name>` section SHALL be handled as follows:
- If `<name>` is in the LEF layer table and its entry has no normal type (obstruction-only or IGNORE entries), report `Layer <name> is only defined for obstructions!` and skip the section.
- If `<name>` is not in the table, resolve it against magic type names, exactly and then in lowercase. If it does not resolve, skip the section silently. If it resolves to a contact type, report `Layer <name> maps to a magic contact layer; must be defined in lef section of techfile` and skip the section. Otherwise add a new ROUTE-class entry named `<name>` (canonical name `<name>`) with that normal type. Its width, spacing, pitch and direction are undefined until set by statements in the section. See Q-LEFDEF-5.
- Then process the body statements:

| statement | effect |
|---|---|
| `TYPE t ;` | t ∈ {ROUTING, CUT, MASTERSLICE, OVERLAP}. Unknown t: warning `Unknown layer type "<t>" in LEF file; ignoring.`. If t is ROUTING or CUT and differs from the entry's class (ROUTE↔ROUTING, VIA↔CUT), error `Attempt to reclassify layer <name> from <old> to <new>`. The class never changes. |
| `WIDTH v ;` | ROUTE class only: width = round(v/s) |
| `SPACING v ... ;` | ROUTE class only: spacing = round(v/s) using the first value. Every SPACING statement overwrites, so the last one in the section wins. See Q-LEFDEF-6. |
| `PITCH v ... ;` | ROUTE class only: pitch = round(v/s) using the first value |
| `DIRECTION d ;` | ROUTE class only: horizontal if d starts with `h` or `H`, otherwise vertical |
| `ACCURRENTDENSITY` | read 2 tokens. If the second is `FREQUENCY`: skip the statement, read one token, and if it is `WIDTH` skip another statement. Then skip a statement. |
| `DCCURRENTDENSITY` | read 2 tokens. If the second is `WIDTH`, skip a statement. Then skip a statement. |
| `MAXWIDTH AREA SPACINGTABLE OFFSET WIREEXTENSION RESISTANCE CAPACITANCE EDGECAPACITANCE THICKNESS HEIGHT PROPERTY MINIMUMDENSITY ANTENNAAREARATIO ANTENNADIFFAREARATIO ANTENNASIDEAREARATIO FOREIGN ENCLOSURE PREFERENCLOSURE OVERHANG METALOVERHANG VIA` | skipped to `;` |
| `DEFAULT`, `GENERATE` | no-op (no `;` consumed) |
| `LAYER n ;` / `RECT ... ;` | via geometry (see "LEF VIA and VIARULE definitions") |
| other | message `Unknown keyword ...`, skipped to `;` |
| `END name` | ends the section; a mismatch reports `Layer END statement missing.` and continues |

Compatibility: exact

(Magic ref: lef/lefRead.c:2722-2944, 3093-3148)

#### Scenario: LEF width overrides tech default
- **GIVEN** the tech line `routing m1 metal1`, s = 0.01, and `LAYER metal1 TYPE ROUTING ; WIDTH 0.14 ; PITCH 0.34 ; DIRECTION HORIZONTAL ; END metal1`
- **WHEN** the file is read
- **THEN** the `metal1` entry has width 14, pitch 34 and horizontal direction

#### Scenario: Last SPACING wins
- **GIVEN** a ROUTE layer with `SPACING 0.14 ;` followed by `SPACING 0.28 RANGE 3 100 ;` and s = 0.01
- **WHEN** the section is read
- **THEN** the spacing is 28

#### Scenario: Reclassification error
- **GIVEN** the tech line `routing m1 metal1`
- **WHEN** `LAYER metal1 TYPE CUT ; END metal1` is read
- **THEN** the error `Attempt to reclassify layer metal1 from ROUTING to CUT` is reported and the class stays ROUTE

### Requirement: LEF VIA and VIARULE definitions

A `VIA <name>` or `VIARULE <name>` section SHALL define a via entry in the LEF layer table:
- If `<name>` is not in the table: create a VIA-class entry with no normal or obstruction type and contact area (0,0,0,0), read the body with the LAYER-section statement table, and set the canonical name to `<name>`.
- If `<name>` exists and the section is VIARULE: skip the whole section unchanged.
- If `<name>` exists and the section is VIA: report the warning `Cut type "<name>" redefined.`, prepare the entry for redefinition (see "Via redefinition"), and read the body into it.

Via geometry statements: `LAYER n ;` sets the current via layer by normal lookup ("LEF layer name resolution"). `RECT x1 y1 x2 y2 ;` reads a rectangle in half internal units (each coordinate round(v/(s/2))), made canonical. If the current layer is none, no geometry is added; in that case the error `No layer defined for RECT.` is still reported, even for IGNORE layers (see Q-LEFDEF-20). Otherwise apply cut growing ("Cut growing") and then:
1. if the entry's contact area equals (0,0,0,0) exactly, the rectangle becomes the contact area and the current layer becomes the entry's normal type;
2. otherwise prepend (current layer, rectangle) to the entry's list of extra rectangles; then, if the current layer is a contact type and the entry's normal type is not, exchange them, so that the list head receives the old (normal type, contact area) and the entry receives (current layer, rectangle).

POLYGON, VIARULE-parameter statements (`VIARULE`, `CUTSIZE`, `LAYERS`, `CUTSPACING`, `ENCLOSURE`, `ROWCOL`, `ORIGIN`, `OFFSET`, `PATTERN`) and `RECT MASK n ...` are not supported. The first group is skipped as unknown, and `RECT MASK` fails with `Bad port geometry: RECT requires 4 values.`. A LEF via defined by a via rule therefore has no geometry. See Q-LEFDEF-7.
Compatibility: exact

```
VIA M1M2_PR DEFAULT          # DEFAULT is a no-op token
  LAYER met1 ;
  RECT -0.16 -0.13 0.16 0.13 ;   # s=0.005: half units (-64,-52,64,52); becomes primary (met1)
  LAYER via ;
  RECT -0.075 -0.075 0.075 0.075 ; # contact type: swapped in as primary; met1 rect moves to list
  LAYER met2 ;
  RECT -0.15 -0.15 0.15 0.15 ;
END M1M2_PR
```

(Magic ref: lef/lefRead.c:3055-3090, 2646-2690, 2895-2906, 868-936)

#### Scenario: Contact type becomes primary
- **GIVEN** s = 0.005, the tech line `cut via via`, no output style that generates `via` cuts, and the VIA example above
- **WHEN** it is read
- **THEN** the entry `M1M2_PR` has normal type `via` with contact area (-30,-30,30,30) half units
- **AND** the extra list is, from the head, (met2, (-60,-60,60,60)) then (met1, (-64,-52,64,52))

#### Scenario: VIARULE after VIA of the same name
- **GIVEN** `VIA V12 ... END V12` followed by `VIARULE V12 ... END V12`
- **WHEN** the file is read
- **THEN** the VIARULE section is skipped and `V12` keeps the VIA geometry

### Requirement: Via redefinition

When an existing via name is redefined, the following SHALL apply. If no other table name shares the entry, the entry SHALL be reused: its extra rectangles are discarded, its normal and obstruction types become none, and its contact area becomes (0,0,0,0). Its class is kept. If other names share the entry, a new entry SHALL be created for the redefined name only, with no types and contact area (0,0,0,0). If the shared entry's canonical name was the redefined name, its canonical name SHALL be changed to another name in the table. Magic picks the first name in hash order that differs from the redefined name, even if that name belongs to an unrelated entry, and it leaves the new entry's class uninitialized. See Q-LEFDEF-8.
Compatibility: exact

(Magic ref: lef/lefRead.c:611-680)

#### Scenario: Reading the same technology LEF twice
- **GIVEN** a technology LEF defining `VIA V12` was already read
- **WHEN** it is read again
- **THEN** the warning `Cut type "V12" redefined.` is reported and `V12` ends with the same geometry as after the first read

### Requirement: Cut growing

When via geometry on a contact type t is added from LEF or DEF (via-definition RECTs and generated via cuts), and an output style is current, the cut rectangle (in half units) SHALL be resized as follows. Use the first output-style layer, in style order, that generates t through squares or slots. That is, a layer whose leading operations are OR operations with no mask-layer operands, one of which includes t, and which is followed by OR or AND-NOT operations and then a squares, squares-grid or slots operation. Let size and border be that operation's cut size and border (slots: short size and short border), in output-style units, and let F be the style's scale factor in output-style units per internal unit. Then
- C = ceil(2·(size + 2·border) / F) and E = ceil(2·size / F), both in half units.
- If C > 0 and E > 0: when the rectangle's width or height is not E, report the warnings `Cut size for magic type "<t>" (<E> x <E>) does not match LEF/DEF` and `Via cut size (<w> x <h>).  Magic layer cut size will be used!`. Then replace the rectangle by xbot = trunc((xbot+xtop)/2) − ⌊C/2⌋, ybot = trunc((ybot+ytop)/2) − ⌊C/2⌋, xtop = xbot + C, ytop = ybot + C, where trunc is integer division toward zero.
- Otherwise, or when t is not a contact type or no output style is current, the rectangle SHALL be unchanged. The input style plays no part in this. See Q-LEFDEF-17.
Compatibility: exact

(Magic ref: lef/lefRead.c:2473-2523, cif/CIFtech.c:2327-2382)

#### Scenario: Cut grown to magic contact size
- **GIVEN** an output style with F = 1 unit per internal unit, contact type `via` generated by squares with size 2 and border 1, and a via cut rectangle (-2,-2,2,2) half units
- **WHEN** the cut is added
- **THEN** C = 8 and E = 4; the size matches E, so no warning is reported
- **AND** the rectangle becomes (-4,-4,4,4) half units, a 4 x 4 internal-unit contact

### Requirement: Generated via geometry

The via-rule parameters used by `def read` SHALL be converted to a via entry with the following algorithm. The parameters are cut size (cx, cy), cut spacing (sx, sy), bottom enclosure (bx, by), top enclosure (tx, ty), R rows, C columns, and the top, cut and bottom layer types. They are integers in input units, with scale u input units per internal unit (binary32) and h = u/2. The algorithm SHALL be:
1. W = cx·C + sx·(C−1) + 2·tx and H = cy·R + sy·(R−1) + 2·ty. The contact area SHALL become (−round(W/u), −round(H/u), round(W/u), round(H/u)) half units, and the entry's normal type SHALL become the top layer type.
2. With bx, by in place of tx, ty, compute the bottom rectangle the same way and prepend it to the extra list with the bottom layer type.
3. Let Wc = cx·C + sx·(C−1), Hc = cy·R + sy·(R−1), x0 = trunc(−Wc/2), y0 = trunc(−Hc/2) (integer division toward zero). For column i = 0..C−1 (outer loop) and row j = 0..R−1 (inner loop), with x = x0 + i·(cx+sx) and y = y0 + j·(cy+sy): build the cut rectangle (round(x/h), round(y/h), round(x/h) + round(cx/h), round(y/h) + round(cy/h)), apply cut growing ("Cut growing"), and prepend it to the extra list with the cut layer type.
The resulting extra list, from the head, SHALL be the last cut generated, ..., the first cut, then the bottom rectangle. Origin, offset and pattern parameters are applied by `def read` and are not part of this algorithm.
Compatibility: exact

(Magic ref: lef/lefRead.c:2545-2626)

#### Scenario: 1 x 2 cut array
- **GIVEN** u = 1, cut size 4 x 4, spacing 2 x 2, bottom enclosure 1 x 1, top enclosure 2 x 2, R = 1, C = 2, and no cut growing
- **WHEN** the geometry is generated
- **THEN** the contact area (top layer) is (−14,−8,14,8) half units and the bottom rectangle is (−12,−6,12,6)
- **AND** the cuts are (−10,−4,−2,4) then (2,−4,10,4) half units, and the list head is the cut at x = 2

### Requirement: LEF NONDEFAULTRULE section

A `NONDEFAULTRULE <name>` section SHALL create a named non-default rule set in the non-default rule table (shared with `def-read` and `def-write`). If `<name>` already exists, report the warning `Nondefault rule "<name>" was already defined.`, use the name `<name>_0`, and report `Renaming this rule "<name>_0"`. Magic never tries an index above 0. See Q-LEFDEF-9. The body SHALL be processed as follows. The rule list is built by prepending, so the last rule read comes first.

| statement | effect |
|---|---|
| `LAYER n` | look up n in the LEF layer table; prepend a rule (layer entry or none, width 0, spacing 0, extension 0); enter layer mode |
| `WIDTH v ;` | current rule width = round(v/s) |
| `SPACING v ;` | current rule spacing = round(v/s) |
| `WIREEXTENSION v ;` | current rule extension = round(2·v/s) (half units) |
| `DIAGWIDTH v ;` | ignored |
| `END n` | in layer mode, leave layer mode; otherwise end the section |
| `VIA n ... END n` | if n is not in the table, read it as a LEF VIA definition into a new entry. Prepend a rule referring to it (not layer mode). |
| `USEVIA n`, `USEVIARULE n` | prepend a rule referring to n's entry (or none) |
| `HARDSPACING` | no-op |
| `MINCUTS a b`, `PROPERTY a b` | the two tokens are ignored |
| other | message `Unknown keyword "<t>" in NONDEFAULTRULES definition; ignoring.`, skipped to `;` |

When the current rule or layer is none, WIDTH, SPACING and WIREEXTENSION SHALL report a message and have no effect. Because `HARDSPACING`, `USEVIA`, `USEVIARULE`, `MINCUTS` and `PROPERTY` do not consume their `;`, the `;` is treated as an unknown keyword and the following statement is skipped as well. A `VIA` whose name already exists has its body parsed as rule statements. See Q-LEFDEF-10.
Compatibility: exact

(Magic ref: lef/lefRead.c:1861-2056)

#### Scenario: Double-width rule
- **GIVEN** s = 0.005 and `NONDEFAULTRULE DW LAYER met1 WIDTH 0.28 ; SPACING 0.28 ; END met1 END DW`
- **WHEN** the file is read
- **THEN** rule set `DW` has one rule for `met1` with width 56, spacing 56 and extension 0

#### Scenario: HARDSPACING swallows the next statement
- **GIVEN** `NONDEFAULTRULE R HARDSPACING ; LAYER met1 WIDTH 0.2 ; END met1 END R`
- **WHEN** it is read
- **THEN** the `;` after HARDSPACING is skipped as an unknown keyword together with the tokens up to the next `;` (`LAYER met1 WIDTH 0.2 ;`), so no rule for `met1` is created

### Requirement: MACRO and SITE cell creation

Each `MACRO <name>` and each `SITE <name>` section SHALL be read with the same macro-reading procedure. For each section:
1. If `<name>` was already read as a MACRO or SITE in this same file, find the smallest k ≥ 1 such that `<name>_k` was not read in this file, and use `<name>_k` as the cell name. Report the warning `Cell "<name>" was already defined in this file.  Renaming this cell "<name>_k"`. If no cell `<name>_k` exists and annotation was requested (`-annotate`), skip the section.
2. If a cell with the (possibly renamed) name exists in the database, the macro SHALL be read in annotation mode ("Annotation of existing cells"). Otherwise a new empty, available cell SHALL be created and read in creation mode. Annotation is chosen only because the cell already exists. The `-annotate` option does not by itself stop new cells from being created for macros that are not duplicates. See Q-LEFDEF-11.
3. If a fixed LEF datestamp is set (see "lef datestamp"), the cell's timestamp SHALL be set to it and marked as fixed.
Reading SHALL disable undo for the whole file, so the read cannot be undone. See Q-LEFDEF-22.
Compatibility: exact

(Magic ref: lef/lefRead.c:2112-2175, 3176-3185, 3018-3024)

#### Scenario: Duplicate macro in one file
- **GIVEN** a file defining `MACRO INV` twice and no cell `INV_1`
- **WHEN** it is read
- **THEN** the second definition creates cell `INV_1` and the rename warning is reported

#### Scenario: Re-reading the same LEF file
- **GIVEN** cell `INV` was created by an earlier `lef read`
- **WHEN** the same file is read again
- **THEN** `INV` is read in annotation mode: OBS geometry is skipped and `LEFview` is not set again

### Requirement: Macro coordinate offset

During a macro, the reader SHALL keep an offset (ox, oy), initially (0, 0), that is added to every converted PIN and OBS coordinate. It is applied after rounding, to each RECT corner and each POLYGON vertex. The offset in effect when a geometry statement is read SHALL be used, so ORIGIN and FOREIGN statements affect only the geometry that follows them.
- `ORIGIN x y ;` (parentheses around the point are optional): set the bounding-box lower-left to (−round(x/s), −round(y/s)); if SIZE was already read, add this lower-left to the stored upper-right; add (−round(x/s), −round(y/s)) to the offset. A malformed point reports `Bad macro ORIGIN; requires 2 values.`.
- `FOREIGN name [x y [orient]] ;`: if a point is present, add (−round(x/s), −round(y/s)) to the offset. A malformed point reports `Bad origin in macro FOREIGN; requires 0 or 2 values.`. The name and orientation are not used.
- `SIZE w BY h ;`: set the upper-right to (round(w/s) + lower-left x, round(h/s) + lower-left y) and record that a size is present. The `BY` token is not checked. A malformed size reports `Bad macro SIZE; requires values X BY Y.`.
At the end of the macro, the lower-left and upper-right SHALL each have the offset subtracted, giving the macro box. See Q-LEFDEF-12.
Compatibility: exact

(Magic ref: lef/lefRead.c:2178-2238, 2329-2350, 2364-2370)

#### Scenario: ORIGIN shifts geometry but not the box (verify against Magic)
- **GIVEN** s = 0.01 and `MACRO A ORIGIN 1 1 ; SIZE 10 BY 10 ; OBS LAYER met1 ; RECT 0 0 2 2 ; END END A`
- **WHEN** the file is read
- **THEN** the obstruction is painted at (−100,−100)–(100,100)
- **AND** the macro box (FIXED_BBOX) is (0, 0, 1000, 1000)

#### Scenario: FOREIGN offset
- **GIVEN** s = 0.01 and `MACRO B FOREIGN B 0.5 0.5 ; SIZE 2 BY 2 ; ... END B` with no ORIGIN
- **WHEN** the file is read
- **THEN** geometry is shifted by (−50, −50) and the macro box is (50, 50, 250, 250)

### Requirement: Macro statements and cell properties

The macro body SHALL be processed with this grammar and these effects. Property values are strings unless noted.

```ebnf
macro_stmt = "CLASS" , tok , { tok } , ";"        (* LEFclass  = tokens joined by single spaces *)
           | "SYMMETRY" , tok , { tok } , ";"     (* LEFsymmetry = tokens joined by single spaces *)
           | "SITE" , tok , { tok } , ";"         (* LEFsite = first token *)
           | "PROPERTY" , key , value , { tok } , ";"  (* append "key value" to LEFproperties *)
           | "SIZE" , num , tok , num , ";"
           | "ORIGIN" , point , { tok } , ";"
           | "FOREIGN" , name , [ point , { tok } ] , ";"
           | "SOURCE" , tok , { tok } , ";"        (* ignored *)
           | "TIMING" , { tok } , "END" , "TIMING" (* skipped *)
           | pin_sect | obs_sect
           | other , { tok } , ";"               (* message "Unknown keyword ..." *) ;
point      = [ "(" ] , num , num , [ ")" ] ;
```

- `LEFproperties`: if the property already exists with value P, the new value is `P key value`, otherwise `key value`. Tokens are copied verbatim, including quotes. Only the first value token is kept.
- `CLASS`, `SYMMETRY`: each statement replaces the property.
- `SITE`: each statement replaces `LEFsite`.
- An empty `CLASS ;` or `SYMMETRY ;` has undefined content in Magic. See Q-LEFDEF-19.
- `PIN` sections are numbered with a pin ordinal that starts at 1 for each macro and increases by one for every PIN section, including pins that produce no labels.
Compatibility: exact

(Magic ref: lef/lefRead.c:2192-2347)

#### Scenario: Class and properties
- **WHEN** a macro has `CLASS CORE WELLTAP ;`, `SYMMETRY X Y R90 ;`, `SITE unithd ;`, `PROPERTY CatenaDesignType deviceLevel ;` and `PROPERTY k2 "v 2" ;`
- **THEN** the cell properties are `LEFclass` = `CORE WELLTAP`, `LEFsymmetry` = `X Y R90`, `LEFsite` = `unithd`, and `LEFproperties` = `CatenaDesignType deviceLevel k2 "v 2"`

### Requirement: Macro completion

At the end of a macro read in creation mode, the system SHALL:
1. Recompute the cell bounding box (paint and label rectangles).
2. Re-check label attachment over the whole cell. All labels created by the LEF reader are sticky, so they keep their types.
3. Set the property `FIXED_BBOX`, a dimension-typed list of 4 integers in internal units, and mark the cell as having a fixed bounding box. With SIZE, the value is the macro box ("Macro coordinate offset"). Without SIZE, report the warning `   Macro does not define size:  computing from geometry` (three leading spaces), use the recomputed bounding box, and queue a full DRC check of that box.
4. Set the property `LEFview` to the string `TRUE`. Only the presence of `LEFview` matters (see the extraction capabilities, which treat such cells as abstract views).
In annotation mode the system SHALL recompute the bounding box and, only when SIZE is present, set `FIXED_BBOX` to the macro box and mark the box fixed. It SHALL NOT set `LEFview` and SHALL NOT queue DRC.
Compatibility: exact

(Magic ref: lef/lefRead.c:2364-2448)

#### Scenario: Macro without SIZE
- **GIVEN** a macro whose only geometry is an OBS RECT 0 0 1 2 on `met1` with s = 0.1 and no SIZE
- **WHEN** it is read into a new cell
- **THEN** the warning is reported and `FIXED_BBOX` is `0 0 10 20`
- **AND** `LEFview` is `TRUE`

#### Scenario: SITE becomes a cell
- **WHEN** `SITE unithd CLASS CORE ; SYMMETRY Y ; SIZE 0.46 BY 2.72 ; END unithd` is read with s = 0.005
- **THEN** a cell `unithd` exists with no paint, `LEFclass` = `CORE`, `LEFsymmetry` = `Y`, `FIXED_BBOX` = `0 0 92 544` and `LEFview` = `TRUE`

### Requirement: PIN attributes

Within `PIN <name> ... END <name>`, the following statements SHALL set the attributes that apply to the PORT statements after them. Attributes start as default for each pin.

| statement | accepted values (full-word, case-insensitive) → port attribute |
|---|---|
| `DIRECTION d ... ;` | DEFAULT→default, INPUT→input, OUTPUT→output, INOUT→bidirectional, FEEDTHRU→feedthrough. Only the first token is examined, so `OUTPUT TRISTATE` gives output. See Q-LEFDEF-13. |
| `USE u ;` | DEFAULT, SIGNAL, ANALOG, POWER, GROUND, CLOCK → the same use. Other values (for example SCAN, TIEOFF, RESET) report `Improper USE statement` and leave the use unchanged. See Q-LEFDEF-14. |
| `SHAPE x ;` | DEFAULT, ABUTMENT→abutment, RING, FEEDTHRU→feedthrough |

An unrecognized DIRECTION or SHAPE value SHALL report `Improper DIRECTION statement` or `Improper SHAPE statement` and leave the attribute unchanged. `CAPACITANCE`, `ANTENNADIFFAREA`, `ANTENNAGATEAREA`, `ANTENNAMODEL`, `ANTENNAPARTIALMETALAREA`, `ANTENNAPARTIALMETALSIDEAREA`, `ANTENNAPARTIALCUTAREA`, `ANTENNAMAXAREACAR`, `ANTENNAMAXSIDEAREACAR` and `NETEXPR` SHALL be ignored up to `;`. Other keywords report a message and are skipped up to `;`. A mismatched `END` reports `Pin END statement missing.`.
Compatibility: exact

(Magic ref: lef/lefRead.c:1508-1820)

#### Scenario: DIRECTION after PORT does not apply
- **GIVEN** `PIN A PORT LAYER met1 ; RECT 0 0 1 1 ; END DIRECTION INPUT ; END A`
- **WHEN** it is read into a new cell
- **THEN** the port label `A` has class default, not input

### Requirement: PORT and OBS geometry

PORT and OBS bodies SHALL be read with the following statements:

```ebnf
geometry  = { geom_stmt } , "END" ;            (* END must end its line *)
geom_stmt = "LAYER" , name , { tok } , ";"
          | "RECT" , coord4 , { tok } , ";"
          | "POLYGON" , { num , num } , ";"
          | ( "WIDTH" | "PATH" | "VIA" | "CLASS" ) , { tok } , ";"   (* ignored *)
          | other , { tok } , ";" ;             (* message "Unknown keyword ..." *)
coord4    = [ "(" ] , num , num , [ ")" ] , [ "(" ] , num , num , [ ")" ] ;
```

- `LAYER`: PORT uses normal lookup and OBS uses obstruction lookup ("LEF layer name resolution").
- `RECT`: if the current layer is none, the statement is skipped without error. Otherwise each coordinate converts to round(v/s), the rectangle is made canonical, and the offset is added. A malformed RECT (including `RECT MASK n ...` and `RECT ITERATE ...`) reports `Bad port geometry: RECT requires 4 values.`.
- Contact sizing: if the current layer is a contact type and the current contact area A (half units) is non-null, the rectangle (x1,y1,x2,y2) SHALL be replaced by (⌊(x1+x2+A.xbot)/2⌋, ⌊(y1+y2+A.ybot)/2⌋, ⌊(x1+x2+A.xtop)/2⌋, ⌊(y1+y2+A.ytop)/2⌋), where ⌊⌋ is the floor (arithmetic shift).
- The rectangle SHALL be painted with the current layer. In OBS, if a secondary obstruction type is set, the same rectangle SHALL also be painted with it.
- `POLYGON`: read x,y pairs until `;`. Each vertex converts to round(v/s) plus the offset. A bad number reports `Bad X value in polygon.` or `Bad Y value in polygon.` and skips the statement; a missing Y reports `Missing Y value in polygon point!`. In each case the vertices read so far are used. The polygon SHALL be painted with the current layer, on every plane that layer occupies, by the polygon-to-rectangle decomposition used for CIF polygon input, applied to the vertex list in reverse file order. In OBS it is also painted with the secondary obstruction type. If the current layer is none, Magic's behavior is undefined. See Q-LEFDEF-18.
- `WIDTH`, `PATH` and `VIA` geometry SHALL be ignored, so path-based and via-instance pin or obstruction geometry produces no paint. See Q-LEFDEF-15.
Compatibility: exact

(Magic ref: lef/lefRead.c:1174-1349, 966-1095, cif/CIFrdpt.c:603-635)

#### Scenario: Contact sizing of a cut RECT
- **GIVEN** a VIA-class tech entry for `pc` with contact area (−4,−4,4,4) half units and a PORT RECT that converts to (10,10,13,13) on that layer
- **WHEN** it is read
- **THEN** the painted rectangle is (⌊19/2⌋, ⌊19/2⌋, ⌊27/2⌋, ⌊27/2⌋) = (9,9,13,13)

#### Scenario: Ignored PATH
- **WHEN** an OBS contains `LAYER met1 ; PATH 0 0 5 0 ;`
- **THEN** nothing is painted for the PATH

### Requirement: Port labels in creation mode

In creation mode, each PORT SHALL turn its accepted geometry into port labels. The geometry list for a PORT SHALL be built in this order: each RECT is prepended, and each POLYGON's decomposition rectangles (from the last plane painted) are appended in decomposition order (see Q-LEFDEF-23). For each list element in order, a label SHALL be created and appended to the cell's label list with:
- text = the pin name; rectangle = the element's rectangle; type = the element's layer;
- default font and an automatically chosen text position;
- port index = the index of the first existing port label in the cell with the same text, or else the macro's pin ordinal for this PIN;
- flags = the pin's class, use and shape, all four connection directions (north, east, south, west), and sticky.
Compatibility: exact

(Magic ref: lef/lefRead.c:1381-1500, 1244-1306, database/DBlabel.c:97-109)

#### Scenario: Label order and numbering
- **GIVEN** a new macro whose first PIN `VDD` has `USE POWER ;` and one PORT with `LAYER met1 ; RECT 0 0 1 1 ; RECT 0 5 1 6 ;`, and s = 1
- **WHEN** it is read
- **THEN** two labels `VDD` are appended, first at (0,5,1,6) and then at (0,0,1,1)
- **AND** both have port index 1, use power, class default, all directions and sticky set

#### Scenario: Second pin numbering
- **GIVEN** the same macro also has a second PIN `A` with one RECT
- **WHEN** it is read
- **THEN** label `A` has port index 2

### Requirement: Annotation of existing cells

In annotation mode (the macro names an existing cell C), OBS sections SHALL be skipped and PIN sections SHALL annotate C's labels with this procedure:

```
on PIN N:
  first := first label of C whose text == N
  if none: N2 := N with its first '<' -> '[' and first '>' -> ']' if N contains '<',
                 else its first '[' -> '<' and first ']' -> '>';
           first := first label with text == N2; if found, N := N2
  if none: first := first label whose text equals N ignoring case; if found, N := its text
  if none: first := C's first label (or none)
  firstport := true
on each PORT of the pin:
  P := first label at or after `first` that is a port and has text N
  lab := P if P exists else first ; annotated := false
  while lab != none:
     if lab.text == N:
        if P exists and lab is not a port: stop
        if lab.rect has zero width or zero height: stop
        idx := lab.port if lab is a port else (max port index in C, or -1) + 1
        lab.flags := (lab.flags minus class/use/shape/direction bits)
                     + pin class + use + shape + all four directions   # sticky unchanged
        lab.port := idx ; annotated := true
     lab := next label
  first := (lab == none) ? none : lab.next
  if annotated: skip the PORT body
  else:
     if lab == none and firstport: erase every label of C whose text is N
     read the PORT body with replacement label R := lab (possibly none)
  firstport := false
```

When reading a PORT body in annotation mode, each RECT SHALL first be checked against C. The rectangle must be covered entirely by material, in C and all of its subcells and array elements (flattened), of types that connect to the RECT's layer. If any part is not covered, the RECT SHALL be skipped: it is not painted and not listed. Accepted RECTs and all POLYGONs SHALL be painted into C and listed as in creation mode. The first list element SHALL update R, if R is given: R's rectangle and type become the element's. R's port index is kept if R is a port, and otherwise becomes (max port index in C) + 1. R's flags become class + use + shape + all directions + sticky. Every other element SHALL create a new label as in creation mode. If no element is accepted, R is unchanged. See Q-LEFDEF-21.
Compatibility: exact

(Magic ref: lef/lefRead.c:1602-1786, 1214-1258, 1397-1441, 2321-2324)

#### Scenario: Area label becomes a port without geometry change
- **GIVEN** an existing cell `inv` with a non-port label `A` on `met1` with rectangle (0,0,10,10)
- **WHEN** a LEF macro `inv` with `PIN A DIRECTION INPUT ; USE SIGNAL ; PORT LAYER met1 ; RECT 0 0 0.5 0.5 ; END END A` is read
- **THEN** label `A` becomes a port with class input, use signal, all directions, and port index (max existing index)+1
- **AND** its rectangle stays (0,0,10,10) and nothing is painted

#### Scenario: Point label replaced by LEF geometry
- **GIVEN** an existing cell with a point label `Y` at (5,5,5,5) on `met1`, `met1` covering (0,0,20,20), and s = 1
- **WHEN** the macro's PIN `Y` has a PORT with `LAYER met1 ; RECT 2 2 8 8 ;`
- **THEN** label `Y` gets rectangle (2,2,8,8) and type `met1`, and becomes a sticky port

#### Scenario: Incompatible port rectangle rejected
- **GIVEN** an existing cell with no paint under (0,0,1,1) and no label `Z`
- **WHEN** the macro's PIN `Z` has a PORT RECT (0,0,1,1) on `met1`
- **THEN** that rectangle is not painted and no label `Z` is created

#### Scenario: Bus delimiter translation
- **GIVEN** an existing cell with a label `D[3]` and a LEF pin named `D<3>`
- **WHEN** the macro is read
- **THEN** the pin annotates label `D[3]`

### Requirement: lef read side effects

`lef read` SHALL report `Reading LEF data from file <path>.` and `This action cannot be undone.` before reading, and `LEF read: Processed <n> lines.` followed by the error and warning summary after reading. It SHALL redisplay each macro cell's area. The table of macro names seen in this file SHALL be cleared after each read, so duplicate renaming applies only within one file. The file SHALL be located by trying, in order:
1. If the last path component has an extension other than `.lef`, the name as given, searched in the cell search path.
2. If the last path component has an extension other than `.lef`, the name with that extension removed and `.lef` appended.
3. If the last path component already ends in `.lef`, the name as given.
4. If the last path component has no extension, the name with `.lef` appended.
Each attempt SHALL search the current directory and the cell search path. See Q-LEFDEF-24. If the file cannot be opened, the error `Cannot open input file <path> (<reason>).` SHALL be reported and nothing is read.
Compatibility: behavioral

(Magic ref: lef/lefRead.c:3003-3024, 3186-3196, lef/lefWrite.c:150-235)

#### Scenario: Unusual extension
- **GIVEN** a file `tech.tlef` exists
- **WHEN** `lef read tech.tlef` is executed
- **THEN** `tech.tlef` is opened as given

#### Scenario: Extension supplied
- **GIVEN** a file `cells.lef` exists
- **WHEN** `lef read cells` is executed
- **THEN** `cells.lef` is read

### Requirement: lef read arguments

`lef read <file> [option ...]` SHALL require the file name as the first argument after `read`; `lef read` without a file name SHALL print the help text and read nothing. Options SHALL be recognized only after the file name. An argument matches an option when it starts with the option's literal text, comparing only that text's length:

| option | chars compared | effect for `lef read` |
|---|---|---|
| `-import` | 7 | accepted; no behavioral effect (FOREIGN cells are never loaded from `.mag` files). See Q-LEFDEF-16. |
| `-anno` | 5 | annotation request (see "MACRO and SITE cell creation") |
| `-label` | 6 | prints `The "-labels" option is only for def read` and is ignored |
| `-noblock` | 8 | prints `The "-noblockage" option is only for def read` and is ignored |

Other arguments that begin with `-`, and arguments that do not, SHALL be silently ignored. The read SHALL use the current LEF datestamp setting.
Compatibility: behavioral

(Magic ref: lef/lefCmd.c:237-284, lef/lefRead.c:2345-2357)

#### Scenario: Unknown option ignored
- **WHEN** `lef read cells.lef -foo -annotate` is executed
- **THEN** `cells.lef` is read with annotation requested and `-foo` has no effect

#### Scenario: Option before file name
- **WHEN** `lef read -annotate cells.lef` is executed
- **THEN** `-annotate` is taken as the file name, no option is recognized, and the system tries to open `-annotate.lef`

### Requirement: lef datestamp

`lef datestamp` SHALL query or set the timestamp given to cells created or annotated by later LEF reads:
- `lef datestamp` with no value SHALL report the current setting. Magic reports the integer value, or `default` when no fixed stamp is set; outside Tcl it prints `Macros will contain a header creation date stamp of <v>.` or `Macros will contain a default header creation date stamp.`.
- `lef datestamp default` (exact, case-sensitive) SHALL clear the fixed stamp.
- `lef datestamp <int>`, where the argument is a valid integer, SHALL set the fixed stamp to that value. Every macro or site cell read afterwards SHALL get that timestamp and be marked as having a fixed timestamp.
- Any other value SHALL report `Unrecognizable date stamp "<v>".` followed by the help text; more than one value SHALL print the help text. In both cases the setting is unchanged.
- `def datestamp` is rejected as specified in capability `def-write` (Requirement "def command argument validation").
The setting SHALL persist for the session and starts as "default".
Compatibility: behavioral

(Magic ref: lef/lefCmd.c:446-482, lef/lefRead.c:2160-2164)

#### Scenario: Fixed stamp applied
- **GIVEN** `lef datestamp 1600000000` was executed
- **WHEN** a LEF file with macro `INV` is read
- **THEN** cell `INV` has timestamp 1600000000 and is marked as fixed-stamp

#### Scenario: Invalid stamp
- **WHEN** `lef datestamp yesterday` is executed
- **THEN** `Unrecognizable date stamp "yesterday".` is reported and the setting is unchanged
