# Mask Input Styles (cifinput)

## Purpose

Defines the `cifinput` section of the technology file and the semantics Hefesto applies when converting mask-layer geometry read from CIF or GDSII files into Magic-compatible tile types. This covers style and variant selection, scale handling (`scalefactor`, `gridlimit`, mid-read grid subdivision), CIF-layer naming, the GDS layer/datatype mapping with wildcards, the input geometric operators (`or`, `and`, `and-not`, `grow`, `grow-grid`, `shrink`, `not-square`, `copyup`, `boundary`, `mask-hints`, `tagged`), the per-cell layer processing order, conversion of CIF-unit tiles to internal units including contact half-grid snapping, polygon-to-region conversion, and how label layers and label types are assigned.

Out of scope: the CIF file syntax (capability `cif-format-io`), GDSII record parsing (capability `gds-read`), output styles (capability `mask-output-styles`) and the full geometric definitions of output operators (capability `mask-output-operators`). This spec references these capabilities by name.

Terminology used throughout:

- **Read unit (RU):** the integer coordinate unit of the CIF-layer planes built while a file is read. One RU is `1/M` centimicrons, where `M` is the current style *multiplier* (see "Scalefactor statement").
- **Internal unit (IU):** one Magic/Hefesto database unit. One IU equals `S` RU, where `S` is the current style *scale factor*.
- **CIF layer:** a named input mask layer. The name is a string, not a tile type. Temporary layers (`templayer`) are also CIF layers.
- **Read plane:** a per-cell, per-CIF-layer region (set of rectangles and right-triangle split tiles) in RU, accumulated while one cell is read.

## Requirements

### Requirement: Cifinput section grammar

Hefesto SHALL accept the `cifinput` section with exactly the statement grammar below. Keywords are matched case-sensitively. Each line is split into whitespace-separated fields. An empty line is ignored. A statement with the wrong number of fields is reported as an error and ignored, and the remainder of the section continues to be processed.

Compatibility: exact

```ebnf
cifinput-section = "cifinput" , NL , { statement , NL } , "end" , NL ;
statement     = style-stmt | scale-stmt | grid-stmt | variant-stmt
              | layer-stmt | templayer-stmt | calma-stmt | labels-stmt
              | ignore-stmt | options-stmt | op-stmt ;
style-stmt    = "style" , name , [ variant-kw , variant-list ] ;
variant-kw    = "variant" , { letter } ;            (* any word whose first 7 chars are "variant" *)
variant-list  = variant , { "," , variant } ;       (* no spaces inside the field *)
scale-stmt    = "scalefactor" , scale , [ word ] , [ unit-kw ] ;   (* 2 to 4 fields *)
scale         = digits , [ "." , digits ] ;
unit-kw       = "nanom" , { letter } | "angstr" , { letter } | "calmaonly" ;
grid-stmt     = "grid" , { letter } , integer ;     (* e.g. "gridlimit 5" *)
variant-stmt  = "variant" , { letter } , variant-sel ;   (* exactly 2 fields *)
variant-sel   = vsel , { "," , vsel } ;  vsel = "*" | variant ;
layer-stmt    = "layer" , magic-type , [ cif-layer-list ] ;
templayer-stmt= ( "templayer" | "fault" ) , cif-layer , [ cif-layer-list ] ;
calma-stmt    = ( "calma" | "gds" , { letter } ) , cif-layer , num-list , num-list ;
num-list      = num-item , { "," , { "," } , num-item } ;
num-item      = "*" | digits ;                      (* 0..255 *)
labels-stmt   = "labels" , cif-layer-list , [ label-kind ] ;
label-kind    = "text" | "sticky" | "port" | "cell" , { letter } ;
ignore-stmt   = "ignore" , cif-layer-list ;
options-stmt  = "options" , option , { option } ;
option        = "ignore-unknown-layer-labels" | "no-reconnect-labels" ;
op-stmt       = ( "or" | "and" | "and-not" | "copyup" ) , cif-layer-list
              | ( "grow" | "grow-grid" | "shrink" ) , integer
              | "not-square" | "boundary"
              | "mask-hints" , name
              | "tagged" , text , [ cif-layer-list ] ;
cif-layer-list= cif-layer , { "," , { "," } , cif-layer } ;
```

Annotated example:

```
cifinput
style  lambda=1.0(gen)            # first style: the default input style
   scalefactor 100                # 1 IU = 100 centimicrons = 1 um; M = 1
   layer m1 CMF                   # paint m1 where CIF layer CMF has material
      labels CMF                  # labels on CMF attach to m1
   layer ndc CCA                  # start empty-OR'd with CCA
      grow 100                    # grow by 100 RU = 1 um
      and CAA                     # intersect with CAA
   templayer XBND BND             # temporary CIF layer XBND = BND
      boundary                    # defines the cell FIXED_BBOX
   ignore CX                      # drop CX geometry and labels silently
   calma CAA 1 *                  # GDS layer 1, any datatype -> CAA
   calma CMF 4 *
   calma CCA 2 *
   calma BND 235 0
   calma CX 63 *
end
```

