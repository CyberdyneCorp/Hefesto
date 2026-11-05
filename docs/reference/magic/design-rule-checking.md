# Design Rule Checking

## Purpose
Magic checks design rules continuously and incrementally. Edits paint "check" tiles on a dedicated DRC plane. A background checker then processes those tiles in step-size squares and writes errors to an error plane. Checking is hierarchical: subcell interactions are flattened into a yank buffer and checked there. The rules come from the technology file's `drc` section and are edge-based. Cif-based rules, which run on generated mask layers, are also supported.

## Requirements

### Requirement: DRC styles and variants
The `drc` section SHALL support the following, with selection by unique prefix:
- `style <name> [variant v1,v2,...]`; each variant SHALL produce a style named `<name><variant>`
- `variant` scoping of rule lines
- `scalefactor N [D]`

Rules that appear before any `style` line SHALL go into a style named `default`. (src: drc/DRCtech.c:233-267,759-1009)

#### Scenario: Unknown style
- **WHEN** `drc style foo` is run and no style matches
- **THEN** the system SHALL report "\"foo\" is not one of the DRC styles Magic knows."

### Requirement: Rule types
The checker SHALL support the following rule keywords (src: drc/DRCtech.c:1100-1141 and handlers):
- `width`, `spacing` (adjacency `touching_ok`, `touching_illegal`, `surround_ok`, `manhattan_dist`, or `corner_ok <types>`), `widespacing`
- `edge`, `edge4way`, `extend`, `overhang`, `surround` (`absence_ok`, `absence_illegal`, `exact_*`, `directional`)
- `rect_only`, `no_overlap`, `exact_overlap`, `area`, `maxwidth`, `angles`, `off_grid`, `rectangle`
- `exception`/`exemption` (scoped to `MASKHINTS_<name>`, at most 126), `option wide-width-noninclusive`, `stepsize`
- `cifstyle`, `cifwidth`, `cifspacing`, `cifarea`, `cifmaxwidth`

The `why` text of each rule SHALL be reported for its violations. In that text, `%d`, `%c` and `%a` SHALL be replaced by the rule distances, shown in the current units.

#### Scenario: Bad rule
- **WHEN** an unknown rule keyword appears
- **THEN** the loader SHALL report `Bad DRC rule type "<kw>"` and list the valid types

#### Scenario: Width violation
- **GIVEN** `width m1 3 "Metal1 width < 3"`
- **WHEN** a 2-unit-wide metal1 wire is painted
- **THEN** an error area SHALL appear, and `drc why` SHALL report "Metal1 width < 3"

### Requirement: Implicit overlap errors
Two layers that cannot legally overlap SHALL be reported as an error with the built-in message "Can't overlap those layers". Layers can legally overlap when painting one over the other yields one of them. Other built-in messages SHALL cover:
- array-element and subcell abutment
- errors inherited from subcells
- off-grid positions

(src: drc/DRCtech.c:652-735)

#### Scenario: Illegal subcell overlap
- **WHEN** two subcells overlap with incompatible layers
- **THEN** the interaction check SHALL flag the overlap area

### Requirement: Continuous background checking
When an edit occurs:
1. The system SHALL paint check tiles over the edited area plus the DRC halo. It SHALL also paint `checksubcell` tiles over the area in each parent, covering the full array area for arrays.
2. The background checker SHALL process check tiles one step-size square at a time, children before parents.
3. In the Tcl build, the checker SHALL yield to pending Tk events, and SHALL show `]` as the prompt while it runs.

The default step size SHALL be 16 × halo. Internal cells SHALL never be checked. (src: drc/DRCcontin.c:183-301,480-775; drc/DRCtech.c:4244-4256)

#### Scenario: Edit triggers check
- **GIVEN** DRC is on
- **WHEN** paint is added
- **THEN** errors for the affected area SHALL be updated without any explicit command

#### Scenario: Read-only cells stay clean
- **WHEN** the checker updates errors in a `CDNOEDIT` or `CDVENDORGDS` cell
- **THEN** the cell SHALL NOT be marked modified

### Requirement: Hierarchical interaction checking
Where subtrees, or paint and a subtree, actually interact, the interaction area plus the halo SHALL be flattened into a yank buffer and checked there, including exact-overlap rules. Errors in non-interacting regions of children SHALL be copied up one level. For arrays, only the strips between neighboring elements SHALL be checked. (src: drc/DRCsubcell.c:405-966, drc/DRCarray.c:101-267)

#### Scenario: Child error visible in parent
- **WHEN** a child cell contains a DRC error
- **THEN** the parent SHALL show it as "See error definition in the subcell"

### Requirement: drc command
`drc` SHALL accept the following options, matched by unique prefix, with an optional `list` or `listall` prefix:
- `on`, `off`, `status`, `check`, `catchup`
- `count [total]`, `find [n|text]`, `why`
- `ignore [text|none]`, `euclidean [on|off]`
- `style [name]`, `printrules [file]`, `rulestats`, `statistics`
- `halo [d]`, `stepsize [d]`
- wizard options `*flatcheck` and `*showint`

The Tcl build SHALL start with DRC off during startup and then restore the saved state. (src: commands/CmdCD.c:4053-4610, tcltk/magic.tcl.in:355-356,467-469)

#### Scenario: Count errors
- **WHEN** `drc count total` is run
- **THEN** the system SHALL print "Total DRC errors found: N"

#### Scenario: Machine-readable reasons
- **WHEN** `drc listall why` is run in Tcl
- **THEN** the system SHALL return a list of `{why {llx lly urx ury}...}` in the current display units

#### Scenario: Catch up
- **WHEN** `drc catchup` is run
- **THEN** all pending check tiles SHALL be processed before the command returns

### Requirement: Cif-based rules
`cifwidth`, `cifspacing`, `cifarea` and `cifmaxwidth` rules SHALL be evaluated on layers generated by the named `cifstyle`. Distances SHALL be in cifoutput units. If the active CIF output style differs from the rule's style, the system SHALL load that style temporarily ("Loading DRC CIF style."). (src: drc/DRCcif.c:114-588)

#### Scenario: Missing cif style
- **WHEN** cif rules are given without a `cifstyle`
- **THEN** the system SHALL warn "Missing cif style for drc" once

### Requirement: Euclidean corner checks
By default, corner distances SHALL be checked as Manhattan. After `drc euclidean on`, corner distances SHALL be checked as a radius, except for rules flagged `manhattan_dist`. (src: drc/DRCbasic.c:327-404,1146-1256)

#### Scenario: Diagonal corner spacing
- **GIVEN** `drc euclidean on`
- **WHEN** two shapes are separated diagonally by a Euclidean distance that meets the spacing rule
- **THEN** no corner error SHALL be reported
