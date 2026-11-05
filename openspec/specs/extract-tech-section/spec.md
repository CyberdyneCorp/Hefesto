# Extract Tech Section

## Purpose

Defines the `extract` section of the technology file: extraction styles and variants, units and scale factors, the connectivity used for nodes, sheet resistance classes, every parasitic capacitance rule (area, perimeter, overlap, sidewall, sideoverlap and their `default*` shorthands), the substrate declaration, and device declarations. It also defines the post-processing that turns the parsed rules into the tables used by the extractor (contact residue expansion, plane-order checking, micron-to-lambda conversion, scaling to internal units, and the fringe-model default chosen from the tech file format).

This capability owns parsing and table construction only. How the tables are used is specified in `extract-capacitance`, `extract-resistance-lumped`, `extract-devices`, `extract-substrate` and `extract-nodes-and-labels`. The semantics of the `antenna`, `model` and `tiedown` keywords are owned by capability `antenna-check`; this capability only specifies how those lines are parsed. Type-name expressions (comma lists, `*` contact expansion, `/plane` suffixes) follow capability `tech-file-format`.

Units used below: "tech units" are the numbers written in the file. "Lambda" is the magic lambda. "Internal units" are database units after `scalegrid` (see `cmd-coordinates-and-units`). Capacitances are attofarads (aF). Resistances are milliohms (mΩ) unless stated.

## Requirements

### Requirement: Extract section line grammar and keyword matching

Each non-blank, non-comment line of the `extract` section SHALL begin with a keyword from the table below, matched case-insensitively by unique prefix (an exact match wins over prefixes). Lines whose argument count (keyword included) is outside the `[min,max]` range SHALL be rejected with `Malformed line for keyword <kw>.  Correct usage:` followed by the usage text, and the line SHALL have no effect. The `device` keyword SHALL have no maximum. An unknown keyword SHALL be reported as `Illegal keyword.  Legal keywords are:` followed by the list, and the line SHALL be ignored. Errors in this section SHALL NOT abort tech loading, except a `step` value `<= 0`.
Compatibility: exact

| keyword | min | max | arguments |
|---|---|---|---|
| `areacap` | 3 | 3 | types cap |
| `connect` | 3 | 3 | types1 types2 |
| `contact` | 3 | 6 | type [size [border [spacing]]] resistance |
| `cscale` | 2 | 2 | int |
| `defaultareacap` | 4 | 6 | types plane [[subtypes] subplane] cap |
| `defaultoverlap` | 6 | 6 | types plane othertypes otherplane cap |
| `defaultperimeter` | 4 | 6 | types plane [[subtypes] subplane] cap |
| `defaultsideoverlap` | 6 | 6 | types plane othertypes otherplane cap |
| `defaultsidewall` | 4 | 5 | types plane cap [offset] |
| `device` | 4 | — | class name types ... |
| `devresist` / `fetresist` | 4 | 4 | type region value |
| `disconnect` | 3 | 3 | types1 types2 |
| `fet` | 8 | 9 | types sdtypes nterm name [substypes] subsnode gscap [gccap] |
| `fringeshieldhalo` | 2 | 2 | distance (ignored, deprecated) |
| `height` | 4 | 4 | types height thickness |
| `antenna` | 4 | 6 | types [surface\|area\|sidewall\|perimeter] gateratio [diffA] diffB\|none |
| `model` | 2 | 3 | partial\|cumulative [surface\|area\|sidewall\|perimeter] |
| `tiedown` | 2 | 2 | types |
| `lambda` | 2 | 2 | float |
| `overlap` | 4 | 5 | toptypes bottomtypes cap [shieldtypes] |
| `perimc` | 4 | 4 | intypes outtypes cap |
| `planeorder` | 3 | 3 | plane index |
| `noplaneordering` | 1 | 1 | — |
| `resist` | 3 | 4 | types int\|None [cornerchop] |
| `rscale` | 2 | 2 | int |
| `sidehalo` | 2 | 2 | float |
| `sideoverlap` | 5 | 6 | intypes outtypes ovtypes cap [shieldtypes] |
| `sidewall` | 6 | 7 | intypes outtypes neartypes fartypes cap [offset] |
| `step` | 2 | 2 | number |
| `style` | 2 | 4 | name [variant[s] v1,v2,...] |
| `substrate` | 3 | 5 | types plane [nodename] [-shieldtypes] |
| `units` | 2 | 2 | lambda\|microns\|um |
| `variants` | 2 | 2 | v1,v2,... or `*` |

