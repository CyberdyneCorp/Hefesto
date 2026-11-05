# Antenna Check

## Purpose

Defines the process-antenna (plasma-induced gate damage) check: the extract-section technology keywords that declare antenna rules (`antenna`, `model`, `tiedown`), the exact antenna-ratio computation performed per gate net and per routing plane, the `antennacheck` command, and the reporting (feedback areas and diagnostic messages). It also records which of these definitions are shared with LEF macro output (`ANTENNAGATEAREA` / `ANTENNADIFFAREA`), whose output format is owned by capability `lef-write`.

Related capabilities (referenced, not duplicated): `tech-file-format` (section/line syntax), `extract-tech-section` (extract section in general, `planeorder`, `height`, `units`), `extract-hierarchy` (the `.ext` files read here), `ext-flattening-read` (hierarchical name flattening), `ui-feedback-and-highlights` (feedback-area storage and display), `mask-output-styles` (contact cut `squares`/`slots` operators), `lef-write`.

## Requirements

### Requirement: Antenna tech-file keywords grammar

The extract section of a technology file SHALL accept the keywords `antenna`, `model` and `tiedown` with the grammar below; keywords are matched exactly (case-sensitive, no prefix abbreviation) and argument-count limits are enforced before parsing (`antenna` 3..5 arguments, `model` 1..2, `tiedown` exactly 1).

Compatibility: exact

```ebnf
antenna_line  = "antenna" , ws , types , ws , [ calc_type , ws ] , ratio_gate , ws , ratio_diff_const , [ ws , ratio_diff_prop ] ;
calc_type     = "surface" | "area" | "sidewall" | "perimeter" ;
ratio_gate    = number ;                       (* dimensionless; metal-area / gate-area limit *)
ratio_diff_const = number | none_kw ;          (* "none" compared case-insensitively *)
none_kw       = ? "none" in any letter case ? ;
ratio_diff_prop  = number ;                    (* per unit of tie-down (diffusion) area *)
model_line    = "model" , ws , ( "partial" | "cumulative" ) , [ ws , model_calc ] ;
model_calc    = "surface" | "area" | "sidewall" | "perimeter" ;
tiedown_line  = "tiedown" , ws , types ;
types         = ? comma-separated tile type / alias list, as in capability tech-file-format ? ;
number        = ? a token accepted as numeric (integer or decimal, optional sign/exponent) ? ;
```

Semantics, applied to every tile type in `types` (types at or above the first technology-dependent type):

- `calc_type` selects how the layer's antenna area is measured: `surface`/`area` = drawn area (SURFACE); `sidewall`/`perimeter` = exposed perimeter times layer thickness (SIDEWALL). If `calc_type` is omitted, the layer takes the calculation type given by a previously parsed `model` line (SURFACE if the model carries SURFACE, else SIDEWALL if it carries SIDEWALL); if neither was declared, an error "No antenna calculation type given for layer(s) <types> and no default calculation type found." is reported and the calculation type is left undefined (see Q-MISC-3).
- A non-numeric third token that is not a calc keyword is an error ("Error in layer antenna calculation type ...; must be "surface" or "sidewall"") and the line is ignored.
- `ratio_gate` is stored as a single-precision float. A non-numeric value is an error and the remaining fields are not stored (the calculation type has already been stored).
- `ratio_diff_const` is stored as a float; the word `none` stores +infinity, which means "this layer has no rule when the net has tie-down area".
- `ratio_diff_prop` defaults to 0 when absent.
- `model partial` / `model cumulative` and the optional calc keyword are OR-ed into a single per-style model flag set; later `model` lines add flags and never clear earlier ones. An unknown model word prints "Unknown antenna model ..." and is otherwise ignored.
- `tiedown` adds the types to the style's tie-down (diffusion) type set; repeated lines accumulate.
- Defaults for every type before any line: no calculation type, `ratio_gate` = 0, `ratio_diff_const` = 0, `ratio_diff_prop` = 0; tie-down set empty. The model flag set has no defined initial value in Magic (see Q-MISC-2); Hefesto SHALL start with an empty set. A line with a calc keyword but only one ratio reads past the argument list in Magic (see Q-MISC-5).