The `labels`, `copyup` and operator statements apply to the most recent `layer`/`templayer` statement; if none has been seen in the current style the statement is an error and is ignored. `grow`, `grow-grid` and `shrink` require exactly one integer argument greater than zero (parsed with C `atoi` semantics: leading digits only); a value `<= 0` is an error and the statement is ignored. `boundary` and `not-square` ignore any extra fields. An unrecognized first word is an error and the statement is ignored.

(Magic ref: cif/CIFrdtech.c:473-1060)

#### Scenario: Operator before any layer is rejected
- **GIVEN** a `cifinput` style whose first statement after `scalefactor 100` is `and CAA`
- **WHEN** the technology file is loaded
- **THEN** an error "Must define layer before specifying operations." is reported
- **AND** the statement contributes nothing to any layer

#### Scenario: Zero grow distance is rejected
- **GIVEN** `layer m1 CMF` followed by `grow 0`
- **WHEN** the technology file is loaded
- **THEN** the `grow 0` statement is rejected with "Grow/shrink distance must be greater than zero."
- **AND** layer `m1` is generated from `CMF` alone

### Requirement: Input style declaration and default style

Hefesto SHALL maintain an ordered list of input style names and SHALL select as default the first style declared in the `cifinput` section. For `style NAME`, the name list receives `NAME`. For `style NAME variants V1,V2,...`, the list receives `NAME+V1`, `NAME+V2`, ... (string concatenation, in order), and the first of these is the style that `NAME` denotes for the purpose of defaulting. Statements that precede the first `style` statement in the section SHALL be ignored.

Compatibility: exact

A `style` statement is considered to refer to an already-listed style when some listed name has the field `NAME` as a prefix (comparison of the first `strlen(NAME)` characters). In that case no new names are added. See Q-MASK-302.

When a specific style `X` is being loaded, a statement belongs to `X` iff it follows a `style` statement that names `X` exactly (for the 2-field form), or whose base name is a prefix of `X` and one of whose listed variants equals the remainder of `X` after the base name (4-field form). Loading stops at the next `style` statement after the matching one. See Q-MASK-328 for the documented-but-unimplemented "default" name.

(Magic ref: cif/CIFrdtech.c:494-625)

#### Scenario: Variant styles expand to concatenated names
- **GIVEN** the statement `style scmos0.18 variants (p),(c),(pc),()`
- **WHEN** the technology file is loaded
- **THEN** the input style list contains, in order, `scmos0.18(p)`, `scmos0.18(c)`, `scmos0.18(pc)`, `scmos0.18()`
- **AND** if this is the first style in the section the default input style is `scmos0.18(p)`

#### Scenario: Prefix-matching style name is merged (verify against Magic)
- **GIVEN** `style foo` followed later by `style fo`
- **WHEN** the technology file is loaded
- **THEN** the style list contains only `foo`
- **AND** the statements following `style fo` are never loaded under any style name

### Requirement: Variant-conditional statements

Hefesto SHALL treat a `variant` statement inside a style as a filter on the statements that follow it, up to the next `variant` statement, `style` statement, or end of section. Let `X` be the full name of the style being loaded. The `variant` statement's single field is split at commas; whitespace immediately preceding a comma is removed from the preceding item. If any item is `*`, or any item `v` equals the last `strlen(v)` characters of `X`, the following statements are processed; otherwise they are skipped. `scalefactor` and `gridlimit` statements are processed even in a skipped region; every other statement is skipped.

Compatibility: exact

(Magic ref: cif/CIFrdtech.c:627-725)

#### Scenario: Statements for a non-matching variant are skipped
- **GIVEN** style `scmos0.18(p)` is being loaded and the style contains `variant (c),(pc)` followed by `layer m2 CMS`
- **WHEN** the style is loaded
- **THEN** no layer rule for `m2` is created

#### Scenario: Wildcard variant re-enables processing
- **GIVEN** the sequence `variant (c)`, `layer a A`, `variant *`, `layer b B` in style `x(p)`
- **WHEN** style `x(p)` is loaded
- **THEN** only the rule for `b` is created

### Requirement: Input style selection

Hefesto SHALL provide an operation that selects the current input style by name, where the given name matches every listed style of which it is a prefix. If exactly one style matches it becomes current (its statements are re-read from the technology source and its scale is re-normalized as in "Scale normalization"). If more than one matches, the selection is reported as ambiguous and the current style is unchanged, even when one of the matches is exact (see Q-MASK-303). If none matches, an error is reported and the list of styles is shown. Selecting the style that is already current SHALL be a no-op.

Compatibility: behavioral

(Magic ref: cif/CIFrdtech.c:1125-1143, cif/CIFrdtech.c:1327-1363)

#### Scenario: Unique prefix selects a style
- **GIVEN** input styles `lambda=1.0(gen)` and `sky130(vendor)`
- **WHEN** the input style `sky` is selected
- **THEN** the current input style becomes `sky130(vendor)`

#### Scenario: Exact name that is a prefix of another is ambiguous
- **GIVEN** input styles `gds` and `gds2`
- **WHEN** the input style `gds` is selected
- **THEN** the selection is reported ambiguous and the current style does not change

### Requirement: Scalefactor statement

Hefesto SHALL parse `scalefactor VALUE [...] [UNIT]` into a pair `(S, M)` of positive integers, where one internal unit equals `S/M` centimicrons. Parsing rules:

