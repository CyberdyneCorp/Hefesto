# DRC Rule Language

## Purpose

Defines the `drc` section of a Magic-compatible technology file: its grammar, the style and variant mechanism, the scale factor, and every rule keyword with its parameters. For each keyword it gives the exact set of *primitive edge rules* (see capability `drc-rule-semantics`) that the keyword compiles into, the halo contribution, the ordering of rules in the rule table, and the post-load rule optimizations. With the `magic-compat` setting ON (the default) Hefesto compiles the same tech file into a rule table that produces the same DRC errors and the same why-texts as Magic 8.3.694 (fc5daa9e). Where an approved decision resolves a Magic quirk as *compat* (replicate under magic-compat, fix with it off) the requirement states both modes; quirks resolved as *replicate* hold in both modes. Tech-file lint warnings named below are Hefesto diagnostics in both modes and never change Magic's console output.

Terminology used throughout:

- *Type set*: a tile-type list parsed with the standard tech-file type-list syntax (commas, `~` complement, `/plane` qualifier, `*` contact expansion, `0` for the empty set; see capability `tech-file-format`). `~S` is the complement of S over all types (including space).
- *Plane mask of a set*: the set of planes on which **every** type of the set exists (intersection of the per-type plane sets). A set is *single-plane* if this mask is non-empty. `plane(S)` is the lowest-numbered plane of the mask.
- *PT(p)*: all types that exist on plane p (including space).
- *Edge pair (A,B)*: an ordered pair of distinct types that share a plane; A is the type on the left of a vertical edge or below a horizontal edge, B the type on the right/above. Rules are stored per edge pair.
- *Primitive rule* fields: `dist`, `cdist`, `OK` (allowed types), `CORNER` (corner-trigger types), `flags` (FORWARD/REVERSE, BOTHCORNERS, OUTSIDE, TRIGGER, AREA, MAXWIDTH, BENDS, MAXWIDTH_BOTH, RECTSIZE, ANGLES_45, ANGLES_90, SPLITTILE, RUNLENGTH, MANHATTAN, DERIVED_EDGE, DERIVED_TARGET, CIFRULE), `edgeplane`, `checkplane`, `why` (index into the why-text table), `exception` (exception/exemption index or none).
- A rule written "R[A][B] FWD d OK CORNER c" means: for every edge pair (A,B) matching the stated condition, insert a primitive rule with those fields.

## Requirements

### Requirement: DRC section grammar
The `drc` section SHALL be parsed line by line as whitespace-separated tokens (quoting and line continuation as in capability `tech-file-format`), where the first token selects a statement and keywords are matched case-insensitively by unique prefix (an exact match wins over prefix matches; an ambiguous or unknown keyword is reported as `Bad DRC rule type "<kw>"` and the line is ignored).
Compatibility: exact

```ebnf
drc_section   = "drc" , NL , { drc_line } , "end" ;
drc_line      = style_line | scale_line | variant_line | rule_line ;
style_line    = "style" , name , [ "variant" , variant_list ] ;
variant_list  = name_suffix , { "," , name_suffix } ;          (* no spaces *)
scale_line    = "scalefactor" , number , [ number ] ;
variant_line  = "variant" , ( "*" | variant_list ) ;
rule_line     = width | spacing | widespacing | edge | edge4way | extend
              | overhang | surround | rect_only | rectangle | no_overlap
              | exact_overlap | area | maxwidth | off_grid | angles
              | exception | exemption | option | stepsize
              | cifstyle | cifwidth | cifspacing | cifarea | cifmaxwidth ;
width         = "width" , types , int , [ "angles" ] , why ;
spacing       = "spacing" , types , types , int ,
                ( adjacency | "corner_ok" , types ) , why ;
adjacency     = "touching_ok" | "touching_illegal" | "surround_ok" | "manhattan_dist" ;
widespacing   = "widespacing" , types , int , [ int ] , types , int , adjacency , why ;
edge          = ( "edge" | "edge4way" ) , types , types , int , types , types , int ,
                [ "manhattan_dist" ] , why , [ plane ] ;
extend        = "extend" , types , types , int , [ "exact_width" | "exclusive" ] , why ;
overhang      = "overhang" , types , types , int , why ;
surround      = "surround" , types , types , int , [ int ] , presence , why ;
presence      = "absence_ok" | "absence_illegal" | "exact_width" | "directional" ;
rect_only     = "rect_only" , types , why ;
rectangle     = "rectangle" , types , int , ( "any" | "even" | "odd" ) , why ;
no_overlap    = "no_overlap" , types , types ;
exact_overlap = "exact_overlap" , types ;
area          = "area" , types , int , int , why ;
maxwidth      = "maxwidth" , types , int , [ bends , [ types ] ] , why ;
bends         = "bend_illegal" | "bend_ok" | "both" ;
off_grid      = "off_grid" , types , int , why ;
angles        = "angles" , types , ( "45" | "90" | "45" , sep , "only" ) , why ;
exception     = "exception" , ( name | "none" ) ;
exemption     = "exemption" , ( name | "none" ) ;
option        = "option" , "wide-width-noninclusive" ;
stepsize      = "stepsize" , int ;
cifstyle      = "cifstyle" , name ;                      (* see drc-cif-rules *)
cifwidth      = "cifwidth" , ciflayer , int , why ;
cifspacing    = "cifspacing" , ciflayer , ciflayer , int ,
                ( "touching_ok" | "touching_illegal" ) , why ;
cifarea       = "cifarea" , ciflayer , int , int , why ;
cifmaxwidth   = "cifmaxwidth" , ciflayer , int , bends , why ;
why           = quoted_string ;
```

