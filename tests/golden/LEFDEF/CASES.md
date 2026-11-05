# LEFDEF golden test cases

Test cases to capture later by running Magic (commit fc5daa9e) headless. Each case covers the capabilities lef-tech-section, lef-read, lef-write, def-read and def-write. ID ranges: 001-099 tech section and LEF read, 100-199 LEF write, 200-299 DEF read, 300-399 DEF write.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| LEFDEF-001 | lef-tech-section | tech with lef section using every keyword (layer on contact/non-contact, route, routing, cut, contact, obstruction with 2 types, masterslice, overlap, bound, ignore), plus bad lines (ambiguous `rout`, missing names, 3 types, duplicate name) | tech-load error messages; `lef write -tech` output of a trivial cell to observe the classes and canonical names | Lef section line grammar; Magic type resolution; Keyword to LEF class mapping; LEF name aliasing |
| LEFDEF-002 | lef-tech-section | 1:1-grid tech with DRC width/spacing for m1 and none for m5; `lef write -tech` | WIDTH/SPACING lines for m1 (expected absent) and m5 (3/4 in µm) | Default routing parameters; Filling defaults after grid scaling |
| LEFDEF-003 | lef-tech-section | same tech with cifoutput `gridlimit` forcing a 2:1 internal grid; and a variant with a shrinking grid | tech-load output and `lef write -tech` WIDTH lines; DEF read default wire width of a net with no width | Filling defaults after grid scaling; Scaling the LEF table |
| LEFDEF-004 | lef-tech-section | after LEFDEF-002, run `scalegrid 1 2` then `lef write -tech` | scaled WIDTH/SPACING/PITCH values | Scaling the LEF table |
| LEFDEF-005 | lef-read | LEF with comments, quoted PROPERTY value, `;` glued to a number, a line longer than 2048 characters | resulting cell properties (.mag) and error messages | LEF/DEF tokenizer |
| LEFDEF-006 | lef-read | LEF producing more than 100 errors (unknown layers) | stdout/stderr transcript (cap notice and totals) | LEF/DEF message reporting |
| LEFDEF-007 | lef-read | tech with cifoutput scale such that s = 0.005; macro with RECT coordinates at .0025 boundaries (positive and negative) | saved .mag rect coordinates (half-away rounding in binary32) | LEF unit conversion |
| LEFDEF-008 | lef-read | LEF with VERSION, UNITS 2000, PROPERTYDEFINITIONS, SPACING, BEGINEXT/ENDEXT sections and an unknown top-level statement, then a MACRO, END LIBRARY and a trailing MACRO | list of cells created; messages | Supported LEF file structure; END statement matching |
| LEFDEF-009 | lef-read | technology LEF: LAYER for a tech routing layer (WIDTH/SPACING twice/PITCH/DIRECTION), TYPE CUT on a routing layer, LAYER for an unmapped magic name, LAYER for a contact-type magic name, then `lef write -tech` | messages; written LAYER blocks | LEF LAYER section |
| LEFDEF-010 | lef-read | technology LEF with VIA (met1/via/met2 RECTs in that order), VIARULE of the same name, VIA redefined, VIA via rule (CUTSIZE...), then a DEF that places that via | `def read` result .mag (via paint) and warnings | LEF VIA and VIARULE definitions; Via redefinition |
| LEFDEF-011 | lef-read | tech whose cifoutput generates `via` with squares (size/border known); LEF VIA with a cut smaller than the magic cut; DEF placing it | warnings and painted contact size in .mag | Cut growing |
| LEFDEF-012 | lef-read | MACRO with ORIGIN 1 1, SIZE 10 BY 10 and OBS RECT; second MACRO with FOREIGN pt 0.5 0.5; third with SIZE before ORIGIN | .mag paint coordinates and FIXED_BBOX property | Macro coordinate offset |
| LEFDEF-013 | lef-read | MACRO with CLASS CORE WELLTAP, SYMMETRY X Y R90, SITE, two PROPERTY statements, SOURCE, TIMING section | .mag `string`/property lines | Macro statements and cell properties |
| LEFDEF-014 | lef-read | macro without SIZE; SITE section; macro defined twice in one file | .mag files (FIXED_BBOX, LEFview, cell names `X_1`) and warnings | Macro completion; MACRO and SITE cell creation |
| LEFDEF-015 | lef-read | macro with PINs using every DIRECTION/USE/SHAPE value (incl. OUTPUT TRISTATE, USE SCAN), DIRECTION after PORT, multiple RECTs and a POLYGON per PORT, two PORTs per PIN | .mag `rlabel`/`port` lines (order, indices, flags) | PIN attributes; Port labels in creation mode; PORT and OBS geometry |
| LEFDEF-016 | lef-read | tech `contact pc cont1` with a shrinking grid so the contact area is filled; macro PORT RECT on cont1 | painted cut rectangle in .mag | PORT and OBS geometry; Default contact area |
| LEFDEF-017 | lef-read | tech `obstruction obsv1,m1 via1`; OBS with LAYER via1 RECT, then LAYER met2 RECT | .mag paint (detects inherited secondary obstruction, Q-LEFDEF-3) | LEF layer name resolution; PORT and OBS geometry |
| LEFDEF-018 | lef-read | OBS with LAYER unknown, RECT, PATH, VIA, WIDTH statements and a POLYGON with an odd number of values | .mag paint and messages | PORT and OBS geometry |
| LEFDEF-019 | lef-read | existing .mag cell `inv` with: a non-port area label A, a point label Y, a port label B plus a non-port label B, labels `D[3]` and `vdd`; LEF macro inv with pins A, Y, B, D<3>, VDD, Z (no paint under Z), SIZE, OBS | resulting .mag labels, paint and properties (no LEFview) | Annotation of existing cells; Macro completion |
| LEFDEF-020 | lef-read | NONDEFAULTRULE with LAYER/WIDTH/SPACING/WIREEXTENSION, HARDSPACING, USEVIA, MINCUTS, PROPERTY, a VIA, then a duplicate rule name; DEF net using the rule | DEF read wire widths in .mag; warnings | LEF NONDEFAULTRULE section |
| LEFDEF-021 | lef-read | `lef read cells` (cells.lef exists), `lef read tech.tlef`, `lef read missing` | which file opened; error text | lef read side effects |
| LEFDEF-022 | lef-read | `lef read x.lef -import -anno -labels -noblockage -foo`; `lef read -annotate x.lef` | messages; cells created | lef read arguments; MACRO and SITE cell creation |
| LEFDEF-023 | lef-read | `lef datestamp`, `lef datestamp 12345`, read LEF, `lef datestamp default`, `lef datestamp abc`, `def datestamp 1` | printed messages; cell timestamps in .mag headers | lef datestamp |
| LEFDEF-100 | lef-write | Minimal tech (scmos-like) with lef section; cell `inv` (edit root) with ports A(0,in,signal), Y(1,out), metal1 obstruction; `lef write` | inv.lef bytes | LEF file grammar; Library header; Macro header lines; Pin enumeration and ordering |
| LEFDEF-101 | lef-write | Same cell; `lef write -tech` | inv.lef bytes (UNITS + LAYER blocks) | Library header; Technology layers; Output scale and LEF database units |
| LEFDEF-102 | lef-write | Same tech with cifoutput in nanometres, gridlimit 5, then gridlimit 10, then angstroms | DATABASE MICRONS value and coordinate decimals | Output scale and LEF database units; Distance number formatting |
| LEFDEF-103 | lef-write | Cell `inv` instance selected inside edit root `top`; `lef write` | inv.lef (no header) | Single-cell file layout |
| LEFDEF-104 | lef-write | Cell with negative lower-left bbox (-20,-10) | ORIGIN/SIZE lines | Macro header lines; Distance number formatting |
| LEFDEF-105 | lef-write | Tech with 0.5 nm internal unit; coordinates 3, -3, 7 on pin geometry | RECT texts (half-grid rounding, float32) | Distance number formatting |
| LEFDEF-106 | lef-write | Cell with FIXED_BBOX property differing from paint bbox | ORIGIN/SIZE | Macro boundary determination |
| LEFDEF-107 | lef-write | Tech with `bound` layer; cell with boundary paint offset from origin | ORIGIN/SIZE and RECT coordinates | Macro boundary determination |
| LEFDEF-108 | lef-write | Cell with LEFclass "CORE TIEHIGH", LEFsymmetry "X Y R90", LEFsite unit; no site cell | CLASS/SYMMETRY/SITE lines | Macro header lines |
| LEFDEF-109 | lef-write | Read a LEF with SITE unit, then write cell with LEFsite unit as edit root | SITE block in header | SITE blocks |
| LEFDEF-110 | lef-write | Site cell with FIXED_BBOX flag, root cell with different FIXED_BBOX | SITE SIZE value | SITE blocks (Q-LEFDEF-33) |
| LEFDEF-111 | lef-write | Cell with LEFproperties `CDL "inv x" MASK 1` and `A 1 B` variants | PROPERTYDEFINITIONS and PROPERTY lines | Library header; Macro PROPERTY lines |
| LEFDEF-112 | lef-write | writeall on a top with 3 children having different LEFproperties keys | PROPERTYDEFINITIONS key order | Library header (Q-LEFDEF-30) |
| LEFDEF-113 | lef-write | Tech with ~10 lef-section records incl. aliases, obstruction-only, bound, ignore, masterslice, overlap; `lef write -tech` | LAYER block order and contents | Technology layers (Q-LEFDEF-30/31/32) |
| LEFDEF-114 | lef-write | `lef read` of a tech LEF setting PITCH/WIDTH/SPACING/DIRECTION, then `lef write -tech` | LAYER ROUTING lines | Technology layers |
| LEFDEF-115 | lef-write | Tech with route and obstruction types sharing a LEF name; cell with obstruction-type paint | LAYER name used for obstruction type | Magic type to LEF layer name |
| LEFDEF-116 | lef-write | Pins with every direction/use/shape value incl. reset/scan/tieoff | PIN header lines | Pin header records (Q-LEFDEF-36) |
| LEFDEF-117 | lef-write | Tcl `set VDD vccd1; set GND vssd1`, pins with default use | USE lines, no antenna | Power and ground pin detection |
| LEFDEF-118 | lef-write | Tech with extract style devices; input pin to poly gates and output pin to tiedown diffusion | ANTENNAGATEAREA/ANTENNADIFFAREA values | Antenna areas |
| LEFDEF-119 | lef-write | Same, with `lef nocheck A`, then `lef nocheck *`, then `lef nocheck none` | antenna lines presence | lef nocheck net list; Antenna areas |
| LEFDEF-120 | lef-write | Labels listed in order Y(1), A(0), VDD(2), gap at index 3, OUT(4) | PIN order | Pin enumeration and ordering |
| LEFDEF-121 | lef-write | Two VDD labels same index on disconnected rails; two A labels on same shape | PORT block counts | Pin enumeration and ordering; PORT blocks |
| LEFDEF-122 | lef-write | Labels "A" and "B" both with index 0 | PIN records and error message | Pin enumeration and ordering (Q-LEFDEF-50) |
| LEFDEF-123 | lef-write | Port label on unmapped poly layer only | absence of PIN | PORT blocks (Q-LEFDEF-46) |
| LEFDEF-124 | lef-write | Pin net metal1-via1-metal2 | PORT RECT order, LAYER lines, via cut RECT | Geometry record emission; Contact cut records; Default pin geometry |
| LEFDEF-125 | lef-write | L-shaped and comb-shaped metal (multi-tile) pin and obstruction | RECT sequence (tile decomposition and order) | Geometry record emission |
| LEFDEF-126 | lef-write | Non-manhattan (45-degree) metal in pin and OBS, all four triangle orientations | POLYGON vertex texts | Geometry record emission (Q-LEFDEF-39) |
| LEFDEF-127 | lef-write | Stacked contact type in a pin | cut LAYER/RECT per plane | Contact cut records |
| LEFDEF-128 | lef-write | Cell `buf` built from `inv` subcells with labels only on buf | macro geometry includes subcell paint | Macro source geometry and layer masks |
| LEFDEF-129 | lef-write | `lef write -pinonly` and `-pinonly 10` on strip pins | PIN and OBS RECTs | Pin-only pin geometry |
| LEFDEF-130 | lef-write | `lef write -hide` on cell with pins and internal metal1/metal2 | full LEF | Hidden-detail pin geometry with zero setback; Obstructions with zero setback |
| LEFDEF-131 | lef-write | `lef write -hide -pinonly` | PIN rects = label rects; OBS cut-outs | Hidden-detail pin geometry with zero setback |
| LEFDEF-132 | lef-write | `lef write -hide` with OBS_BBOX property valid and malformed | OBS cover extent; error text | Obstructions with zero setback |
| LEFDEF-133 | lef-write | `lef write -hide 10` (lambda) and `-hide 0.1um` | full LEF | Hidden-detail pin geometry with positive setback; Obstructions with positive setback; Distance option values |
| LEFDEF-134 | lef-write | `lef write -hide 10 -pinonly 20` | full LEF (same as -hide 10) | Hidden-detail pin geometry with positive setback (Q-LEFDEF-37) |
| LEFDEF-135 | lef-write | `lef write -hide 60` on 100x200 cell | equality with plain write | Degenerate setback |
| LEFDEF-136 | lef-write | `lef write -toplayer` on metal1/metal2 pin, with and without nwell masterslice | PORT planes and OBS | Top-layer pins |
| LEFDEF-137 | lef-write | `lef write -nomaster` vs default on cell with nwell | presence of nwell geometry/cover | Macro source geometry and layer masks |
| LEFDEF-138 | lef-write | Pin names `a#1`, `x[3]`, `n 1` | PIN/END lines | Names written verbatim (Q-LEFDEF-35) |
| LEFDEF-139 | lef-write | `lef write`, `lef write cells.tlef`, `lef write out` | output file names | Output file naming |
| LEFDEF-140 | lef-write | Hierarchy top{a{c},b}; `lef writeall`, `-notop`, `-notop -all` | MACRO order and set | lef writeall arguments and scope; writeall macro order (Q-LEFDEF-41) |
| LEFDEF-141 | lef-write | `lef write -hide out.lef`, `lef writeall out.lef`, `lef write x -units 5` | help output, no file | lef write argument grammar; lef writeall arguments and scope |
| LEFDEF-142 | lef-write | `lef nocheck` listing with empty and non-empty list | printed/returned list | lef nocheck net list |
| LEFDEF-200 | def-read | tech with s0=0.001; DEF with UNITS 1000, DIEAREA (0 0)(200000 150000); `def read t` | FIXED_BBOX property of design cell (.mag) | Unit scaling; Die area; Design cell selection |
| LEFDEF-201 | def-read | s0=0.01, UNITS 1000, DIEAREA (1235 -1235)(2000 2000) | FIXED_BBOX values (rounding half away from zero) | Unit scaling |
| LEFDEF-202 | def-read | DEF without UNITS, DIEAREA (0 0)(10 20) | FIXED_BBOX (microns interpretation) | Unit scaling |
| LEFDEF-203 | def-read | DEF with two UNITS statements | FIXED_BBOX and wire coords | Unit scaling (Q-LEFDEF-79) |
| LEFDEF-204 | def-read | VIAS with 3-RECT via V12 used in a NETS route | painted .mag geometry | Rectangle-based DEF vias; Route via placement |
| LEFDEF-205 | def-read | Generated via CUTSIZE/LAYERS/CUTSPACING/ENCLOSURE/ROWCOL 1 2, placed in a route at (1000,500) | painted .mag geometry incl. cuts | Generated DEF vias; Route via placement |
| LEFDEF-206 | def-read | Generated via with ROWCOL 3 2 and cifoutput contact size (cut growing active) | painted cut rects + LEF warnings | Generated DEF vias |
| LEFDEF-207 | def-read | LAYERS given in order bottom top cut | warning text + geometry | Generated DEF vias |
| LEFDEF-208 | def-read | VIAS section: generated via followed by RECT-only via | geometry of second via (stale generation) | Generated DEF vias (Q-LEFDEF-60) |
| LEFDEF-209 | def-read | LEF defines V12; DEF VIAS redefines V12 | warning + route geometry | Via redefinition |
| LEFDEF-210 | def-read | LEF macro inv (SIZE 1.0 BY 2.0) placed with each of N,S,E,W,FN,FS,FE,FW at (1000 2000) | `use` transforms in written .mag | Component placement transform |
| LEFDEF-211 | def-read | .mag master with bbox (-10,-10)-(90,190) and no FIXED_BBOX, placed N and UNPLACED | transforms, warning | Component placement transform; Unplaced components |
| LEFDEF-212 | def-read | component name `core/u1,a` | use id in .mag, warnings | COMPONENTS section |
| LEFDEF-213 | def-read | component with unknown master and no .mag | error message, no use | COMPONENTS section |
| LEFDEF-214 | def-read | component master available only as FOO.mag on search path | use created, bbox | COMPONENTS section |
| LEFDEF-215 | def-read | PINS with LAYER then PLACED S, and PLACED then LAYER | painted rects + flabel/port lines in .mag | Pin geometry placement; Pin port labels |
| LEFDEF-216 | def-read | multi-PORT power pin (2 ports), PINS count 2 | port indices, count message | PINS section grammar; Pin geometry placement |
| LEFDEF-217 | def-read | two pins, first with DIRECTION OUTPUT USE POWER, second with none | port class/use of second pin in .mag | Pin port labels (Q-LEFDEF-64) |
| LEFDEF-218 | def-read | pin with `+ PLACED ... + LAYER a + LAYER b`, followed by another pin | geometry/labels of both pins | Pin geometry placement (Q-LEFDEF-65) |
| LEFDEF-219 | def-read | pin label sizes for tall (140x1000) and square (600x600) pins, font loaded vs none | flabel size/rotation in .mag | Pin port labels |
| LEFDEF-220 | def-read | regular net met1 ( 0 0 ) ( 1000 0 ), and with ( 0 0 0 ) | painted rects | Route wire segment geometry |
| LEFDEF-221 | def-read | regular net reversed vertical ( 0 1000 0 ) ( 0 0 ) | painted rect | Route wire segment geometry |
| LEFDEF-222 | def-read | special net width 3 ( 0 0 ) ( 10 0 ), width 0 leg, width 200 leg | painted rects | Route layer resolution and width selection; Route wire segment geometry |
| LEFDEF-223 | def-read | wildcard points ( 0 0 ) ( 1000 * ) ( * 500 ) | painted rects | Route wire segment geometry |
| LEFDEF-224 | def-read | nonmanhattan segment and `*` without reference | error messages, painted rects | Route wire segment geometry |
| LEFDEF-225 | def-read | NONDEFAULTRULES W2 (met1 WIDTH 280) used before and after ROUTED | painted widths | NONDEFAULTRULES section; NETS and SPECIALNETS grammar (Q-LEFDEF-74) |
| LEFDEF-226 | def-read | TAPER and TAPERRULE W2 / DEFAULT within a leg | painted widths | Route layer resolution and width selection |
| LEFDEF-227 | def-read | route with RECT ( -50 -20 50 20 ) after point | painted rect | Other route elements |
| LEFDEF-228 | def-read | route with POLYGON and VIRTUAL elements | errors + painted rects | Other route elements (Q-LEFDEF-70/71) |
| LEFDEF-229 | def-read | route with via orientation token and MASK 2 | errors + geometry | Route via placement (Q-LEFDEF-72) |
| LEFDEF-230 | def-read | special net with `+ SHAPE STRIPE` mid-route and one with unknown `+ MASK 1` followed by another net | painted geometry; processed count | NETS and SPECIALNETS grammar (Q-LEFDEF-69) |
| LEFDEF-231 | def-read | `def read -labels` on nets with two ROUTED statements | label list in .mag | Route painting and net labels |
| LEFDEF-232 | def-read | `def read -annotate` on pre-existing layout (subcells + top routing) | labels added, no paint (except pins) | Net annotation mode (Q-LEFDEF-80/81) |
| LEFDEF-233 | def-read | BLOCKAGES with LAYER met1 RECT, a PLACEMENT blockage, read with/without -noblockage | painted obstruction types | BLOCKAGES section (Q-LEFDEF-76) |
| LEFDEF-234 | def-read | count mismatches in each section | warning texts | Section count checks |
| LEFDEF-235 | def-read | DEF with FILLS, ROW, TRACKS, GROUPS, BEGINEXT sections | message log; layout unchanged | Top-level DEF grammar and section dispatch |
| LEFDEF-236 | def-read | DEF with DIEAREA before DESIGN; DEF with no DESIGN | cell names | Design cell selection (Q-LEFDEF-78) |
| LEFDEF-237 | def-read | TECHNOLOGY mismatch | warning text | Design cell selection |
| LEFDEF-238 | def-read | file lookup: `t`, `t.def`, `t.txt` variants | which file opened | DEF file lookup |
| LEFDEF-239 | def-read | >100 errors in one DEF | truncation message and summary | Diagnostics and error limits |
| LEFDEF-240 | def-read | full OpenROAD-style small DEF (sky130 inverter chain) after tech LEF + cell LEF | complete written .mag of design cell | all |
| LEFDEF-300 | def-write | tech demo (10 nm grid, metal1/metal2/via1 lef section); empty cell `e`; `def write` | full e.def | Output scale and number formatting; DEF output file grammar and section order |
| LEFDEF-301 | def-write | cell with paint bbox (0,0)-(100,50), no FIXED_BBOX; `def write` | DIEAREA line | DIEAREA |
| LEFDEF-302 | def-write | same cell with property FIXED_BBOX "-10 -10 110 60" | DIEAREA line | DIEAREA |
| LEFDEF-303 | def-write | cell `top`; `def write out -units 2000` and `def write r.txt` | file names, UNITS line, coordinates | def write command; Output file naming |
| LEFDEF-304 | def-write | `def write -hide 5`, `def write a b`, `def writeall`, `def nocheck`, `def` | messages, absence of files | def command argument validation; def writeall is not supported |
| LEFDEF-305 | def-write | single via1 26x26 between metal1/metal2 on a net; squares cut rule | VIAS section | Generated via definitions; VIAS section content |
| LEFDEF-306 | def-write | via1 80x80 DEF units with cut array | VIAS cut RECT lines | VIAS section content |
| LEFDEF-307 | def-write | two identical vias + one different-size via | VIAS count and names | Generated via definitions |
| LEFDEF-308 | def-write | tech with directional surround on via1 by metal1; metal1 wide horizontally | via name suffix _hx/_vx | Generated via definitions (Q-LEFDEF-105) |
| LEFDEF-309 | def-write | stacked contact (via1+via2 stacked type) | VIAS records for each residue cut | Contact region identification; Generated via definitions |
| LEFDEF-310 | def-write | LEF read with VIA named `via_1_2` then def write | VIAS count vs records written | VIAS section content (Q-LEFDEF-91) |
| LEFDEF-311 | def-write | uses of `inv` in all 8 orientations | COMPONENTS orientations and positions | Component orientation; COMPONENTS section |
| LEFDEF-312 | def-write | child with bbox (-5,0)-(20,10) in orientations N, S, FN | PLACED coordinates | COMPONENTS section (Q-LEFDEF-93) |
| LEFDEF-313 | def-write | child with FIXED_BBOX differing from paint bbox, rotated use | PLACED coordinates | COMPONENTS section |
| LEFDEF-314 | def-write | 2x2 array, 1x3 row array, 3x1 column array, reversed array 3:0 | component names, order, count | COMPONENTS section (Q-LEFDEF-94) |
| LEFDEF-315 | def-write | port labels with every class/use combination incl. default, reset, tieoff | PINS section | PINS section (Q-LEFDEF-103) |
| LEFDEF-316 | def-write | point port label and label on unmapped layer | PINS LAYER fragment / error | PINS section |
| LEFDEF-317 | def-write | top cell with vdd! (no port), A (port), labeled internal clkint, unlabeled net; extracted | SPECIALNETS / NETS membership and counts | Net classification |
| LEFDEF-318 | def-write | same as 317 with `-allspecial` | SPECIALNETS contents; no NETS | Net classification |
| LEFDEF-319 | def-write | Tcl `set globals(gnd) 1`, `set VDD vpwr` | special classification | Net classification (Q-LEFDEF-102) |
| LEFDEF-320 | def-write | nets named net-1, a_n5#, x1/sig*b | DEF net names | DEF net naming |
| LEFDEF-321 | def-write | net connecting to 20 subcell pins | connection list and line breaks | Net record header and connections |
| LEFDEF-322 | def-write | regular net single metal1 wire width 3 (odd) | ROUTED line | Wire segment derivation (Q-LEFDEF-95) |
| LEFDEF-323 | def-write | special net metal1 200x10 | ROUTED line with width | Wire segment derivation |
| LEFDEF-324 | def-write | regular net metal1 wire 8 wide (non-default) | TAPERRULE, NONDEFAULTRULES | Wire segment derivation; NONDEFAULTRULES section |
| LEFDEF-325 | def-write | wire → centred via → metal2 wire; wire → off-centre via | via continuation and jog points | Route record assembly |
| LEFDEF-326 | def-write | L-shaped metal1 route; two disjoint wires; dogleg | NEW records, * compression | Route record assembly (Q-LEFDEF-96, Q-LEFDEF-108) |
| LEFDEF-327 | def-write | long special net with many segments | line breaking at column 70 | Route record assembly |
| LEFDEF-328 | def-write | net whose port label sits on subcell paint only | fallback search, ROUTED output | Net geometry discovery |
| LEFDEF-329 | def-write | no .ext present | warning text; sections present | Preconditions and diagnostics |
| LEFDEF-330 | def-write | -anal with metal1..metal3 and one generated rule | NONDEFAULTRULES lines | NONDEFAULTRULES section |
| LEFDEF-331 | def-write | write cell with wide wire, then write second cell without | second file still has rule | Session state after writing (Q-LEFDEF-99) |
| LEFDEF-332 | def-write | obsm1 obstruction paint (non-electrical) | BLOCKAGES section | BLOCKAGES section |
| LEFDEF-333 | def-write | floating unlabeled metal1 net (not port/special) | BLOCKAGES or crash | BLOCKAGES section (Q-LEFDEF-101) |
| LEFDEF-334 | def-write | tech with 40+ lef layers forcing hash rebuild; several vias | order of VIAS and BLOCKAGES records | Table enumeration order |
| LEFDEF-335 | def-write | cell named (UNNAMED) | error, no file | Preconditions and diagnostics |