```ebnf
extract_section = "extract" , NL , { line } , "end" , NL ;
line            = keyword , { WS , arg } , NL ;
cap             = REAL ;                 (* parsed with C "%lf"; non-number -> error, value 0 *)
resistance      = INT ;                  (* parsed with C "%d" *)
types           = TYPE_EXPR ;            (* see tech-file-format *)
plane           = PLANE_NAME ;
offset          = REAL ;                 (* parsed with "%lg", stored as single-precision float *)
```

Every type mentioned in the first type argument of `areacap, connect, contact, devresist, disconnect, fet, fetresist, height, antenna, tiedown, overlap, perimc, resist, sidewall, sideoverlap, substrate`, in the third argument of `device`, and in every other type argument of the capacitance, connect and device keywords SHALL be recorded as "mentioned in the extract section" (see Requirement: Non-electrical types).

(Magic ref: extract/ExtTech.c:84-185, extract/ExtTech.c:2002-2265, utils/lookup.c:LookupStruct)

#### Scenario: Prefix keyword
- **GIVEN** an extract style containing the line `are metal1 30`
- **WHEN** the tech file is loaded
- **THEN** the line is accepted as `areacap metal1 30`

#### Scenario: Too many arguments
- **GIVEN** the line `perimc metal1 space 20 extra`
- **WHEN** the tech file is loaded
- **THEN** the error `Malformed line for keyword perimc.  Correct usage:` is reported
- **AND** no perimeter capacitance is recorded

### Requirement: Extraction styles and variants

The section SHALL be partitioned into styles by `style` lines. `style NAME` SHALL declare style NAME. `style BASE variants V1,V2,...` (third token beginning with `variant`) SHALL declare one style per variant, named by concatenating BASE and the variant text (`BASE` + `Vi`, an empty variant giving `BASE`); the first variant is that line's default. Style names SHALL be kept in declaration order. Styles SHALL be looked up by prefix match of the style name against previously declared names when deciding whether a `style` line re-declares an existing style.
Compatibility: exact

The first declared style SHALL be loaded when the technology loads. Only lines between the `style` line of the selected style and the next `style` line SHALL be applied. Within the selected style, a `variants V1,V2,...` line SHALL enable the following lines only if the current style name ends with one of the listed variant strings (trailing whitespace before each comma ignored) or the list contains `*`; otherwise the following lines SHALL be skipped until the next `variants` line or style end. If the section declares no style, a style named `default` SHALL exist with all defaults.

Selecting another style (`extract style NAME`, see `extract-commands`) SHALL re-initialize every table to defaults and re-read the extract section for that style, then re-apply scaling (Requirement: Scaling to internal units).

(Magic ref: extract/ExtTech.c:2058-2230, extract/ExtTech.c:1084-1109, extract/ExtTech.c:3556-3570)

#### Scenario: Variant style names
- **GIVEN** `style ngspice variants (),(si),(orig)`
- **WHEN** the tech file is loaded
- **THEN** the styles `ngspice()`, `ngspice(si)` and `ngspice(orig)` exist in that order, and `ngspice()` is the default style

#### Scenario: Variant-conditional lines
- **GIVEN** the current style is `ngspice(si)` and the style body contains `variants (si)` followed by `areacap m1 25` and then `variants (),(orig)` followed by `areacap m1 30`
- **WHEN** the style is loaded
- **THEN** the area capacitance of `m1` is 25

### Requirement: Style defaults

Before any line of a style is applied, the style SHALL be initialized to: step size 100, side halo 0, units-per-lambda 100.0, `rscale` 1000, `cscale` 1000, zero resistance classes, no device records, all area/perimeter/overlap/sidewall/sideoverlap capacitances zero, corner-chop 1.0 for every type, via resistance 0, height and thickness 0 (defaults applied later), plane order unset with status "planeorder required", no substrate declaration (substrate plane −1, default substrate type none, no substrate name), units = lambda, every type except space and the DRC error types "active" for extraction, and node connectivity equal to the global `connect` section table.
Compatibility: exact

(Magic ref: extract/ExtTech.c:837-978)

#### Scenario: Empty style
- **GIVEN** a style `style plain` with no further lines
- **WHEN** a cell is extracted
- **THEN** the `.ext` header contains `scale 1000 1000 100` (before grid scaling) and `resistclasses` with no values

### Requirement: Units, lambda, cscale and rscale

`units microns` (or `units um`) SHALL mark the style as written in microns; `units lambda` SHALL be the default; any other value SHALL be reported (`Units must be microns or lambda.  Using the default value (lambda).`) and ignored. `lambda F` SHALL set units-per-lambda to `atof(F)` as a single-precision float. `cscale N` SHALL set the capacitance scale to the base-10 integer prefix of N; if no digits parse, the error `Cannot parse cap scale value "<N>"` SHALL be reported and the scale set to 1. `rscale N` SHALL set the resistance scale to `atoi(N)` (no validation).
Compatibility: exact