(Magic ref: extract/ExtTech.c:156-163, extract/ExtTech.c:942-951, extract/ExtTech.c:2990-3109, extract/extractInt.h:49-60)

#### Scenario: Full antenna line with diffusion terms
- **GIVEN** an extract section containing `antenna metal1 sidewall 400 2200 0.5`
- **WHEN** the technology is loaded
- **THEN** type metal1 has calculation type SIDEWALL, gate ratio 400.0, constant diffusion ratio 2200.0 and proportional diffusion ratio 0.5 (before unit conversion)

#### Scenario: Default calculation type taken from model
- **GIVEN** the lines `model partial surface` followed by `antenna poly 50 none`
- **WHEN** the technology is loaded
- **THEN** poly has calculation type SURFACE, gate ratio 50.0, constant diffusion ratio +infinity and proportional ratio 0
- **AND** the model flag set contains PARTIAL and SURFACE

#### Scenario: Model line after antenna line does not supply a default
- **GIVEN** the lines `antenna poly 50 none` followed by `model partial surface`
- **WHEN** the technology is loaded
- **THEN** the error "No antenna calculation type given for layer(s) poly ..." is reported for the antenna line

### Requirement: Antenna coefficient unit conversion

The antenna coefficients SHALL be converted with the rest of the extract style: when the extract section declares `units microns` (or `um`), the proportional diffusion ratio is multiplied by the square of the micron-per-lambda output scale (s = 10 * cifoutput scalefactor / (expander * 1000)), layer thickness used for sidewall area is divided by s, and the gate ratio and constant diffusion ratio are not converted (dimensionless). When the internal grid is rescaled by n/d (e.g. `scalegrid`, `tech lambda`), the proportional ratio is multiplied by n² and divided by d², and thickness is multiplied by d and divided by n. A thickness left at 0 by the `height` keyword defaults to 0.05 (before conversion).

Compatibility: exact

(Magic ref: extract/ExtTech.c:3850-3856, extract/ExtTech.c:3858-3870, extract/ExtTech.c:3962-3968, extract/ExtTech.c:4088-4096, cif/CIFmain.c:139-146)

#### Scenario: Micron conversion of proportional ratio
- **GIVEN** `units microns`, a cifoutput style with scalefactor 10 and expander 1 (s = 0.1 µm per internal unit), and `antenna metal1 surface 400 2200 2.0`
- **WHEN** the style is finalised
- **THEN** the proportional diffusion ratio becomes 2.0 × 0.01 = 0.02 per internal-unit² while 400 and 2200 are unchanged

#### Scenario: Thickness conversion
- **GIVEN** `units microns`, s = 0.1, and `height metal1 1.0 0.36`
- **WHEN** the style is finalised
- **THEN** the metal1 thickness used for sidewall antenna area is 3.6 internal units

### Requirement: Antenna check command

The system SHALL provide an `antennacheck` command with the forms `antennacheck`, `antennacheck run [options] [cell]`, `antennacheck debug` and `antennacheck help`, where the first argument is matched by unique prefix against `run`, `debug`, `help`, and any first argument that matches none of them is treated as if `run` had been given with that argument kept as an option/cell argument.

Compatibility: behavioral