Token counts (keyword included) are validated before dispatch: angles 4; edge/edge4way 8–10; exact_overlap 2; exception/exemption 2; extend 5–6; no_overlap 3; option 2; overhang 5; rect_only 3; spacing 6–7; stepsize 2; surround 6–7; width 4–5; widespacing 7–8; area 5; off_grid 4; maxwidth 4–6; cifstyle 2; cifwidth 4; cifspacing 6; cifarea 5; cifmaxwidth 5; rectangle 5. A count outside the range prints `Rule type "<kw>" usage: <kw> <usage>` and the line is ignored. Integer parameters are parsed like C `atoi` (leading integer prefix, 0 if none). Errors never stop reading the section. A rule line of the style being loaded whose distance is negative keeps the rules it added but is a rejected line: `tech load` lists `drc` among the sections that contained errors and the load fails (capability `tech-file-format`; the previous technology stays in effect).

Annotated example:

```
drc
 style default                 # first style = default
 scalefactor 10                # distances below are in 1/10 lambda
 width      poly 20 "poly width < %d"                    # 2 lambda
 spacing    poly poly 20 touching_ok "poly spacing < %d"
 spacing    poly ndiff,pdiff 10 corner_ok nfet,pfet "poly-diff spacing"
 widespacing m1 100 m1 30 touching_ok "wide m1 spacing < %d"
 surround   m1c m1 10 absence_illegal "m1 must surround contact by %d"
 area       m1 400 20 "m1 area < %a"
 exemption  SRAM
 spacing    m1 m1 25 touching_ok "m1 spacing < %d"       # not checked inside MASKHINTS_SRAM
 exemption  none
 exact_overlap m1c
end
```

(Magic ref: drc/DRCtech.c:760, drc/DRCtech.c:1093, utils/lookup.c)

#### Scenario: Unique prefix keyword
- **GIVEN** a drc line `spac poly poly 2 touching_ok "x"`
- **WHEN** the section is read
- **THEN** it is compiled exactly as `spacing poly poly 2 touching_ok "x"`

#### Scenario: Ambiguous keyword rejected
- **GIVEN** a drc line `wid poly 2 "x"` (prefix of both `width` and `widespacing`)
- **WHEN** the section is read
- **THEN** the error `Bad DRC rule type "wid"` is reported, the list of valid rule types is printed, and no rule is added

#### Scenario: Wrong token count
- **GIVEN** the line `width poly "x"`
- **WHEN** the section is read
- **THEN** the message `Rule type "width" usage: width layers width why` is reported and no rule is added

#### Scenario: Negative distance fails the technology load
- **GIVEN** the default style contains `width m1 -2 "w"`
- **WHEN** `tech load` reads the file
- **THEN** the line keeps its rules, `The following sections of <file> contained errors:` is printed followed by `    drc`
- **AND** the load fails with `Error in loading technology file` and the previous technology stays in effect

### Requirement: DRC styles and variants
The drc section SHALL support multiple named styles; only the rules of the currently selected style are loaded, with the following selection rules.
Compatibility: exact

- `style NAME` declares style NAME. `style NAME variant v1,v2,...` declares one style per variant named by plain concatenation `NAME`+`vi` (e.g. `style sky130 variant (fast),(full)` declares `sky130(fast)` and `sky130(full)`).
- The style list is kept in declaration order; the first declared style is the default.
- Rule lines encountered before any `style` line create an implicit style named `default`.
- When a style is being loaded, a `variant` line restricts the following rules (until the next `variant` line, `style` line or end of section) to styles whose name ends with one of the comma-separated suffixes (spaces before a comma are trimmed); `variant *` re-enables all variants.
- When the style list contains exactly one style, that style is always loaded (also on reload).
- A rule line outside the selected style is ignored silently.
- If the section is empty, a style named `default` with no rules exists.

(Magic ref: drc/DRCtech.c:760-1010, drc/DRCtech.c:4030)

#### Scenario: Variant selection
- **GIVEN** `style s variant A,B` followed by `width m1 3 "w"`, `variant B`, `width m2 4 "w2"`, `variant *`, `width m3 5 "w3"`
- **WHEN** style `sA` is loaded
- **THEN** the m1 and m3 width rules are present and the m2 rule is absent
- **AND** when style `sB` is loaded all three rules are present

#### Scenario: Implicit default style
- **GIVEN** a drc section whose first line is `width poly 2 "w"`
- **WHEN** the tech file is loaded
- **THEN** a single style named `default` exists and contains the poly width rule

### Requirement: Scale factor and distance units
Rule distances SHALL be converted from tech-file DRC units to internal database units by the factor D/N, where `scalefactor N [D]` (default N=D=1) appears in the style, combined with the current internal-grid scaling, using ceiling rounding for distances and floor rounding for maxwidth distances.
Compatibility: exact

- `scalefactor` values are parsed as floating point and truncated to integers; N≤0 or D≤0 resets both to 1 with an error.
- Effective ratio: N' = N·λn, D' = D·λd where the internal grid is λn/λd lambda per internal unit (initially 1/1; changed by `scalegrid`, by CIF/GDS input requiring a finer grid, or by `.mag` files with a different scale — see capability `cmd-coordinates-and-units`). N' and D' are reduced by their greatest common factor.
- For every primitive rule with `dist > 0`: `dist_int = floor(dist·D'/N')` if the remainder is 0 or the rule has the MAXWIDTH flag; otherwise `floor(dist·D'/N') + 1` (ceiling).
- For every primitive rule with `cdist > 0`: ceiling of `cdist·D'/N'`; for AREA rules the value is an area and uses `(D'/N')²` with ceiling.
- Values ≤ 0 are not scaled (e.g. the `-1` "any" parity of `rectangle`).
- The halo is `ceil(halo_file·D'/N')` (see "Halo and step size"). CIF-layer rules (capability `drc-cif-rules`) are not scaled by D'/N'.
- On a later grid rescale (`scalegrid`, or reading a `.mag`/GDS/CIF file on a finer grid) the result depends on the `magic-compat` setting (see Q-DRC-1, resolution: replicate under magic-compat, fix with it off):
  - With it ON (the default), Magic's method is reproduced: each rule is first restored from its compiled value using a remainder stored per rule in one byte, then rescaled. The restore subtracts 1 when the stored remainder is non-zero, multiplies back by the reduced factor N and adds the remainder. A remainder of N > 256 wraps modulo 256, and one that wraps to 0 also skips the subtract-1. For AREA rules the stored value is `(area mod N²) / N` (integer division), so the restored area is `area − area mod N`, except that when `0 < area mod N² < N` the stored 0 skips the subtract-1 and the restored area is `ceil(area / N²)·N²`. With N = 1 nothing is stored and rescaling is exact.
  - With it OFF, every distance is recomputed from its tech-file value and the current D'/N' with the rounding rules above.