The `.ext` header SHALL report these values (see `extract-file-format`); every capacitance written SHALL equal internal aF divided by `cscale`, and every lumped resistance SHALL equal internal mΩ divided (with rounding, see `extract-resistance-lumped`) by `rscale`.

(Magic ref: extract/ExtTech.c:2314-2321, extract/ExtTech.c:3110-3121, extract/ExtTech.c:3441-3443)

#### Scenario: Unparseable cscale
- **GIVEN** `cscale abc`
- **WHEN** the style is loaded
- **THEN** `Cannot parse cap scale value "abc"` is reported and the capacitance scale is 1

### Requirement: Step size and side halo parsing

`step V` SHALL set the step size to `(int)atof(V)` (truncation toward zero); a result `<= 0` SHALL report `Hierarchical interaction step size must be > 0` and SHALL abort the tech-file load as a fatal error. `sidehalo D` SHALL parse D as a double, multiply by 1000.0, truncate to an integer and store it (milli-units). After all lines are parsed (Requirement: Micron conversion), the stored halo SHALL be divided by 1000 with integer truncation.
Compatibility: exact

See Q-EXTRACT-19.

(Magic ref: extract/ExtTech.c:3362-3375, extract/ExtTech.c:3444-3452, extract/ExtTech.c:3986-3997)

#### Scenario: Fractional halo in lambda
- **GIVEN** `units lambda` and `sidehalo 2.5`
- **WHEN** the style is loaded with a 1:1 internal grid
- **THEN** the side halo is 2 internal units (2500/1000 truncated)

#### Scenario: Fractional step
- **GIVEN** `step 7.9`
- **WHEN** the style is loaded
- **THEN** the step size before conversion is 7

### Requirement: Plane ordering

`planeorder P N` SHALL assign order index N (`atoi`) to plane P; N SHALL be in `[0, numTechPlanes-1]` (else `Planeorder index must be [0..k]`), and an unknown plane SHALL be reported (`Unknown planeorder plane <P>`). Any `planeorder` line SHALL set the style status to "seen". `noplaneordering` SHALL set status "not needed" unless a `planeorder` was already seen (error `"noplaneordering" specified after "planeorder".`); a `planeorder` after `noplaneordering` SHALL report `"planeorder" specified after "noplaneordering".` and still apply.
Compatibility: exact

At the end of the style, unless status is "not needed", the orders of all tech planes SHALL be checked to be a permutation of `0..k`. On a conflict or missing index a warning SHALL be printed (`Warning: Extraction Style <name>` followed by `Conflicting planeorder for plane(s):` / `Missing planeorder for plane(s):`) and every plane SHALL receive the default order `planeIndex - firstTechPlane` (i.e., declaration order of planes). A higher order index means "farther from the substrate".

`defaultareacap`, `defaultperimeter`, `defaultoverlap` and `defaultsideoverlap` SHALL be rejected (`Cannot parse ... line without plane ordering!`) unless a `planeorder` line has been seen earlier in the style.

(Magic ref: extract/ExtTech.c:3496-3520, extract/ExtTech.c:3796-3847, extract/ExtTech.c:1207-1211)

#### Scenario: Missing plane order falls back to default
- **GIVEN** a tech with planes `well, active, metal1` and a style that only declares `planeorder active 1`
- **WHEN** the style is loaded
- **THEN** the warning lists `well` and `metal1` as missing and the orders become well=0, active=1, metal1=2

### Requirement: Connectivity overrides

`connect A B` SHALL add a symmetric connection between every type in A and every type in B to the node-connectivity table of the style (initialized from the global `connect` section). `disconnect A B` SHALL remove those connections symmetrically. Space MAY appear in either list.
Compatibility: exact

The resistive connectivity table SHALL be derived as: types r and s are resistively connected iff they are node-connected and belong to the same resistance class (types with no class are class −1). The device connectivity table SHALL contain, for each device type t, all types listed in the same `device`/`fet` line's type argument; non-device types SHALL connect to nothing in it.

(Magic ref: extract/ExtTech.c:2277-2289, extract/ExtTech.c:2322-2337, extract/ExtTech.c:3651-3668)

#### Scenario: Disconnect a well tap
- **GIVEN** the global connect section connects `psd` and `pwell`, and the style contains `disconnect psd pwell`
- **WHEN** nodes are extracted
- **THEN** `psd` and `pwell` shapes that touch are separate nodes

### Requirement: Resistance classes and contact resistance

