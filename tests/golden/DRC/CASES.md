# Golden Cases — DRC

Test cases to be captured later by running Magic 8.3.694 (fc5daa9e) headless (`magic -dnull -noconsole`). Unless stated, each case uses a small synthetic tech file `drctest.tech` (planes: well, active, metal1, via1, metal2; types: nwell, pwell, ndiff, pdiff, poly, nfet, pfet, ndc, pdc, pc, m1, m1c(=via1 contact m1/m2), m2; CIF output style with scale 1 internal unit = 0.01 µm) plus the rule lines given, and a `.mag` layout built with `box`/`paint`. Standard capture procedure "CAP": `drc off; drc check; drc catchup; drc listall why; drc list count total`, then save the cell and record the `<< error_p >>`, `<< error_s >>`, `<< error_ps >>` sections of the `.mag` file (error tiles) and the `drc printrules` output.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| DRC-PARSE-1 | drc-rule-language | Rule `spac poly poly 2 touching_ok "s"`; `wid poly 2 "w"`; `width poly "w"` | tech-load messages; `drc printrules` | DRC section grammar |
| DRC-STYLE-1 | drc-rule-language | `style s variant A,B` with variant-restricted rules (see spec scenario); load `sA` and `sB` via `drc style` | printrules for each style; `drc listall style` | DRC styles and variants; drc style |
| DRC-STYLE-2 | drc-rule-language | Rules before any `style` line | `drc list style`, printrules | DRC styles and variants |
| DRC-SCALE-1 | drc-rule-language | `scalefactor 10`; `spacing m1 m1 25 touching_ok`; `maxwidth m1 25 bend_illegal`; `area m1 1050 20` | printrules (dist/cdist values) | Scale factor and distance units |
| DRC-SCALE-2 | drc-rule-language | DRC-SCALE-1 tech, then `scalegrid 1 2` | printrules; `drc *halo`; `drc *stepsize` | Scale factor; Halo and step size |
| DRC-SCALE-3 | drc-rule-language | `scalefactor 300` with `width m1 451`; then `scalegrid 1 2` | printrules before/after rescale | Scale factor (Q-DRC-1) |
| DRC-HALO-1 | drc-rule-language | widespacing m1 10 m1 3; no stepsize | `drc *halo`, `drc *stepsize` output | Halo and step size |
| DRC-ORDER-1 | drc-rule-language | `width m1 3 "A"` then `edge4way space m1 3 m1 m1 3 "B"` | printrules order for (space,m1) | Rule table ordering |
| DRC-OPT-1 | drc-rule-language | `spacing poly ndiff 1 touching_illegal "A"`, `width ndiff 3 "B"`; poly abutting ndiff | printrules for (poly,ndiff); CAP | Illegal-edge rule pruning |
| DRC-OPT-2 | drc-rule-language | `width m1 2 "small"`, `width m1 3 "big"`; m1 1 wide | CAP (why text) | Covered rule elimination |
| DRC-OPT-3 | drc-rule-language | edge4way (BOTHCORNERS) dist 2 followed by `edge` (single corner) dist 3 same OK/corner; L-shaped layout exercising the lost corner | CAP | Covered rule elimination (Q-DRC-5) |
| DRC-W-1 | drc-rule-semantics | `width poly 2 "w"`; poly [0,1]×[0,10] | CAP | width compilation; Standard edge rule check region |
| DRC-W-2 | drc-rule-semantics | `width poly 2 "w"`; poly L-shape with 1-wide notch at the inner corner | CAP | Standard edge rule check region (corner extension) |
| DRC-SP-OK-1 | drc-rule-semantics | `spacing m1 m1 3 touching_ok`; m1 [0,10]² and [12,20]×[0,10] | CAP | spacing compilation (touching_ok); check region |
| DRC-SP-OK-2 | drc-rule-semantics | `spacing m1,m1c m1,m1c 3 touching_ok`; m1 abutting m1 plus distant m1c | CAP | touching_ok (masking by touching material) |
| DRC-SP-OK-3 | drc-rule-semantics | `spacing poly pc 2 touching_ok` (different sets, reverse rules) | printrules; CAP | touching_ok with S1≠S2 |
| DRC-SP-ILL-1 | drc-rule-semantics | `spacing poly ndiff 1 touching_illegal`; poly touching ndiff; poly 1 unit from ndiff | CAP | touching_illegal |
| DRC-SP-ILL-2 | drc-rule-semantics | `spacing m1c via 2 touching_illegal` across planes (coincidence rule) with exactly coincident shapes | printrules; CAP | touching_illegal multi-plane coincidence rule |
| DRC-SP-MAN-1 | drc-rule-semantics | `spacing m1 m1 5 manhattan_dist`; diagonal neighbors (dx=4,dy=3); `drc euclidean on` | CAP | manhattan_dist; Euclidean corner filter |
| DRC-SP-SUR-1 | drc-rule-semantics | `spacing nwell pdiff 5 surround_ok`; pdiff inside nwell crossing its edge partially; pdiff outside at 3 | CAP | surround_ok; Trigger rules |
| DRC-SP-SUR-2 | drc-rule-language | `spacing poly ndiff 2 surround_ok` (same plane) | tech-load warning; printrules | surround_ok fallback to touching_ok |
| DRC-SP-CORN-1 | drc-rule-semantics | `spacing poly ndiff 1 corner_ok nfet`; poly and ndiff meeting at an nfet corner; and a real 0.5-gap violation | CAP | corner_ok |
| DRC-SP-MP-1 | drc-rule-language | spacing with types partitioned over two planes in each list | printrules (only first plane pair) | spacing compilation (Q-DRC-3) |
| DRC-EUC-1 | drc-rule-semantics | `spacing m1 m1 3 touching_illegal`; m1 [0,10]², [12,20]×[12,20]; euclidean on/off | CAP for both settings | Euclidean corner filter |
| DRC-EUC-2 | drc-rule-semantics | `spacing m1 m1 5 touching_illegal`; m1 [0,10]², [14,30]×[13,30]; euclidean on/off | CAP for both settings | Euclidean corner filter |
| DRC-EUC-3 | drc-rule-semantics | contact type with squares border (border 5 mask units) in `spacing m1c m1c 4 touching_illegal`; diagonal contacts; euclidean on | CAP | Euclidean filter derived borders |
| DRC-EUC-4 | drc-rule-semantics | split-tile m1 near corner extension, euclidean on | CAP | Euclidean filter split tiles |
| DRC-EUC-5 | drc-rule-semantics | `spacing m1 m1 5000 touching_illegal` diagonal neighbors, euclidean on | CAP | Q-DRC-20 |
| DRC-WS-1 | drc-rule-semantics | `widespacing m1 10 m1 4 touching_ok`; block [0,12]² and line [14,30]×[0,2] | CAP | widespacing compilation; Wide-spacing trigger |
| DRC-WS-2 | drc-rule-semantics | same rule; block [0,8]×[0,30] with line 2 away | CAP (no error) | Wide-spacing trigger |
| DRC-WS-3 | drc-rule-semantics | same; block exactly 10×10; with and without `option wide-width-noninclusive` | CAP for both | option statement; trigger inclusivity |
| DRC-WS-RL-V | drc-rule-semantics | `widespacing m1 10 20 m1 4 touching_ok` (runlength 20); vertical parallel run 25 and 15 | CAP | runlength (Q-DRC-7) |
| DRC-WS-RL-H | drc-rule-semantics | DRC-WS-RL-V rotated 90° | CAP; compare with V | runlength (Q-DRC-7) |
| DRC-EDGE-1 | drc-rule-language | `edge poly space 2 ~poly ~poly 2 "e"` (one-way) | printrules; CAP on L-shapes | edge compilation |
| DRC-EDGE-2 | drc-rule-semantics | `edge4way nfet ndiff 2 ndiff,ndc ndiff 2 "ov"` | CAP | edge4way; BOTHCORNERS |
| DRC-EDGE-3 | drc-rule-semantics | edge4way with OK types on another plane (`~m2c/metal2`) | printrules (check plane); CAP | edge compilation check plane |
| DRC-EDGE-4 | drc-rule-semantics | edge rule with cdist < dist (corner point offset) and euclidean on | CAP | Euclidean corner points |
| DRC-EXT-1 | drc-rule-semantics | `extend nfet poly 3`; gate length 2 | CAP | extend compilation |
| DRC-EXT-2 | drc-rule-semantics | `extend nfet poly 3 exact_width`; gate length 4 | CAP | extend exact_width (OUTSIDE) |
| DRC-EXT-3 | drc-rule-semantics | `extend ... exclusive` with second gate within distance | CAP | extend exclusive |
| DRC-EXT-4 | drc-rule-semantics | multi-plane extend (layers on different planes) | printrules; CAP | extend multi-plane trigger |
| DRC-OVH-1 | drc-rule-semantics | `overhang poly nfet 2`; endcap 1; poly absent on one side | CAP | overhang compilation |
| DRC-OVH-2 | drc-rule-semantics | multi-plane overhang | printrules; CAP | overhang multi-plane |
| DRC-SUR-1 | drc-rule-semantics | `surround via m2 1 absence_ok`; via with 0.5 surround; via without m2 | CAP | surround compilation |
| DRC-SUR-2 | drc-rule-semantics | `surround via m2 1 absence_illegal`; via without m2 | CAP | surround absence_illegal |
| DRC-SUR-3 | drc-rule-semantics | `surround pc poly 1 absence_illegal` (same plane) | CAP | surround absence_illegal same plane |
| DRC-SUR-4 | drc-rule-semantics | `surround m1c m1 1 exact_width` (same plane via stacked residue) and different-plane variant | CAP | surround exact_width (Q-DRC-18) |
| DRC-SUR-5 | drc-rule-semantics | `surround via m1 2 5 directional`; via with surround 1 on left/right and 3 top/bottom; and 1 everywhere | CAP | surround directional |
| DRC-SUR-6 | drc-rule-semantics | `surround m1c m1 1 absence_illegal` with contact squares border, euclidean on | CAP | DERIVED_TARGET border |
| DRC-RO-1 | drc-rule-semantics | `rect_only m1c`; L-shaped m1c | CAP (1×1 errors at concave corner) | rect_only compilation |
| DRC-RECT-1 | drc-rule-semantics | `rectangle via 10 odd`; via 4×3 | CAP | Rectangle size rule |
| DRC-RECT-2 | drc-rule-semantics | `rectangle via 3 any`; via 5×2; non-rectangular via | CAP | Rectangle size rule; rect-ness rules |
| DRC-AREA-1 | drc-rule-semantics | `area m1 20 4`; isolated m1 4×4; m1 5×4 | CAP | Area rule |
| DRC-AREA-2 | drc-rule-semantics | same rule; small m1 in a subcell abutting parent m1 so the region reaches the flattened buffer edge | CAP | Area rule (Q-DRC-8) |
| DRC-AREA-3 | drc-rule-semantics | area region made of m1 and m1c (both in layer set) where tiles start right of another set type | CAP | Q-DRC-21 |
| DRC-MW-1 | drc-rule-semantics | `maxwidth m1 10 bend_ok`; m1 12×12; m1 10×30 | CAP | Maxwidth rules (BENDS) |
| DRC-MW-2 | drc-rule-semantics | `maxwidth pmc 4 bend_illegal`; L-shape | CAP | Maxwidth bend_illegal |
| DRC-MW-3 | drc-rule-semantics | `maxwidth m1 4 both`; 3×10 bar | CAP | MAXWIDTH_BOTH |
| DRC-MW-4 | drc-rule-semantics | `maxwidth m5 100 bend_ok glass "slot"` with glass covering half of a wide m5 | CAP | maxwidth exclude types |
| DRC-MW-5 | drc-rule-semantics | `maxwidth m1 0 "x"` (4-token, w=0) | printrules; CAP | maxwidth compilation |
| DRC-OG-1 | drc-rule-semantics | `off_grid m1 5`; m1 [3,10]×[0,10]; m1 [−8,−3]×[−7,2] | CAP | Off-grid rule (Q-DRC-9) |
| DRC-ANG-1 | drc-rule-semantics | `angles poly 90`; split poly tile | CAP | Angle rules |
| DRC-ANG-2 | drc-rule-semantics | `angles m1 45`; split tiles 4×3 and 3×3 | CAP | Angle rules |
| DRC-ANG-3 | drc-rule-semantics | `angles m1 45-only`; Manhattan m1 rectangle with one beveled corner | CAP | angles 45-only corner rules |
| DRC-NM-1 | drc-rule-semantics | `spacing m1 m1 3 touching_ok`; `/` split m1 tile [0,10]² (m1 lower-right) and m1 [−2,0]×[3,5] | CAP | Diagonal edge checks (Q-DRC-10) |
| DRC-NM-2 | drc-rule-semantics | `width m1 4 angles`; thin 45° m1 wire of perpendicular width 2 | CAP | width angles option; diagonal checks |
| DRC-NM-3 | drc-rule-semantics | two parallel 45° m1 wires 1 unit apart (perpendicular) | CAP | Diagonal edge checks |
| DRC-EXC-1 | drc-rule-semantics | `exemption SRAM` spacing rule; MASKHINTS_SRAM over half of the layout; violations inside and outside | CAP | Exceptions and exemptions at check time |
| DRC-EXC-2 | drc-rule-semantics | `exception SRAM` width rule; same layout | CAP | Exceptions |
| DRC-EXC-3 | drc-rule-semantics | violation edge lying exactly on the left/right boundary of the hint | CAP | Q-DRC-11 |
| DRC-EXC-4 | drc-rule-semantics | violation edge lying exactly on the bottom/top boundary of the hint | CAP | Q-DRC-11 |
| DRC-EXC-5 | drc-engine | hint property defined in a subcell; violation in parent interaction area | CAP | Flattening (mask hints) |
| DRC-OVL-1 | drc-rule-language | default illegal overlap: poly in parent over ndiff in child | CAP (error_s, why 2); printrules overlap lines | Overlap rules and DRC paint table; Flattening |
| DRC-OVL-2 | drc-rule-language | `no_overlap pfet,nfet pfet,nfet`; nfet in two cells overlapping | CAP | no_overlap |
| DRC-XOV-1 | drc-engine | `exact_overlap m1c`; sibling cells with m1c at [0,4]² and [2,6]×[0,4] | CAP | Exact-overlap check |
| DRC-XOV-2 | drc-engine | exact_overlap contacts exactly coincident in two cells | CAP (no error) | Exact-overlap check |
| DRC-ENG-1 | drc-engine | two-level hierarchy; paint in leaf; inspect check planes before catchup | `.mag` `<< checkpaint >>` sections | Marking changed areas |
| DRC-ENG-2 | drc-engine | large flat cell spanning several step squares with a violation across a square boundary | CAP | Step square processing |
| DRC-ENG-3 | drc-engine | layout at negative coordinates crossing x=0,y=0 with violations | CAP | Step square processing; Interaction check grid |
| DRC-ENG-4 | drc-engine | parent paint near child paint (spacing violation across hierarchy) | CAP (error_s location) | Interaction area; Interaction check |
| DRC-ENG-5 | drc-engine | child with internal error, far from parent paint | CAP (parent error_p with why 4); `drc why` over it | Copying subcell errors; drc why |
| DRC-ENG-6 | drc-engine | same as ENG-5 but parent paint within halo of child | CAP | Interaction area |
| DRC-ARR-1 | drc-engine | 2×1 array with spacing violation across element boundary (spec scenario) | CAP | Array checks |
| DRC-ARR-2 | drc-engine | 3×3 array with overlapping elements and exact_overlap contacts | CAP (why 1) | Array checks; exact overlap |
| DRC-ARR-3 | drc-engine | rotated/mirrored array (transform R90) | CAP | Array checks |
| DRC-NMG-1 | drc-engine | two cells with 45° geometry whose overlap intersection is off-grid | CAP (why 5) | Flattening (off-grid) |
| DRC-RO-CELL-1 | drc-engine | read-only (vendor GDS) cell with violations | modified flag after catchup; CAP | Step square processing (read-only) |
| DRC-HALO-2 | drc-engine | `drc *halo 2` with a 5-unit spacing rule; violation 4 apart | CAP | Halo override |
| DRC-FLAT-1 | drc-engine | `drc *flatcheck` on hierarchical design of ENG-4 | printed total | Flat check |
| DRC-CHK-1 | drc-commands | `drc check` over partial box of hierarchy | `.mag` checkpaint sections before catchup | Forced recheck of an area |
| DRC-CMD-1 | drc-commands | `drc c`, `drc halo`, `drc *halo`, `drc why extra`, `drc help`, `drc help wizard` | console text | Command syntax and option matching |
| DRC-CMD-2 | drc-commands | `drc off; drc status; drc on; drc status` (Tcl) | results | Background checker control |
| DRC-CNT-1 | drc-commands | top with 3 error tiles, child with 1 | `drc count`, `drc count total`, `drc list count`, `drc listall count`, `drc list count total` | drc count (Q-DRC-14) |
| DRC-FIND-1 | drc-commands | top with 4 errors; `drc find` ×5, `drc find 3`, `drc find 9`, `drc find spacing` | box positions, console, Tcl results | drc find (Q-DRC-15) |
| DRC-WHY-1 | drc-commands | box touching but not overlapping an error | `drc why` output | drc why |
| DRC-WHY-2 | drc-commands | `drc list why`, `drc listall why` with `units microns` and default units | Tcl results | drc why; Why-text substitution |
| DRC-SUB-1 | drc-commands | why texts with `%d`, `%c`, `%a`, `%%`, `%x` after a `%d` | `drc why` output under units default/lambda/internal/microns | Why-text substitution (Q-DRC-16) |
| DRC-IGN-1 | drc-commands | `drc ignore spacing` where the last-defined why contains "spacing"; `drc ignore`; `drc ignore none` | console text; `drc why` | drc ignore (Q-DRC-17) |
| DRC-EUCL-CMD-1 | drc-commands | `drc euclidean`, `drc euclidean on`, `drc euclidean yes` | console text | drc euclidean |
| DRC-PR-1 | drc-commands | sky130A tech `drc printrules /tmp/x` | file content | drc printrules |
| DRC-STAT-1 | drc-commands | `drc rulestats`, `drc statistics` on sky130A | console text | drc rulestats and statistics |
| DRC-SHOWINT-1 | drc-commands | `drc *showint 10` on flat and hierarchical layouts | box after command | drc *flatcheck and drc *showint |
| DRC-CIF-1 | drc-cif-rules | `cifwidth MET1 140` without `cifstyle` | load messages | Mask style binding |
| DRC-CIF-2 | drc-cif-rules | `cifstyle gdsii` + `cifwidth MET1 140`; generated MET1 100 wide | CAP | Mask-layer check procedure |
| DRC-CIF-3 | drc-cif-rules | `cifspacing NW PW 600 touching_illegal` and `cifspacing NW NW 600 touching_ok` | printrules-equivalent (halo); CAP | Mask-layer rule statements |
| DRC-CIF-4 | drc-cif-rules | `cifarea VIA1 10000 50`; via squares generating small vias | CAP | Mask area rule |
| DRC-CIF-5 | drc-cif-rules | `cifmaxwidth MET5 3000 bend_ok/bend_illegal/both` | CAP | Mask maxwidth rule |
| DRC-CIF-6 | drc-cif-rules | CIF errors at negative coordinates and sub-unit widths | CAP | Mapping mask errors (Q-DRC-13) |
| DRC-CIF-7 | drc-cif-rules | `cifstyle` naming a non-current style; then `cif ostyle` switch | load messages; CAP before/after | Mask style binding |
| DRC-CIF-8 | drc-cif-rules | `scalefactor 10` drc section with only cif rules | `drc *halo` | Q-DRC-2 |
| DRC-PDK-1 | all | sky130A tech; sky130_fd_sc_hd inverter and a deliberately broken copy | CAP | regression over real PDK |
| DRC-PDK-2 | all | gf180mcu tech; standard cell row | CAP | regression over real PDK |
| DRC-PDK-3 | all | IHP sg13g2 tech; a metal1/metal2 MOM finger capacitor drawn with `box`/`paint` (stand-in for the Tcl `cap_cmomf` generator, M4) with one finger too close | CAP | regression over real PDK |
| DRC-STD-SKY130-HD | drc-engine | sky130A tech, PDK startup settings; every `sky130_fd_sc_hd` cell in sorted order: `load`, `select top cell`, `drc check`, `drc catchup`, `drc listall why`, `drc list count total`, `save` (design D14; label `golden-slow`) | stdout; the DRC sections of every saved cell (derived output) | standard-cell exit criterion |
| DRC-STD-GF180-7T5V0 | drc-engine | gf180mcuD tech, PDK startup settings; every `gf180mcu_fd_sc_mcu7t5v0` cell, as DRC-STD-SKY130-HD | as DRC-STD-SKY130-HD | standard-cell exit criterion |