(Magic ref: drc/DRCtech.c:4068-4204, drc/DRCtech.c:4214-4230, drc/DRCtech.c:4550-4600)

#### Scenario: Ceiling rounding of spacing
- **GIVEN** `scalefactor 10` and `spacing m1 m1 25 touching_ok "s"` with internal grid = 1 lambda
- **WHEN** the style is loaded
- **THEN** the compiled spacing rules have dist 3 and cdist 3 (25/10 = 2.5 rounded up)

#### Scenario: Floor rounding of maxwidth
- **GIVEN** `scalefactor 10` and `maxwidth m1 25 bend_illegal "mw"`
- **WHEN** the style is loaded
- **THEN** the compiled maxwidth rule has dist 2 (floor) and cdist 3 (ceiling)

#### Scenario: Area scaling
- **GIVEN** `scalefactor 10` and `area m1 1050 20 "a"`
- **WHEN** the style is loaded
- **THEN** the compiled rule has dist (horizon) 2 and cdist (area) 11 internal units² (1050/100 = 10.5 rounded up)

#### Scenario: Finer internal grid
- **GIVEN** no scalefactor, `width poly 2 "w"`, and the internal grid rescaled to 1/2 lambda (`scalegrid 1 2`)
- **WHEN** rules are rescaled
- **THEN** the poly width rule has dist 4 and cdist 4 internal units

#### Scenario: Area remainder lost on rescale with magic-compat ON
- **GIVEN** `magic-compat` ON, `scalefactor 10` and `area m1 1001 2 "a"` (compiled area ⌈1001/100⌉ = 11 internal units²)
- **WHEN** `scalegrid 1 2` rescales the rules (reduced factor 5)
- **THEN** the area is restored as 1100 (stored remainder 0) and recompiled as 1100/25 = 44 internal units² (golden DRC-SCALE-3)

#### Scenario: Area recomputed on rescale with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the rule and rescale of the previous scenario
- **WHEN** the rules are rescaled
- **THEN** the area is ⌈1001/25⌉ = 41 internal units²

### Requirement: Why-text table
Each rule's error explanation SHALL be stored in a per-style why-text table indexed from 1, where identical strings share one index and indices 1–5 are predefined.
Compatibility: exact

Predefined entries, in order:
1. `This layer can't abut or partially overlap between array elements`
2. `Can't overlap those layers`
3. `This layer can't abut or partially overlap between subcells`
4. `See error definition in the subcell`
5. `This position does not align with the manufacturing grid`

New strings are appended in order of first appearance in the loaded style (only lines actually compiled count; a why string is registered even if the rule later fails validation when registration precedes validation, as for all rule keywords except those that fail on argument count). Why strings may contain substitution escapes (`%d`, `%c`, `%a`) expanded at report time (see capability `drc-commands`).

(Magic ref: drc/DRCtech.c:368-405, drc/DRCtech.c:661-685)

#### Scenario: Shared why index
- **GIVEN** two `width` rules with the identical why string `"too narrow"`
- **WHEN** loaded
- **THEN** both rules reference the same why index, which is 6 if they are the first rules of the style

### Requirement: Rule table ordering
Primitive rules for each edge pair SHALL be kept in a list ordered by ascending `dist`, with each new rule inserted immediately before the first existing rule whose `dist` is greater than or equal to the new rule's `dist`, treating a trigger rule and the rule it triggers as an inseparable pair keyed by the triggered rule's `dist`.
Compatibility: exact

Exceptions to the insertion rule:
- The multi-plane coincidence rule of `spacing` (see "spacing compilation") is appended at the end of the list.
- The size rule of `rectangle` is inserted before the first rule with `dist ≥ maxwidth`, without trigger-pair awareness.
- A trigger rule is always placed immediately before the rule it triggers.

Ordering is observable through the order of why messages and through rule optimization.

(Magic ref: drc/DRCtech.c:476-505, drc/DRCtech.c:3850-3860)

#### Scenario: Equal distances
- **GIVEN** `width m1 3 "A"` followed by `edge4way space m1 3 m1 m1 3 "B"`
- **WHEN** loaded
- **THEN** in the list for edge pair (space, m1) the rule from "B" precedes the rule from "A"

### Requirement: Halo and step size
The style SHALL define a halo equal to the maximum, over all rule lines, of each keyword's halo contribution, converted with ceiling rounding to internal units, and a step size equal to the `stepsize` value or a derived default.
Compatibility: exact

Halo contributions (tech-file units): width d; spacing/touching variants max(d, d)=d; widespacing max(wwidth, d); corner_ok spacing d; edge/edge4way max(d, cdist); extend d; overhang d; surround d (the first distance); rect_only 1; rectangle maxwidth; area horizon; off_grid pitch; maxwidth d; angles 1; cif rules ceil(cifdist / cifscale) (see Q-DRC-2); no_overlap, exact_overlap, option, stepsize, exception, exemption, scalefactor 0.