Each `resist TYPES VALUE [CHOP]` line SHALL create a new resistance class (numbered from 0 in line order) with sheet resistance VALUE (integer mΩ/square) and assign every type in TYPES to it, overwriting any earlier class of those types; CHOP (default 1.0, `atof`) SHALL be stored as the corner-chop factor of those types. `VALUE` = `None` SHALL instead remove TYPES from the set of active (electrical) types and create no class. A VALUE with a decimal point SHALL be truncated to its integer part (with message `Resist argument truncated to integer value.` when the fraction is not exactly `0`); other non-integers SHALL be rejected (`Resist argument must be integer or "None".`) and create no class.
Compatibility: exact

`contact TYPES [size [border [spacing]]] R` SHALL require R to be an integer (`Contact resistivity <R> must be an integer value (in milliohms/square).`) and set the via resistance of every type in TYPES to R; the optional geometric arguments SHALL be ignored with a message (cut counts come from the `cifoutput` squares rules). Via resistance is not used by lumped extraction (it is consumed by capability `extresist-command`).

`devresist T REGION V` / `fetresist T REGION V` SHALL require numeric V and store `(int)V` (parsed with `%d`, so `12.5` stores 12) under key REGION in every device record of type T that exists at that point. Keys `area`, `perimeter`, `terminal` and `linear` have meaning (see `extract-devices`).

(Magic ref: extract/ExtTech.c:3391-3440, extract/ExtTech.c:2290-2313, extract/ExtTech.c:2946-2966, extract/ExtTech.c:1056-1066)

#### Scenario: Class numbering
- **GIVEN** the lines `resist poly 7000`, `resist metal1,m1c 80` and `resist poly 6000`
- **WHEN** the style is loaded
- **THEN** there are three classes with values 7000, 80, 6000; `poly` belongs to class 2; the header line is `resistclasses 7000 80 6000`

#### Scenario: Non-electrical marker via None
- **GIVEN** `resist nodraw None`
- **WHEN** a cell containing `nodraw` paint is extracted
- **THEN** no node is created for `nodraw` shapes

### Requirement: Area and perimeter capacitance rules

`areacap TYPES C` SHALL set the area capacitance (aF per square unit) of every non-space type in TYPES to C and set the fringe multiplier of the pair (type, space) and (space, type) to `0.02 · C` (single precision). `perimc IN OUT C` SHALL, when C ≠ 0, set the perimeter capacitance (aF per unit length) of every edge with a type of IN (excluding space) on the inside and a type of OUT (space allowed) on the outside to C; C = 0 SHALL leave the table unchanged.
Compatibility: exact

(Magic ref: extract/ExtTech.c:2267-2276, extract/ExtTech.c:3377-3390, extract/ExtTech.c:78)

#### Scenario: Fringe multiplier from areacap
- **GIVEN** `areacap metal1 30`
- **WHEN** the style is loaded in lambda units with a 1:1 grid
- **THEN** the area capacitance of metal1 is 30 aF/λ² and its substrate fringe multiplier is 0.6 per λ

### Requirement: Overlap capacitance rules

`overlap TOP BOTTOM C [SHIELD]` SHALL, for every non-contact type s in TOP and non-contact type t in BOTTOM, record overlap capacitance `C` (aF per square unit) for the ordered pair (s,t), set the fringe multiplier of (s,t) and (t,s) to `0.02·C`, and record the shielding types SHIELD and the set of their planes. A pair with s = t SHALL be rejected (`Can't have overlap capacitance between tiles of the same type (<t>)`), as SHALL a pair on the same plane, a pair for which (s,t) already has a positive value (`Only one of "overlap s t" or "overlap t s" allowed`), and a shield type on the plane of s or t. Contact types SHALL be skipped in all three lists (they are handled through residues, Requirement: Contact residues).
Compatibility: exact

(Magic ref: extract/ExtTech.c:3122-3210)

#### Scenario: Duplicate overlap pair
- **GIVEN** `overlap metal2 metal1 20` followed by `overlap metal2 metal1 25`
- **WHEN** the style is loaded
- **THEN** the second line reports `Only one of "overlap metal2 metal1" or "overlap metal1 metal2" allowed` and the value stays 20

### Requirement: Sidewall coupling rules and halving

`sidewall IN OUT NEAR FAR C [OFFSET]` SHALL, for every non-space type s in IN and every type t in OUT, append to the rule list of edge (s|t) a sidewall rule with coefficient `0.5 · C`, offset `OFFSET` (default 0, parsed as double and stored as single-precision float, in tech units), near-type set NEAR and far-type set FAR, and add FAR to the "coupling target" set of edge (s|t). Rules SHALL be prepended, so the rule list of an edge is in reverse declaration order. Space in IN SHALL be reported (`Can't have space on inside of edge [ignored]`) and dropped.
Compatibility: exact