## Authoring status (M3)

Case directories under `cases/` hold the inputs; `expected/` comes from the CI capture (golden-capture.yml). Generated by `tools/golden/authoring/m3_cases.py` from `drc_cases.py` (all 113 rows above, plus the two standard-cell corpus cases, which are generated from the installed libraries). The technology `drctest.tech` is committed as `tools/golden/authoring/drctest.tech`; each case's input carries it with the case's rule lines as its drc section. CAP runs with the box grown by 100 units around the top cell, so that `drc listall why` also reports the error areas outside the paint; the console is compared with the `drc-console` comparator (why texts and cell order canonical) and the saved cells without the `mag-checkpaint` mask.

Rows reworded for the integer grid or the drctest types (each case's `case.toml` notes say so): DRC-SP-CORN-1 and DRC-SUR-1 use distance 2 with a 1-unit gap or surround; DRC-MW-2 uses poly for `pmc`; DRC-MW-4 uses metal2 with `via` excluded; DRC-EDGE-3 uses `~m2/metal2`; DRC-SUB-1 leaves out `%%`, `%x` and a trailing `%` (Magic's output is undefined, Q-DRC-16; unit tests cover them); DRC-PR-1 compares the 26 MB printrules file by digest; DRC-XOV-1 adds a third interaction so that Magic reports the exact-overlap errors.

Deferred: DRC-CIF-2..7 (`deferred = "M5"` once captured, design Resolved Question 1).