1. If `VALUE` contains no `.`, `S = atoi(VALUE)` and `M = 1`.
2. If `VALUE` is `I.F` with `F` of `k` digits, let `n = atoi(I)*10^k + atoi(F)` and `d = 10^k`; reduce `n/d` by their greatest common divisor; `S = n`, `M = d`.
3. If there are 3 or 4 fields, the last field is examined: if it begins with `nanom`, `M` is SET to 10; if it begins with `angstr`, `M` is SET to 100; anything else (including `calmaonly`) is ignored. The value from step 2 is overwritten, not multiplied (see Q-MASK-300).
4. If `S <= 0` the statement is an error and `S` is set to 0. At the end of the section, a style with `S <= 0` is reported ("bad scalefactor; using 1") and `S` becomes 1.

The units of all `grow`, `grow-grid` and `shrink` distances in the style are read units, that is `1/M` centimicrons, with `M` as obtained here (see Q-MASK-304).

Compatibility: exact

(Magic ref: cif/CIFrdtech.c:627-655, cif/CIFtech.c:506-535, cif/CIFrdtech.c:1080-1105)

#### Scenario: Integer centimicron scale
- **GIVEN** `scalefactor 100`
- **WHEN** the statement is parsed
- **THEN** `S = 100` and `M = 1`, so one internal unit is 1 micron

#### Scenario: Decimal scale
- **GIVEN** `scalefactor 1.5`
- **WHEN** the statement is parsed
- **THEN** `n = 15`, `d = 10`, reduced to `S = 3`, `M = 2`

#### Scenario: Nanometer keyword
- **GIVEN** `scalefactor 5 nanometers`
- **WHEN** the statement is parsed
- **THEN** `S = 5` and `M = 10`, so one internal unit is 5 nm and `grow 10` means 10 nm

#### Scenario: Decimal scale with nanometers overwrites the multiplier
- **GIVEN** `scalefactor 2.5 nanometers`
- **WHEN** the statement is parsed
- **THEN** step 2 yields `S = 5`, `M = 2` and step 3 sets `M = 10`
- **AND** one internal unit is taken to be 0.5 centimicron = 5 nm, not 2.5 nm

### Requirement: Scale normalization

Hefesto SHALL, after a style has been loaded, normalize its scale with the procedure `rescale(n, d, reduceFully)` called first as `rescale(1, 1, true)` and, when a style is (re)selected after the technology was loaded, again as `rescale(L0, L1, true)` where `L0/L1` is the current internal-grid ratio of the database (1/1 unless the internal grid has been subdivided or expanded).

`rescale(n, d, reduceFully)`:

1. `S := S*n`; `M := M*d`; every nonzero operator distance `D := D*d`.
2. `g := gcd(M, all nonzero |D|, S)` (with `gcd` over an empty distance set equal to `gcd(M, S)`).
3. If not `reduceFully`: `g := d` if `g mod d == 0`, otherwise `g := 1`.
4. If `g > 1`: `S := S/g`, `M := M/g`, every nonzero `D := D/g`.

The normalization never changes the physical meaning of any value (`S/M` centimicrons per IU; `D/M` centimicrons per distance).

Compatibility: exact

(Magic ref: cif/CIFrdtech.c:1080-1105, cif/CIFrdtech.c:1125-1143, cif/CIFrdtech.c:1451-1516)

#### Scenario: Common factors are divided out
- **GIVEN** `scalefactor 10 nanometers` and a single rule containing `grow 5`
- **WHEN** the style is normalized with `rescale(1, 1, true)`
- **THEN** `g = gcd(10, 5, 10) = 5`
- **AND** the style ends with `S = 2`, `M = 2`, `grow 1`

#### Scenario: No reduction possible
- **GIVEN** `scalefactor 100` and `grow 25`
- **WHEN** the style is normalized
- **THEN** `g = gcd(1, 25, 100) = 1` and the style is unchanged

### Requirement: Grid limit

Hefesto SHALL support `gridlimit G` (any keyword starting with `grid`; `G` an integer, negative values reported and replaced by 0, non-integer values an error). `G = 0` means no limit. When `G > 0`, a proposed subdivision of the internal grid by an integer factor `den` is permitted only if `q = S*10 / (G*M*den)` (integer division) is nonzero AND `(S*10) mod (G*M*den) == 0`; that is, only if the subdivided internal unit is a positive integer multiple of `G` nanometers. When the subdivision is not permitted, the coordinate being converted SHALL be snapped instead (see "Conversion of read units to internal units"). The doc comment calls `G` "CIF units"; the arithmetic makes it nanometers (see Q-MASK-305).

Compatibility: exact

(Magic ref: cif/CIFrdtech.c:93-114, cif/CIFrdtech.c:657-676)

#### Scenario: Subdivision allowed by grid limit
- **GIVEN** `scalefactor 100`, `gridlimit 5`, so `S*10 = 1000`
- **WHEN** a subdivision by `den = 2` is proposed
- **THEN** `G*M*den = 10`, `1000 / 10 = 100 != 0` and `1000 mod 10 = 0`, so the subdivision is allowed

#### Scenario: Subdivision refused by grid limit
- **GIVEN** `scalefactor 100`, `gridlimit 1000`
- **WHEN** a subdivision by `den = 2` is proposed
- **THEN** `1000 / 2000 = 0`, so the subdivision is refused and the coordinate is snapped

