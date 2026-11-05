# Mask Hints

## Purpose

Defines mask hints: per-cell geometric annotations stored as cell properties whose key starts with `MASKHINTS_`. A mask hint is a set of rectangles (a Manhattan region) in the cell's internal coordinates that (a) adds geometry to a mask layer on output through the cifoutput `mask-hints` operator, (b) is created from GDS/CIF input either through the cifinput `mask-hints` operator or the `gds maskhints` option, and (c) scopes DRC rules through `exception`/`exemption` blocks. This capability owns the hint data model, its creation, storage in `.mag` files, copying/flattening/scaling, and output.

Related capabilities (referenced, not duplicated): `cmd-labels-properties` (general `property` command), `cell-file-format` (properties section layout, scale reducer, property order), `mask-output-styles` and `mask-output-operators` (cifoutput layer operator pipeline), `mask-input-styles` (cifinput templayers), `gds-read` (GDS reading, coordinate scaling), `drc-rule-language` (exception/exemption block semantics), `layout-hierarchy` (flatten commands), `layout-geometry-model` (tile-plane region representation and enumeration order).

## Requirements

### Requirement: Mask-hint property data model

The system SHALL treat every cell property whose key begins with the ten characters `MASKHINTS_` as a mask hint named by the remainder of the key, whose value is a Manhattan region in the cell's internal coordinates represented as the union of the rectangles assigned to it. Overlapping or abutting rectangles merge; the region is canonically decomposed into maximal horizontal strips (the same canonical decomposition as layout paint). Non-Manhattan hint geometry is not supported. A rectangle with zero or negative width or height contributes nothing. Setting a hint property to an empty value removes the property.

Compatibility: exact

(Magic ref: database/database.h.in:789-793, database/DBprop.c:100-125, commands/CmdLQ.c:2550-2564, database/DBio.c:4960-4985)

#### Scenario: Overlapping rectangles merge
- **GIVEN** a hint MASKHINTS_SRAM assigned the rectangles (0,0)-(10,10) and (5,5)-(15,15)
- **WHEN** the hint region is enumerated
- **THEN** it consists of the strips (5,10)-(15,15), (0,5)-(15,10) and (0,0)-(10,5)

### Requirement: Setting mask hints with the property command

When the `property` command sets a key beginning with `MASKHINTS_`, the system SHALL force the hint (region) type regardless of any type keyword given, parse the value either as one argument containing whitespace-separated tokens or as multiple arguments, interpret every token as a coordinate in the current user units (odd positions as x, even positions as y, accepting the same unit suffixes as other coordinate arguments), group consecutive tokens in fours as x1 y1 x2 y2, and paint each complete group into a new region that replaces any previous value of the property; trailing tokens that do not complete a group of four are ignored. Querying a hint property returns the strip rectangles in enumeration order as four coordinates each, printed in current user units. The cell is marked modified.

Compatibility: behavioral

(Magic ref: commands/CmdLQ.c:2563-2564, commands/CmdLQ.c:2582-2690, commands/CmdLQ.c:2700-2770, commands/CmdLQ.c:2455-2461, commands/CmdLQ.c:2831-2870)

#### Scenario: Set and query a hint in lambda units
- **GIVEN** an edit cell at 1 internal unit per lambda
- **WHEN** the user runs `property MASKHINTS_SRAM "0 0 10 10"` and then `property MASKHINTS_SRAM`
- **THEN** the query returns `0 0 10 10`

#### Scenario: Incomplete group ignored
- **WHEN** the user runs `property MASKHINTS_X "0 0 10 10 20 20"`
- **THEN** the hint region is (0,0)-(10,10) only

### Requirement: Mask hints in .mag files

