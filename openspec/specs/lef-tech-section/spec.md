# LEF Tech Section

## Purpose

Defines the `lef` section of the technology file: the table that maps LEF/DEF layer, cut and via names to magic tile types, the per-entry LEF class, the obstruction types, and the default routing and contact parameters derived from the DRC rules. This table (the "LEF layer table") is the shared state consulted by `lef read`, `def read`, `lef write` and `def write` (capabilities `lef-read`, `def-read`, `lef-write`, `def-write`). LEF files read later can add entries to it and change their parameters (see `lef-read`).

## Requirements

### Requirement: Lef section line grammar

Each line of the `lef` section SHALL have the form `<keyword> <magic-type> <lefname> [<lefname> ...]`, except that the `ignore` keyword SHALL take only LEF names (`ignore <lefname> [<lefname> ...]`).
Compatibility: exact

```ebnf
lef_section   = "lef" , NEWLINE , { lef_line } , "end" , NEWLINE ;
lef_line      = mapped_line | ignore_line ;
mapped_line   = keyword , WS , magic_types , WS , lefname , { WS , lefname } , NEWLINE ;
ignore_line   = ignore_kw , WS , lefname , { WS , lefname } , NEWLINE ;
keyword       = "obstruction" | "layer" | "route" | "routing" | "masterslice"
              | "cut" | "contact" | "overlap" | "bound" ;   (* matched as below *)
ignore_kw     = "ignore" ;
magic_types   = TYPE_NAME_OR_LIST ;   (* any magic type-name expression, e.g. "m1" or "m2c,m2" *)
lefname       = ? any whitespace-free token; case-sensitive ? ;
```

Keyword matching SHALL be case-insensitive and SHALL accept any unique prefix of a keyword; an exact match SHALL win over prefix matches (so `route` selects `route`, `rout` is ambiguous and is rejected, `ob` selects `obstruction`, `ig` selects `ignore`). An unrecognized or ambiguous keyword SHALL cause the line to be ignored with the error `Unknown LEF section keyword: <kw>.  Line ignored.`. A non-`ignore` line with fewer than 3 fields SHALL be ignored with the error `No LEF layer names present!`. Errors in this section SHALL be non-fatal: tech loading continues.

Annotated example:

```
lef
    masterslice ndiff  diffusion active      # LEF names "diffusion","active" -> ndiff
    routing     m1     m1 metal1 METAL1      # three LEF names share one entry
    layer       m2c    via1 VIA12            # m2c is a contact type -> treated as "cut"
    contact     pc     cont1                 # cut whose RECTs are resized to magic contact size
    obstruction m1     OBS1                  # OBS1 geometry paints m1 as obstruction
    obstruction obsv1,m1 via1                # via1 obstruction: primary = lower-numbered type, other = secondary
    bound       comment OVERLAP_BOUND
    ignore      PWELL NWELL                  # recognized, never painted
end
```

(Magic ref: lef/lefTech.c:205-232, utils/lookup.c:Lookup)

#### Scenario: Ambiguous keyword prefix
- **GIVEN** a `lef` section line `rout m1 metal1`
- **WHEN** the technology file is loaded
- **THEN** the line is ignored with error `Unknown LEF section keyword: rout.  Line ignored.`
- **AND** no LEF name `metal1` is defined

#### Scenario: Missing LEF names
- **GIVEN** a line `routing m1`
- **WHEN** the technology file is loaded
- **THEN** the error `No LEF layer names present!` is reported and the line is ignored

### Requirement: Magic type resolution in lef lines

For non-`ignore` lines, the second field SHALL be resolved to a set of user tile types using the tech type-name expression rules. Let T be the set of resolved types ordered by ascending type number. The line SHALL be rejected when |T| = 0 (error `Bad magic layer type "<name>" in LEF layer definition.`), when |T| = 2 and the keyword is not `obstruction` (error `Can only define multiple types for via obstruction layers.`), or when |T| > 2 (error `Too many types in LEF layer definition.`). The first type of T SHALL be the primary type; for an `obstruction` line with |T| = 2 the second type SHALL be the secondary via-obstruction type; otherwise the secondary type is "none".
Compatibility: exact

(Magic ref: lef/lefTech.c:247-279)

#### Scenario: Two types on an obstruction line
- **GIVEN** types `obsv1` (number 40) and `m1` (number 12) and the line `obstruction obsv1,m1 via1`
- **WHEN** the line is processed
- **THEN** the primary type is `m1` (lower type number) and the secondary via-obstruction type is `obsv1`

#### Scenario: Two types on a routing line
- **GIVEN** the line `routing m1,m2 metal1`
- **WHEN** the line is processed
- **THEN** it is rejected with `Can only define multiple types for via obstruction layers.`

### Requirement: Keyword to LEF class mapping