- `help` prints the option summary and does nothing else.
- `debug` only arms diagnostic output for the next run and returns without checking; the flag is cleared at the end of the next run.
- `run` accepts the standard flattening options (`-R`, `-C`, `-r rthresh`, `-c cthresh`, `-v`, `-p searchpath`, `-s sym=value`, `-S symfile`, `-t trimchars`, `-h`); any other flag letter is an error "Unrecognized flag: ..." followed by "Usage: antennacheck". The capacitance and resistance thresholds are forced to infinity before options are parsed.
- The root cell is the named cell, or else the root cell of the window under the pointer (or the only layout window); with no such window the error "Point to a window or specify a cell name." is reported. Feedback is always attached to the root cell of that window.
- If the extract style declared `noplaneordering`, the command reports "No planeorder specified for this process:  Cannot run antenna checks!" and does nothing.
- The check consumes previously generated extraction files for the root cell and its hierarchy (it does not re-extract); devices are taken from the flattened `.ext` netlist, the geometry from the cell database.
- Progress messages (informative): "Reading extract file.", "Building flattened netlist.", "Running antenna checks.", "   <n> gates analyzed." after every 100 distinct gate nets, and "antennacheck finished.".

(Magic ref: extflat/EFantenna.c:126-265, extflat/EFantenna.c:294-315, extflat/EFargs.c:158-283, dbwind/DBWcommands.c:235)

#### Scenario: Debug arms the next run only
- **GIVEN** a layout with extraction files present
- **WHEN** the user runs `antennacheck debug`
- **THEN** no check is performed
- **AND** the subsequent `antennacheck` prints per-violation diagnostics and then clears the debug flag

#### Scenario: Unknown flag
- **WHEN** the user runs `antennacheck run -x`
- **THEN** the errors "Unrecognized flag: -x" and "Usage: antennacheck" are reported and no check is performed

### Requirement: Gate nets visited once

The check SHALL visit every device of class `mosfet`, `fet`, `asymmetric` or `msubcircuit` in the flattened netlist, and SHALL evaluate the antenna rule once per distinct flattened gate-terminal node (the first such device encountered for that node), skipping later devices whose gate node was already checked. The device rectangle (transformed to root coordinates) is the seed of the net trace, and the device's tile type (the first tile type whose extract `device` declaration has the device's model name) determines the gate plane.

Compatibility: behavioral

(Magic ref: extflat/EFantenna.c:388-425, extflat/EFantenna.c:251-255, extflat/EFantenna.c:437-441)

#### Scenario: Two transistors sharing a gate
- **GIVEN** an inverter whose nfet and pfet share one poly gate net
- **WHEN** `antennacheck` runs
- **THEN** the gate net is evaluated exactly once and the progress counter increases by one

### Requirement: Per-plane progressive evaluation

For a gate whose tile type lies on plane G, the system SHALL evaluate the antenna rule once for every plane-order position k from order(G)+1 up to the highest plane order in the style, where the "check plane" P_k is the plane whose `planeorder` index equals k, and for each k SHALL:

1. Trace the electrically connected geometry of the whole hierarchy (flattened, labels ignored) starting from the device rectangle with the connectivity of the gate type, but with the connectivity of the first (lowest type number) contact type whose home plane is P_k restricted to types whose plane number is less than or equal to the plane number of P_k, so that the trace does not climb above P_k through that contact.
2. Compute the tie-down area D = sum of areas of traced tiles whose type is in the `tiedown` set, on any plane (a contact tile counts only on its home plane; a split/diagonal tile counts half its bounding-box area, integer division).
3. Compute the gate area A_g = sum of areas of traced tiles on plane G whose type is the device type of any `mosfet`/`fet`/`asymmetric`/`msubcircuit` device declaration (same contact and split rules).
4. Compute per-type antenna areas a[t] over the traced tiles of every plane whose plane order is ≤ k (cumulative model), or of plane P_k only (partial model), using the per-type measurement of requirement "Antenna area measurement".
5. Apply the ratio test of requirement "Antenna ratio test".

All areas are in internal units squared and are accumulated in 64-bit integers. If no plane has order k, Magic reuses the previous check plane (see Q-MISC-6); the connectivity restriction compares plane numbers, not plane orders, and applies to one contact type only (see Q-MISC-7).

Compatibility: exact

(Magic ref: extflat/EFantenna.c:437-566, extflat/EFantenna.c:782-806)