The system SHALL write each mask hint in the `.mag` properties section as one line `string MASKHINTS_<name>` followed, for each strip of the region in enumeration order, by ` <x1> <y1> <x2> <y2>` (each value the internal coordinate divided by the file's scale reducer, decimal integer, single leading space), and SHALL read back a properties-section line whose type keyword is `string` and whose key starts with `MASKHINTS_` by parsing the value as integers in groups of four, multiplying each by the file scale-up factor and dividing (truncating) by the scale-down factor, and painting each group into the hint region. When the property compatibility mode is turned off (`property compat false`) the type keyword written is `dimension` instead of `string`. A value containing a non-integer token reports "Mask-hint "<rest>" has non-integer values!" and the property is dropped; a count not divisible by four reports "Mask-hint property number of values is not divisible by four.  Truncated." and keeps the complete groups. Mask-hint coordinates participate in the greatest-common-factor computation that selects the file scale reducer. The type keyword `plane` is not accepted in a `.mag` properties section (see Q-MISC-12).

Compatibility: exact

```ebnf
hint_line  = type_kw , " " , "MASKHINTS_" , name , { " " , int , " " , int , " " , int , " " , int } , "\n" ;
type_kw    = "string" | "dimension" ;     (* "dimension" only written when compat mode is off; see Q-MISC-10 *)
name       = ? non-blank characters, total key length <= 127 ? ;
int        = [ "-" ] , digit , { digit } ;
```

Example (one hint covering an L-shaped area, reducer 1):

```
<< properties >>
string MASKHINTS_SRAM 5 10 15 15 0 5 15 10 0 0 10 5
```

(Magic ref: database/DBio.c:3211-3245, database/DBio.c:3309-3320, database/DBio.c:3386-3462, database/DBio.c:4405-4421, database/DBio.c:4985-5000, database/DBio.c:5030-5096, database/DBprop.c:37, commands/CmdLQ.c:2525-2541)

#### Scenario: Round trip through a .mag file
- **GIVEN** a cell whose MASKHINTS_SRAM region is the union of (0,0)-(10,10) and (5,5)-(15,15), saved with default property compatibility
- **WHEN** the cell is written and read back
- **THEN** the file line is `string MASKHINTS_SRAM 5 10 15 15 0 5 15 10 0 0 10 5` (verify strip order against Magic)
- **AND** the region read back equals the original region

#### Scenario: Read with scale-down
- **GIVEN** a `.mag` file read with scale factor 1/2 and the line `string MASKHINTS_A 0 0 9 9`
- **WHEN** the file is read
- **THEN** the hint region is (0,0)-(4,4)

#### Scenario: Non-integer value
- **GIVEN** the line `string MASKHINTS_A 0 0 abc 9`
- **WHEN** the file is read
- **THEN** an error is reported and no MASKHINTS_A property exists on the cell

### Requirement: Mask-hint output operator

In a cifoutput layer definition, the operator `mask-hints <name>` SHALL union into the layer being generated, at that point of the operator sequence, the region of the property `MASKHINTS_<name>` of the cell whose geometry is being generated, each strip scaled from internal units to output units by multiplying every coordinate by the style's scalefactor; if the cell has no such property the operator leaves the layer unchanged. The operator takes exactly one argument. For hierarchical interaction distances, operators after a `mask-hints` operator in the same layer are not considered. When output geometry is generated for a flattened region (hierarchical interaction areas, `cif see`, and similar), the hint regions of the flattened subcells are first transformed into the flattened coordinate system and unioned into the flattened cell's hints of the same name.

Compatibility: exact

```ebnf
mask_hints_op = "mask-hints" , ws , name ;
```

(Magic ref: cif/CIFtech.c:1120-1121, cif/CIFtech.c:1374-1377, cif/CIFtech.c:1682-1687, cif/CIFgen.c:4998-5017, cif/CIFgen.c:5675-5701, cif/CIFhier.c:241-347, cif/CIFhier.c:479-480, cif/CIFhier.c:888-889, cif/CIFsee.c:170-171)

#### Scenario: Hint adds geometry to an output layer
- **GIVEN** a cifoutput style with scalefactor 10 and the layer definition `layer SRAMID` with operator `mask-hints SRAM`, and a cell with MASKHINTS_SRAM = (0,0)-(10,10) and no other geometry
- **WHEN** GDS is written
- **THEN** layer SRAMID of that cell contains one boundary (0,0)-(100,100) in output units

#### Scenario: Hint combined with paint
- **GIVEN** the layer definition `layer MET1` with operators `bloat-or m1 * 0` then `mask-hints MET1`, a metal1 rectangle (0,0)-(4,4) and MASKHINTS_MET1 = (4,0)-(6,4)
- **WHEN** GDS is written with scalefactor 1
- **THEN** layer MET1 contains the merged region (0,0)-(6,4)

### Requirement: Mask hints from cifinput templayers

When a cifinput `templayer` contains the operator `mask-hints <name>` and no `copyup` operator, the system SHALL, after computing that templayer's geometry for a cell being read, convert each resulting rectangle from input units to internal units (exact conversion, which may rescale the internal grid as described in capability `gds-read`) and union it into the cell's `MASKHINTS_<name>` property, creating the property if needed. A templayer with no geometry creates no property.

Compatibility: exact

```ebnf
cifinput_mask_hints_op = "mask-hints" , ws , name ;   (* exactly one argument *)
```

(Magic ref: cif/CIFrdtech.c:1000-1001, cif/CIFrdtech.c:1030-1033, cif/CIFrdcl.c:688-800)

#### Scenario: Templayer becomes a hint (verify against Magic)
- **GIVEN** a cifinput style with `templayer SRAMH 81,2` followed by `mask-hints SRAM`, and a GDS cell with a boundary on 81/2 from (0,0) to (1000,1000) nm at 10 nm per internal unit
- **WHEN** the GDS is read
- **THEN** the cell has MASKHINTS_SRAM = (0,0)-(100,100)
- **AND** a second templayer hint with the same name in the same cell adds to the existing region (see Q-MISC-11)

### Requirement: Mask hints generated by gds maskhints

The `gds maskhints` option SHALL control generation of hints that preserve input geometry not reproducible by output generation: `gds maskhints` with no argument reports whether any layers are enabled; `no|false|off|0` disables all; `yes|true|on|1` enables all input layers; any other argument is a comma-separated list of cifinput layer names to enable. For each cell read and each enabled input layer L whose name equals the name of a cifoutput layer that contains a `mask-hints` operator, the system SHALL generate the output geometry of the cell (its own paint only, using the current output style), compute H = (input geometry of L scaled to output units) minus (generated output layer), divide each coordinate of each rectangle of H by the output scalefactor with truncation toward zero, and union the result into the cell's `MASKHINTS_<L>` property.

Compatibility: exact

(Magic ref: commands/CmdCD.c:145-146, commands/CmdCD.c:168, commands/CmdCD.c:643-676, cif/CIFrdcl.c:821-975)

#### Scenario: Unreproducible input geometry becomes a hint
- **GIVEN** `gds maskhints MET1`, an output layer MET1 whose operators are `bloat-or m1 * 0` and `mask-hints MET1`, scalefactor 10, and a GDS cell whose MET1 data is the polygon (0,0)-(60,40) that reads back as metal1 (0,0)-(4,4) plus extra MET1 (40,0)-(60,40)
- **WHEN** the GDS is read
- **THEN** the cell receives MASKHINTS_MET1 = (4,0)-(6,4)
- **AND** writing GDS again reproduces MET1 = (0,0)-(60,40)

### Requirement: Copying, flattening and transforming hints

Hints SHALL follow their cell's geometry: (a) `flatten` copies the hints of every flattened descendant instance, transformed by the instance transform and clipped to the copied area, into the destination cell's hints of the same name (union); (b) `flatten -inplace` (instance flattening into the parent) copies each flattened instance's (and each array element's) complete hints into the parent with the instance transform; (c) when hierarchical DRC builds flattened interaction areas, the hints of the instances involved are copied in the same way so that exception/exemption scoping sees them; (d) when a cell's internal grid is rescaled by n/d, every hint region is rescaled with the same rule as paint; (e) when a cell's contents are translated (origin change), hint regions are translated by the same offset. Hints are not otherwise affected by editing paint.