Each LEF table entry SHALL have exactly one class: ROUTE, VIA (cut), MASTERSLICE, OVERLAP, BOUND or IGNORE. When a line creates a new entry, the class SHALL be determined from the keyword and from whether the primary type P is a contact type:

| keyword | contact P | non-contact P |
|---|---|---|
| `layer` | VIA (as `cut`) | ROUTE (as `route`) |
| `route`, `routing` | ROUTE (error reported, see below) | ROUTE |
| `cut` | VIA, "cut" flavor | VIA, "cut" flavor (error reported) |
| `contact` | VIA, "contact" flavor | VIA, "contact" flavor (error reported) |
| `obstruction` | VIA, "contact" flavor | ROUTE |
| `masterslice` | MASTERSLICE (error reported) | MASTERSLICE |
| `overlap` | OVERLAP (error reported) | OVERLAP |
| `bound` | BOUND (error reported) | BOUND |
| `ignore` | IGNORE | IGNORE |

The consistency errors SHALL be reported but the entry SHALL still be created as shown: a contact type used with any keyword other than `cut`, `contact`, `obstruction` or `layer` reports `Attempt to define cut type <type> as <keyword>.`; a non-contact type used with `cut` or `contact` reports `Attempt to define non-cut type <type> as a cut.`.

For a non-`obstruction` line the entry's (normal) type SHALL be P and its obstruction type SHALL be none. For an `obstruction` line the entry's normal type SHALL be none and its obstruction type SHALL be P; for a VIA-class entry the secondary via-obstruction type SHALL be recorded as well. An `ignore` entry SHALL have neither type.
Compatibility: exact

(Magic ref: lef/lefTech.c:234-245, 281-381)

#### Scenario: layer keyword on a contact type
- **GIVEN** `m2c` is a contact type
- **WHEN** the line `layer m2c via1` is processed
- **THEN** LEF name `via1` maps to an entry of class VIA with type `m2c`, cut flavor

#### Scenario: Obstruction of a routing type
- **WHEN** the line `obstruction m1 OBS_M1` is processed
- **THEN** `OBS_M1` maps to a ROUTE-class entry with no normal type and obstruction type `m1`

#### Scenario: Contact type declared as routing
- **GIVEN** `m2c` is a contact type
- **WHEN** the line `routing m2c via1` is processed
- **THEN** the error `Attempt to define cut type m2c as routing.` is reported
- **AND** `via1` is still defined as a ROUTE-class entry of type `m2c`

### Requirement: LEF name aliasing and canonical names

All LEF names on one line that are not yet in the table SHALL be bound to one shared entry, created when the first such name is encountered; that first new name SHALL be the entry's canonical name (the name used when writing LEF/DEF). LEF names SHALL be case-sensitive keys. For each LEF name already present in the table whose entry is not of class IGNORE:
- if the line is an `obstruction` line and the existing entry has no obstruction type, the existing entry's obstruction type SHALL become P (and, for a VIA-class entry, its secondary via-obstruction type SHALL become this line's secondary type);
- else if the line is not an `obstruction` line and the existing entry has no normal type, its normal type SHALL become P;
- otherwise the error `LEF name <name> already used for magic type <type>` SHALL be reported and the name is left unchanged.
A name already bound to an IGNORE entry SHALL be silently left unchanged. Existing entries SHALL never change class through a later line.
Compatibility: exact

(Magic ref: lef/lefTech.c:292-401)

#### Scenario: Routing layer plus obstruction on the same LEF name
- **GIVEN** the lines `routing m1 metal1` then `obstruction obsm1 metal1`
- **WHEN** both are processed
- **THEN** `metal1` has normal type `m1` and obstruction type `obsm1`, and its canonical name is `metal1`

#### Scenario: Duplicate mapping
- **GIVEN** the lines `routing m1 metal1` then `routing m2 metal1`
- **WHEN** both are processed
- **THEN** the error `LEF name metal1 already used for magic type m1` is reported and `metal1` still maps to `m1`

#### Scenario: Canonical name is the first new name
- **GIVEN** `metal1` is already defined and the line `routing m1 metal1 M1 MET1` follows
- **WHEN** the line is processed
- **THEN** `M1` and `MET1` share a new entry whose canonical name is `M1`

### Requirement: Default routing parameters

A new ROUTE-class entry SHALL be given a width, a spacing, a pitch of 0 and a preferred direction of horizontal. These values are in magic internal units. The initial width SHALL be `3` if the default DRC width of P is 0, and otherwise the sentinel `-1` ("to be filled"). The initial spacing SHALL be `4` if the default DRC spacing of P to itself is 0, and otherwise the sentinel `-1`. The DRC values SHALL be taken from the current DRC style at the time the line is processed:
- default width of type t: scanning the rules for edges from space to t in rule order, ignoring each "trigger" rule together with the rule immediately following it, ignoring area rules, and considering only forward rules that are not max-width or split-tile rules, whose OK-type set contains t and equals the corner-type set, whose plane is t's plane, and whose distance equals the corner distance; the distance of the last such rule, or 0 if none;
- default spacing of t to u: scanning the rules for edges from t to space in rule order with the same trigger/area exclusions, considering only forward rules whose OK-type set does not contain u, whose plane is one of u's planes and whose distance equals the corner distance; the distance of the last such rule, or 0 if none.
Compatibility: exact