### Requirement: CIF layer name table

Hefesto SHALL identify CIF layers by exact, case-sensitive name. A name is added to the current style's set of known CIF layers when it appears in `templayer`, in the layer list of `layer`, `labels`, `ignore`, `and`, `and-not`, `or`, `copyup` or `tagged`. A `calma` statement SHALL NOT create a CIF layer: its CIF layer must already be known in the current style, otherwise the statement is reported ("Unrecognized CIF layer") and ignored. At most 255 distinct CIF layer names and at most 255 `layer`/`templayer` rules per style are allowed; excess entries are reported and ignored. A CIF layer that is known but used by no rule is silently discarded when read (this is the mechanism behind `ignore`).

Compatibility: exact

(Magic ref: cif/CIFrdtech.c:135-185, cif/CIFrdtech.c:255-296, cif/CIFrdtech.c:822-854, cif/CIFread.h:67)

#### Scenario: calma statement before the layer is declared
- **GIVEN** a style whose first mention of `CPO` is `calma CPO 7 *`
- **WHEN** the technology file is loaded
- **THEN** "Unrecognized CIF layer: "CPO"" is reported and GDS layer 7 remains unmapped

### Requirement: Layer and templayer rules

Hefesto SHALL build, per style, an ordered list of layer rules. `layer T [L1,L2,...]` creates a rule whose result is painted as Magic tile type `T` (any type name or alias accepted by the types section; an unknown name is an error that discards the rule and makes following operators errors). `templayer N [L1,...]` (or the deprecated `fault`, which is accepted with an error message) creates a rule whose result replaces the read plane of CIF layer `N`; `N` may be a new name or an existing CIF layer name. When the optional list is present, the rule's operator list begins with an implicit `or L1,L2,...`; otherwise the rule starts from the empty region. Each following operator statement is appended to the rule's operator list in order.

Compatibility: exact

(Magic ref: cif/CIFrdtech.c:728-820)

#### Scenario: Templayer overriding an input layer
- **GIVEN** rules `templayer CAA CAA` + `shrink 10`, followed by `layer ndiff CAA`
- **WHEN** a cell with a CAA rectangle (0,0)-(100,100) RU is processed
- **THEN** after the templayer rule the CAA read plane holds (10,10)-(90,90)
- **AND** `ndiff` is generated from the shrunk CAA

### Requirement: Per-cell layer processing order

Hefesto SHALL convert the read planes of one cell into Magic paint only after the whole cell has been read (at end of a CIF symbol definition, at end of a GDS structure, and for top-level CIF geometry at end of file). For each rule, in the order the rules were declared (`layer` and `templayer` interleaved), Hefesto SHALL evaluate the rule's operator list from an empty region over the whole plane, reading CIF-layer operands from the current read planes. Then:

- for a `layer` rule, the result is painted into the cell as type `T` (see "Painting rule results");
- for a `templayer` rule, the result replaces the read plane of that CIF layer for all later rules of the same cell, after the `copyup`, `boundary` and `mask-hints` side effects described in their own requirements.

After all rules, every read plane is cleared. Painting uses the standard paint composition of the database: a later layer painted over an earlier one has the same effect as interactive painting (for example a contact painted over diffusion replaces it). No hierarchical interaction between cells is computed; each cell is processed independently.

Compatibility: exact

(Magic ref: cif/CIFrdcl.c:600-707, cif/CIFgen.c:5041-5703)

#### Scenario: Later rule overrides earlier paint
- **GIVEN** `layer ndiff CSN` + `and CAA` declared before `layer nfet CPG` + `and CAA` + `and CSN`, scalefactor 100
- **AND** a cell with CAA (0,0)-(1000,400), CSN (0,0)-(1000,400), CPG (400,0)-(600,400) in RU
- **WHEN** the cell is processed
- **THEN** `ndiff` is first painted over (0,0)-(10,4) IU and then `nfet` is painted over (4,0)-(6,4) IU, leaving ndiff at (0,0)-(4,4) and (6,0)-(10,4)

### Requirement: Input operators or, and, and-not

Hefesto SHALL evaluate the boolean input operators on the current region `R` of the rule as follows, where `U` is the union of the read planes of the listed CIF layers (non-manhattan triangles included exactly):

- `or L` : `R := R ∪ U`
- `and L` : `R := R ∩ U`
- `and-not L` : `R := R \ U`

Operands are always CIF layers; Magic tile types are never consulted in input rules. The geometric definition of the set operations, including split (triangular) tiles, is shared with capability `mask-output-operators`.

Compatibility: exact

(Magic ref: cif/CIFgen.c:5113-5150, cif/CIFgen.c:3974-4030)

#### Scenario: and-not removes overlap
- **GIVEN** rule `layer poly CPG` + `and-not CAA`, CPG (0,0)-(100,100) and CAA (50,0)-(150,100) RU, scalefactor 50
- **WHEN** the cell is processed
- **THEN** poly is painted over (0,0)-(1,2) IU

### Requirement: Input operators grow and shrink