Compatibility: behavioral

(Magic ref: database/DBcellcopy.c:377-560, database/DBcellcopy.c:674, commands/CmdFI.c:2207, drc/DRCsubcell.c:938, drc/DRCsubcell.c:1001, drc/DRCarray.c:302, database/DBcellsrch.c:1855-1882, database/DBcellsrch.c:1911-1937, database/DBcellsrch.c:2083-2097, database/DBcellsrch.c:2285-2297)

#### Scenario: Flatten in place with a rotated instance
- **GIVEN** child cell C with MASKHINTS_SRAM = (0,0)-(10,20), placed in parent P with the instance transform (x, y) → (−y + 100, x)
- **WHEN** the instance is flattened in place
- **THEN** P has MASKHINTS_SRAM = (80,0)-(100,10)

#### Scenario: Grid rescale
- **GIVEN** a cell with MASKHINTS_A = (0,0)-(5,5)
- **WHEN** the internal grid is rescaled by 2/1
- **THEN** MASKHINTS_A becomes (0,0)-(10,10)

### Requirement: Mask hints scope DRC exception and exemption rules

A DRC rule declared under `exception <name>` SHALL be applied to an edge only if the edge segment touches the region of `MASKHINTS_<name>` of the cell being checked, and a rule declared under `exemption <name>` SHALL be applied only if it does not; when the property is absent, exception rules are never applied and exemption rules are always applied. The touch test uses the zero-width segment from (x, max(edge bottom, window bottom)) to (x, min(edge top, window top)). At most 126 distinct names are supported per DRC style. Whether an edge lying exactly on the hint boundary counts as touching is to be confirmed (see Q-MISC-13). Block grammar and rule semantics are owned by capability `drc-rule-language`.

Compatibility: exact

(Magic ref: drc/DRCtech.c:420-455, drc/DRCtech.c:3858-3912, drc/DRCbasic.c:921-957, drc/DRCbasic.c:1381-1417)

#### Scenario: Exception rule only inside hint
- **GIVEN** a rule `width m1 3` declared under `exception SRAM`, and a cell with a 2-wide metal1 wire half inside and half outside MASKHINTS_SRAM
- **WHEN** DRC runs
- **THEN** width errors are reported only for edges touching the SRAM hint region