(Magic ref: lef/lefTech.c:350-368, lef/lefInt.h:20-21, drc/DRCtech.c:4620-4722)

#### Scenario: Layer without DRC rules
- **GIVEN** type `m5` has no width and no spacing rules in the current DRC style
- **WHEN** the line `routing m5 metal5` is processed
- **THEN** the entry for `metal5` has width 3, spacing 4, pitch 0 and horizontal direction

### Requirement: Default contact area

A new VIA-class entry created with the "cut" flavor SHALL have an empty (null) contact area. A new VIA-class entry created with the "contact" flavor (keywords `contact` and `obstruction` on a contact type) SHALL have the sentinel contact area (-1, -1, -1, -1). Contact areas are stored in half internal units (one stored unit = 0.5 internal unit), centered on the origin. A contact area is "null" when xbot ≥ xtop or ybot ≥ ytop; the sentinel area is therefore null. The meaning of a non-null contact area for reading is defined in capability `lef-read`.
Compatibility: exact

(Magic ref: lef/lefTech.c:333-349, lef/lefInt.h:55-64)

#### Scenario: Cut flavor
- **WHEN** the line `cut m2c via1` is processed
- **THEN** the entry for `via1` has a null contact area and no secondary via-obstruction type

### Requirement: Filling defaults after grid scaling

Route width and spacing and the contact area of a contact-flavor VIA entry SHALL be filled from the DRC defaults according to the `magic-compat` setting (Q-LEFDEF-1, resolution: compat; Q-LEFDEF-2, resolution: compat). Hefesto keeps an "unset" flag on these values instead of a numeric sentinel; the sentinel values below describe what Magic stores and are reproduced with `magic-compat` ON.

With `magic-compat` ON (the default), as in Magic: when the technology is loaded with an internal grid that is not 1:1 with lambda (internal units per lambda = n/d with (n, d) ≠ (1, 1)), after the LEF table has been scaled by n/d (see "Scaling the LEF table") the system SHALL fill sentinels as follows, for every entry:
- VIA class: if all four contact-area coordinates equal exactly -1, the area SHALL become (-w, -w, w, w) where w is the default DRC width of the entry's normal type (now in internal units); the stored area is in half units, so the contact square has side w internal units.
- ROUTE class: if the width equals exactly -1, the width SHALL become the default DRC width of the entry's normal type; if the spacing equals exactly -1, the width (not the spacing) SHALL be overwritten with the default DRC spacing of the type to itself, and the spacing SHALL remain -1.

When the grid is 1:1 this fill step SHALL NOT run, so the sentinels remain. Scaling truncates toward zero for route values and floors for contact areas, and is applied before the fill. A route sentinel -1 therefore stays -1 (and is filled) for d < n < 2d, becomes -2 or less for n ≥ 2d and 0 for n < d, and in those two cases is not filled. A contact-area sentinel stays -1 and is filled only for n < d. Because the spacing is still -1 at the fill only for d < n < 2d, the width overwrite by the default spacing occurs only for those grid ratios. `scalegrid` and the rescaling done to read CIF or a finer-grid file SHALL NOT refill. An unfilled contact-flavor entry behaves like a `cut` entry (no cut resizing on read).

With `magic-compat` OFF: after every tech load and every grid change (`scalegrid` included), an unset route width SHALL become the default DRC width of the entry's normal type, an unset route spacing SHALL become the default DRC spacing of the type to itself (the width keeps its own default), and an unset contact area SHALL become (-w, -w, w, w) as above, whatever the grid ratio.

In both modes, `def read` replaces a non-positive layer width by the DRC default width (capability `def-read`).
Compatibility: exact

(Magic ref: lef/lefTech.c:340-366, 416-462, 477-516, utils/tech.c:817-830, commands/CmdRS.c:339)

#### Scenario: 1:1 grid keeps sentinels
- **GIVEN** a technology with a 1:1 internal grid and a DRC width rule of 3 for `m1`
- **WHEN** the line `routing m1 metal1` is processed and the tech load completes
- **THEN** with `magic-compat` ON the entry for `metal1` has width -1 and spacing -1 (unless a LEF LAYER section later sets them)
- **AND** with `magic-compat` OFF the width is 3 and the spacing is the DRC default spacing of `m1` to itself