#### Scenario: Check planes for a three-metal stack
- **GIVEN** planeorder well 0, active 1, metal1 2, metal2 3, metal3 4, and an nfet gate type on plane active
- **WHEN** `antennacheck` evaluates that gate
- **THEN** it evaluates the rule three times, with check planes metal1, metal2 and metal3 in that order

#### Scenario: Trace stops at the check plane
- **GIVEN** a gate connected through poly contact to metal1 and through via1 to a large metal2 plate
- **WHEN** the check plane is metal1
- **THEN** the metal2 plate is not part of the traced geometry
- **AND** when the check plane is metal2 the plate is traced

### Requirement: Antenna area measurement

For each traced tile of type t on the searched plane, the system SHALL add to a[·] according to t's calculation type; types with no calculation type contribute nothing:

- SURFACE, non-contact tile: a[t] += width × height (halved with integer division for a split tile).
- SURFACE, contact tile of width w and height h: let (s, sep, b) be the cut size, separation and border of the first cifoutput `squares`/`slots` operator generating the contact type (values in output-style units), f = s / scalefactor (float), nx = trunc((w − 2b)/(s + sep)), ny likewise with h, each replaced by 1 when 0; cut area C = nx·ny × trunc(f²) (integer). C is added to every residue type of the contact that lies on the searched plane; then, for a user-visible contact type, C is also added to a[contact]; for a stacked contact type, C is added once more to the first of its residues on the searched plane. (w, h are internal units while s, sep, b are output units; see Q-MISC-4.)
- SIDEWALL, non-contact tile: perimeter p = total length of the tile's boundary segments that abut space-type tiles on the same plane; a[t] += trunc(p × thick[t]).
- SIDEWALL, contact tile: cperim = trunc(nx·ny·4·s / scalefactor) (integer arithmetic); for each residue r on the searched plane a[r] += trunc(cperim × thick[r]); additionally, only in the cumulative model, a[contact] (user contact) or a[first residue on plane] (stacked contact) += trunc(cperim × thick).

Compatibility: exact

(Magic ref: extflat/EFantenna.c:822-1016, cif/CIFtech.c:2327-2372)

#### Scenario: Surface area of plain metal
- **GIVEN** metal1 declared `antenna metal1 surface 400 none` and a traced metal1 rectangle 100 × 100
- **WHEN** areas are measured on plane metal1
- **THEN** a[metal1] = 10000

#### Scenario: Sidewall area of isolated wire
- **GIVEN** metal1 declared sidewall with thickness 1.0 internal unit and an isolated metal1 rectangle 100 × 10 surrounded by space
- **WHEN** areas are measured
- **THEN** a[metal1] = trunc(220 × 1.0) = 220

#### Scenario: Contact cut area with mixed units (verify against Magic)
- **GIVEN** a via type whose cifoutput rule is `squares 10 20 20` in a style with scalefactor 10, declared surface, and a 10 × 10 via tile on its home plane metal1
- **WHEN** areas are measured
- **THEN** nx = trunc((10 − 20)/40) = 0 → 1, ny = 1, f = 2.0, C = 4
- **AND** 4 is added to a[metal1] and 4 to a[via]

### Requirement: Antenna ratio test

With gate area A_g, tie-down area D, and per-type areas a[t], the system SHALL evaluate, over user-visible types t only:

- If D = 0: for each t with ratio_gate[t] > 0, in the partial model add a[t]/ratio_gate[t] to S_metal when t is not a contact, or to S_via when t is a contact whose home plane is P_k. Limit L = max over all user types of ratio_gate.
- If D > 0: for each t with ratio_diff_const[t] finite, R[t] = ratio_gate[t] + ratio_diff_const[t] + ratio_diff_prop[t] × D (single-precision); when R[t] > 0, in the partial model add a[t]/R[t] to S_metal (non-contact) or S_via (contact with home plane P_k). L = max of R[t] over those types.
- A violation exists when S_metal > A_g or S_via > A_g; when D = 0 a violation additionally requires A_g > 0.
- In the cumulative model neither sum is accumulated, so no violation is ever reported (see Q-MISC-1).