Hefesto SHALL implement `grow D` and `shrink D` on input exactly as the output operators of the same name defined in capability `mask-output-operators`, with `D` in read units. In particular the choice between the default (square/Minkowski with a square of half-size `D`) and the Euclidean grow algorithm SHALL follow the grow-euclidean option of the CURRENT OUTPUT style, because Magic consults that flag even while reading (see Q-MASK-306). `shrink D` is the complement of `grow D` applied to the complement of `R`.

Compatibility: exact

(Magic ref: cif/CIFgen.c:5081-5082, cif/CIFgen.c:5151-5205)

#### Scenario: Grow a contact cut to its Magic size
- **GIVEN** scalefactor 100 and rule `layer ndc CCA` + `grow 100` + `and CAA`
- **AND** a CCA cut (100,100)-(300,300) RU inside CAA (0,0)-(400,400)
- **WHEN** the cell is processed
- **THEN** the grown region is (0,0)-(400,400) RU and `ndc` is painted at (0,0)-(4,4) IU

### Requirement: Input operator grow-grid

Hefesto SHALL implement `grow-grid G` on input by replacing every maximal horizontal strip (tile) `[x1,x2]×[y1,y2]` of `R` with `[x1 - (|x1| mod G), x2 + (|x2| mod G)] × [y1 - (|y1| mod G), y2 + (|y2| mod G)]` and taking the union (coordinates at the plane's infinite extent are left unchanged). Non-manhattan tiles are treated as their bounding rectangles. Note that the upper edges are moved by `|x| mod G`, not to the next multiple of `G`, and negative lower edges are moved further than the nearest multiple (see Q-MASK-334). This is the same function used by the output operator in capability `mask-output-operators`.

Compatibility: exact

(Magic ref: cif/CIFgen.c:502-539, cif/CIFgen.c:5179-5191)

#### Scenario: grow-grid on positive coordinates
- **GIVEN** `grow-grid 5` and `R` = (7,3)-(12,8)
- **WHEN** the operator is applied
- **THEN** the result is (5,0)-(14,11)

### Requirement: Input operator not-square

Hefesto SHALL implement `not-square` as: the result is the union of every manhattan tile of `R` except tiles that are both square (width equal to height) and isolated (no tile of `R` touches any of its four sides). Non-manhattan (triangular) tiles of `R` SHALL be dropped from the result (see Q-MASK-330). Tiles are the maximal-horizontal-strip decomposition of `R`.

Compatibility: exact

(Magic ref: cif/CIFgen.c:4727-4800, cif/CIFgen.c:5420-5431)

#### Scenario: Isolated square cut is removed
- **GIVEN** `R` consists of an isolated square (0,0)-(20,20) and an isolated rectangle (100,0)-(140,20)
- **WHEN** `not-square` is applied
- **THEN** the result is (100,0)-(140,20) only

### Requirement: Input operator copyup

Hefesto SHALL treat `copyup L1,L2,...` in a `templayer` rule as a request to move that rule's result into every PARENT cell instance-wise when reading GDS. When the cell is processed, if the rule's result is non-empty, the result is added (union) to a per-cell saved plane for each listed CIF layer `Li`, and the cell is marked as carrying saved planes. When a parent structure later places this cell (single reference or array, see capability `gds-read`), each saved plane is transformed by the instance transform (and, for arrays, each element offset) and unioned into the parent's read plane of the same CIF layer before the parent's rules are evaluated. The instance itself is still created if the cell contains paint or subcells. The rule still replaces the templayer's own read plane as any templayer. Saved planes are discarded after the read completes. The CIF reader SHALL NOT perform the copy into parents (see Q-MASK-309).

Compatibility: exact

(Magic ref: cif/CIFrdcl.c:342-406, calma/CalmaRdcl.c:1450-1600, cif/CIFrdcl.c:1345-1388)

#### Scenario: Well geometry copied up to the parent
- **GIVEN** rule `templayer NW_UP NWELL` + `copyup NWELL` and a GDS cell `A` with an NWELL box (0,0)-(100,100) RU and some metal
- **AND** a parent `TOP` with an SREF of `A` at translation (1000,0) RU
- **WHEN** the GDS file is read
- **THEN** the NWELL read plane of `TOP` contains (1000,0)-(1100,100) RU before TOP's rules run
- **AND** `TOP` contains an instance of `A`

### Requirement: Input operator boundary

Hefesto SHALL treat a `templayer` rule (without `copyup`) whose operator list contains `boundary` as defining the cell's fixed bounding box: for each tile of the rule's result (enumeration order of a bottom-to-top, left-to-right tile search), the tile rectangle is converted to internal units with exact conversion (which may subdivide the grid, see "Conversion of read units to internal units") and stored as the cell property `FIXED_BBOX` with value `xbot ybot xtop ytop`, and the cell is flagged as having a fixed bounding box; the last tile wins. If the cell already had a different `FIXED_BBOX`, the warning "Cell NAME boundary was redefined." is issued. In a `layer` rule (not templayer) `boundary` instead ORs the cell's existing `FIXED_BBOX`, scaled by the output style scale factor, into the region (see Q-MASK-307).

Compatibility: exact

(Magic ref: cif/CIFrdcl.c:407-425, cif/CIFrdcl.c:994-1083, cif/CIFgen.c:5601-5633)

#### Scenario: GDS boundary layer sets FIXED_BBOX
- **GIVEN** `scalefactor 100` and rule `templayer BND BOUNDARY` + `boundary`
- **AND** a GDS cell with one BOUNDARY-layer rectangle (0,0)-(500,300) RU
- **WHEN** the cell is processed
- **THEN** the cell receives property `FIXED_BBOX` = `0 0 5 3`

### Requirement: Input operator mask-hints

Hefesto SHALL treat a `templayer` rule (without `copyup`) containing `mask-hints NAME` as follows: if the rule's result is non-empty, each tile rectangle is converted to internal units with exact conversion and painted into a plane-valued cell property named `MASKHINTS_NAME`. Magic creates a fresh plane property each time (its lookup of an existing plane uses the key `NAME` instead of `MASKHINTS_NAME`), so a later templayer with the same `NAME` in the same cell replaces rather than extends the earlier hints (see Q-MASK-308). When `mask-hints NAME` is evaluated as an operator inside any rule, an existing `MASKHINTS_NAME` property of the cell is ORed into the region after scaling by the output style scale factor (output semantics, see capability `mask-output-operators`). The global GDS-read option that derives mask hints by comparing input to regenerated output is specified in capability `gds-read`.

Compatibility: exact

(Magic ref: cif/CIFrdcl.c:426-520, cif/CIFgen.c:5682-5700)

#### Scenario: Hint geometry stored as property
- **GIVEN** scalefactor 100 and rule `templayer HM POLYHINT` + `mask-hints poly`
- **AND** a cell with POLYHINT (200,0)-(400,100) RU
- **WHEN** the cell is processed
- **THEN** the cell has a plane property `MASKHINTS_poly` containing (2,0)-(4,1)

### Requirement: Input operator tagged

Hefesto SHALL implement `tagged TEXT [L1,...]` on input with the semantics of the output operator `tagged` in capability `mask-output-operators`: for every label of the cell whose text equals `TEXT`, the connected regions of the listed CIF layers (or of the current region when no list is given) that overlap the label rectangle are added to the result. On input the label rectangle (in internal units, degenerate sides widened by 1 on each side) is used directly as a search area in the read planes, which are in read units (see Q-MASK-310). Only labels already attached to the cell when the rule runs are considered.

Compatibility: exact

(Magic ref: cif/CIFgen.c:5458-5599, cif/CIFgen.c:4057-4110)

#### Scenario: Tagged region selected by label (verify against Magic)
- **GIVEN** scalefactor 1 and rule `layer m1pin tagged VDD CMF`
- **AND** a cell with label `VDD` at (5,5) and two disjoint CMF rectangles (0,0)-(10,10) and (20,0)-(30,10) RU
- **WHEN** the cell is processed
- **THEN** only (0,0)-(10,10) is painted as `m1pin`

### Requirement: Painting rule results

Hefesto SHALL paint each tile of a `layer` rule's result into the cell as type `T` on every plane on which `T` is painted, using the standard paint composition, after converting the tile's four edges from read units to internal units as follows: if `T` is a contact type, the top and right edges use half-up conversion and the bottom and left edges use half-down conversion; otherwise all edges use exact conversion. The four edges are converted in the order xtop, ytop, xbot, ybot; if a conversion subdivides the internal grid, the edges already converted are multiplied by the subdivision factor. A tile whose converted width or height is zero is skipped. Triangular tiles keep their diagonal orientation.

Compatibility: exact

(Magic ref: cif/CIFrdcl.c:1085-1160)

#### Scenario: Contact on half-lambda is expanded outward
- **GIVEN** scalefactor 100, rule `layer ndc CCA` and a CCA rectangle (50,50)-(250,250) RU
- **WHEN** the cell is processed
- **THEN** no grid subdivision occurs and `ndc` is painted at (0,0)-(3,3) IU

#### Scenario: Off-grid non-contact subdivides the grid
- **GIVEN** scalefactor 100, rule `layer m1 CMF`, no grid limit, rescaling allowed, and a CMF rectangle (0,0)-(150,100) RU
- **WHEN** the cell is processed
- **THEN** the warning "Input off lambda grid by 1/2; grid redefined." is issued once per read
- **AND** the internal grid is subdivided by 2 (all existing geometry doubled, S becomes 50) and m1 is painted at (0,0)-(3,2)

### Requirement: Conversion of read units to internal units

Hefesto SHALL convert a read-unit coordinate `c` to internal units with one of four modes — exact, half-up, half-down, any — as follows. If `c mod S == 0` the result is `c / S`. Otherwise let `g = gcd(|c|, S)`, `rem = |c mod S| / g`, `den = S / g`. If rescaling is disallowed (option `cif rescale no`, see capability `cif-format-io`) or the grid limit refuses a subdivision by `den`, the mode becomes "any". Then:

- **exact:** subdivide the internal grid by `den` (every database coordinate and every technology distance is multiplied by `den`; `S := S/den` keeping `M`, see "Scale normalization" with `reduceFully = false`; the output style is rescaled correspondingly) and return `c / S` with the new `S`. Emit "Input off lambda grid by REM/DEN; grid redefined." at most once per read.
- **half-up / half-down:** if `den > 2`, first subdivide the grid by `den/2` when `den` is even, or by `den` when odd (same warning). Then return `(c + floor(S/2)) / S` for half-up or `(c - floor(S/2)) / S` for half-down, using C integer division (truncation toward zero), with the possibly new `S`.
- **any:** return `(c + floor((S-1)/2)) / S` if `c >= 0`, or `(c - floor(S/2)) / S` if `c < 0`, with truncating division; ties round toward negative infinity. Emit "Input off lambda grid by REM/DEN; snapped to grid." at most once per read.

Compatibility: exact

(Magic ref: cif/CIFrdutils.c:208-335)

#### Scenario: Snapping rounds ties downward
- **GIVEN** `S = 100` and mode "any"
- **WHEN** coordinates 50, 51, -50 and -51 are converted
- **THEN** the results are 0, 1, -1 and -1

#### Scenario: Grid limit forces snapping
- **GIVEN** scalefactor 100, gridlimit 1000 and a non-contact rectangle edge at 150 RU
- **WHEN** the edge is converted in exact mode
- **THEN** the mode becomes "any" and the result is 1

### Requirement: Mid-read subdivision of read units

Hefesto SHALL, when an input coordinate multiplied by the file scale numerator is not divisible by the file scale denominator (see capability `cif-format-io` for CIF and `gds-read` for GDS), either subdivide the read unit or round, as follows. Let `v` be the value times the current numerator `N`, `Dn` the denominator, `r = Dn / gcd(Dn, |v|)`. If `N*r` exceeds the rescale limit (default 100, range 1..100, settable with `cif limit`) the value is rounded to nearest with ties toward positive infinity (`(v + floor(Dn/2))/Dn` for `v >= 0`, `(v - floor((Dn-1)/2))/Dn` for `v < 0`) and the warning "CIF units at maximum scale; value is rounded" is issued. Otherwise `N := N*r`, and the input style is rescaled by `r`: `S := S*r`, `M := M*r`, every nonzero operator distance `D := D*r`, and every read plane of the current cell, of the top level and of the subcell work area is scaled by `r`; the warning "CIF style NAME: units rescaled by factor of r / 1" is issued; coordinates already parsed for the current element are multiplied by `r`.

Compatibility: exact

(Magic ref: cif/CIFrdutils.c:586-660, cif/CIFrdcl.c:351-385, cif/cif.h:28)

#### Scenario: Half-unit coordinate subdivides read units
- **GIVEN** a CIF symbol `DS 1 1 2;` with style multiplier 1 (so N=1, Dn=2) and S=100
- **WHEN** the coordinate 3 is parsed
- **THEN** `r = 2`, N becomes 2, S becomes 200, M becomes 2 and the coordinate value is 3 RU in the new units

### Requirement: GDS layer and datatype mapping

Hefesto SHALL map a GDS (layer, datatype) pair to a CIF layer using the `calma` statements of the current input style. `calma C LAYERS TYPES` records the CIF layer `C` for every pair in the Cartesian product of the two lists; `*` stands for the wildcard (any value). A later statement for the same pair replaces the earlier one. Numbers must be 0..255 and consist only of digits; any other character, a value out of range, or 255 or more list entries make the whole statement an error that is ignored (see Q-MASK-326). Repeated commas are skipped. Lookup of a pair `(l, t)` SHALL try, in order: `(l, t)`, `(l, *)`, `(*, t)`, `(*, *)`; the first hit wins; no hit means "unknown layer" (handled by capability `gds-read`).

Compatibility: exact

(Magic ref: cif/CIFrdtech.c:822-854, cif/CIFrdtech.c:200-240, cif/CIFrdtech.c:1388-1430, calma/calmaInt.h:117)

#### Scenario: Wildcard precedence
- **GIVEN** statements `calma CMF 4 *`, `calma CPG 7 0`, `calma CX * 5`
- **WHEN** pairs (4,9), (7,0), (7,1), (3,5) and (7,5) are looked up
- **THEN** the results are CMF, CPG, unknown, CX and CX respectively

#### Scenario: Product of lists
- **GIVEN** `calma CMF 4,5 0,20`
- **WHEN** the style is loaded
- **THEN** pairs (4,0), (4,20), (5,0), (5,20) all map to CMF

### Requirement: Label layer statements

Hefesto SHALL record, for each CIF layer, a Magic label type and a label kind. `labels L1,L2,... [KIND]` sets, for each listed CIF layer, the label type to the current rule's type `T` and, if `KIND` is given, the kind: `text` → TEXT, `sticky` → TEXT (identical to `text`, see Q-MASK-301), `port` → PORT, any word starting with `cell` → CELLID; any other word is an error and the statement is ignored. Without `KIND` the kind is left unchanged (initially NONE). Reassigning a CIF layer already mapped to a different type is accepted; an error message is printed only when neither type can be painted on the other's plane. Under a `templayer` rule the recorded type is the templayer's CIF-layer index (see Q-MASK-311). `ignore L1,...` sets the label type of each listed CIF layer to "discard" if it has no label type yet. `options ignore-unknown-layer-labels` sets a style flag; `options no-reconnect-labels` is accepted and has no effect; any other option is an error.

Compatibility: exact

(Magic ref: cif/CIFrdtech.c:856-976, calma/calmaInt.h:141)

#### Scenario: Port labels
- **GIVEN** `layer m1 CMF` followed by `labels CMFPIN port`
- **WHEN** the style is loaded
- **THEN** CIF layer CMFPIN has label type `m1` and kind PORT

#### Scenario: ignore after labels keeps labels
- **GIVEN** `layer m1 CMF`, `labels CMF`, then `ignore CMF`
- **WHEN** the style is loaded
- **THEN** CMF labels are still attached to `m1`

### Requirement: Label placement semantics during read

Hefesto SHALL place a label read from a file on CIF layer `C` as follows (record syntax in `cif-format-io` and `gds-read`):

- label type = label type of `C`; if `C` has the "discard" type the label is dropped;
- a label with kind TEXT or PORT (or any kind other than NONE) is flagged sticky (its layer is not re-assigned by the post-read label adjustment), except that a label whose type is space is never sticky;
- kind PORT additionally makes the label a port; port indices are assigned in order of arrival, starting at the highest existing index + 1, and labels with identical text share the same index (details in `gds-read`);
- kind CELLID on a GDS text renames the containing cell to the label text instead of creating a label (details in `gds-read`);
- a GDS text whose (layer, datatype) maps to no CIF layer is placed on space with an error, unless the style flag `ignore-unknown-layer-labels` is set, in which case it is dropped silently.

After a cell is completely read, each non-sticky label is re-attached to the material under it as if the user had painted the label's layer, then the material (database label adjustment rule).

Compatibility: exact

(Magic ref: cif/CIFrdcl.c:1432-1520, calma/CalmaRdpt.c:775-800, calma/CalmaRdpt.c:1005-1110, cif/CIFrdcl.c:1163-1200)

#### Scenario: Label on an ignored layer is dropped
- **GIVEN** a style with `ignore CX` and no `labels CX`
- **WHEN** a CIF file contains `94 FOO 10 10 CX;`
- **THEN** no label is created

### Requirement: Polygon to region conversion

Hefesto SHALL convert a polygon vertex list (in read units) into a region of rectangles and right-triangle tiles as follows, for both CIF and GDS input:

1. If the last vertex differs from the first, the polygon is closed by appending the first vertex (GDS input reports "Boundary is not closed.").
2. **Manhattanization:** the polygon orientation is determined from the leftmost vertex `p2` (minimum x among successors, first found), its predecessor `p1` and successor `p3`: clockwise iff `(p2.x-p1.x)*(p3.y-p1.y) - (p2.y-p1.y)*(p3.x-p1.x) < 0` (64-bit arithmetic); degenerate collinear cases restart the search after the leading run of vertices at the minimum x. For each non-manhattan edge `a→b`, a corner vertex is inserted: with `(first,last) = (b,a)` if clockwise else `(a,b)`, the corner is `(first.x, last.y)` when the edge from `first` to `last` goes down-left or up-right, else `(last.x, first.y)`. If the axis-parallel leg from `a` to the corner crosses another polygon edge (or a manhattan edge lies strictly inside the triangle), the corner is moved to the nearest such crossing and a second vertex is inserted on the diagonal at the same coordinate (computed with 64-bit integer multiply then truncating divide). The right triangle bounded by `a`, the corner and the following vertex is painted immediately as a split tile whose bounding box is non-degenerate, with orientation derived from the edge direction, and adjacent compatible split tiles are merged.
3. **Scanline fill** of the remaining manhattan polygon: with fewer than 4 edges, report "polygon with fewer than 4 points." (when more than 0) and stop. Otherwise sort vertices by y and edges by x; for each consecutive pair of distinct vertex y values `[yb, yt]`, walk the edges in x order, summing +1 for upward vertical edges and −1 for downward vertical edges that span the whole range; a rectangle `[xstart, x] × [yb, yt]` is emitted each time the sum returns to 0 (xstart being the x of the edge where the sum left 0); zero-width rectangles are skipped. This is the nonzero winding rule.

Compatibility: exact

(Magic ref: cif/CIFrdpoly.c:234-349, cif/CIFrdutils.c:852-1236)

#### Scenario: Manhattan L-shape
- **GIVEN** the polygon (0,0) (0,20) (10,20) (10,10) (20,10) (20,0)
- **WHEN** it is converted
- **THEN** the region is the union of (0,0)-(20,10) and (0,10)-(10,20)

#### Scenario: Right triangle becomes a split tile (verify against Magic)
- **GIVEN** the polygon (0,0) (10,10) (10,0)
- **WHEN** it is converted
- **THEN** the region is a single split tile with bounding box (0,0)-(10,10) covering the half below the diagonal from (0,0) to (10,10)

#### Scenario: Self-overlapping polygon with nonzero winding
- **GIVEN** a polygon tracing the square (0,0)-(10,10) twice in the same direction
- **WHEN** it is converted
- **THEN** the region is the square (0,0)-(10,10)

### Requirement: Input-scale reporting

Hefesto SHALL report the input scale in microns per internal unit as the single-precision value `10*S / (M*1000)` for the current input style, and SHALL report an error ("No style is set") and return 0 when there is no input style.

Compatibility: behavioral

(Magic ref: cif/CIFrdtech.c:1223-1238)

#### Scenario: Microns per internal unit
- **GIVEN** an input style with `S = 5`, `M = 10`
- **WHEN** the input scale is requested
- **THEN** the value is 0.005 microns