The halving SHALL be applied for both `lambda` and `microns` units. The coefficient C is the capacitance between two facing edges of unit length at unit separation; because each facing pair is visited once from each edge (see `extract-capacitance`), the halved per-visit value restores the full value.

(Magic ref: extract/ExtTech.c:3318-3361, extract/ExtCouple.c:3091-3134, commit 9fd4ee57)

#### Scenario: Halved coefficient
- **GIVEN** `sidewall metal1 space space metal1 40 0.5`
- **WHEN** the style is loaded
- **THEN** the edge (metal1|space) has one rule with per-visit coefficient 20, offset 0.5, near {space}, far {metal1}

### Requirement: Sideoverlap rules

`sideoverlap IN OUT OV C [SHIELD]` SHALL, for every non-space, non-contact type s in IN and every non-contact type t in OUT (space allowed), prepend to the sideoverlap rule list of edge (s|t) a rule with capacitance C (aF per unit length), target set OV, shield set SHIELD and target-plane mask = planes of OV; add OV to the target-type set of edge (s|t) and the planes of OV to its target-plane mask; and OR the planes of SHIELD into the shield-plane mask of every pair (s, o) for o in OV. When OV contains space, the OV argument SHALL carry a `/plane` suffix naming the target plane (else `Must specify plane for sideoverlap to space`), and that plane SHALL be the target-plane mask. An OV plane equal to the plane of s, or (for t ≠ space) of t, SHALL abort the line with `Overlapped types in "sideoverlap" rule must be on a different plane from intypes and outtypes.`; a shield type on the plane of s or t SHALL be reported as an error.
Compatibility: exact

(Magic ref: extract/ExtTech.c:3211-3317)

#### Scenario: Sideoverlap to substrate space
- **GIVEN** `sideoverlap metal1 space space/well 15`
- **WHEN** the style is loaded
- **THEN** edge (metal1|space) has a sideoverlap rule of 15 aF/unit targeting space on plane `well`

### Requirement: Default capacitance shorthands

The `default*` keywords SHALL expand into the elementary rules using the plane order (which MUST have been declared, Requirement: Plane ordering). For a line naming plane P1 (types) and P2 (other/substrate plane), let pnum(P) be the plane order; "planes strictly between" means planes Q with `pnum(P2) < pnum(Q) < pnum(P1)`; their non-space types are the shield types and the planes are the shield planes.
Compatibility: exact

- `defaultareacap TYPES P1 [[SUBTYPES] P2] C`: area cap C for TYPES restricted to P1 (plus fringe multipliers as `areacap`). If P2 is absent, P2 SHALL be the substrate plane of the `substrate` line, if any; with neither, stop. For every non-contact s in TYPES and every type t in SUBTYPES (restricted to planes with order `<= pnum(P2)`, space excluded) with no existing positive (s,t) overlap value, record overlap C with the shields above. When the line has exactly 3 arguments after the keyword and a `substrate` line exists, the substrate types minus substrate shield types (space excluded) SHALL be added to SUBTYPES, and if the substrate has a default type, the substrate plane SHALL be a shield plane, the default type SHALL be removed from SUBTYPES and every non-substrate type of the substrate plane SHALL be a shield type.
- `defaultperimeter TYPES P1 [[SUBTYPES] P2] C`: perimeter cap C for edges (s|space) for s in TYPES∩P1 (the outside is only space). Then, with P2 (or the substrate plane) as above, for every non-contact s, a sideoverlap rule on edge (s|space) with capacitance C, target SUBTYPES (default: the substrate types, or `{space}` when there is no substrate line), target plane P2 (none if absent) and shields as computed; in the 3-argument form with a substrate default type, space SHALL be added back to the targets.
- `defaultsidewall TYPES P C [OFFSET]`: equivalent to `sidewall TYPES space space TYPES C [OFFSET]` restricted to plane P, including the halving.
- `defaultoverlap TYPES P1 OTHER P2 C`: equivalent to `overlap` with automatic shields (planes strictly between), skipping pairs that already have a positive value.
- `defaultsideoverlap TYPES P1 OTHER P2 C`: sideoverlap rules on edges (s|space) targeting OTHER on P2; P1 and P2 with equal order SHALL be rejected (`Cannot have fringing capacitance between types on the same plane`); space in TYPES or OTHER SHALL reject the line. Shield planes SHALL be the planes strictly between P1 and P2 in plane order, choosing the comparison direction by comparing the plane *indices* of P1 and P2 (see Q-EXTRACT-1).

(Magic ref: extract/ExtTech.c:1194-1347, extract/ExtTech.c:1372-1533, extract/ExtTech.c:1558-1622, extract/ExtTech.c:1647-1720, extract/ExtTech.c:1745-1872)