The halo depends on the `magic-compat` setting (see Q-DRC-2, resolution: replicate under magic-compat, fix with it off). With it ON (the default), as in Magic, the CIF contributions, which are already in internal units, are folded into maxContribution with the tech-file-unit ones and Halo = `ceil(maxContribution · D'/N')`, so with `scalefactor N` (N > 1) a halo set by a CIF rule is divided by N. With it OFF, Halo = max(`ceil(maxTechContribution · D'/N')`, maxCifContribution), where maxTechContribution excludes the CIF rules. Step size: if `stepsize` was given (positive; values < 16 warn) it is used as an internal-unit value; otherwise 16·halo, or if halo is 0: `6400 / cifScale` when a CIF output style is loaded (cifScale = CIF units per internal unit, integer division) else 64. Subsequent grid rescales multiply halo by λd/λn with ceiling and step size with floor. A non-positive `stepsize` reports `Step size must be a positive integer.` and leaves the default.

(Magic ref: drc/DRCtech.c:1182-1190, drc/DRCtech.c:4228-4256, drc/DRCtech.c:3990-4010)

#### Scenario: Default step size
- **GIVEN** a style whose largest rule distance is a widespacing with wwidth 10, d 3, no scalefactor, no stepsize
- **WHEN** loaded
- **THEN** the halo is 10 and the step size is 160

#### Scenario: Halo set by a mask-layer rule with magic-compat ON
- **GIVEN** `magic-compat` ON, a drc style with `scalefactor 10`, `cifstyle` bound to a mask output style of scalefactor 10, and the single rule `cifwidth MET1 140 "w"` (contribution ⌈140/10⌉ = 14 internal units)
- **WHEN** the style is loaded
- **THEN** the halo is ⌈14 · 1/10⌉ = 2 (golden DRC-CIF-8)

#### Scenario: Halo set by a mask-layer rule with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the style of the previous scenario
- **WHEN** the style is loaded
- **THEN** the halo is 14

### Requirement: width compilation
`width S w [angles] why` SHALL compile, for every edge pair (A,B) on a plane of S with A∈~S and B∈S, into R[A][B] FWD dist w, cdist w, OK=S, CORNER=S, and for every pair with A∈S, B∈~S into R[A][B] REVERSE|SPLITTILE dist w, cdist w, OK=S, CORNER=S; with `angles` the forward rule also carries SPLITTILE.
Compatibility: exact

S must be single-plane, else `All layers for "width" must be on same plane` and nothing is added. edgeplane = checkplane = plane(S ∩ planes of the pair). SPLITTILE rules are evaluated only on diagonal edges (see `drc-rule-semantics`). The 4th token is treated as the `angles` option if it begins with `angle`.

(Magic ref: drc/DRCtech.c:1396-1480)

#### Scenario: Compiled width rule
- **GIVEN** a tech with types space, poly on plane active and `width poly 2 "w"`
- **WHEN** compiled
- **THEN** R[space][poly] contains FWD dist 2 cdist 2 OK={poly} CORNER={poly}
- **AND** R[poly][space] contains REVERSE|SPLITTILE dist 2 cdist 2 OK={poly} CORNER={poly}

### Requirement: spacing compilation
`spacing S1 S2 d adjacency why` SHALL compile into primitive rules according to the adjacency keyword as specified below, where for each produced rule the check plane is the lowest plane of the *other* set and the edge plane is the lowest plane shared by the edge pair and the rule's own set.
Compatibility: exact

Plane validation: each set must be single-plane, or else its types must be partitioned across planes with no type present on two of those planes (`Types in first list must either be in one plane or else types must not share planes.`); such a *multiplane* rule is compiled according to the `magic-compat` setting (see Q-DRC-3, resolution: replicate under magic-compat, fix with it off): with it ON (the default) only for the first (plane1, plane2) combination that matches, in ascending plane order, as in Magic; with it OFF for every (plane1, plane2) combination. In both modes the loader emits a tech-file lint warning: a structured Hefesto diagnostic of severity `warning` with a stable code (capability `hefesto-diagnostics`), which is not Magic console output, naming the rule's line and the plane pairs that Magic does not compile.

Let P1/P2 be the plane masks, d the distance, and define for a pair on plane p and check plane q: `OKx = PT(q) \ Sx` and `CRx = PT(p) ∩ Rx`.

**touching_ok** (requires all types on one common plane, else `Spacing check with "touching_ok" must have all types in one plane...` and nothing is added): R1 = R2 = ~S1 ∩ ~S2.
- For A∈S1, B∈R1: R[A][B] FWD d, cdist d, OK2, CR1; plus DERIVED flags (below). If S1≠S2 the forward rule also gets BOTHCORNERS and R[B][A] REVERSE|BOTHCORNERS (same fields) is added.
- If S1≠S2, for A∈S2, B∈R2: R[A][B] FWD d, cdist d, OK1, CR2 (+DERIVED), BOTHCORNERS, and R[B][A] REVERSE|BOTHCORNERS.

**touching_illegal**: R1 = ~S1, R2 = ~S2. As above but with no BOTHCORNERS and no REVERSE rules (forward only). In addition, when the edge pair's plane differs from the check plane (types on different planes): for A∈R1, B∈S1 (skipping the pair if the non-space member of the pair exists on the check plane): R[A][B] FWD dist 1, cdist d, OK = PT(plane(S2)) \ S2, CORNER = ∅, appended at the end of the list (detects S2 coinciding with S1).

**manhattan_dist**: identical to touching_illegal but every produced rule carries MANHATTAN and no DERIVED flags.

**surround_ok** (requires S1 and S2 on different planes and not multiplane; if they share a plane it warns `"surround_ok" used when spacing rule types are in the same plane.  Did you mean "touching_ok"?` and compiles as touching_ok): R1=~S1. For A∈S1, B∈R1: trigger pair R[A][B] = { FWD|TRIGGER dist 1 cdist 1 OK=S2 CORNER=CR1 ; FWD|BOTHCORNERS dist d cdist d OK2 CR1 }, and R[B][A] = { REVERSE|TRIGGER dist 1 cdist 1 OK=S2 CORNER=CR1 ; REVERSE|BOTHCORNERS dist d cdist d OK2 CR1 }. No S2-side rules and no coincidence rule.

