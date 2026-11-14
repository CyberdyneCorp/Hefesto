# MASK golden test-case inventory

Cases to be captured later by running Magic 8.3.694 (commit fc5daa9e) headless. Capabilities: mask-output-styles, mask-output-operators, mask-hierarchical-generation, mask-input-styles, gds-read, gds-write, cif-format-io. Unless stated otherwise, inputs are minimal hand-written technology files and `.mag` layouts; GDS captures compare decoded records (time fields masked).

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| MASK-OS-001 | mask-output-styles | tech with `style a`, `style b variant x,y`; `cif listall ostyle` | registered style names and order | Style declaration and the style list |
| MASK-OS-002 | mask-output-styles | style with `variant` blocks; load each variant; write GDS of a cell with metal1+metal2 | layers present per variant | Style loading and variant-conditional statements |
| MASK-OS-003 | mask-output-styles | `cif ostyle` with exact, prefix, ambiguous prefix, unknown | resulting current style / error | Output style selection |
| MASK-OS-004 | mask-output-styles | styles with `scalefactor 1`, `4.5`, `10 nanometers`, `2.5 angstroms`; `cif ostyle`; query output scale | scaleFactor, expander (via `cif` scale query and GDS UNITS/coords) | scalefactor statement; Unit normalisation |
| MASK-OS-005 | mask-output-styles | `units angstroms` style; write GDS of 1x1 lambda box | UNITS record and coordinates | units, stepsize and gridlimit statements |
| MASK-OS-006 | mask-output-styles | `gridlimit 5` with `scalefactor 10 nanometers`, squares on off-grid contact | cut coordinates | units, stepsize and gridlimit statements; squares-to-grid conversion by gridlimit |
| MASK-OS-007 | mask-output-styles | layer/templayer/labellayer, inline lists, empty layer | which layers are written; tech warnings | Mask layer declarations |
| MASK-OS-008 | mask-output-styles | lists with `*m1c`, `(a,b)`, alias, duplicate templayer names | generated geometry of referencing layers | Layer list syntax; Automatic stacked-contact inclusion |
| MASK-OS-009 | mask-output-styles | labels with none/port/noport/text on metal1; cell with plain label and port label | GDS TEXT layer/texttype and port boundary layer | labels statement and label layer mapping |
| MASK-OS-010 | mask-output-styles | `calma 300 0`, `calma 5 x` | tech errors; layer omitted from GDS | GDS layer mapping statement |
| MASK-OS-011 | mask-output-styles | `options` with each flag and an unknown word; `set-minimum-grid` with gridlimit | lambda after load, flags observed via outputs | min-width, render and options statements |
| MASK-OS-012 | mask-output-styles | every operator with valid and invalid argument counts | tech errors and which ops are kept | Operator statement arguments |
| MASK-OS-013 | mask-output-styles | `scalefactor 9` with `squares 12 44 44`, and with only `grow 30`; write CIF | CIF scale line (DS a b), coordinates | Unit normalisation; CIF reducer |
| MASK-OS-014 | mask-output-styles | style with `bloat-all` vs without, `scalefactor 10 nanometers`; squares on odd-sized contact | cut coordinates (reduction granularity) | Unit normalisation |
| MASK-OS-015 | mask-output-styles | style with grow 30 / shrink 10; `cif` halo query (e.g. via `cif see` interaction behaviour) | interaction radius | Interaction radius |
| MASK-OS-016 | mask-output-styles | contact type with squares/slots; LEF write or `cif` contact-size query | contact size value | Contact cut size query |
| MASK-OS-017 | mask-output-styles | style with `options no-errors` and too-small contact | feedback count | Mask generation error reporting |
| MASK-OP-001 | mask-output-operators | metal1 rect, scale 10; `or metal1` | mask geometry | Geometric model of mask planes; or, and, and-not |
| MASK-OP-002 | mask-output-operators | long tile partly outside area; `or`+`shrink`; `cif see` on sub-area | clipped geometry | Layer evaluation pipeline for an area |
| MASK-OP-003 | mask-output-operators | overlapping layers; `and`, `and-not` | geometry | or, and, and-not |
| MASK-OP-004 | mask-output-operators | two rects gap 4; `grow 2` then `shrink 2`; 45° triangle grow | geometry | grow and shrink (default, square metric) |
| MASK-OP-005 | mask-output-operators | `options grow-euclidean`; triangles of slopes 1, 2, 1/3; grow/shrink 10 | geometry | Euclidean grow and shrink |
| MASK-OP-006 | mask-output-operators | rects at positive and negative coords; `grow-grid 5` | geometry | grow-grid |
| MASK-OP-007 | mask-output-operators | narrow isolated line, narrow stub on wide shape, gridlimit 0 and 5; `grow-min 10` | geometry | grow-min |
| MASK-OP-008 | mask-output-operators | ndiff with poly/space neighbours; `bloat-or ndiff poly 3 space 1`; also split tile | geometry | bloat-or |
| MASK-OP-009 | mask-output-operators | pdiff with mixed right neighbours; `bloat-max`, `bloat-min` with negative values; inversion | geometry, error feedback | bloat-max and bloat-min |
| MASK-OP-010 | mask-output-operators | nfet+ndiff+ndc chain; `bloat-all` with and without clip 5; CIF-layer form | geometry | bloat-all |
| MASK-OP-011 | mask-output-operators | guard ring of contact, strip shapes, exactly-cut-size contact | cut geometry, errors | Contact strip pre-pass for squares and slots |
| MASK-OP-012 | mask-output-operators | contacts 90x90, 92x30, L-shape, negative-coordinate contact, gridlimit 5 | cut geometry | squares |
| MASK-OP-013 | mask-output-operators | `squares-grid 5 20 20 10 10` on several offsets incl. negative | cut geometry | squares-grid |
| MASK-OP-014 | mask-output-operators | `slots` 3/4/6/7/8-arg forms on stripes and large areas, offset/start | cut geometry | slots |
| MASK-OP-015 | mask-output-operators | ring with hole area 100; `close 100`, `close 101`, `close` | geometry | close |
| MASK-OP-016 | mask-output-operators | catecorner squares and notches; `bridge 5 4` | geometry | bridge |
| MASK-OP-017 | mask-output-operators | narrow tiles near limiting layer; catecorner pairs; `bridge-lim 4 6 L` | geometry | bridge-lim |
| MASK-OP-018 | mask-output-operators | L-shape, T-shape, region with hole, tie case; `maxrect`, `maxrect external` | geometry | maxrect |
| MASK-OP-019 | mask-output-operators | rect + triangle; `orthogonal`, `orthogonal fill` | geometry | orthogonal |
| MASK-OP-020 | mask-output-operators | isolated squares, non-isolated squares, triangle; `not-square` | geometry | not-square |
| MASK-OP-021 | mask-output-operators | regions touching/overlapping/diagonal to M; all four interact forms; paint and templayer M; scale 1 (Magic crashes at tech load on the interacting family unless the reducer is already 1, Q-MASK-12) | geometry | overlapping, nonoverlapping, interacting, noninteracting |
| MASK-OP-022 | mask-output-operators | flat cell with labelled net VDD; `net VDD metal1,metal2` and `net VDD`; a `cif see` before the write (Q-MASK-412) | geometry | net |
| MASK-OP-023 | mask-output-operators | labels A on regions; `tagged A`, `tagged A metal1`; scaleFactor 1 and 10 | geometry | tagged |
| MASK-OP-024 | mask-output-operators | top cell and child; `bbox`, `bbox top` | geometry per cell | bbox |
| MASK-OP-025 | mask-output-operators | cell with FIXED_BBOX; `boundary` | geometry | boundary |
| MASK-OP-026 | mask-output-operators | cell property MASKHINTS_HVI; `mask-hints HVI` | geometry | mask-hints |
| MASK-OP-027 | mask-output-operators | hierarchical write with squares, squares+grow, bbox layers in overlapping subcells | parent cell geometry | Hierarchical-mode truncation |
| MASK-OP-028 | mask-output-operators | non-Manhattan layout; `cif unfracture yes/no`; gds write | polygon partition | Non-Manhattan merge option |
| MASK-OP-029 | mask-output-operators | catecorner squares `[0,10]²`,`[12,22]²`; `bridge 5 4` with gridLimit 0 and 5 | geometry | bridge (expansion, grid snap) |
| MASK-OP-030 | mask-output-operators | overlapping corners `[0,10]² ∪ [9,20]²`; `bridge 5 4` | geometry | bridge (pass B pinch) |
| MASK-OP-031 | mask-output-operators | SE-catecorner pair where the bottom-left and bottom-right neighbours differ; `bridge 5 4` and `bridge-lim 5 4 L` | geometry | bridge, bridge-lim (Q-MASK-3) |
| MASK-OP-032 | mask-output-operators | `bridge 10 4` with x-gap 8, y-gap 2 (d > w); run on x86-64 and arm64 | geometry per platform | bridge (Q-MASK-227) |
| MASK-OP-033 | mask-output-operators | catecorner squares; `bridge-lim 5 4 L` with L empty, L = `[13,30]×[0,8]`, L = that plus `[6,14]×[15,30]` | geometry | bridge-lim (candidate order, erase, Q-MASK-229) |
| MASK-OP-034 | mask-output-operators | 2-wide wire, 2-high wire with solid neighbours above/below; `bridge-lim 4 6 L` with and without a limiting tile beside it; L as database type at scalefactor 10 | geometry | bridge-lim pass 0 (Q-MASK-5) |
| MASK-OP-035 | mask-output-operators | bridge-lim where a bridge creates new corners (staircase of three squares) | geometry | bridge-lim (Q-MASK-228) |
| MASK-OP-036 | mask-output-operators | scalefactor 10; mask layer trigger `[-17,23]×[0,10]`; `bloat-all M ndiff` and `bloat-all M ndiff 5` | geometry | bloat-all (case A, Q-MASK-232) |
| MASK-OP-037 | mask-output-operators | poly contact trigger with poly bloat type; two layers both using `bloat-all pc poly` | geometry of both layers | bloat-all (Q-MASK-230) |
| MASK-OP-038 | mask-output-operators | two triggers over the same two disjoint ndiff regions, d = 0, triggers in both orders | geometry | bloat-all (Q-MASK-231) |
| MASK-OP-039 | mask-output-operators | non-Manhattan ndiff reached by bloat-all, with and without clip | geometry | bloat-all (split tiles) |
| MASK-HG-001 | mask-hierarchical-generation | Leaf cell, S=10, `layer CMF; or m1`, m1 (0,0)-(10,4); `gds write` | Boundaries in the leaf structure | Per-cell output composition |
| MASK-HG-002 | mask-hierarchical-generation | Parent with no paint and one SREF to the leaf | Parent structure has no boundaries | Per-cell output composition |
| MASK-HG-003 | mask-hierarchical-generation | Cell bbox (0,0)-(10,10) with `FIXED_BBOX -20 -20 30 30` and layer `bbox`; GDS and CIF write | Generated bbox-layer geometry in GDS vs CIF | Own-paint generation area |
| MASK-HG-004 | mask-hierarchical-generation | Styles with grow 30 / shrink 25 / or-only; `cif statistics` plus geometry near cell edge | Effective radius inferred from clip extent of generated geometry | Halo radius |
| MASK-HG-005 | mask-hierarchical-generation | Parent metal (104,40)-(130,60) and one child bbox (0,0)-(100,100) near origin; layer `or m1; grow 30; shrink 30` | `cif *hier` output and parent residual; compare with a child moved far from the origin | Interaction area discovery; Q-MASK-400 |
| MASK-HG-006 | mask-hierarchical-generation | Two children with m1 gap of 5 internal units, S=10, `or m1; grow 30; shrink 30` | Parent residual rect (1000,100)-(1050,900) | Interaction area correction |
| MASK-HG-007 | mask-hierarchical-generation | Child pdiff under parent nwell, `layer PSD; or pdiff; and-not nwell` | Feedback error text/area and output geometry in parent and child | Parent/child disagreement reporting; Known limitations |
| MASK-HG-008 | mask-hierarchical-generation | Classification styles: or-only, and-not with templayer, bbox templayer dependency | Presence/absence of parent residuals per layer | Classification of hierarchically processed layers; Q-MASK-403 |
| MASK-HG-009 | mask-hierarchical-generation | `layer FILLBLK; or m1; maxrect` with two abutting children, and a variant with no m1 | Parent FILLBLK geometry | Non-local layers |
| MASK-HG-010 | mask-hierarchical-generation | Parent 300×100 with interacting children at x=139..141 (square boundary), stepsize unset vs `stepsize 50` | Parent residual identical across step sizes (or differences) | Interaction search grid |
| MASK-HG-011 | mask-hierarchical-generation | Parent poly crossing child ndiff; style layer reading the transistor type through and-not | Parent residual from cross-cell composition | Flattening for interaction processing |
| MASK-HG-012 | mask-hierarchical-generation | Child 10×20 arrayed 4×1 pitch 12 (and 90°-rotated 1×3 pitch 25), `or m1; grow 10; shrink 10` | Parent seam rectangles at x 100-120, 220-240, 340-360 | Array interaction geometry parameters; Array interaction correction; Array correction replication |
| MASK-HG-013 | mask-hierarchical-generation | 2×2 and 3×3 abutting arrays pitch 10, r=2, and a reversed-index array (xlo>xhi) with large radius | Parent array residuals | Array interaction regions; Array correction replication; Q-MASK-406 |
| MASK-HG-014 | mask-hierarchical-generation | 3×3 array with separation 20 > size+r | No array residual | Array interaction regions |
| MASK-HG-015 | mask-hierarchical-generation | Contact layer `or pc; and-not nwell; squares 2 4 4` across child seam; variant with `grow 2` after squares | Parent CC geometry (uncut residual vs cut) | Hierarchy-stopping operators; Q-MASK-402 |
| MASK-HG-016 | mask-hierarchical-generation | Style with `min-width 40` on M and thin residual seam | Parent residual widened to 40; stdout lines | Sliver growth of hierarchical pieces; Q-MASK-409 |
| MASK-HG-017 | mask-hierarchical-generation | MASK-HG-006 layout with `cif *hier write disable`, then with `CIFhier` property on parent; GDS and CIF | Presence of parent residual in each output | Hierarchy and array output switches; Q-MASK-410 |
| MASK-HG-018 | mask-hierarchical-generation | MASK-HG-012 layout, `cif *array write disable` | No seam rectangles | Hierarchy and array output switches |
| MASK-HG-019 | mask-hierarchical-generation | `cif *hier M1X` and `cif *array M` with box over layouts 006/012 | Highlighted areas (feedback/see output) | Hierarchical layer preview |
| MASK-HG-020 | mask-hierarchical-generation | Style with `net` operator, two cells written in one session after a `cif see` (the first `net` generation of a fresh session crashes Magic) | Generated net-layer geometry per cell | Q-MASK-412 |
| MASK-HG-021 | mask-hierarchical-generation | MASK-HG-006 layout, `cif flat out.cif` | Full CIF text (single `__CIF2__` symbol, no labels) | Flat CIF output; Q-MASK-411 |
| MASK-HG-022 | mask-hierarchical-generation | MASK-HG-006 layout, `cif see M1X`, `cif coverage M1X`, `cif paint M1X m2` | Highlight area, coverage numbers, painted m2 | Flat region generation |
| MASK-HG-023 | mask-hierarchical-generation | 45° wire layer; `cif unfracture x yes` vs off, GDS write | Boundary record count and union equality | Non-manhattan merge option; Q-MASK-413 |
| MASK-HG-024 | mask-hierarchical-generation | Vendor (`gds readonly true`) cell abutting parent m1, `or m1; grow 5; shrink 5` | Vendor structure bytes and parent residual | Vendor cells in hierarchical generation; Q-MASK-401 |
| MASK-HG-025 | mask-hierarchical-generation | Any hierarchical write followed by `cif statistics` twice | Printed counters | Generation statistics |
| MASK-CI-001 | mask-input-styles | scmos tech; `cif istyle` listall; tech with `style x variants (p),(c)` | style list output | Input style declaration and default style |
| MASK-CI-002 | mask-input-styles | tech with `variant (c)` / `variant *` blocks; select `x(p)` then `x(c)`; read same CIF | painted types per style | Variant-conditional statements |
| MASK-CI-003 | mask-input-styles | styles `gds` and `gds2`; `cif istyle gds`, `cif istyle gds2`, `cif istyle zz` | messages, current style | Input style selection |
| MASK-CI-004 | mask-input-styles | styles with `scalefactor 100`, `1.5`, `5 nanometers`, `2.5 nanometers`, `10 angstroms`; `cif scale in` each | reported scale, painted coordinates of a 1000-unit GDS box | Scalefactor statement; Input-scale reporting |
| MASK-CI-005 | mask-input-styles | `scalefactor 10 nanometers` + `grow 5`; GDS box | resulting geometry (grow equivalence) | Scale normalization |
| MASK-CI-006 | mask-input-styles | `gridlimit 5` vs `gridlimit 1000`; GDS rect edge at half-lambda | grid redefinition vs snap, warnings, .mag coords | Grid limit; Conversion of read units to internal units |
| MASK-CI-007 | mask-input-styles | `calma CPO 7 *` before CPO declared | tech load error; GDS layer 7 handling | CIF layer name table |
| MASK-CI-008 | mask-input-styles | `templayer CAA CAA` + `shrink 10` before `layer ndiff CAA` | ndiff geometry | Layer and templayer rules |
| MASK-CI-009 | mask-input-styles | ndiff/nfet ordering test cell (CAA, CSN, CPG) | .mag paint | Per-cell layer processing order |
| MASK-CI-010 | mask-input-styles | rules using `or`, `and`, `and-not` on overlapping CIF boxes incl. triangles | .mag paint | Input operators or, and, and-not |
| MASK-CI-011 | mask-input-styles | contact `grow 100` + `and`; also `shrink`; with output style grow-euclidean on/off and a 45° shape | .mag paint | Input operators grow and shrink |
| MASK-CI-012 | mask-input-styles | `grow-grid 5` on rects at (7,3)-(12,8) and (-7,-7)-(-2,-2) | .mag/plane geometry | Input operator grow-grid |
| MASK-CI-013 | mask-input-styles | `not-square` on isolated square, rectangle, touching squares, triangle | .mag paint | Input operator not-square |
| MASK-CI-014 | mask-input-styles | GDS: child with NWELL copyup, parent SREF and AREF 2x2; same via CIF | parent geometry, instances | Input operator copyup |
| MASK-CI-015 | mask-input-styles | GDS cell with boundary layer rect (and two rects; and redefinition) | FIXED_BBOX property, warnings | Input operator boundary |
| MASK-CI-016 | mask-input-styles | `templayer HM POLYHINT` + `mask-hints poly`; two templayers same name | MASKHINTS_poly property contents | Input operator mask-hints |
| MASK-CI-017 | mask-input-styles | `layer m1pin tagged VDD CMF` with S=1 and S=100 | painted m1pin | Input operator tagged |
| MASK-CI-018 | mask-input-styles | contact at half-lambda (50..250) and quarter-lambda; non-contact at 150 | .mag coords, rescale messages, DBLambda | Painting rule results; Conversion of read units to internal units |
| MASK-CI-019 | mask-input-styles | `cif rescale no` then off-grid input; snapping of 50/51/-50/-51 | .mag coords | Conversion of read units to internal units |
| MASK-CI-020 | mask-input-styles | CIF `DS 1 1 2;` with odd coordinates; `cif limit 1` vs 100 | resulting coords, warnings | Mid-read subdivision of read units |
| MASK-CI-021 | mask-input-styles | GDS file with pairs (4,9),(7,0),(7,1),(3,5),(7,5) under wildcard mappings; list `4,5 0,20` | layer assignment per element | GDS layer and datatype mapping |
| MASK-CI-022 | mask-input-styles | `labels CMFPIN port`, `labels X text`, `labels X sticky`, `labels X cell`; ignore after labels | label flags/ports/cell name in .mag | Label layer statements; Label placement semantics during read |
| MASK-CI-023 | mask-input-styles | GDS texts on unmapped layer with and without `options ignore-unknown-layer-labels` | labels created, messages | Label placement semantics during read |
| MASK-CI-024 | mask-input-styles | CIF polygons: L-shape, right triangle, general 30° polygon, self-overlapping, unclosed GDS boundary | .mag tiles incl. split tiles | Polygon to region conversion |
| MASK-CI-025 | mask-input-styles | tech load errors: op before layer, grow 0, unknown statement | error messages | Cifinput section grammar |
| MASK-CF-001 | cif-format-io | CIF using lower-case words, nested comments, missing `;` | parsed cells, messages | CIF lexical rules |
| MASK-CF-002 | cif-format-io | CIF with unknown command `Q` and `92` extension | messages, result | CIF input grammar |
| MASK-CF-003 | cif-format-io | `DS 3 25 2` with M=1 and M=10; DS without b; nested DS; DF without DS | geometry scale, messages | Symbol definitions and scale |
| MASK-CF-004 | cif-format-io | symbol defined twice; `DD`; `9 name` inside and outside DS; name collision | cell names | Symbol-to-cell naming on read |
| MASK-CF-005 | cif-format-io | calls with `R 0 1 T 200 300`, `M X`, `M Y`, combined, non-manhattan R; `91 id`; recursive call | instance transforms/ids | Calls and transformations |
| MASK-CF-006 | cif-format-io | geometry before L; unknown L (a 5- or 6-char L name overflows Magic's buffer and prints memory, Q-MASK-312: unit-tested only) | painted layers, messages | Layer selection |
| MASK-CF-007 | cif-format-io | boxes `B 40 20 20 10`, `B 3 2 10 0`, `B 3 2 -10 0`, rotated `0 1`, zero length | .mag geometry | Box command; Point scaling on input |
| MASK-CF-008 | cif-format-io | `R 10 50 50`, odd diameter with scale 1/2 | .mag geometry, warning | Round flash command |
| MASK-CF-009 | cif-format-io | `P` rectangle, L-shape, triangle | .mag geometry | Polygon command |
| MASK-CF-010 | cif-format-io | wires: straight, L-bend, 45° bend, reversal, acute angle (mitre error) | .mag geometry, messages | Wire command |
| MASK-CF-011 | cif-format-io | `94` with/without layer, unknown layer, ignored layer; `95` even/odd corners | labels in .mag | Label user extensions |
| MASK-CF-012 | cif-format-io | file without End; text after End; top-level geometry; undefined called cell | messages; edit cell contents | End of file and top-level geometry |
| MASK-CF-013 | cif-format-io | file producing >150 errors under `cif warning limit` and `none` | message count | Read diagnostics; cif warning option |
| MASK-CF-014 | cif-format-io | `cif write` of scmos inverter | full file text (header lines masked) | CIF output header |
| MASK-CF-015 | cif-format-io | 3-level hierarchy (top→a,b; a→c), `cif write` | DS numbering and order | CIF output symbol numbering and order |
| MASK-CF-016 | cif-format-io | cell with rects and four triangle orientations, R≠1 style, `(UNNAMED)` cell | DS/9/L/B/P lines | CIF output symbol definition format |
| MASK-CF-017 | cif-format-io | labels with/without output label layer; style with labellayer; `cif arealabels yes` | 94/95 lines | CIF output labels; cif output-format toggles |
| MASK-CF-018 | cif-format-io | instances in all 8 orientations; 1x3 and 2x2 arrays with ids; `cif idcell no` | C/91 lines | CIF output calls and arrays |
| MASK-CF-019 | cif-format-io | `cif flat` of 2-level hierarchy; GDS-only style (reducer 0) with `cif write` | file text, error | CIF output preconditions and flat output |
| MASK-CF-020 | cif-format-io | `cif` with no args; `cif xyz`; `cif list istyle` | file name, help, result | cif command dispatch; cif style options |
| MASK-CF-021 | cif-format-io | `cif read foo`, `cif write foo`, `cif write foo.cif` | files opened | cif read, write and flat options |
| MASK-CF-022 | cif-format-io | `cif scale in`, `cif scale out`, `cif lambda out` | values | cif scale and lambda options |
| MASK-CF-023 | cif-format-io | `cif limit`, `cif limit 500`, `cif limit 0`, `cif rescale no` | values, warning level | cif limit and rescale options |
| MASK-CF-024 | cif-format-io | `cif prefix /lib` then write; `cif prefix` clear | 9 lines | cif output-format toggles |
| MASK-CF-025 | cif-format-io | `cif drccheck no` then read | DRC pending state | cif drccheck option |
| MASK-CF-026 | cif-format-io | `cif polygon subcells yes` then read CIF with polygons; `cif unfracture yes` then write | no change on read; merged triangles on write | cif polygon and unfracture options |
| MASK-CF-027 | cif-format-io | `cif see CMF`, `cif paint CMF m1 newcell`, `cif coverage CMF box`, `cif list coverage`, `cif statistics` | feedback, painted cell, numbers | cif see, paint, coverage and statistics options |
| MASK-GR-001 | gds-read | Hand-built GDS with HEADER 600, LIBDIRSIZE, SRFNAME, REFLIBS, FONTS, GENERATIONS, UNITS 1e-3/1e-9, one structure; `gds read` | console messages, cell list | Library grammar and header processing |
| MASK-GR-002 | gds-read | LIBNAME "lib/my top", structure `my_top` | which cell is loaded into window | Library grammar and header processing |
| MASK-GR-003 | gds-read | Same layout encoded with UNITS mpd 1e-8, 1e-9, 1e-10 under a centimicron and a nanometer cifinput style | .mag output of each read; internal grid scale | Units and input scale |
| MASK-GR-004 | gds-read | 1 nm GDS with vertex x=15 (needs rescale) and a variant forcing the limit (num·r > 100) | .mag geometry, warnings | Coordinate conversion and automatic rescaling |
| MASK-GR-005 | gds-read | Top-down file (TOP before A), default options | cells, uses, absence of errors | Structure processing and cell creation |
| MASK-GR-006 | gds-read | BGNSTR dates (year 124), 12-byte BGNSTR; options `datestamp yes/no/1700000000`, `nodatestamp yes` | cell timestamps in saved .mag (TZ=UTC) | Structure timestamps |
| MASK-GR-007 | gds-read | Structure `A` defined twice with different boxes | contents of A, error text | Duplicate structures within one file |
| MASK-GR-008 | gds-read | Pre-loaded `INV.mag` then GDS defining INV; repeat with `noduplicates yes` and `unique yes` | cell names and contents | Cells already in memory before reading |
| MASK-GR-009 | gds-read | LEF-abstract cell `PAD` loaded, `noduplicates yes`, GDS containing PAD | properties GDS_START/END/FILE, timestamp | Cells already in memory before reading; Read-only vendor views |
| MASK-GR-010 | gds-read | Elements with NODE, ELFLAGS, PLEX, unknown PROPATTR on boundary | geometry, messages | Element dispatch and unsupported content |
| MASK-GR-011 | gds-read | cifinput with `calma M1 68 20`, `calma M1PIN 68 *`, `calma X * 5`; shapes on 68/20, 68/16, 1/5, 99/99 | resulting layers; unknown-layer messages | Layer and datatype mapping |
| MASK-GR-012 | gds-read | BOX elements: 5-point, non-rectangular 5-point, 4-point | painted rect, errors | BOX elements |
| MASK-GR-013 | gds-read | Boundaries: L-shape, unclosed square, doubly wound square, 3-point degenerate, self-intersecting bow-tie (manhattan) | .mag paint | BOUNDARY elements |
| MASK-GR-014 | gds-read | Non-Manhattan boundaries: right triangle, octagon, 30-degree trapezoid, concave diagonal polygon | .mag paint incl. split tiles | Non-Manhattan boundary decomposition |
| MASK-GR-015 | gds-read | Paths: type 0/1/2/4, widths 10 and 5, BGNEXTN/ENDEXTN, single-point type 0 and 2 | .mag paint | PATH elements |
| MASK-GR-016 | gds-read | Path with negative WIDTH, zero WIDTH, negative BGNEXTN, diagonal end with extension | .mag paint, messages | PATH elements (Q-MASK-114, Q-MASK-115) |
| MASK-GR-017 | gds-read | Paths with right bend, 45-degree bend, 135-degree bend, direction reversal, 170-degree bend (mitre limit) | .mag paint, messages | Path-to-polygon conversion |
| MASK-GR-018 | gds-read | Very wide (width 1e6 DB) 45-degree path | exact vertices | Path-to-polygon conversion (Q-MASK-118) |
| MASK-GR-019 | gds-read | Short inner-corner path (segment shorter than width) | exact vertices | Path-to-polygon conversion inner-corner correction |
| MASK-GR-020 | gds-read | Path with duplicate first point and duplicate interior points | .mag paint | Path-to-polygon conversion (Q-MASK-116) |
| MASK-GR-021 | gds-read | `gds savepaths yes`, two paths, style scale factor != 1 | property names/values in .mag | Path centerline properties |
| MASK-GR-022 | gds-read | TEXT with every PRESENTATION nibble 0x0..0xF, fonts 0..3 | label positions/fonts in .mag | TEXT element parsing |
| MASK-GR-023 | gds-read | TEXT with STRANS+MAG 0.5/0/20000+ANGLE 45.7, WIDTH after PRESENTATION, WIDTH without PRESENTATION; `gds magscale 2` and `default` | label size/rotation in .mag | TEXT element parsing; Text magnification scale option |
| MASK-GR-024 | gds-read | Labels on layer with no DRC width vs layer with DRC width 3 | default font size | TEXT element parsing (Q-MASK-122) |
| MASK-GR-025 | gds-read | Empty-string TEXT; off-grid TEXT at negative coordinates | labels, warnings | TEXT element parsing (Q-MASK-121) |
| MASK-GR-026 | gds-read | Labels on: port layer, text layer, ignored layer, layer without labels, unmapped pair; with/without `options ignore-unknown-layer-labels` | label types, sticky flags, messages | Label layer assignment |
| MASK-GR-027 | gds-read | `labels ... cell` layer with TEXT `real_name` in structure XYZ | cell name after read | Label layer assignment |
| MASK-GR-028 | gds-read | Pin shape before text, after text, no text; ports A,B,A | port indices, rects | Pin shapes and port labels |
| MASK-GR-029 | gds-read | Non-rectangular pin boundary (L-shape) with text inside lower part | label rectangle chosen | Pin shapes and port labels |
| MASK-GR-030 | gds-read | Pin shape on `text` (sticky) layer consumed by TEXT | port flags/index of label | Pin shapes and port labels (Q-MASK-123) |
| MASK-GR-031 | gds-read | SREFs with all 8 orientations (STRANS 0/0x8000 × ANGLE 0/90/180/270), plus ANGLE 44.9, -90, 450 | use transforms in .mag | Structure reference transforms |
| MASK-GR-032 | gds-read | SREF at half-grid origin with `rescale yes`, and with `rescale no` | internal scale, use positions | Instance placement scaling; Grid subdivision option |
| MASK-GR-033 | gds-read | AREF 3×2 unrotated, rotated 90 and mirrored; 4×1 with junk P1.y/P2; non-divisible column and row displacements | array params in .mag, errors | Array references (Q-MASK-124) |
| MASK-GR-034 | gds-read | AREF/SREF with PROPATTR 61 "a/b", 98 "x,y", 99 "1_2_0_0", malformed 99, unknown attr 5 before 61 | use ids, array limits | Instance properties (Q-MASK-125) |
| MASK-GR-035 | gds-read | Self-reference (A uses A), identical duplicate SREFs | uses, errors | Use placement rules |
| MASK-GR-036 | gds-read | SREF MAG 1.5, MAG 2 containing MAG 0.5, MAG 0.333333, MAG 0 (invalid), MAG of undefined cell | cell names, GDS_MAG_SOURCE, geometry | Magnified instances |
| MASK-GR-037 | gds-read | Top-down file with `gds ordering on`; and default ordering with a reference after EOF requiring rewind | read order, messages | Post-order reading and file repositioning |
| MASK-GR-038 | gds-read | STRNAME padded with 2 NULs for even-length name, `ordering on` | read success | Post-order reading and file repositioning (Q-MASK-126) |
| MASK-GR-039 | gds-read | `gds flatten yes` with a 2-shape VIA used as AREF 2×2; `gds flatten 3` with 3-shape cell; `gds flatten 0` | parent paint, presence of uses | Flattening small cells on input |
| MASK-GR-040 | gds-read | Default flatglob: `pcell_CDNS_1` with/without subcells; `gds flatglob none`; `gds flatglob via*` | parent paint/uses | Flattening small cells on input |
| MASK-GR-041 | gds-read | Contact split between child (cut) and parent (metal) with flatten: verify combined cifinput result | .mag contact types | Flattening small cells on input |
| MASK-GR-042 | gds-read | `gds polygon subcells temporary`, `keep`, `none` with 2 triangles + 1 rect; plus real structure named `polygon_ring` in the keep and none files (Magic's temporary mode frees a genuine `polygon*` cell, Q-MASK-127: unit-tested only) | cells created, final paint | Polygon subcells (Q-MASK-127) |
| MASK-GR-043 | gds-read | `gds path subcells yes` with 3 paths | cells path00001..3 | Path subcells |
| MASK-GR-044 | gds-read | `gds drccheck no` vs default | DRC pending status of cells | End-of-structure processing |
| MASK-GR-045 | gds-read | Reference to never-defined `MISSING` | messages, empty cell | End-of-read cleanup |
| MASK-GR-046 | gds-read | `gds readonly yes` on a 1 nm library (uncompressed and .gds.gz) | GDS_START/GDS_END/GDS_FILE values, flags | Read-only vendor views |
| MASK-GR-047 | gds-read | `gds readonly yes` on 1e-8 file; and on 1e-10 file with angstrom output style | refusal message | Read-only vendor views |
| MASK-GR-048 | gds-read | readonly read with PDK_ROOT set to a prefix of file path | GDS_FILE string | Read-only vendor views (Q-MASK-128, Q-MASK-129) |
| MASK-GR-049 | gds-read | readonly read of two-level library (A leaf, B uses A), empty structure, duplicate names, reference cycle | cell timestamps vs. reference FNV implementation | Content-hash timestamps for read-only cells |
| MASK-GR-050 | gds-read | Same library rewritten with different BGNLIB/BGNSTR dates | timestamps equal | Content-hash timestamps for read-only cells |
| MASK-GR-051 | gds-read | readonly + `datestamp no`, readonly + `datestamp 1700000000` | timestamps | When content-hash timestamps are assigned |
| MASK-GR-052 | gds-read | Edit a vendor cell then save; change GDS_START/END via property command then save | timestamps of cell and parent | When content-hash timestamps are assigned |
| MASK-GR-053 | gds-read | Vendor cell .mag in subdirectory with relative GDS_FILE, file only as .gz | stamp found | When content-hash timestamps are assigned |
| MASK-GR-054 | gds-read | `gds maskhints yes` and `gds maskhints MET1` with output style having mask-hints on MET1, input geometry not reproduced by output rules | MASKHINTS_MET1 property | Mask hints on input |
| MASK-GR-055 | gds-read | `gds warning limit`, `none`, `redirect err.txt` with file producing >100 errors | messages/err file | Read diagnostics |
| MASK-GR-056 | gds-read | Option queries and prefix forms (`gds noduplicates t`, `gds ord on`, `gds flatten`) | reported states | Read option syntax and defaults |
| MASK-GR-057 | gds-read | `gds read chip` where only chip.gds.gz exists; `gds read chip.gds` | which file opened | Read command and file lookup |
| MASK-GR-058 | gds-read | `gds library yes` then read | no window load | Library grammar and header processing |
| MASK-GR-059 | gds-read | GDS with truncated final structure (no ENDSTR before next BGNSTR) | geometry of following cell | Structure processing and cell creation (Q-MASK-110) |
| MASK-GR-060 | gds-read | Top-down file where an in-memory cell is referenced before definition with `noduplicates yes` | whether cell overwritten | Cells already in memory before reading (Q-MASK-112) |
| MASK-GW-001 | gds-write | Minimal tech (scalefactor 10, layer M1 = metal1, calma 49 0); cell `top` with metal1 (0,0)-(4,2); `gds datestamp 0`; TZ=UTC; `gds write top` | Full file bytes (mask 24 modification-date bytes) | Complete annotated example; Record encoding; Library header and trailer |
| MASK-GW-002 | gds-write | Same tech; `units angstroms` + `scalefactor 25 angstroms`; one rect | UNITS record bytes and XY values | Library header and trailer; Output scale factors |
| MASK-GW-003 | gds-write | Session: write angstrom style, then switch to nanometer style and write again | UNITS of the second file | Library header and trailer (Q-MASK-201) |
| MASK-GW-004 | gds-write | `scalefactor 25 angstroms` without `units angstroms` | Error text and XY values | Output scale factors (Q-MASK-225) |
| MASK-GW-005 | gds-write | Labels with texts `A<0>`, `a%b&c`, 36-char text, control char; strict vs `calma-permissive-labels` vs `string-limit` | STRING record bytes; label text after write | String records and character mapping; Magic write-time side effects |
| MASK-GW-006 | gds-write | Cells `nand-2`, `a-b` + `a_b`, non-ASCII name, 40-char name with string-limit | STRNAME/SNAME bytes | Structure naming |
| MASK-GW-007 | gds-write | `gds lower no` with mixed-case cell and labels | All string records | Write option settings (lower) |
| MASK-GW-008 | gds-write | `gds datestamp 1700000000`, `datestamp yes`, `datestamp no`, `nodatestamp yes`; cell timestamp 1600000000; TZ=UTC and TZ=America/New_York | BGNLIB/BGNSTR creation dates | Date records |
| MASK-GW-009 | gds-write | Hierarchy top{a,b}, a{c}, b{c}; .mag use order a then b | Structure order; XXXXX ids when names illegal | Structure ordering; Structure naming |
| MASK-GW-010 | gds-write | Cell with 200 uses edited (moved/deleted) then written | SREF order vs .mag order | Structure ordering (Q-MASK-204) |
| MASK-GW-011 | gds-write | `gds library yes` on top{inv} | Structure list, LIBNAME | Write option settings (library); Structure ordering |
| MASK-GW-012 | gds-write | Eight use orientations (identity, 90, 180, 270, and their mirrors) of one cell | STRANS/ANGLE/XY bytes per SREF | Cell reference elements |
| MASK-GW-013 | gds-write | Arrays: 3x2 identity at origin; rotated 2x2; array with indices 1..3; array with descending indices | AREF COLROW, XY, PROPATTR 61/99 | Cell reference elements |
| MASK-GW-014 | gds-write | Same arrays with `gds arrays yes` | SREF list and XYs, absence of PROPATTR | Array flattening on output |
| MASK-GW-015 | gds-write | Use with non-default id `myinst` and default id `sub_0` | PROPATTR 61 PROPVALUE | Cell reference elements |
| MASK-GW-016 | gds-write | L-shape metal1 (0,0)-(4,2)+(0,2)-(2,4), `merge no` | BOUNDARY elements and order | Unmerged boundary output; Tile decomposition and enumeration order |
| MASK-GW-017 | gds-write | Same L-shape, `merge yes` | Polygon XY points | Merged polygon output |
| MASK-GW-018 | gds-write | `merge yes`: ring with hole (square annulus), U-shape, comb needing >200 points, isolated rect | Polygon count and XY | Merged polygon output (Q-MASK-217) |
| MASK-GW-019 | gds-write | Non-Manhattan: 45-degree triangles of all 4 orientations, `merge no` and `merge yes` | Triangle XY order | Unmerged boundary output; Merged polygon output (Q-MASK-218) |
| MASK-GW-020 | gds-write | Output layer without calma statement, layer with calma 300 (invalid), templayer | Elements present | Structure body order |
| MASK-GW-021 | gds-write | `labellayer` with calma 63 5 and tiles of odd widths | TEXT records (position rounding) | Label layers |
| MASK-GW-022 | gds-write | Labels: default font point label; font 0 size 80 N rot 90; all 9 justifications; rectangle label on op-less layer | TEXT/PRESENTATION/MAG/ANGLE/BOUNDARY bytes | Label TEXT records |
| MASK-GW-023 | gds-write | Port labels idx 2,1,1 + plain labels; style with `labels metal1 port` vs only `labels metal1` | TEXT order and presence | Label TEXT records (Q-MASK-205, Q-MASK-206) |
| MASK-GW-024 | gds-write | `gds labels no` with labels and labellayer | Presence of TEXT elements | Write option settings (labels); Label layers |
| MASK-GW-025 | gds-write | `FIXED_BBOX` larger than geometry with a grow operator | Mask geometry extent | Mask geometry for each structure |
| MASK-GW-026 | gds-write | Cell with `CIFhier` property while `cif *hier write disable` | Interaction geometry presence | Mask geometry for each structure |
| MASK-GW-027 | gds-write | Read vendor lib with `gds readonly yes`; write top using vendor cells | Copied byte ranges; BGNSTR/STRNAME regenerated | Verbatim copy of read-only cells |
| MASK-GW-028 | gds-write | Vendor cell with old-style GDS_BEGIN property | Verbatim BGNSTR copy | Verbatim copy of read-only cells |
| MASK-GW-029 | gds-write | Vendor cell with corrupted GDS_END; renamed vendor cell; missing GDS_FILE | Fallback behaviour and error status | Verbatim copy of read-only cells (Q-MASK-216) |
| MASK-GW-030 | gds-write | Vendor cell instanced with magnification 1.5 (cell_mag1p5) | SREF target name in output | Verbatim copy of read-only cells |
| MASK-GW-031 | gds-write | `gds polygon subcells yes` on read-only read, then write | Absence of polygonNNN structures | Verbatim copy of read-only cells |
| MASK-GW-032 | gds-write | LEF abstract cell with GDS_FILE + GDS_START (full dump); with `LEFview no_prefix` | Structure names, prefixes, SNAME rewrites | Full dump of a vendor library (Q-MASK-211, Q-MASK-214) |
| MASK-GW-033 | gds-write | `gds addendum yes` with vendor cells | Absence of vendor structures | Addendum mode |
| MASK-GW-034 | gds-write | Abstract (LEF) cell without GDS_FILE, `abstract disallow` and `allow` | Status, structures present, ENDLIB | Abstract views; Write result and error propagation |
| MASK-GW-035 | gds-write | Missing subcell file, `undefined disallow` and `allow` | Status, SREF present, structure absent | Write preconditions and subtree loading |
| MASK-GW-036 | gds-write | `gds compress 0`, `6`, `9`, `12` | File name extension; gunzip equality | Compressed output; Write command and output file naming |
| MASK-GW-037 | gds-write | `gds contacts yes` then write | Output identical to baseline | Write option settings (contacts) |
| MASK-GW-038 | gds-write | `gds write ./dir/top`, `gds write top`, bare `gds` | Created file names | Write command and output file naming (Q-MASK-210) |
| MASK-GW-039 | gds-write | No cifoutput style loaded, `gds write x` | Error and empty file | Write preconditions (Q-MASK-219) |
| MASK-GW-040 | gds-write | R8 encoding unit test: 0.001, 1e-9, 1e-4, 1e-10, 1, 0.5, 90, 180, 270, -1, 0.1 | 8-byte encodings | Eight-byte real encoding |

## Use-order probe (m5-mask-gds-cif task 1.5, design D7)

Not MASK rows of the open-questions inventory: these cases record Magic's use enumeration order for INDEX's trace test (task 5.1). Each runs `tools/golden/harness/use_order/use_order_probe.tcl` in a fresh session and compares its `trace.txt` (format: `tests/data/db/use_index/README.md`). Generated by `tools/golden/authoring/use_order_cases.py`. Children `ca` (4x4) and `cb` (6x3); use k is at a fixed permutation of a 6x6 grid with pitch 20. MASK-UO-001..008 frame the uses with paint and disable hierarchical and array generation, so a readout `gds write` does not search the index; MASK-UO-009..012 are plain writes.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| MASK-UO-001 | gds-write | 6 then 12 `getcell` placements; `getcell` on two existing uses (DBCellFindDup) | SREF order after each step; rejected lookups | Structure ordering |
| MASK-UO-002 | gds-write | 12 uses; `expand` over a box surrounding the frame, over the uses' bbox grown by 1, then over the uses' bbox itself | SREF order after each search (bins built by the last) | Structure ordering |
| MASK-UO-003 | gds-write | partial `expand` with 9 uses, then with 10 | SREF order: unbinned, then binned | Structure ordering |
| MASK-UO-004 | gds-write | 12 uses + partial search; 5 more + partial search; 6 more + partial search (rebuild with bins present); 2 lookups | SREF order after each step | Structure ordering |
| MASK-UO-005 | gds-write | `select cell <id> at <ll>` + `delete` with 6 unbinned uses, then with 13 uses, then from bins | SREF order after each removal | Structure ordering |
| MASK-UO-006 | gds-write | 6 unbinned uses; paint in child `ca` grows its bbox (DBReComputeBbox re-placement) | `cellname list instances ca`; SREF order before and after | Structure ordering |
| MASK-UO-007 | gds-write | 12 binned uses; grow `ca`, then `cb` | as MASK-UO-006 | Structure ordering |
| MASK-UO-008 | gds-write | framed `top.mag` with 12 uses in a non-alphabetical file order; `load`; partial search | SREF order after the read and after the search | Structure ordering |
| MASK-UO-009 | gds-write | fresh session: `load` of `top.mag` (12 uses, no paint, so uses touch its bbox); one `gds write` | SREF and structure order of the first write (binned by subtree loading) | Structure ordering; Write preconditions and subtree loading |
| MASK-UO-010 | cif-format-io | as MASK-UO-009 with one `cif write` | symbol numbering and `C` call order | CIF output symbol numbering and order |
| MASK-UO-011 | gds-write | fresh session: framed `top.mag` with 12 uses; `gds write` twice | SREF order of both writes (the first write's CIFGenSubcells searches bin the root) | Structure ordering; Write preconditions and subtree loading |
| MASK-UO-012 | gds-write | fresh session: `top.mag` with 13 uses, two overlapping; `gds write`, `cif write`, `gds write` | SREF order, CIF numbering and calls of each write | Structure ordering |