#### Scenario: Grid 2:1 leaves scaled sentinel (verify against Magic)
- **GIVEN** a technology whose internal grid is 2 units per lambda and a DRC width rule for `m1`
- **WHEN** the tech load completes
- **THEN** with `magic-compat` ON the `metal1` width is -2 (scaled sentinel) and is not replaced by the DRC width
- **AND** with `magic-compat` OFF the width is the DRC default width of `m1`

#### Scenario: Grid 3:2 fills the route width with the default spacing
- **GIVEN** a technology whose internal grid is 3 units per 2 lambda, the line `routing m1 metal1`, and DRC default width 3 and default spacing 4 for `m1` after scaling
- **WHEN** the tech load completes
- **THEN** with `magic-compat` ON the scaled sentinels stay -1, the width becomes 3 and is then overwritten by 4, and the spacing stays -1
- **AND** with `magic-compat` OFF the width is 3 and the spacing is 4

#### Scenario: Grid 1:2 fills the contact area
- **GIVEN** a grid of 1 internal unit per 2 lambda, a `contact pc cont1` line, and a default DRC width of 2 (internal units) for `pc` after scaling
- **WHEN** the tech load completes
- **THEN** the contact area becomes floor-scaled (-1,-1,-1,-1), which stays (-1,-1,-1,-1), and is then filled to (-2,-2,2,2) half units, a 2 x 2 internal-unit square

### Requirement: Scaling the LEF table

Whenever magic's internal grid is rescaled by a factor m/k (on tech load with a non-1:1 grid, on an explicit grid rescale, and when the database is rescaled to read a file written at a finer grid), every entry SHALL be scaled exactly once (entries shared by several names are scaled once):
- VIA class: each contact-area coordinate v becomes floor(v·m / k), with floor rounding toward negative infinity for negative products.
- ROUTE class: width, spacing and pitch each become trunc(trunc(v·m) / k), using C integer division (truncation toward zero).
- Other classes are unchanged.
Compatibility: exact

(Magic ref: lef/lefTech.c:415-462, database/DBcellsrch.c:1457-1480, utils/tech.c:826-829, commands/CmdRS.c:339, database/DBio.c:804, cif/CIFrdutils.c:249,284)

#### Scenario: Width scaling truncates
- **GIVEN** a ROUTE entry with width 5
- **WHEN** the grid is rescaled by 1/2
- **THEN** the width becomes 2

#### Scenario: Negative contact coordinate floors
- **GIVEN** a VIA entry with contact area (-3, -3, 3, 3)
- **WHEN** the grid is rescaled by 1/2
- **THEN** the area becomes (-2, -2, 1, 1)

### Requirement: LEF table lifecycle

The LEF layer table and the non-default rule table SHALL be cleared at the start of each technology load, before the `lef` section is read. If no technology load has initialized them, `lef read` and `def read` SHALL initialize empty tables and use fallback name matching (see `lef-read`). Entries added by reading LEF or DEF files (VIA, VIARULE, LAYER and NONDEFAULTRULE sections) SHALL persist until the next technology load. Temporary via entries created while writing DEF SHALL be removed at the end of that write (see `def-write`), leaving their names in the table with no entry; such a name SHALL be treated as absent.
Compatibility: behavioral

(Magic ref: lef/lefTech.c:72-175, lef/lefRead.c:2999-3001, lef/defRead.c:2517, lef/defWrite.c:1752)

#### Scenario: Reload clears LEF-defined vias
- **GIVEN** a LEF file defining `VIA M1M2_PR` has been read
- **WHEN** the technology is reloaded
- **THEN** `M1M2_PR` is no longer in the LEF layer table

### Requirement: Use of entry classes by readers and writers

The class and types of an entry SHALL determine LEF/DEF semantics as follows; the details are specified in the referenced capabilities:
- ROUTE: routing layer; its normal type is painted for pin, route and via geometry, and its obstruction type (if any) for OBS geometry; width, spacing, pitch and direction are the default routing parameters (`lef-read`, `def-read`, `lef-write`, `def-write`).
- VIA: cut layer or via definition; the normal type is the cut type and the contact area may force the cut size (`lef-read`).
- MASTERSLICE: written as a masterslice layer and optionally included in LEF output (`lef-write`).
- OVERLAP: written as an overlap layer (`lef-write`).
- BOUND: excluded from technology LEF output and used for the cell boundary (`lef-write`).
- IGNORE: the name is recognized on input and produces no geometry and no error.
Compatibility: behavioral

(Magic ref: lef/lefWrite.c:406-440, 1249-1280, lef/lefRead.c:697-765)

#### Scenario: Ignored layer in a macro
- **GIVEN** the line `ignore NWELL` and a LEF macro OBS containing `LAYER NWELL ; RECT 0 0 1 1 ;`
- **WHEN** the LEF file is read
- **THEN** nothing is painted for that RECT and no error is reported for the layer name