**corner_ok** (`spacing S1 S2 d corner_ok S3 why`): all three sets must share a plane (`Spacing check with "corner_ok" must have all types in one plane.`). With X = ~(S1 ∪ S3): for A∈S1, B∈X: R[A][B] FWD|BOTHCORNERS d, cdist d, OK=~S2, CORNER=X and R[B][A] REVERSE|BOTHCORNERS (same).

**DERIVED flags** (touching_ok/illegal only): DERIVED_EDGE if A is a contact type whose residue on the edge plane is a non-space type not in S1; DERIVED_TARGET if any non-builtin type of S2 on the check plane is a contact whose residue on that plane is a non-space type not in S2. (For the S2-side rules swap S1 and S2.)

Any other adjacency keyword reports `Badly formed drc spacing line...` and adds nothing. A 7-token `spacing` line whose 5th token is not `corner_ok` reports `Unknown argument in spacing line.`

(Magic ref: drc/DRCtech.c:2175-2660, drc/DRCtech.c:2696-2860, drc/DRCtech.c:1999-2090, drc/DRCtech.c:2093-2160)

#### Scenario: touching_ok same-set spacing
- **GIVEN** types space, m1, m1c(contact whose m1-plane residue is m1) on plane metal1 and `spacing m1,m1c m1,m1c 3 touching_ok "s"`
- **WHEN** compiled
- **THEN** R[m1][space] and R[m1c][space] each contain FWD dist 3 cdist 3 OK=PT(metal1)\{m1,m1c} CORNER={space}, without BOTHCORNERS
- **AND** no REVERSE spacing rule exists for (space, m1)

#### Scenario: touching_illegal two sets
- **GIVEN** `spacing poly ndiff 1 touching_illegal "pd"` with poly and ndiff on plane active
- **WHEN** compiled
- **THEN** R[poly][space] and R[poly][ndiff] contain FWD dist 1 OK=PT(active)\{ndiff} CORNER=~{poly}∩PT(active)
- **AND** R[ndiff][space] and R[ndiff][poly] contain FWD dist 1 OK=PT(active)\{poly}

### Requirement: widespacing compilation
`widespacing S1 W [L] S2 d adjacency why` SHALL compile like `spacing` for the S1 side only, with each forward spacing rule preceded by a wide-width trigger, and with reverse rules always generated.
Compatibility: exact

For A∈S1, B∈R1 (R1 per adjacency, `surround_ok` rejected with `Widespacing checks cannot use "surround_ok".`):
- Without runlength L: R[A][B] = { REVERSE|MAXWIDTH|TRIGGER|BENDS dist W cdist d OK=S1 CORNER=S1 ; FWD|BOTHCORNERS dist d cdist d OK2 CR1 } and R[B][A] = { FWD|MAXWIDTH|TRIGGER|BENDS dist W cdist d OK=S1 CORNER=S1 ; REVERSE|BOTHCORNERS dist d cdist d OK2 CR1 }.
- With runlength L: the trigger cdist is L, the spacing rules carry RUNLENGTH with cdist L (FWD rule: FWD|RUNLENGTH, not BOTHCORNERS-marked by the runlength branch but marked BOTHCORNERS afterwards; REVERSE rule: REVERSE|RUNLENGTH|BOTHCORNERS).
- No S2-side rules and no coincidence rule are generated. Halo contribution max(W, d).
- `option wide-width-noninclusive` changes the trigger width test from "≥ W" to "> W" (see `drc-rule-semantics`).

(Magic ref: drc/DRCtech.c:2330-2420, drc/DRCtech.c:2730-2760)

#### Scenario: Compiled widespacing
- **GIVEN** `widespacing m1 10 m1 4 touching_ok "wide"`
- **WHEN** compiled
- **THEN** R[m1][space] holds, in order, a REVERSE|MAXWIDTH|TRIGGER|BENDS rule (dist 10, cdist 4) immediately followed by a FWD|BOTHCORNERS spacing rule (dist 4, cdist 4)

### Requirement: edge and edge4way compilation
`edge S1 S2 d OK CORNER c [manhattan_dist] why [plane]` SHALL compile, for every edge pair A∈S1, B∈S2 sharing a plane with both sets, into R[A][B] FWD dist d cdist c OK CORNER, and `edge4way` SHALL additionally set BOTHCORNERS on it and add R[B][A] REVERSE|BOTHCORNERS with the same fields.
Compatibility: exact

- S1, S2 must share a plane (`All edges in edge rule must lie in shared planes.`); CORNER must share a plane with them (`Corner types aren't in same plane as edges.`); OK must be single-plane except that OK=`space` (or `0`) takes the edge plane (or the optional `plane` argument when OK is exactly space).
- If S1∩S2 ≠ ∅ a warning is printed; same-type edges are never checked.
- Check plane: if OK's plane mask intersects the pair's plane mask, both edge and check plane are the lowest common plane; otherwise edge plane = lowest plane of the pair (within S1/S2 mask) and check plane = lowest plane of OK.
- `manhattan_dist` sets MANHATTAN on both rules. A `plane` argument inconsistent with OK prints `Ignoring bad plane argument.`
- Halo contribution max(d, c).

(Magic ref: drc/DRCtech.c:2871-3040)

#### Scenario: edge4way
- **GIVEN** `edge4way nfet ndiff 2 ndiff,ndc ndiff 2 "overhang"`
- **WHEN** compiled
- **THEN** R[nfet][ndiff] has FWD|BOTHCORNERS dist 2 cdist 2 OK={ndiff,ndc} CORNER={ndiff}
- **AND** R[ndiff][nfet] has REVERSE|BOTHCORNERS with the same fields