#### Scenario: Default perimeter creates fringe-to-substrate rule
- **GIVEN** planes ordered `well=0, active=1, metal1=2`, a line `substrate *psd,space well` and `defaultperimeter metal1 metal1 12`
- **WHEN** the style is loaded
- **THEN** edge (metal1|space) has perimeter cap 12 and a sideoverlap rule of 12 targeting the substrate types on plane `well` with the types of plane `active` as shields

### Requirement: Substrate declaration

`substrate TYPES PLANE [NODENAME] [-SHIELDS]` SHALL set the substrate types to TYPES, the substrate plane to PLANE, the optional global substrate node name to NODENAME, and the substrate shield types to SHIELDS (a last argument beginning with `-`; an unparsable list gives no shields). The default substrate type SHALL be the lowest-numbered user type in TYPES that lies on PLANE (none if no such type). Detailed semantics are in capability `extract-substrate`.
Compatibility: exact

(Magic ref: extract/ExtTech.c:3453-3495)

#### Scenario: Default substrate type
- **GIVEN** `substrate *psd,space,pwell well $SUB -dnwell`
- **WHEN** the style is loaded
- **THEN** the substrate types are psd, psd contacts, space and pwell; the default substrate type is `pwell`; the node name is `$SUB`; the shield type is `dnwell`

### Requirement: Device declaration parsing

`device CLASS NAME TYPES ARGS...` SHALL resolve CLASS by unique prefix among `mosfet, bjt, capacitor, capreverse, resistor, diode, pdiode, ndiode, subcircuit, rsubcircuit, msubcircuit, csubcircuit, dsubcircuit, veriloga` (unknown: `Illegal device.  Legal devices are:` and the line is ignored). Before positional parsing, trailing arguments SHALL be consumed from the end in this order:
Compatibility: exact

1. Parameter declarations `K=NAME`, `K=NAME*S`, `K=NAME+O`, `K=NAME-O` where K is one or two characters (more is an error) and the character before `=` is not `<` or `>`: a parameter (key K, output name NAME, scale S as `atof` default 1.0, offset `(int)(0.5 + O·1000)`, negated for `-`, only when no scale) is prepended to the device's parameter list, so the list ends up in declaration order.
2. Range limits `K>V`, `K>=V`, `K<V`, `K<=V`: V parsed as `%lg`, converted to `ival = (int)(0.5 + V·1000)`; `>` sets minimum `ival+1`, `>=` minimum `ival`, `<` maximum `ival−1`, `<=` maximum `ival`. A limit for an undeclared key SHALL create a parameter with no output name. Defaults: minimum 0, maximum −1 (min > max means "no range").
3. An identifier-layer argument `+TYPES`: the identifier types of the record.

Positional arguments by class (argument 4 is the first after TYPES):

| class | positional arguments | terminals |
|---|---|---|
| mosfet (symmetric) | SD [SUBTYPES\|None [SUBNODE [GSCAP [GCCAP]]]] | 2 (both SD) |
| mosfet (asymmetric, when argument 8 exists and is non-numeric) | SRC DRN SUBTYPES\|None SUBNODE [GSCAP [GCCAP]] | SRC, DRN in order |
| bjt | EMITTER COLLECTOR(sub types) | 1 |
| capacitor / capreverse | BOTTOM [SUBTYPES\|None [SUBNODE]] [[PERIMCAP] AREACAP] | 1 |
| resistor | TERM [SUBTYPES\|None [SUBNODE]] | 2 (both TERM) |
| diode / pdiode / ndiode | OTHER [SUBTYPES\|None [SUBNODE]] (see Q-EXTRACT-2) | 1 |
| subcircuit / msubcircuit / veriloga | [N] T1 ... TN [SUBTYPES\|None SUBNODE] | N (or all remaining) |
| rsubcircuit | TERM [SUBTYPES\|None [SUBNODE]] | 2 |
| csubcircuit / dsubcircuit | TERM [SUBTYPES\|None [SUBNODE]] | 1 |

For asymmetric mosfet, identical SRC and DRN masks SHALL make the device symmetric; overlapping but different masks SHALL report `Device mosfet <name> has overlapping drain and source types!` and remain asymmetric. For capacitor classes, a numeric last argument is the area cap (`gccap`, aF/unit²) and a numeric argument before it is the perimeter cap (`gscap`, aF/unit). For subcircuit-like classes the optional substrate pair is present iff the last argument does not parse as a type list; when N is given and exceeds the available arguments, `Not enough terminals for subcircuit, %d were required, %d found.` SHALL be reported and N reduced; two identical terminal masks SHALL collapse to one (symmetric). NAME SHALL be the model name; the reserved names `Ignore`, `Short` and `None` have the meaning given in `extract-devices`.