Sums use double precision. The reported effective ratio is L × S / A_g computed in single precision.

Compatibility: exact

(Magic ref: extflat/EFantenna.c:567-714)

#### Scenario: Gate-only violation
- **GIVEN** model partial, `antenna metal1 surface 400 none`, no tie-down area on the net, A_g = 20 and a[metal1] = 10000 at check plane metal1
- **WHEN** the rule is evaluated
- **THEN** S_metal = 25.0 > 20, a violation is reported with effective ratio 500 and limit 400

#### Scenario: Diffusion protects the net
- **GIVEN** model partial, `antenna metal1 surface 400 2200`, D = 50 (tie-down), A_g = 20 and a[metal1] = 10000
- **WHEN** the rule is evaluated
- **THEN** R = 2600, S_metal = 3.846 ≤ 20 and no violation is reported

#### Scenario: Layer exempt when diffusion present
- **GIVEN** model partial, `antenna metal1 surface 400 none`, D = 50, A_g = 20, a[metal1] = 10^6
- **WHEN** the rule is evaluated
- **THEN** metal1 contributes nothing because its constant diffusion ratio is infinite, and no violation is reported

#### Scenario: Cumulative model never reports (verify against Magic)
- **GIVEN** model cumulative surface with the same geometry as the gate-only violation scenario
- **WHEN** `antennacheck` runs
- **THEN** no violation is reported

### Requirement: Violation reporting

On a violation at check plane P_k the system SHALL create feedback areas in the root cell, all with the message "Antenna error at plane <long name of P_k>" followed by a newline character, pale-highlight style and scale 1: one for every traced tile of a gate type on plane G, and one for every traced non-space tile on each plane included in the area accumulation (planes of order ≤ k in the cumulative model, plane P_k only in the partial model). Feedback rectangles are the maximal-horizontal-strip tiles of the traced (flattened) geometry. A gate net may produce feedback for several check planes.

In debug mode, for each violated sum the following lines are additionally written to the error stream (informative): "Cell: <instance id or definition name>", "Antenna violation detected at plane <name>" (with " contact" appended for the via sum), "Effective antenna ratio <%g> > limit <%g>", "Gate rect (<x1> <y1>) to (<x2> <y2>)" (the last gate tile visited) and "Antenna rect (...) to (...)" (the last non-contact antenna tile visited).

Compatibility: behavioral

(Magic ref: extflat/EFantenna.c:597-640, extflat/EFantenna.c:716-735, extflat/EFantenna.c:757-769)

#### Scenario: Feedback geometry for a violation
- **GIVEN** the gate-only violation scenario with one 2 × 10 gate tile and one 100 × 100 metal1 tile
- **WHEN** `antennacheck` finishes
- **THEN** two feedback areas exist in the root cell, (gate rectangle) and (metal1 rectangle), each with text "Antenna error at plane metal1\n"

### Requirement: Shared antenna definitions for LEF output

The gate-type set (tile types of `mosfet`/`fet`/`asymmetric`/`msubcircuit` device declarations) and the tie-down type set SHALL be the same sets used when computing LEF pin `ANTENNAGATEAREA` and `ANTENNADIFFAREA` values: per pin net, the area of the respective types on the first plane (in plane-number order) that has any such area, excluding pins whose use is power or ground and nets matched by the LEF ignore-net list; the values are written only when positive, scaled by the square of the output scale with `%f`. The output format is owned by capability `lef-write`.

Compatibility: exact

(Magic ref: extract/ExtTech.c:514-560, lef/lefWrite.c:1490-1541, lef/lefWrite.c:1697-1705)

#### Scenario: Pin gate area
- **GIVEN** a pin net connected to two gates of 0.15 µm × 1 µm each and no tie-down types
- **WHEN** the macro is written to LEF
- **THEN** the pin carries `ANTENNAGATEAREA 0.300000 ;` and no `ANTENNADIFFAREA`