### Requirement: extend compilation
`extend S1 S2 d [exact_width|exclusive] why` SHALL compile as follows, where M = S1∪S2 (or S1 with `exclusive`).
Compatibility: exact

- Same-plane case (pair on S2's plane also on S1's plane), for A∈S2, B∈S1: R[A][B] FWD d cdist 0 OK=M CORNER=∅ and R[B][A] REVERSE (same). With `exact_width` additionally R[A][B] FWD|OUTSIDE d OK=~S1 and R[B][A] REVERSE|OUTSIDE d OK=~S1.
- Multi-plane case, for A∈S2, B∈~S2 on S2's plane with S1 on another plane: R[A][B] = { FWD|TRIGGER dist 1 OK=~S1 ; FWD dist d OK=M } and R[B][A] = { REVERSE|TRIGGER dist 1 OK=~S1 ; REVERSE dist d OK=M }, check plane = plane(S1).
- The option keyword is recognised by prefix `exact_` or `exclu`; otherwise the 5th token is the why string.

(Magic ref: drc/DRCtech.c:1220-1390)

#### Scenario: Gate extension
- **GIVEN** `extend nfet poly 3 "gate length"` with nfet and poly on plane active
- **WHEN** compiled
- **THEN** R[poly][nfet] has FWD dist 3 OK={nfet,poly} and R[nfet][poly] has REVERSE dist 3 OK={nfet,poly}

### Requirement: overhang compilation
`overhang S2 S1 d why` (S2 must overhang S1) SHALL compile, with M = S1∪S2 and S2' = S2∪{space}, into R[A][B] FWD|BOTHCORNERS d cdist d OK=M CORNER=M and R[B][A] REVERSE|BOTHCORNERS (same) for every A∈S1, B∈S2' on a common plane; when S1 and S2 are on different planes it SHALL instead compile, for A∈S2', B∈~S2 on S2's plane, trigger pairs { FWD|TRIGGER 1 OK=~S1 CORNER=∅ ; FWD d cdist d OK=S1 CORNER=S1 } (and the REVERSE mirror) checked on S1's plane.
Compatibility: exact

If S1∩S2 ≠ ∅ a warning is printed.

(Magic ref: drc/DRCtech.c:3052-3185)

#### Scenario: Poly endcap
- **GIVEN** `overhang poly nfet 2 "endcap"` on one plane
- **WHEN** compiled
- **THEN** R[nfet][space] and R[nfet][poly] contain FWD|BOTHCORNERS dist 2 OK={nfet,poly}, and R[space][nfet], R[poly][nfet] contain the REVERSE|BOTHCORNERS mirror

### Requirement: surround compilation
`surround S1 S2 d [d2] presence why` (S2 must surround S1 by d) SHALL compile into the primitive rules below; S1 and S2 must each be single-plane.
Compatibility: exact

Base rules (presence absence_ok, absence_illegal, exact_width): for A∈~S2, B∈S2 on S2's plane: R[A][B] FWD|BOTHCORNERS|DERIVED_TARGET d cdist d OK=~S1 CORNER=S2 (check plane = plane(S1), edge plane = S2's) and R[B][A] REVERSE|BOTHCORNERS|DERIVED_TARGET (same).

`absence_illegal` and `exact_width` add presence rules:
- If S1 and S2 share a plane: with X = ~(S1∪S2), for A∈S1, B∈X: R[A][B] FWD|BOTHCORNERS d cdist d OK=∅ CORNER=X and REVERSE mirror at R[B][A].
- Otherwise: for A∈S1, B∈~S1 on S1's plane: R[A][B] FWD|BOTHCORNERS d cdist d OK=S2 CORNER=~S1, check plane plane(S2), and REVERSE mirror.

`exact_width` (prefix `exact_`) adds, for A∈S1, B∈S2 sharing a plane among S1's coincident planes: R[A][B] FWD|BOTHCORNERS|OUTSIDE d cdist d OK=~S2 CORNER=S2 and REVERSE mirror. Contact types are on their residue planes, so the usual via-in-metal case gets these rules; pairs with no common plane (for example a well-plane outer layer around an active-plane inner layer) get only the absence_illegal rules. This holds in both `magic-compat` settings (see Q-DRC-18, resolution: replicate); when some (A, B) pair has no common plane the loader emits a tech-file lint warning: a structured Hefesto diagnostic of severity `warning` with a stable code (capability `hefesto-diagnostics`), which is not Magic console output naming the rule's line.

`directional` (prefix): with R = S1∪S2, for A∈S1, B∈~S1 on S1's plane: R[A][B] = { FWD|TRIGGER dist d2 cdist 0 OK=S2 CORNER=∅ ; REVERSE|BOTHCORNERS dist d cdist d OK=R CORNER=all types } and R[B][A] = { REVERSE|TRIGGER d2 OK=S2 ; FWD|BOTHCORNERS d OK=R CORNER=all }, check plane plane(S2). d2 defaults to d; a non-integer d2 token reports `Incorrect arguments in directional "surround" rule`.

Any other presence keyword behaves as absence_ok.

(Magic ref: drc/DRCtech.c:3344-3640)

#### Scenario: Via surround, absence illegal, different planes
- **GIVEN** `surround via m2 1 absence_illegal "m2 surround"` with via on plane via and m2 on plane metal2
- **WHEN** compiled
- **THEN** R[space][m2] (metal2 plane) contains FWD|BOTHCORNERS|DERIVED_TARGET dist 1 OK=~{via} CORNER={m2} with check plane via
- **AND** R[via][space] (via plane) contains FWD|BOTHCORNERS dist 1 OK={m2} with check plane metal2