One device record SHALL be created per type in TYPES and SHALL be prepended to that type's record list, so the record tried first for a type is the one declared last. All records of one line SHALL share one parameter list.

The legacy `fet TYPES SD NTERM NAME [SUBTYPES] SUBNODE GSCAP [GCCAP]` SHALL create class `fet` records (area/perimeter output); argument 6 is treated as SUBTYPES iff it does not parse as a number; SUBNODE not ending in `!` (and not `error`) SHALL be warned as `Fet substrate node <n> is not a global name`.

See Q-EXTRACT-21 for fractional limits.

(Magic ref: extract/ExtTech.c:2433-2945, extract/ExtTech.c:2338-2405, extract/ExtTech.c:2484-2600)

#### Scenario: Parameter and range parsing
- **GIVEN** `device msubcircuit nfet_01v8 nfet ndiff,ndiffc ndiff,ndiffc pwell,space VSUBS l=l w=w l>=0.15 w<20`
- **WHEN** the style is loaded with `units lambda`
- **THEN** the record has terminals {ndiff,ndiffc} (collapsed to symmetric), substrate types {pwell, space}, substrate node `VSUBS`, parameters `l=l` with minimum 150/1000→0 after normalization and `w=w` with maximum (20000−1)/1000 → 19 (integer division)

#### Scenario: Capacitor values
- **GIVEN** `device capacitor None mimcap metal2 10 2`
- **WHEN** the style is loaded
- **THEN** the record has bottom terminal type metal2, no substrate, perimeter cap 10 and area cap 2

### Requirement: Contact residues and coupling table completion

After parsing, for each contact type c: c SHALL be added to the overlap-type and side-type sets of every plane c occupies; for each residue r of c, the side-edge set of c SHALL include that of r; wherever r is an overlap target of some type q, c SHALL also be; wherever r is an overlap shield for (q,t) with q and t not residues of c, c SHALL also be; wherever r is a sideoverlap target of edge (q|t) (t from space upward), c SHALL also be. Parasitic values are never stored for contacts; they are computed from the residue on the plane being examined.
Compatibility: exact