### Requirement: rect_only and rectangle compilation
`rect_only S why` SHALL compile, for A∈S, B∈~S on S's plane, into R[A][B] FWD|BOTHCORNERS dist 1 cdist 1 OK=~S CORNER=PT(plane) and R[B][A] REVERSE|BOTHCORNERS (same); `rectangle S maxw any|even|odd why` SHALL compile into R[A][B] FWD 1 cdist 1 OK=~S CORNER=all types, R[B][A] REVERSE 1 cdist 1 OK=~S CORNER=all types, and, if maxw > 0, R[B][A] RECTSIZE dist maxw, cdist parity (any=-1, even=0, odd=1), OK=S, CORNER=∅.
Compatibility: exact

`rectangle` reports `bad maxwidth in rectangle rule` or `bad [even|odd|any] selection in rectangle rule` on malformed input. Halo contributions 1 and maxw.

(Magic ref: drc/DRCtech.c:3189-3265, drc/DRCtech.c:3742-3870)

#### Scenario: rect_only rules
- **GIVEN** `rect_only m1c "contact must be rectangular"`
- **WHEN** compiled
- **THEN** R[m1c][space] contains FWD|BOTHCORNERS dist 1 cdist 1 OK=~{m1c} CORNER=PT(metal1)

### Requirement: area, maxwidth and off_grid compilation
`area S a h why` SHALL compile into R[A][B] AREA|FWD dist h cdist a OK=S CORNER=S for A∈~S, B∈S; `maxwidth S w [bends [E]] why` SHALL compile into R[A][B] MAXWIDTH|flag dist w cdist w OK=S CORNER=S (or CORNER=E with check plane plane(E) when exclude types E are given) for A∈~S, B∈S; `off_grid S p why` SHALL compile into R[A][B] OFFGRID|FWD dist p OK=S and R[B][A] OFFGRID|REVERSE for every A∈~S, B∈S that share any plane.
Compatibility: exact

maxwidth flag: 4-token form → BENDS if w>0 else none; `bend_illegal` → none; `bend_ok` → BENDS; `both` → MAXWIDTH_BOTH; other → `unknown bend option <x>` and no rule. Exclude types must be single-plane; if they lie on S's plane a warning `Exclude types for "maxwidth" are on the same plane and so cannot be checked.` is printed. Exclude types are honored only by the `bend_ok` (BENDS) variant; `bend_illegal` and `both` ignore them in both `magic-compat` settings (see Q-DRC-19, resolution: replicate), and when exclude types are combined with `bend_illegal` or `both` the loader emits a tech-file lint warning: a structured Hefesto diagnostic of severity `warning` with a stable code (capability `hefesto-diagnostics`), which is not Magic console output naming the rule's line. off_grid does not require S to be single-plane.

(Magic ref: drc/DRCtech.c:1507-1580, drc/DRCtech.c:1587-1650, drc/DRCtech.c:1692-1815)

#### Scenario: maxwidth default bends
- **GIVEN** `maxwidth m1 100 "slot"`
- **WHEN** compiled
- **THEN** R[space][m1] contains a MAXWIDTH|BENDS rule with dist 100

### Requirement: angles compilation
`angles S 90|45|45<sep>only why` SHALL compile, for every type t∈S and every u∉S on t's plane (or space), into R[u][t] dist 1 cdist 1 OK=S CORNER=S flags=F and R[t][u] F|REVERSE, where F = ANGLES_90 for `90`, ANGLES_45|ANGLES_90 for `45`, and ANGLES_45 for `45` followed by any single separator character and `only`; for the 45-only form it SHALL additionally add rect_only-style corner rules: for A∈S, B∈~S: R[A][B] and R[B][A] (FWD/REVERSE) with F|BOTHCORNERS dist 1 cdist 1 OK=~S CORNER=PT(plane), and for A∈~S, B∈S: R[A][B], R[B][A] with F|BOTHCORNERS dist 1 cdist 1 OK=~{A} CORNER=PT(plane).
Compatibility: exact

Values other than 45/90 report `angles must be 45 or 90` and add nothing. The token is never read past its end: `45` alone means 45 and 90 degrees, and only `45` followed by exactly one separator character and `only` means 45 only, in both `magic-compat` settings (Magic reads one byte past a bare `45`; see Q-DRC-4, resolution: fix). Halo contribution 1.

(Magic ref: drc/DRCtech.c:1819-1995)

#### Scenario: 90-degree only poly
- **GIVEN** `angles poly 90 "no diagonal poly"`
- **WHEN** compiled
- **THEN** R[space][poly] contains a rule with flags ANGLES_90 and R[poly][space] a rule with ANGLES_90|REVERSE

### Requirement: Overlap rules and the DRC paint table
The style SHALL derive a DRC paint table from the database paint table that marks illegal hierarchical overlaps, `no_overlap S1 S2` SHALL mark every combination of S1 and S2 illegal on all planes in both painting orders, and `exact_overlap S` SHALL add S to the set of types that must overlap exactly between cells.
Compatibility: exact