For every pair (s,t) with non-empty overlap shield planes, every shield type r SHALL be made an overlap target of s (so shields always cut s's substrate area capacitance). Finally every type set used for coupling SHALL be intersected with the active types.

(Magic ref: extract/ExtTech.c:3693-3794)

#### Scenario: Via counted as metal for overlap
- **GIVEN** `overlap metal2 metal1 20` and a contact type `via` with residues metal1 and metal2
- **WHEN** a metal2 shape overlaps a `via` tile
- **THEN** the via tile is an overlap target and its residue on the metal1 plane (metal1) is used for the value

### Requirement: Non-electrical types

At the end of the extract section, every user type that is not mentioned in any extract line (Requirement: Extract section line grammar) and does not connect (in the global connect table) to any type other than itself SHALL be removed from the active types, and the message `The following types are not handled by extraction and will be treated as non-electrical types:` followed by the type names SHALL be printed once.
Compatibility: exact (set of active types); informative (message text)

(Magic ref: extract/ExtTech.c:3588-3620)

#### Scenario: Unmentioned marker layer
- **GIVEN** a type `comment` that appears in no extract line and connects to nothing
- **WHEN** the technology is loaded
- **THEN** `comment` is listed as non-electrical and never forms a node

### Requirement: Layer height and thickness defaults

`height TYPES H T` SHALL require numeric H and T and store them as floats. After plane order is final, any type with thickness 0 SHALL get 0.05 and any type with height 0 SHALL get `0.1 · planeorder(plane(type))`.
Compatibility: behavioral (values are not written to `.ext`; consumed by 3-D view and other tools)

(Magic ref: extract/ExtTech.c:2967-2989, extract/ExtTech.c:3849-3856)

#### Scenario: Default height
- **GIVEN** no `height` line for `metal1` whose plane has order 3
- **WHEN** the style is loaded
- **THEN** metal1 height is 0.3 and thickness 0.05

### Requirement: Antenna keywords parse-level reference

The keywords `antenna`, `model` and `tiedown` SHALL be accepted with the argument counts in the keyword table and stored for capability `antenna-check`, which owns their semantics. The `antenna` diffusion ratio `none` (case-insensitive) SHALL be stored as +∞. In micron units, the proportional diffusion ratio SHALL be multiplied by `dscale²`, and on grid rescale by `(scalen/scaled)²`.
Compatibility: exact (parsing); semantics see `antenna-check`

(Magic ref: extract/ExtTech.c:2990-3109, extract/ExtTech.c:3936-3938)

#### Scenario: Antenna line accepted
- **GIVEN** `model partial surface` and `antenna metal1 surface 400 none`
- **WHEN** the style is loaded
- **THEN** both lines are accepted without error and metal1 has gate ratio 400 and an infinite diffusion ratio

### Requirement: Micron conversion

When the style declares `units microns`, after parsing, values SHALL be converted to lambda using `dscale` = microns per lambda = `10·cifScaleFactor / (cifExpander·1000)` of the current `cifoutput` style evaluated at an unscaled (1:1) lambda grid (1.0 if there is no cifoutput style), computed in single precision:
Compatibility: exact

- area caps, overlap caps and device gate-cap (`gccap`) and device perimeter cap (`gscap`, see Q-EXTRACT-3) SHALL be multiplied by `dscale²`;
- perimeter caps, sideoverlap caps and fringe multipliers SHALL be multiplied by `dscale`;
- sidewall coefficients SHALL NOT be scaled; sidewall offsets SHALL be divided by `dscale`;
- the side halo SHALL become `(int)(halo_milli / dscale + 0.5)` (still in milli-units), and the step SHALL become `(int)(step / dscale + 0.5)`;
- device parameter offsets and range limits (stored ×1000) SHALL be divided by `dscale²` for keys beginning with `a` and by `dscale` otherwise, with C integer truncation; `devresist` values under keys `area` (÷`dscale²`), `perimeter` and `linear` (÷`dscale`) SHALL be converted with integer truncation;
- heights and thicknesses SHALL be divided by `dscale`.

Then, for all units, parameter offsets and limits SHALL be divided by 1000 (integer division), and a step size `<= 0` SHALL be reset to 100 with `Warning:  zero step size!  Resetting to default.`

(Magic ref: extract/ExtTech.c:3858-3998, cif/CIFmain.c:139-146, extract/ExtTech.c:1097-1105)

#### Scenario: Micron conversion of area cap
- **GIVEN** `units microns`, a cifoutput style for which `dscale` = 0.005 µm/λ, and `areacap metal1 40` (aF/µm²)
- **WHEN** the style is loaded
- **THEN** the area cap of metal1 is 40 × 0.005² = 0.001 aF/λ²

#### Scenario: Micron halo
- **GIVEN** `units microns`, `dscale` = 0.005 and `sidehalo 2`
- **WHEN** the style is loaded
- **THEN** the side halo is (int)(2000/0.005 + 0.5) / 1000 = 400 λ

### Requirement: Scaling to internal units

After loading (and after every grid rescale, see `cmd-coordinates-and-units`), extraction values SHALL be scaled from lambda to internal units with `n/d` = the current internal-units-per-lambda ratio (scale numerator `n`, denominator `d`, applied as "multiply by d/n" to lengths): units-per-lambda SHALL be multiplied by `n/d` (float); side halo and step size SHALL be scaled as integer lengths `v·d/n` (positive values rounded toward zero, negative toward −∞); area, overlap and device caps SHALL be multiplied by `n²` and divided by `d²` (gscap included, see Q-EXTRACT-3); perimeter caps, fringe multipliers and sideoverlap caps SHALL be multiplied by `n` and divided by `d`; sidewall offsets SHALL be multiplied by `d` and divided by `n` (float); sidewall coefficients SHALL NOT be scaled; parameter offsets and limits SHALL be scaled like lengths (`a*` keys like areas) with integer truncation.
Compatibility: exact

(Magic ref: extract/ExtTech.c:4022-4130, database/DBcellsrch.c:1457-1480)

#### Scenario: Half-lambda grid
- **GIVEN** `lambda 100`, `areacap metal1 30`, `perimc metal1 space 20`, `sidehalo 4` and a grid of 2 internal units per lambda
- **WHEN** the style is scaled
- **THEN** units-per-lambda is 50, area cap is 7.5 aF/unit², perimeter cap 10 aF/unit and the halo is 8 units

### Requirement: Fringe model default chosen from tech format

When a technology is loaded (not when only the extract style changes), after the extract section is processed the `fringehalo` extraction option SHALL be set on if the tech file format version is `>= 34` and off otherwise. The format version is the integer of the `tech` section `format`/`version` line, defaulting to 27. Selecting a different extract style SHALL NOT change the option.
Compatibility: exact

(Magic ref: extract/ExtTech.c:64-68, extract/ExtTech.c:3574-3587, database/DBtech.c:78-118, utils/tech.h:40)

#### Scenario: Old format
- **GIVEN** a tech file without a `format` line
- **WHEN** it is loaded
- **THEN** `fringehalo` is off

#### Scenario: Style change keeps user choice
- **GIVEN** a format-35 tech is loaded and the user runs `extract no fringehalo`
- **WHEN** the user runs `extract style <other>`
- **THEN** `fringehalo` stays off