For plane p, painted type i over existing type j with database result r = paint(p, i, j):
1. If i or j is the subcell-error type, the result is the error type.
2. Else if i or j is space, or j is not on p, or i does not affect the planes of j, the result is r.
3. Else if i is a stacked-contact type (index ≥ number of user layers), or r is a stacked type equal to the stacking of i and j, the result is r.
4. Else if r is neither i/a type composed with layer i nor j/a type composed with layer j, or (r ≠ paint(p, j, i) and i is on p and j affects i's planes), the result is the error type (illegal overlap).
5. Else r.

The DRC paint table is used when flattening hierarchy for interaction and array checks (see `drc-engine`); error-type tiles produced there are reported with why index 2.

(Magic ref: drc/DRCtech.c:700-737, drc/DRCtech.c:3644-3735)

#### Scenario: Default illegal overlap
- **GIVEN** a tech where painting poly over ndiff yields nfet
- **WHEN** the DRC paint table is built
- **THEN** painting poly over ndiff in the DRC paint table yields the subcell-error type

### Requirement: Exceptions and exemptions
`exception NAME` and `exemption NAME` SHALL tag all subsequently compiled rules of the style with an exception index referring to mask-hint property `MASKHINTS_NAME` (exception: rule applies only inside the hint area; exemption: rule applies only outside), until the next `exception`/`exemption` line; the name `none` clears the tag.
Compatibility: exact

Names are registered in order of first use; at most 126 distinct names (`Error:  Too many rule exceptions!  Limit is 126.` and the rule is untagged). The tag is reset to none at the start of each style load. The run-time test is in `drc-rule-semantics`.

(Magic ref: drc/DRCtech.c:420-470, drc/DRCtech.c:3884-3920)

#### Scenario: Exemption block
- **GIVEN** `exemption SRAM`, `spacing m1 m1 3 touching_ok "s"`, `exemption none`, `width m1 2 "w"`
- **WHEN** compiled
- **THEN** the spacing rules carry exemption index for `MASKHINTS_SRAM` and the width rule carries none

### Requirement: option statement
`option` SHALL accept the option name `wide-width-noninclusive`, which makes widespacing triggers require width strictly greater than W; any other name SHALL report `Unrecognized DRC option "<name>" (ignored).`
Compatibility: exact

(Magic ref: drc/DRCtech.c:3943-3970)

#### Scenario: Unknown option
- **GIVEN** `option foo`
- **WHEN** loaded
- **THEN** the message `Unrecognized DRC option "foo" (ignored).` is reported and no flag changes

### Requirement: Illegal-edge rule pruning
After loading, for each edge pair (A,B) the compiler SHALL detect rules that are violated by the edge itself and keep, for each edge plane, only the first such rule in list order (with its TRIGGER flag cleared), discarding all other rules of that edge pair on the same edge plane.
Compatibility: exact

A rule is "always violated" when it is not nonstandard (AREA, MAXWIDTH, RECTSIZE, ANGLES_90, OFFGRID) and not OUTSIDE, and the type adjacent on the checked side (B for FWD, A for REVERSE) is not space and is not in OK, and, when check plane ≠ edge plane, that adjacent type exists on the check plane. Rules of the pair on other edge planes are kept. This pruning changes error geometry (only the kept rule's region is flagged) and why-text, and Hefesto replicates it in both `magic-compat` settings.

(Magic ref: drc/DRCtech.c:4273-4350)

#### Scenario: Pruned illegal edge
- **GIVEN** `spacing poly ndiff 1 touching_illegal "A"` and `width ndiff 3 "B"` on one plane
- **WHEN** rules are finalized
- **THEN** R[poly][ndiff] keeps only the "A" rule (ndiff ∉ OK of "A"), and the width rule "B" (which lives in R[poly][ndiff] because poly∈~ndiff) is removed from that pair

### Requirement: Covered rule elimination
After illegal-edge pruning, a rule X in a pair's list SHALL be removed when a later rule Y (not part of a trigger pair) in the same list covers it: Y.OK ⊆ X.OK, Y.CORNER = X.CORNER, X.dist ≤ Y.dist, X.cdist ≤ Y.cdist, same check plane, same exception tag, same direction, not (Y has BOTHCORNERS and X has not), Y standard, equal OUTSIDE flags when distances are equal, and equal SPLITTILE flags; when X is the rule triggered by a trigger, both the trigger and X are removed.
Compatibility: exact

Nonstandard rules are never removed or used as cover. The conditions depend on the `magic-compat` setting (see Q-DRC-5, resolution: replicate under magic-compat, fix with it off):
- With it ON (the default), Magic's coded condition is reproduced as listed above: Y is skipped as cover when Y has BOTHCORNERS and X has not, the inverse of the documented intent ("if A is BOTHCORNERS then B must be, too"), so a BOTHCORNERS rule can be removed in favor of a single-corner rule; the edge plane and the MANHATTAN, RUNLENGTH and DERIVED_EDGE/DERIVED_TARGET flags are not compared.
- With it OFF, Y covers X only if Y has BOTHCORNERS whenever X has it, and Y and X also have the same edge plane and the same MANHATTAN, RUNLENGTH, DERIVED_EDGE and DERIVED_TARGET flags.

(Magic ref: drc/DRCtech.c:4352-4470)

#### Scenario: Width rule covered by larger width rule
- **GIVEN** `width m1 2 "small"` and later `width m1 3 "big"`
- **WHEN** finalized
- **THEN** in R[space][m1] the dist-2 forward rule is removed and only the "big" rule remains, so violations report "big"

#### Scenario: BOTHCORNERS rule covered by a single-corner rule with magic-compat ON
- **GIVEN** `magic-compat` ON, `edge4way space m1 2 m1 m1 2 "A"` (BOTHCORNERS) followed by `edge space m1 3 m1 m1 3 "B"` (single corner)
- **WHEN** the rules are finalized
- **THEN** in R[space][m1] the forward rule "A" is removed in favor of "B", so the second corner of "A" is no longer checked (golden DRC-OPT-3)

#### Scenario: BOTHCORNERS rule kept with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the rules of the previous scenario
- **WHEN** the rules are finalized
- **THEN** rule "A" is kept, because "B" lacks BOTHCORNERS

### Requirement: Style selection at run time
Selecting a DRC style SHALL re-read the drc section of the current tech file for that style, apply the current grid scale, and re-establish halo and step size; selecting by name uses unique-prefix matching over the style list.
Compatibility: behavioral

Messages: `DRC style is now "<given name>"`, `DRC style "<name>" is ambiguous.`, `"<name>" is not one of the DRC styles Magic knows.` (informative). A style that references CIF rules is reloaded when the CIF output style changes (see `drc-cif-rules`).

(Magic ref: drc/DRCtech.c:234-280, drc/DRCtech.c:519-580)

#### Scenario: Prefix style selection
- **GIVEN** styles `fast` and `full`
- **WHEN** the user selects `fa`
- **THEN** style `fast` is loaded
- **AND** selecting `f` reports that the style is ambiguous and lists the styles
