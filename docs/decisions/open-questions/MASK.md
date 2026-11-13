# MASK open questions

Area MASK: mask output styles and operators, hierarchical mask generation, mask input styles, GDS read/write, CIF format I/O. ID ranges: 1–99 output styles/operators, 110–199 gds-read, 200–299 gds-write, 300–399 mask-input-styles and cif-format-io, 400–499 hierarchical generation. Q-MASK-334 duplicates Q-MASK-1 (grow-grid rounding); resolve together.

### Q-MASK-1: grow-grid rounds by |c| mod g on both sides
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: lower/left edges move down by `|c| mod g`, upper/right edges move up by `|c| mod g`; correct only for non-negative lower edges and non-positive upper edges (e.g. upper edge 12 with g=5 becomes 14, not 15; lower edge -7 becomes -9, not -10).
- Evidence: cif/CIFgen.c:533-541 (corrected 2026-11-13: The behaviour is described correctly, but the line numbers have moved. The tile is first scaled by cifScale (cif/CIFgen.c:519-522). Edges then move by |c| mod g at cif/CIFgen.c:526-533, not 533-541: 12 with g=5 becomes 14, and -7 becomes -9. This is the same function as Q-MASK-334. No sky130, gf180mcuD or IHP cifoutput/cifinput section uses grow-grid.)
- Options: replicate | fix (snap lower edges to floor, upper edges to ceil multiples of g) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto moves each edge by |c| mod g, as Magic does (GDS identity). With it OFF, it rounds outward to the g-grid: lower/left edges go to floor(c/g)*g and upper/right edges to ceil(c/g)*g. Infinite edges stay unchanged in both modes. Because the grid is only rounded outward, the result still contains the input. None of the reference PDKs use grow-grid, so the PDK goldens are the same in both modes. Resolve together with Q-MASK-334. (corrected 2026-11-13, was replicate with an opt-in corrected mode)

### Q-MASK-2: close threshold is an area but normalised as a length
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `close a` compares a summed area (mu²) against `a`, but unit normalisation multiplies/divides `a` linearly by the scale factors, so the effective area threshold depends on the reduction factor.
- Evidence: cif/CIFgen.c:2205-2215, cif/CIFtech.c:2562-2567, cif/CIFtech.c:2655-2656 (corrected 2026-11-13: The behaviour is described correctly, but the CIFtech.c lines have moved. The comparison is `atotal < growDistance` at cif/CIFgen.c:2205-2223. In CIFTechOutputScale the threshold is multiplied by d at cif/CIFtech.c:2500-2502 and divided by lexpand at 2648-2649, so it scales linearly. It also enters the reducer GCD as a length at CIFtech.c:1912-1915. PDK uses: sky130A (7), gf180mcuD (4), ihp-sg13g2 (2). Their styles load with d=1, and bloat-all blocks any reduction (Q-MASK-13), so for the PDK styles the declared value is the effective threshold.)
- Options: replicate | fix (scale quadratically) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto scales the close threshold linearly, as Magic does. With it OFF, the threshold is rescaled by the square of each unit change and compared exactly in 64-bit (atotal·L² < a·D² for cumulative multiply D and reduce L), so `close a` always means a in the declared mask units². The reduction/GCD choice stays the same in both modes, so scaleFactor, expander, the GDS UNITS record and the CIF DS scale do not change. Only the threshold differs. PDK styles behave the same in both modes because d=1 and no reduction happens. (corrected 2026-11-13, was replicate)

### Q-MASK-3: bridge/bridge-lim test the wrong bottom neighbour for the SE corner
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the loop meant to find the bottom neighbour at the tile's right end tests the right neighbour's edge instead and never advances, so the bottom-LEFT neighbour is used.
- Evidence: cif/CIFgen.c:1894-1895, cif/CIFgen.c:4509-4510
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. Confirmed at cif/CIFgen.c:1894-1895 and 4509-4510. The loop condition tests RIGHT(tp1), which is always >= RIGHT(tile), so tp2 stays LB(tile). The pass-2 functions (CIFgen.c:2020-2021, 4656-4657) correctly use RIGHT(tp2). With magic-compat ON, Hefesto uses the bottom-left neighbour for the pass-1 SE corner test, as Magic does. bridge is used by sky130A (12) and ihp-sg13g2 (3), so GDS identity needs this. With it OFF, Hefesto uses the bottom neighbour at the tile's right end, as pass 2 does. This adds missed SE bridges and drops spurious ones. (corrected 2026-11-13, was replicate)

### Q-MASK-4: bridge width/grid expansion is a no-op (withdrawn)
- Kind: doc-mismatch
- Magic behavior: withdrawn. The helper that computes the minimum-width, grid-snapped bridge rectangle stores its result back through the rectangle pointer (`*area = r`), so the expansion IS applied. The `bridge` requirement now specifies the expansion exactly.
- Evidence: cif/CIFgen.c:1703-1803 (line 1802)
- Options: n/a
- Recommendation: none; kept for traceability.
- Resolution: n/a — withdrawn; the spec follows Magic's code and applies the expansion (spec conventions §2.7, 2026-11-07).

### Q-MASK-5: bridge-lim searches database planes with mask-unit coordinates
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: limiting database types are searched on internal-unit planes using mask-unit rectangles (no division by scaleFactor); erasure of limiting geometry likewise uses whatever scale is current.
- Evidence: cif/CIFgen.c:4170-4228, cif/CIFgen.c:4379-4426
- Verified 2026-11-13: Confirmed. bridgeLimSrTiles (cif/CIFgen.c:4169-4198) passes the mask-unit `area` unchanged to DBSrPaintArea on cd_planes, and Xmax/Ymax are raw tile edges of the plane that was searched. bridgeErase (CIFgen.c:4376-4400) erases database tiles through cifPaintFunc with cifScale=1 (set at CIFgen.c:5247), so the tiles are not scaled. The correction to the entry: scaleFactor=1 is not the common case. No reference PDK uses bridge-lim, so this matters only for user techs.
- Options: replicate | fix (convert units) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto replicates Magic: the mask-unit rectangle is searched against database planes, overlap extents are taken raw, and limiting database tiles are erased unscaled. With it OFF, the search area is converted to internal units with outward rounding (floor for lower edges, ceil for upper edges), and database tile edges are multiplied by scaleFactor before they are used for Xmax/Ymax and for erasure. In both modes the style loader warns when bridge-lim names database types and scaleFactor ≠ 1, the case where the two modes differ. (corrected 2026-11-13, was replicate)

### Q-MASK-6: tagged uses internal-unit label coordinates against mask planes
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the label rectangle (internal units) is divided by scaleFactor for database planes and used unscaled on mask-unit planes; tags only work correctly when scaleFactor = 1.
- Evidence: cif/CIFgen.c:5548-5570, cif/CIFgen.c:4078-4110 (corrected 2026-11-13: The output behaviour is confirmed: CIFgen.c:5545-5566 passes the internal-unit lab_rect to cifSrTiles2, which divides by scaleFactor for database planes (CIFgen.c:4077-4083) and uses the rectangle unscaled on mask planes (4094-4110). The entry misses that `tagged` also runs on cifinput through CIFGenLayer (CIFrdcl.c:615), and the same mismatch exists there. Read planes are in read units, while label rects are in internal units because calmaElementText divides by crs_scaleFactor (calma/CalmaRdpt.c:951-952). ihp-sg13g2-cifin uses `tagged` 5 times (isolbox, well/sub! taps), so the IHP GDS round-trip depends on this. No PDK uses it in cifoutput.)
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto replicates Magic on both the input and output paths. The IHP read goldens must be captured in this mode, because they depend on it. With it OFF, the label rectangle is first converted to the units of the plane being searched. On output it is multiplied by cs_scaleFactor before cifSrTiles2, so database planes see internal units and mask planes see mask units. On input it is converted to read units. Add one golden case with scaleFactor ≠ 1 per path, and one IHP read case comparing ON with OFF, so any change OFF makes to IHP imports is recorded. (corrected 2026-11-13, was "decide after golden capture with scaleFactor ≠ 1")

### Q-MASK-7: not-square drops all non-Manhattan geometry
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: split tiles are skipped (not copied), so triangles disappear; the four neighbour checks all use the bottom kind of the neighbour.
- Evidence: cif/CIFgen.c:4740-4790 (corrected 2026-11-13: Confirmed with corrected lines: cifNotSquareFunc is at cif/CIFgen.c:4727-4800. `if (IsSplit(tile)) return 0;` drops triangles, although the comment says they are "never square", which means they should be kept. All four neighbour loops test TiGetBottomType (Left/Right/Top should test Right/Left/Bottom types), which matters only when a neighbour is a split tile. The only PDK use is cifinput: ihp-sg13g2-cifin.tech:613 on CONT for CONTBAR mask hints.)
- Options: replicate | fix (copy triangles) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto replicates Magic: split tiles are skipped and the bottom type is used for all neighbour checks. With it OFF, split tiles are copied as triangles (they are never square), and each side uses the neighbour's facing type. The IHP CONT input is Manhattan, so IHP reads are the same in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-8: gridlimit is never rescaled and has two interpretations
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: operators apply `gridLimit` directly to final mask-unit coordinates (after reduction), while the grid-scale guard multiplies it by the expander (treating it as nanometres-like units). Neither rescales it when lambda changes.
- Evidence: cif/CIFtech.c:466-490, cif/CIFgen.c:163-237, cif/CIFtech.c:775-790
- Verified 2026-11-13: Confirmed at CIFtech.c:775-790 (parse), CIFtech.c:466-490 (CIFTechLimitScale: gridup = limit*expander*ds vs scale*ns*10) and CIFgen.c:163-237 (SetBoxGrid applies it to mask coordinates). The two interpretations agree only when the expander is 10, i.e. 1 nm mask units. All three PDKs declare `gridlimit 5` with `scalefactor 10 nanometers`, and bloat-all stops any reduction (Q-MASK-13), so their expander stays 10 and the two readings agree.
- Options: replicate | fix (store in nm and convert)
- Recommendation: replicate. Hefesto replicates Magic in both modes: gridLimit is stored as written, applied to final mask-unit coordinates, and never rescaled, and the scale guard is Magic's formula. Rescaling would need a non-integer limit whenever the expander is reduced, and would change which `scalegrid` values are refused, with no PDK benefit. Instead, the style loader warns, after each unit normalisation, when the current expander differs from 10, the case where the operator and guard interpretations disagree.

### Q-MASK-9: slots with exactly 6 tokens reads past the argument list
- Kind: bug
- Magic behavior: the 6-token form reads `argv[6]` before rejecting the line.
- Evidence: cif/CIFtech.c:1567-1585, cif/CIFtech.c:1611-1612
- Options: replicate (reject) | fix
- Recommendation: reject the line with a wrong-argument error without undefined behaviour.
- Resolution: fix — the 6-token `slots` line is rejected with a wrong-argument error without reading past the argument list (X-2 tier 1, 2026-11-07).

### Q-MASK-10: style name prefix shadowing
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: a new `style` name is treated as existing if an earlier name begins with it (`style gds` after `style gdsfoo` is not registered).
- Evidence: cif/CIFtech.c:594-602 (corrected 2026-11-13: The behaviour is confirmed; the line is cif/CIFtech.c:596-604 (strncmp(cs_name, argv[1], strlen(argv[1])) at 603). The prefix test exists so that `style base variants ...` matches the stored names base+variant (e.g. `wafflefill(tiled)`, sky130), so a plain exact comparison would break variant re-reads. The same prefix test is used in the variant re-read at CIFtech.c:688. The effect: `style gds` after `style gdsii` is never registered, and its statements can never load, because the loader uses strcmp at 676. There is no Magic mask output for that style.)
- Options: replicate | fix (exact comparison)
- Recommendation: Fix in both modes, with a tech warning. A 2-argument `style name` is an existing style only if some stored name is exactly `name`. A 4-argument variant `style base variants v1,...` matches only stored names equal to base+vi. Styles that work in Magic keep the same list order and output, and only a style Magic silently drops becomes available, appended at the end of the list. This is tier 3 with no Magic output to match, the same reasoning as the approved text-only SCRIPT fixes. Any `cif listall`-style listing golden gets a deviation entry only if a test tech has such a pair.

### Q-MASK-11: `labels ... text` has no effect
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the `text` option is parsed into a flag that is never used, so it behaves like no option.
- Evidence: cif/CIFtech.c:905-955
- Verified 2026-11-13: Confirmed at cif/CIFtech.c:903-958. textOnly is set but never read. strncmp(argv[2],"text",6) matches exactly "text". The PDK lines `labels ... text` are all in cifinput sections (handled by CIFrdtech.c:871-873 as LABEL_TYPE_TEXT), not cifoutput.
- Options: replicate | implement intended "text only" semantics
- Recommendation: replicate. Hefesto treats `labels <types> text` in cifoutput as `labels <types>` in both modes. Magic has no implemented or documented meaning to correct toward, and no reference PDK uses the option in cifoutput. The cifinput `text` option is a separate, implemented feature.

### Q-MASK-12: interacting-family flags treated as bloat data in reducer computation
- Kind: bug
- Magic behavior: for `nonoverlapping`, `interacting`, `noninteracting` (non-zero flag stored in the client field) the CIF reducer loop falls into the bloat branch and dereferences the small integer as a pointer (undefined behaviour; likely crash) unless the reducer already reached 1.
- Evidence: cif/CIFtech.c:1996-2037, cif/CIFtech.c:1131-1146
- Options: replicate (impossible) | fix (skip these operators)
- Recommendation: fix: skip; verify against a tech using `interacting` to confirm observed reducer.
- Resolution: fix — the reducer computation skips `nonoverlapping`, `interacting` and `noninteracting` instead of dereferencing their flag as a pointer (X-2 tier 1, 2026-11-07).

### Q-MASK-13: bloat-all prevents unit reduction
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: bloat-all stores the value 1 per selected type; it enters the GCD in unit normalisation so the expander/scale are never reduced when d = 1, and these values are multiplied but never divided.
- Evidence: cif/CIFtech.c:2620-2635, cif/CIFtech.c:2690-2700 (corrected 2026-11-13: The behaviour is confirmed, but every line number has moved. bloat-all sets bl_distance=1 at cif/CIFtech.c:1284-1294. The value enters the reducer GCD at 2022-2031 (cs_reducer becomes 1, which changes the CIF DS scale and the coordinate divisions in CIFwrite.c:368-692). It enters the expander GCD in CIFTechOutputScale at 2601-2612, so lexpand=1 and the function returns early at 2633 when d=1. The divide pass at 2689-2698 omits BLOATALL. Only nonzero-ness of the value is used later. CIFTechFinal calls CIFTechOutputScale(1,1) (CIFtech.c:1873), so for sky130, gf180mcuD and IHP (27-40 bloat-all each) this is what keeps expander=10. GDS UNITS depend on the expander (calma/CalmaWrite.c:1455-1466).)
- Options: replicate | fix (exclude bloat-all)
- Recommendation: replicate. Hefesto replicates Magic in both modes: bloat-all flags count as 1 in both GCDs and are never divided. This departs from the compat rule on purpose. Excluding them would let the expander and scaleFactor shrink for every PDK style, which changes the GDS UNITS record, the CIF DS scale, the meaning of the raw gridlimit (Q-MASK-8) and centring rounding. That breaks byte identity and the PDK grids for no geometric gain. The value is an internal unit choice, not a wrong result.

### Q-MASK-14: maxrect tie-breaking, overflow and split tiles
- Kind: ambiguity
- Magic behavior: the largest rectangle is chosen by strict `>` in candidate-list order, which depends on area-search enumeration order; areas are computed in 32-bit int (overflow for large regions); split tiles of the region count as fully inside.
- Evidence: utils/maxrect.c:80-160, utils/maxrect.c:324-333
- Options: replicate | define deterministic tie-break
- Recommendation: replicate enumeration order (top-to-bottom, left-to-right area search) and use 64-bit areas; capture golden ties.
- Resolution: compat — candidates are taken in Magic's tile enumeration order (tile order bullet) and ties keep the first by strict `>`; areas are computed in 64 bits, since the 32-bit overflow is undefined behaviour (X-1 as amended by D-1; X-2 tier 1, 2026-11-07).

### Q-MASK-15: grow-min geometric formula is incomplete
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the code comment states the algorithm does not cover many cases; `changed` is only set when both width and height are short; the vertical extension uses `(int)sqrt(g² - (g+W)²/4 + 0.5)` (0.5 added inside the root, truncating conversion), which is not a symmetric rounding; the neighbour-column pass can repaint the same rectangle several times.
- Evidence: cif/CIFgen.c:300-485
- Verified 2026-11-13: Confirmed at cif/CIFgen.c:297-485. `changed` is set only inside the width<g and height<g branch. h = (int)sqrt(g*g - 0.25*((g+W)*(g+W)) + 0.5) uses a 32-bit int product (g+W)² and truncating conversion. The second pass repaints overlapping rectangles. Magic's own comment says the algorithm needs rework. Used by sky130A (3) and ihp-sg13g2 (2).
- Options: replicate | fix
- Recommendation: replicate. Hefesto replicates Magic exactly in both modes: the same pass order, the `changed` condition, the IEEE double sqrt of the expression evaluated in the same order with an int product, (int) truncation, and the SetMinBoxGrid calls. Magic defines no correct algorithm to switch to, and sky130 and IHP GDS identity depend on this exact output. Hefesto lints grow-min used on layers whose DRC is not rect_only.

### Q-MASK-16: set-minimum-grid divides by gridlimit
- Kind: bug
- Magic behavior: lambda denominator is `scaleFactor / gridLimit`; with gridLimit 0 this divides by zero.
- Evidence: utils/tech.c:806-812
- Options: replicate (crash) | fix (ignore option when gridLimit = 0)
- Recommendation: fix with a tech error.
- Resolution: fix — with gridlimit 0 the set-minimum-grid option is ignored and a tech error is reported instead of dividing by zero (X-2 tier 1, 2026-11-07).

### Q-MASK-17: no implicit "default" style
- Kind: doc-mismatch
- Magic behavior: the manual says a style named `default` is used if the section lacks a `style` line; the code fails the section instead.
- Evidence: cif/CIFtech.c:713-716, doc/latexfiles/maint2.tex:1180-1186
- Options: replicate | follow doc
- Recommendation: replicate the code (error), mention in tech-file lint.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (spec conventions §2.7, 2026-11-07).

### Q-MASK-18: interaction radius not recomputed when no reduction occurs
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: unit normalisation returns early when the common factor is ≤ 1, skipping the radius recomputation even though scaleFactor and distances changed (e.g. after lambda rescaling).
- Evidence: cif/CIFtech.c:2640-2650, cif/CIFtech.c:2716-2718 (corrected 2026-11-13: The behaviour is confirmed with corrected lines: the early `if (lexpand <= 1) return;` is at cif/CIFtech.c:2633, which skips cifComputeHalo at 2719. This happens after distances were multiplied by d (2500-2612). cs_radius = 2*max(grow,shrink)/scaleFactor + 1 (CIFtech.c:1777-1801) is then stale whenever d>1 and nothing reduces. A stale, smaller radius can miss subcell interactions, so hierarchical output can differ. A larger radius would not change correct output. This is reached by scalegrid, tech load with DBLambda, and GDS reads that rescale (CmdRS.c:322, CIFrdutils.c:244, DBio.c:799).)
- Options: replicate | fix (always recompute) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto keeps Magic's stale radius after a non-reducing rescale. With it OFF, it always recomputes the halo at the end of output-scale normalisation. Units and the reduction choice are the same in both modes, so only interaction-area coverage, and therefore the hierarchical output, can differ. PDK styles load with d=1, where the radius was already computed in CIFTechFinal, so their output is the same in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-19: GDS layer range documented as 0..63
- Kind: doc-mismatch
- Magic behavior: code accepts 0..255; the tech manual says 0..63.
- Evidence: calma/calmaInt.h:117, doc/latexfiles/maint2.tex (Calma layers subsection)
- Options: follow code
- Recommendation: follow code (0..255).
- Resolution: n/a — spec follows Magic's code; documentation is wrong; GDS layer and datatype numbers are 0..255 (spec conventions §2.7, 2026-11-07).

### Q-MASK-20: bloat-or on split tiles ignores corner extensions
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: diagonal tiles are bloated only along the hypotenuse direction by the distance of the type across the diagonal; leg-side corner extensions are not handled (comment says so).
- Evidence: cif/CIFgen.c:951-1040
- Verified 2026-11-13: Confirmed at cif/CIFgen.c:946-1040, including Magic's own comment that corner extensions are not considered. With grow-euclidean, cifGrowEuclideanFunc is used instead.
- Options: replicate | fix
- Recommendation: replicate. Hefesto replicates Magic in both modes: the two leg rectangles plus the shifted triangle, or the Euclidean grow when grow-euclidean is set. Magic defines no corrected geometry for the missing corner extensions, and sky130, gf180mcuD and IHP use bloat-or on layouts that can be non-Manhattan, so GDS identity needs this exact shape. A corrected variant can be added later as a separate compat item once its geometry is specified.

### Q-MASK-21: interacting tests against database types use truncating division
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: mask-unit tile rectangles (grown by 1 for touching forms) are divided by scaleFactor with truncation toward zero before searching database planes, so touching tests can be lost or asymmetric when coordinates are not multiples of scaleFactor.
- Evidence: cif/CIFgen.c:4078-4083, cif/CIFgen.c:4868-4882
- Verified 2026-11-13: Confirmed. The touching-grow by 1 is at cif/CIFgen.c:4868-4876, and the truncating division in cifSrTiles2 is at 4077-4083. Truncation toward zero makes results asymmetric across the origin. No reference PDK cifoutput uses overlapping or interacting.
- Options: replicate | fix (floor/ceil outward) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto divides with C truncation toward zero, as Magic does. With it OFF, it rounds outward: floor for lower and left edges, ceil for upper and right edges. Then an overlap or touch is detected the same way on both sides of the origin, and a database tile that the mask-unit rectangle reaches is never missed. PDK output is the same in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-22: net with no layer list adds nothing
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `net name` without layers ORs no geometry because the type mask is empty.
- Evidence: cif/CIFtech.c:1365-1372, cif/CIFgen.c:5446-5452
- Verified 2026-11-13: Confirmed at cif/CIFtech.c:1365-1372 (layers are optional), cif/CIFgen.c:5433-5457 and cifSrTiles at 3974-4030 (an empty paintMask and cifMask search nothing). There is a related point the spec should state: CIF or temp layers in a `net` layer list OR the whole temp plane (cifSrTiles searches *temps over TiPlaneRect), not only the net's geometry. No reference PDK uses `net`.
- Options: replicate | default to all types
- Recommendation: replicate. Hefesto replicates Magic in both modes: `net name` with no layers adds nothing. Magic documents no default to correct toward. The style loader warns about a `net` operator without layers, and about one whose layer list names CIF or temp layers, because those are not limited to the net.

### Q-MASK-23: bbox operator uses the database cell bounding box
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the cell definition bounding box (which in Magic includes subcells and may include label extents) is used rather than mask geometry extents.
- Evidence: cif/CIFgen.c:5665-5675 (corrected 2026-11-13: Confirmed with corrections. The code is at cif/CIFgen.c:5635-5672. The box used is CIFErrorDef->cd_bbox (the cell designated for errors, i.e. the cell being written), scaled by scaleFactor. With `bbox top` it is skipped when that cell has a non-internal parent. cd_bbox (database/DBcellbox.c:604-741) covers subcell bboxes, paint and label anchor rects (lab_rect). It does not include rendered text extents, which are in cd_extended. All three PDKs use `bbox top`.)
- Options: replicate
- Recommendation: replicate. Hefesto replicates Magic in both modes, using the bounding box of the cell being generated, as layout-geometry-model defines it: paint ∪ subcell bboxes ∪ label anchor rectangles, without text extents. With `top`, it is skipped when the cell has a non-internal parent. GDS identity for the PDK boundary layers depends on this.

### Q-MASK-24: boundary with malformed FIXED_BBOX
- Kind: bug
- Magic behavior: if FIXED_BBOX exists but is not a 4-integer dimension property, an uninitialised rectangle is painted.
- Evidence: cif/CIFgen.c:5611-5630
- Options: fix (paint nothing)
- Recommendation: fix.
- Resolution: fix — a malformed FIXED_BBOX paints nothing instead of an uninitialised rectangle (X-2 tier 1, 2026-11-07).

### Q-MASK-25: centring divisions truncate toward zero
- Kind: quirk
- Magic behavior: cut-array centring `(x1 + x2 + sep - n*P) / 2`, grow-min halving and squares-grid margins use C integer division (truncation toward zero), so results for negative odd sums differ from floor division by one unit.
- Evidence: cif/CIFgen.c:3810-3818, cif/CIFgen.c:3950-3956, cif/CIFgen.c:290-305
- Options: replicate
- Recommendation: replicate (C++ `/` has the same semantics).

### Q-MASK-26: hierarchical truncation returns partial layer geometry
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: in hierarchical mode the layer stops at net/tagged/boundary/bbox and at squares/slots not followed by grow, returning the pre-operator geometry, which is then used in interaction processing.
- Evidence: cif/CIFgen.c:5332-5380, cif/CIFgen.c:5714-5718
- Verified 2026-11-13: Confirmed. hstop is set for squares, squares-grid and slots not followed by grow (cif/CIFgen.c:5332-5380) and for net, tagged, boundary and bbox (5434-5437, 5459-5462, 5601-5605, 5636-5639), and the loop breaks at 5706. Every PDK contact-cut layer is `squares-grid` followed by `calma` with no grow (sky130, gf180mcuD, IHP), so this path is taken for all cut layers in hierarchical generation. This is the same mechanism as Q-MASK-402.
- Options: replicate
- Recommendation: replicate. Hefesto replicates the truncation itself in both modes. It is how Magic keeps non-hierarchical operators out of interaction processing, and every PDK cut layer goes through it, so GDS identity needs it. Whether the partial result should still go into the residual, which gives an uncut bloated region in the parent when a seam is crossed, is a separate question. That should be decided under Q-MASK-402 (compat recommended there: ON replicate, OFF drop the stopped layer from the residual).

### Q-MASK-27: squares grid-limit fix diverges for negative coordinates
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: for `squares`, `δ = |cx| mod gridLimit` always shrinks the upper bound by 2δ; for negative cx this moves the cut further off-grid and repeats until no columns remain (slots handles the sign correctly).
- Evidence: cif/CIFgen.c:3823-3861, cif/CIFgen.c:3686-3697
- Verified 2026-11-13: Confirmed. cifSquareFunc (cif/CIFgen.c:3818-3858) always does `area->r_xtop -= 2*delta` and the same for y. For negative cut origins the cut moves further off-grid, and the loop repeats until it lands on the grid or no columns remain. cifSlotsFunc (3688-3700) branches on the sign. All three PDKs use `squares-grid` or `slots`, not `squares`, so PDK output is not affected.
- Options: replicate | fix (sign-aware like slots) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto replicates Magic's sign-blind shrink. With it OFF, it uses the sign-aware correction that slots uses (axtop -= 2δ for cut ≥ 0, += 2δ for cut < 0), so contacts left of or below the origin keep their cuts. Add a golden case with a contact at negative coordinates and gridlimit > 1 that shows the lost cuts with magic-compat ON. (corrected 2026-11-13, was replicate)

### Q-MASK-28: squares placed per region bounding box, not per tile
- Kind: doc-mismatch
- Magic behavior: modern code centres cut arrays on each connected region's bounding box (after strip extraction); the tech manual describes per-tile placement.
- Evidence: cif/CIFgen.c:2738-2990, doc/latexfiles/maint2.tex (squares paragraph)
- Options: follow code
- Recommendation: follow code.
- Resolution: n/a — spec follows Magic's code; documentation is wrong; cuts are placed per region bounding box (spec conventions §2.7, 2026-11-07).

### Q-MASK-400: Phantom interaction area around the origin
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: When interaction areas are found, each subcell that is not fully surrounded by nearby parent paint is checked for siblings. The accumulator for that check starts as the rectangle (0,0)-(0,0). If no sibling is found, this empty accumulator is still expanded by the radius r, giving (-r,-r)-(r,r). It is then clipped to the subcell's halo and added to the interaction bounding box. So whenever a subcell's halo covers the origin, a spurious region of up to 2r×2r around (0,0) is added. That region can enlarge the interaction rectangle, which is then recomputed flat.
- Evidence: drc/DRCsubcell.c:460-476, utils/geometry.c:77
- Options: replicate (the interaction rectangle boundary can change the output of non-distributive operators) | fix (skip the include when no sibling was found) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. Confirmed: drcFindOtherCells (drc/DRCsubcell.c:133-140) only calls GeoInclude when it finds a sibling. dlu_area is seeded with GeoNullRect (0,0)-(0,0) (utils/geometry.c:77, DRCsubcell.c:470), and GEO_EXPAND/GeoClip/GeoInclude (DRCsubcell.c:476-478) still add (-r,-r)-(r,r) clipped to the halo whenever that halo covers the origin. GeoInclude ignores a null source, so nothing is added when the halo misses the origin. CIF reaches this code through DRCFindInteractions (cif/CIFhier.c:1075). With magic-compat ON Hefesto seeds B with (0,0,0,0) and expands it even when no sibling is found. With it OFF, B is skipped when it is empty. Both mask generation and hierarchical DRC call the same shared interaction finder and read the same compat switch. Golden case MASK-HG-005 covers this. (corrected 2026-11-13, was replicate)

### Q-MASK-401: Vendor-cell exclusion during flattening never applies
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The flattening copy is meant to skip the paint of vendor (read-only GDS) cells unless the style option `see-no-vendor` changes that. The test is written as `if (!CIFCurStyle) if (!(CIFCurStyle->cs_flags & CWF_SEE_NO_VENDOR)) return 0;`. When a style is loaded, the first condition is false, so vendor paint is always copied. When no style is loaded, the second condition dereferences NULL. The style option `see-no-vendor` therefore has no effect on generation. Its intended meaning is also unclear: the flag comment says "Hide magic's GDS from vendor cells", while the code comment says "unless overridden".
- Evidence: cif/CIFhier.c:396-405, cif/CIFint.h:338, cif/CIFtech.c:1055-1056
- Options: replicate (always include vendor paint) | fix (honour the flag with a clarified meaning)
- Recommendation: replicate in both modes. Confirmed at cif/CIFhier.c:392-397: whenever a style is loaded, vendor (CDVENDORGDS) paint is always copied. The NULL dereference is unreachable in practice because every caller (CIFhier.c:476/885/1227/1234, CIFwrite.c:757, CIFsee.c:169/290/462) runs only with a style loaded, and Hefesto simply must not dereference a missing style. Compat is not used here because there is no coherent intended behaviour for OFF. The flag comment (CIFint.h:338, "Hide magic's GDS from vendor cells") and the code comment ("see-vendor" override) contradict each other. Skipping vendor paint would also drop vendor geometry from `cif flat` and from interaction residuals, which is data loss. No sky130/gf180/IHP cifout uses `see-no-vendor`. Hefesto parses and stores the option, and it has no effect on generation.

### Q-MASK-402: Hierarchy-stopping operators leave a partial layer result
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: During hierarchical evaluation, processing of a layer stops at `squares`, `squares-grid` or `slots` not followed by `grow`, and at `net`, `tagged`, `boundary` or `bbox`. The partial result computed before that operator then takes part in the residual and is ORed into the parent. A layer such as `bloat-or ...; squares` can therefore output an uncut bloated region in the parent at a cell seam.
- Evidence: cif/CIFgen.c:5332-5376, cif/CIFgen.c:5433-5462, cif/CIFgen.c:5601-5636, cif/CIFgen.c:5700-5705
- Verified 2026-11-13: Behaviour confirmed. The break on hstop is at cif/CIFgen.c:5706, not 5700-5705. The bbox/boundary stops (5601-5606, 5636-5641) can never be reached in hierarchical mode, because CIFtech.c:2229-2238 removes every layer containing bbox/boundary from cs_hierLayers and CIFGenLayer(hier=TRUE) runs only for those layers. In practice only squares/squares-grid/slots without a following grow, and net/tagged, stop early.
- Options: replicate | fix (drop the layer from the residual when it stopped early) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the partial plane produced before the stopping operator takes part in total/components/residual and is ORed into the parent, as in Magic. With it OFF, a layer that stopped early contributes no hierarchical residual: its cuts or net geometry come only from per-cell generation. Hefesto's style loader warns in both modes when a hierarchical layer contains a stopping operator after a hierarchy-forcing one, e.g. `bloat-or; squares`. sky130/IHP use squares-grid and slots heavily, so ON must match exactly for byte-identical GDS. (corrected 2026-11-13, was replicate)

### Q-MASK-403: Layers that depend on bbox/boundary layers stay hierarchical
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Layers that contain `bbox` or `boundary` are removed from the hierarchical set after dependencies have been propagated. A hierarchical layer that reads such a templayer therefore sees it as empty during interaction processing, in both the total and the components. The residual is computed against the wrong operand.
- Evidence: cif/CIFtech.c:2222-2238
- Options: replicate | fix (also remove the dependents, or generate the dependency non-hierarchically) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. Confirmed at cif/CIFtech.c:2205-2238: dependency propagation runs first, and only the layers that themselves contain bbox/boundary are removed afterwards. CIFGen with genAllPlanes=TRUE gives non-generated layers an empty plane, so dependents read them as empty. This affects real PDKs. sky130 COREID/STDCELL (`bloat-all ... CELLBOUND`) and `bbox_missing` (`and-not abutment_box`) are hierarchical layers that read boundary templayers, and gf180mcu/IHP have bbox/boundary layers too. With magic-compat ON, Hefesto replicates this, which golden byte identity requires. With it OFF, Hefesto also removes from HierLayers, transitively, every layer that depends on a removed bbox/boundary layer. That follows the intent stated in Magic's own comment (CIFtech.c:2214-2221): bbox/boundary geometry is restricted to the child cell. Hefesto also documents the behaviour in the style-writing guide. (corrected 2026-11-13, was replicate)

### Q-MASK-404: Halo radius ignores several operators
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The radius counts only grow, grow-min, grow-grid, shrink and bloat-or distances, and stops at bbox/net/tagged/mask-hints. The reach of bloat-all, bloat-max, bloat-min, bridge, bridge-lim, close, slots, squares and interact is not counted. For styles that use those operators, interaction areas and yank windows may be too small, so hierarchical output differs from flat output near cell boundaries.
- Evidence: cif/CIFtech.c:1668-1760
- Verified 2026-11-13: cifComputeRadii is at cif/CIFtech.c:1668-1754 (grow/grow-min/grow-grid add co_distance; shrink adds; only CIFOP_BLOAT (bloat-or) contributes). Also uncounted: squares-grid, not-square, orthogonal, maxrect, boundary, close, interact, bloat-all/max/min, bridge/bridge-lim, slots.
- Options: replicate | fix (count all reach-bearing operators) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. The radius sets the halo, the yank and clip areas, and the interaction step. Each of these changes the output, and sky130/gf180/IHP use bloat-all (27/30/40 times), bridge and close heavily. With magic-compat ON, Hefesto computes the radius exactly as Magic does. With it OFF, Hefesto also counts the bounded reaches: the largest positive bloat-max/bloat-min distance, and the bridge/bridge-lim width. bloat-all, interact and close have no fixed distance, so they stay uncounted in both modes, and the style loader prints a note when a hierarchical layer uses them. (corrected 2026-11-13, was replicate)

### Q-MASK-405: Non-distributive operators do not force hierarchical processing
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Only and, and-not, interact, shrink, close, bridge, bridge-lim and the bloat-family operators force a layer into the hierarchical set. `grow-min`, `not-square`, `orthogonal`/`manhattan`, `squares`, `slots`, `net` and `tagged` do not, although their result on a union of geometries generally differs from the union of their results.
- Evidence: cif/CIFtech.c:2085-2121
- Verified 2026-11-13: Force set at cif/CIFtech.c:2086-2119 (bloat-or/max/min/all, and, and-not, interact, shrink, close, bridge, bridge-lim). Also not forcing: maxrect, grow-grid.
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto uses Magic's force set exactly, which golden GDS requires: IHP uses orthogonal 19 times and grow-min twice. With it OFF, grow-min, not-square, orthogonal/manhattan and maxrect also force a layer into HierLayers. squares/slots/net/tagged are not added in OFF mode, because they stop hierarchical evaluation anyway (Q-MASK-402). (corrected 2026-11-13, was replicate)

### Q-MASK-406: Array element filter with reversed index ranges
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The low-corner filter is `x - xlo < 2 && y - ylo < 2` and the high-corner filter is `xhi - x < 2 && yhi - y < 2`. When an array is declared with xlo > xhi (descending indices), the difference is negative for every element, so every element that overlaps the region's halo is accepted. For small radii this matches the intended 2×2 corner. With a large radius, more elements are included in the total and in the components.
- Evidence: cif/CIFhier.c:1137-1180
- Verified 2026-11-13: Functions are at cif/CIFhier.c:1137-1177 (low filter 1148, high filter 1173).
- Options: replicate | fix (use absolute index distance from the geometric corner) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. Confirmed: the filters use signed index differences, so when xlo>xhi (or ylo>yhi) every element is accepted. With magic-compat ON, Hefesto uses the same signed differences. With it OFF, it uses the absolute index distance from the corner element (|x-xlo|<2 and so on), which gives the intended 2x2 corner. The effect on the residual is usually nil, and golden MASK-HG-013 checks it. (corrected 2026-11-13, was replicate)

### Q-MASK-407: Zero-separation arrays
- Kind: bug
- Magic behavior: For an array with more than one element along an axis and a separation of 0 along that axis, the element count is computed by dividing by the separation. That is an integer division by zero, which can crash.
- Evidence: cif/CIFhier.c:1446-1449
- Options: replicate (not acceptable) | fix (treat the axis as having nx=1, or reject the array)
- Recommendation: fix. Treat the axis as one element for the correction and warn.
- Resolution: fix — an axis with separation 0 is treated as one element for the correction, with a warning, instead of dividing by zero (X-2 tier 1, 2026-11-07).

### Q-MASK-408: Array replication copies whole residual tiles beyond the window
- Kind: quirk
- Magic behavior: Replication copies every residual tile that overlaps the copy window, including the parts of the tile outside the window. The result depends on the maximal horizontal strip decomposition of the residual. For split (non-manhattan) tiles the decomposition depends on history. Parts of region A outside the core window are also replicated (nx-1)×(ny-1) times.
- Evidence: cif/CIFhier.c:1330-1360, cif/CIFhier.c:1568-1617
- Options: replicate (needs a canonical strip decomposition) | fix (clip to the window)
- Recommendation: replicate for manhattan geometry, where the decomposition is canonical. Track non-manhattan differences as a known deviation.
- Resolution: compat — tile order bullet: replication copies whole residual tiles of Magic's canonical strip decomposition; the non-Manhattan case follows Q-MASK-218 (X-1 as amended by D-1, 2026-11-07).

### Q-MASK-409: Grow-slivers details
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior:
  - `min-width` is stored in output units and is never rescaled when the style scale changes.
  - Growth uses (m-h) div 2 on each side, so an odd difference grows one unit less than m.
  - The early return in the first branch skips the second branch.
  - The function prints `got sliver ...` and `created sliver ...` to stdout.
  - Sliver growth applies only to hierarchical residual pieces, never to own-paint output.
- Evidence: cif/CIFhier.c:1268-1305, cif/CIFtech.c:979-989
- Verified 2026-11-13: cifGrowSliver at cif/CIFhier.c:1268-1305, called from cifHierPaintFunc (653) and cifHierPaintArrayFunc (1339). min-width is parsed at CIFtech.c:979-989 and never rescaled (min_width is referenced only in CIFtech.c:871/987 and CIFhier.c). Also unlisted: dinfo is ignored, so a split tile is grown as its full rectangle, and `min-width` on any layer sets CWF_GROW_SLIVERS for the whole style. No sky130/gf180/IHP cifout uses min-width.
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat for the geometry. With magic-compat ON, Hefesto copies Magic: no rescale, (m-h) div 2 per side, the early return that skips the second branch, full-rectangle growth of split tiles, and growth of residual pieces only. With it OFF, min-width is rescaled with the style scale, the growth reaches at least m (the odd unit goes to the top/right side), and both axes are checked. In both modes Hefesto drops the `got sliver`/`created sliver` stdout prints. They are diagnostics that do not change output, so at most they go to a debug log. (corrected 2026-11-13, was replicate)

### Q-MASK-410: CIFhier property ignored by CIF writer
- Kind: doc-mismatch
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The documentation says the `CIFhier` property overrides `*hier write disable` and `*array write disable` "for that cell and its descendents". Only the GDS writer honours it. Within GDS it affects only descendants first written while the cell's subcells are processed.
- Evidence: doc/html/cif.html:200-220, calma/CalmaWrite.c:1026, cif/CIFwrite.c:385-396
- Verified 2026-11-13: GDS handling is at calma/CalmaWrite.c:1026 (read), 1060-1080 (around the subcell enumeration) and 1354-1368 (around the cell's own calmaOutFunc). The CIF writer at cif/CIFwrite.c:385-396 never reads CIFhier. doc/html/cif.html:208-220.
- Options: replicate | fix (honour it in CIF too) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. The GDS behaviour is replicated in both modes. In GDS the property re-enables hier/array generation for the cell itself and for descendants first written during its subcell enumeration. With magic-compat ON, the CIF writer ignores CIFhier, as in Magic. With it OFF, the CIF writer honours it the same way the GDS writer does. This changes output only when the user has disabled hierarchical or array generation. (corrected 2026-11-13, was fix)

### Q-MASK-411: Flat CIF output loses labels and mask hints, and names the cell `__CIF2__`
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `cif flat` flattens paint only. Labels and `MASKHINTS_*` properties are not copied, so mask-hints layers are empty. The single symbol is named after the internal scratch cell, `__CIF2__`.
- Evidence: cif/CIFwrite.c:729-790
- Options: replicate | fix (copy labels and hints, and name the symbol after the root cell) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. Confirmed at cif/CIFwrite.c:729-790. CIFWriteFlat copies only paint into CIFComponentDef (__CIF2__) through cifHierCopyFunc. The scratch def has no labels and no MASKHINTS_* or FIXED_BBOX properties, so mask-hints and boundary layers come out empty, and cifOut writes the symbol as `9 __CIF2__;`. With magic-compat ON, Hefesto replicates all of this. With it OFF, Hefesto copies the flattened labels and the transformed MASKHINTS_* planes into the flat cell, and names the symbol after the root cell. (corrected 2026-11-13, was replicate)

### Q-MASK-412: `net` operator in own-paint generation uses a stale dummy cell
- Kind: bug
- Magic behavior: The `net` operator looks up the named net through an internal dummy use. That use's cell is only set by hierarchical, array or flat-view processing. During own-paint generation of cell D it still points at whatever cell was processed before (or at the internal scratch cell). If no such processing has happened yet in the session, the pointer may be NULL. The net may therefore be searched in the wrong cell, or not found.
- Evidence: cif/CIFgen.c:5433-5446, cif/CIFhier.c:157-162, cif/CIFwrite.c:388-394, calma/CalmaWrite.c:1509-1516
- Options: replicate | fix (search D)
- Recommendation: fix, and verify against Magic with MASK-HG-020 before deciding.

### Q-MASK-413: `cif unfracture` needs a dummy argument
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The yes/no value is read from the fourth word (`cif unfracture <x> yes`). `cif unfracture yes` only reports the current state.
- Evidence: commands/CmdCD.c:2056-2080
- Verified 2026-11-13: Confirmed at commands/CmdCD.c:2060-2080: argc==3 reports, argc==4 reads argv[3], and `cif unfracture` alone is wrongNumArgs. The help text at CmdCD.c:1804 documents `unfracture [yes|no]`, so the documented form only queries.
- Options: replicate | fix (accept the value as the third word) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, which differs from D-20's draft 'always accept both (fix)'. Accepting the value as the third word is not output-neutral. In Magic, `cif unfracture yes` (the form the help text documents) leaves the switch unchanged. If Hefesto set it, the output would differ: non-Manhattan tiles would be merged, so GDS polygon partitioning changes. That makes it a parse quirk that changes results, which X-2 tier 2 lists explicitly. With magic-compat ON, `cif unfracture <x>` reports the state and the fourth word sets it, as in Magic. Hefesto also warns when a three-word form's argument is yes/no, saying the value was not applied. With it OFF, `cif unfracture yes|no` sets the switch, and the four-word form is still accepted. No PDK uses the command. Resolving this question also settles D-20 with its option 'third-word form only with magic-compat OFF'. (corrected 2026-11-13, was "accept both forms")

### Q-MASK-414: Progress reporting and event blocking during interaction processing
- Kind: platform
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Magic uses a 5-second signal timer, prints `Completed N%`, and restricts Tk events so that the database cannot change during a write.
- Evidence: cif/CIFhier.c:974-988, cif/CIFhier.c:1035-1041, cif/CIFhier.c:1110-1124
- Verified 2026-11-13: Event restriction is at cif/CIFhier.c:974-988, restored at 1119-1124. The timer is at 1035-1041. The `Completed N%` print and Tcl_DoOneEvent are at 1086-1103, and the final print and SigRemoveTimer are at 1112-1117. Magic's loop has no interrupt check.
- Options: platform alternative (progress callback with cancellation, and a read-only snapshot of the database during the write)
- Recommendation: platform, on every target including iPadOS. Hierarchical generation reports progress (fraction of interaction squares done) to a callback, rate-limited to about 5 s. The console shell may print `Completed N%`, which is informative and not part of the golden output. The callback also offers cancellation: a cancelled write produces no output file, by writing to a temporary file and renaming it. Generation reads a read-only snapshot of the database, so edits cannot corrupt a write in progress, and the UI is not blocked the way Tk_RestrictEvents blocks it. None of this changes the mask output.

### Q-MASK-300: cifinput "nanometers"/"angstroms" overwrites the decimal multiplier
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: In `cifinput`, `scalefactor 2.5 nanometers` parses 2.5 as S=5, M=2, then SETS M=10 (output side multiplies the expander by 10). The resulting internal unit is 5 nm instead of 2.5 nm.
- Evidence: cif/CIFrdtech.c:638-644 vs cif/CIFtech.c:731-735
- Options: replicate | fix (multiply M by 10/100 like the output side) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. Confirmed: CIFrdtech.c:638-644 SETS crs_multiplier to 10/100, while CIFtech.c:730-735 multiplies cs_expander. `scalefactor 2.5 nanometers` gives S=5, M=10 (5 nm). magic-compat ON: set M as Magic does. OFF: multiply M by 10 or 100, as the output side does (2.5 nm). Both modes warn at tech load when a decimal scalefactor is combined with a unit keyword. The sky130 (10), gf180mcuD (50) and IHP (10) cifinput styles use integer scalefactors, so PDK results are the same in both modes. Spec change: mask-input-styles step 3 of the scalefactor requirement (spec.md:160) gets an OFF-mode clause and the warning. (corrected 2026-11-13, was replicate)

### Q-MASK-301: labels kind "sticky" identical to "text"
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `labels L sticky` stores LABEL_TYPE_TEXT, the same as `text`; both make labels sticky.
- Evidence: cif/CIFrdtech.c:870-873
- Verified 2026-11-13: Exact lines are CIFrdtech.c:869-874. calmaLabelType is a per-call local (CIFrdtech.c:484), so `labels L` without a KIND leaves crs_labelSticky unchanged, as the spec says.
- Options: replicate | fix (distinct kind)
- Recommendation: Replicate in both modes. `sticky` maps to LABEL_TYPE_TEXT, the same as `text`. Magic defines no separate sticky kind, so there is no corrected behaviour to offer. The spec already states this.

### Q-MASK-302: style statement prefix-matches existing style names
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `style NAME` is considered an existing style when any listed style starts with NAME (strncmp with strlen(NAME)); `style fo` after `style foo` adds nothing and its statements are unreachable.
- Evidence: cif/CIFrdtech.c:509-514
- Verified 2026-11-13: Confirmed at CIFrdtech.c:509-514 with l = strlen(argv[1]) (line 487). It works in one direction only: `style fo` after `style foo` merges into foo, while `style foo` after `style fo` adds a new style. The "fo" statements cannot be reached because loading uses strcmp for an exact match (CIFrdtech.c:585-588). No PDK cifinput section (sky130, gf180mcuD, IHP) has a style name that is a prefix of another.
- Options: replicate | fix (exact match) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, following the standing rule. magic-compat ON: prefix match, so the shorter style is never listed and its block is dead. OFF: exact match, so the shorter style is listed and selectable. Both modes warn at tech load when a new style name is a strict prefix of an earlier one, because ON silently drops a style. No shipped PDK triggers it, so PDK results are the same. The entry's own recommendation was fix. compat is chosen because the behaviour changes the set of styles (`cif listall istyle`) with no tier-1 harm. (corrected 2026-11-13, was fix)

### Q-MASK-303: istyle selection ambiguous even with an exact match
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `cif istyle gds` with styles `gds` and `gds2` reports ambiguity although `gds` is exact.
- Evidence: cif/CIFrdtech.c:1339-1352
- Verified 2026-11-13: Confirmed at CIFrdtech.c:1339-1352 (CIFSetReadStyle). It reports ambiguity on the second prefix match before checking for exact equality. On success it also prints the typed name rather than the matched style name (line 1357).
- Options: replicate | fix (exact match wins, like Lookup)
- Recommendation: Fix in both modes: an exact name match wins over prefix matches, and prefix ambiguity is reported only when there is no exact match. This turns only commands that failed into commands that succeed. It changes no geometry and no file output, so it is a superset, the same reasoning as D-20/Q-MASK-413. The variant styles ("sky130()", "sky130(vendor)", "sg13g2()") already select uniquely by full name, so PDK flows are unaffected.

### Q-MASK-304: grow/shrink distances use 1/M centimicron units
- Kind: doc-mismatch
- Magic behavior: Distances are interpreted in read units (1/M centimicron). With a decimal scalefactor (M from the fraction) they are not centimicrons, contrary to the manual.
- Evidence: cif/CIFrdtech.c:1036-1046, cif/CIFrdtech.c:1451-1516, doc/latexfiles/maint2.tex:1760-1768
- Options: replicate | fix
- Recommendation: replicate; document in user docs.
- Resolution: n/a — spec follows Magic's code; documentation is wrong; distances are in read units of 1/M centimicron (spec conventions §2.7, 2026-11-07).

### Q-MASK-305: gridlimit is in nanometers, not CIF units
- Kind: doc-mismatch
- Magic behavior: The limit test compares S*10 against G*M*den, i.e. G is in nanometers regardless of M.
- Evidence: cif/CIFrdtech.c:93-114, cif/CIFread.h:86-91
- Options: replicate | fix
- Recommendation: replicate; document as nanometers.
- Resolution: n/a — spec follows Magic's code; documentation is wrong; gridlimit is in nanometres (spec conventions §2.7, 2026-11-07).

### Q-MASK-306: input grow/shrink algorithm depends on the output style
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: CIFGenLayer picks Euclidean vs square grow from the current OUTPUT style's grow-euclidean flag even when evaluating cifinput rules.
- Evidence: cif/CIFgen.c:5081-5082 (corrected 2026-11-13: The line numbers moved. The selection is at cif/CIFgen.c:5078-5079 (cifGrowFuncPtr = CIFCurStyle->cs_flags & CWF_GROW_EUCLIDEAN ? ...). cifinput has no grow-euclidean option of its own (CIFrdtech.c parses none, and only CIFtech.c:1053 does). The described behaviour is correct. No sky130, gf180mcuD or IHP cifoutput style sets grow-euclidean.)
- Options: replicate | fix (square grow on input always) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. magic-compat ON: grow and shrink on input use Euclidean growth when the current OUTPUT style has grow-euclidean, as Magic does. OFF: input grow and shrink always use the square (default) algorithm, since an input style has no way to request Euclidean growth. Both modes add a diagnostics note when the output style is Euclidean and the input style uses grow or shrink. The shipped PDKs give the same result in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-307: boundary/mask-hints operators on regular input layers use output semantics
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Only templayer rules (without copyup) apply the input meaning of `boundary` (define FIXED_BBOX) and `mask-hints` (store hints). In a `layer` rule, these ops OR in existing FIXED_BBOX / MASKHINTS geometry scaled by the output scalefactor. With several boundary tiles, the last enumerated tile wins.
- Evidence: cif/CIFrdcl.c:342-527, cif/CIFgen.c:5601-5633, cif/CIFgen.c:5682-5700 (corrected 2026-11-13: The behaviour is correct, but the CIFrdcl.c lines are wrong: 342-527 is CIFInputRescale/CIFParseStart/cifCopyPaintFunc. The input meanings are in CIFPaintCurrent at cif/CIFrdcl.c:624-819: copyup at 624-689, then boundary at 690-703 and mask-hints at 705-805, reached only when the templayer has no copyup. cifMakeBoundaryFunc is at CIFrdcl.c:994-1083. The CIFgen.c lines 5601-5633 (BOUNDARY) and 5682-5700 (MASKHINTS, through cifCopyPropPlaneFunc at 4998-5017, which multiplies by cs_scaleFactor) are correct. Two details the entry omits: (a) the CIFGenLayer pass also runs these output cases for templayer rules before CIFPaintCurrent applies the input meaning, so an existing FIXED_BBOX or MASKHINTS is ORed in there too; (b) in a templayer with copyup, boundary and mask-hints are ignored entirely. The spec's Magic ref cif/CIFrdcl.c:407-425 (spec.md:362) is also wrong and should read 690-703. The sky130, gf180mcuD and IHP styles use boundary and mask-hints only in templayer rules without copyup.)
- Options: replicate | fix (error on boundary/mask-hints in layer rules) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. magic-compat ON: replicate. In layer rules, and in the CIFGenLayer pass of templayer rules, boundary and mask-hints OR in the cell's existing FIXED_BBOX or MASKHINTS geometry scaled by the OUTPUT scalefactor into read-unit planes. With several boundary tiles, the last one wins. OFF: in layer rules and copyup templayers these operators contribute nothing (no mixed-unit OR), and only the templayer input meaning applies. Both modes warn at tech load for boundary or mask-hints in a layer rule or a copyup templayer. PDK results are the same in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-308: mask-hints input property lookup uses wrong key
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: After a cifinput templayer with `mask-hints NAME` yields geometry, the existing plane is looked up under the bare key `NAME` but stored under `MASKHINTS_NAME`. Normally nothing is found, so a fresh plane is stored and `DBPropPut` frees any earlier `MASKHINTS_NAME` of the cell: a second templayer with the same NAME replaces the first one's hints instead of adding to them. If the cell does hold a property named exactly `NAME`, its value is used as a plane whatever its type (a string read as a `Plane *`: undefined behavior) and nothing is stored under `MASKHINTS_NAME`. The `gds maskhints` path in the same function looks up `MASKHINTS_<layer>` and unions, which shows the intent.
- Evidence: cif/CIFrdcl.c:CIFPaintCurrent (:736-754, lookup at :743; correct sibling lookup at :930-947), database/DBprop.c:DBPropPut (:107-124, frees the old plane)
- Options: replicate (replace; reuse a same-named non-hint property) | fix (look up `MASKHINTS_NAME` and union)
- Recommendation: fix everywhere (X-2 tier 1: discarded hint geometry plus undefined behavior on the bare-key hit). Golden output only differs when one style has two templayers with the same NAME, which no tech in the repo has; the mask-hints and mask-input-styles specs then describe the union.
- Canonical entry for Q-MISC-11.
- Resolution: fix everywhere (X-2 tier 1: discarded hint geometry plus undefined behavior on the bare-key hit). Golden output only differs when one style has two templayers with the same NAME, which no tech in the repo has; the mask-hints and mask-input-styles specs then describe the union. Approved 2026-11-10.

### Q-MASK-309: copyup ignored by the CIF reader
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: copyup planes are saved but CIF `C` calls never copy them into parents; only the GDS SREF/AREF handler does.
- Evidence: cif/CIFrdcl.c:342-406, cif/CIFrdcl.c:1334-1399, calma/CalmaRdcl.c:1525-1565 (corrected 2026-11-13: The behaviour is correct, but the CIFrdcl.c:342-406 lines are wrong (that range is CIFInputRescale). The copyup save is in CIFPaintCurrent at cif/CIFrdcl.c:624-689. It stores planes in cd_client and sets CDFLATGDS. CIFParseCall (cif/CIFrdcl.c:1334-1371) never reads cd_client. GDS applies the planes at calma/CalmaRdcl.c:1507-1565, and the planes are freed at CIFrdcl.c:1797-1835. All three PDK cifinput styles use copyup (28-31 rules each, for the contact and diffusion checks), so CIF reads with these styles lose the cross-hierarchy checks.)
- Options: replicate | fix (apply in CIF calls too) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. magic-compat ON: the CIF reader saves copyup planes but never copies them into parents, as Magic does. OFF: a CIF `C n T` call applies the child's saved copyup planes to the parent's read planes under the call transform, reusing the GDS SREF code. This takes effect only when the child symbol was defined before the call. For forward calls, Hefesto warns once and does not copy, because the child's planes do not exist yet. Hefesto's own CIF output (DS before C) is therefore covered. (corrected 2026-11-13, was fix)

### Q-MASK-310: tagged on input mixes internal and read units
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The label rectangle (internal units) is used directly as a search area on read planes (read units).
- Evidence: cif/CIFgen.c:5560-5580, cif/CIFgen.c:4078-4105 (corrected 2026-11-13: The behaviour is correct, but the lines moved: the tagged loop is at cif/CIFgen.c:5458-5598, with the label search at 5542-5564, and cifSrTiles2 at CIFgen.c:4058-4105. The CIF-plane branch at 4089-4103 uses `area` unscaled. Labels are stored in internal units (calma/CalmaRdpt.c:1089), while the read planes use read units (crs_scaleFactor read units per internal unit), so the search is correct only when S=1. This is a live PDK case: the IHP sg13g2 default cifinput style uses tagged on input (`tagged isolbox NWELL` at ihp-sg13g2-cifin.tech:91, and `tagged well/sub! DIFF` at 459, 484 and 529) with scalefactor 10 nanometers, so S>1. Magic searches at about 1/S of the label's true position. The same unit mixing exists on output (Q-MASK-6, still open).)
- Options: replicate | fix (scale label rect by S) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. magic-compat ON: use the label rectangle unscaled on the read planes, as Magic does. This is required for IHP GDS read and round-trip results to match Magic, because IHP's isolbox, well and sub! tagging depends on it. OFF: multiply the label rectangle by the current S before the search, after widening degenerate sides, so tagged finds the regions under the label. Q-MASK-6, on the output side, should get the same resolution. In both modes, diagnostics note when a tagged input rule runs with S≠1. Expect IHP layouts read with OFF to differ, for example isolbox appearing where Magic produces none. (corrected 2026-11-13, was fix)

### Q-MASK-311: labels statement under a templayer stores a CIF index as a Magic type
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: For a templayer rule, crl_magicType holds the CIF layer index, which `labels` then stores as a Magic tile type.
- Evidence: cif/CIFrdtech.c:856-925, cif/CIFrdtech.c:787-800
- Verified 2026-11-13: Confirmed. The templayer rule sets crl_magicType = CIFReadNameToType(argv[1], TRUE), a CIF read-layer index (CIFrdtech.c:789-800). `labels` stores it in crs_labelLayer (CIFrdtech.c:912-913), and it is used as a Magic type at CIFrdcl.c:1477 and in the GDS label path. This is a live PDK case: sky130A-cifin.tech puts `labels` under templayers ndiffarea, mvndiffarea, pdiffarea and mvpdiffarea (lines 98-100, 118-120, 167-169, 211-213), all later overridden by layer rules. Its final mapping for FILLOBSPOLY is the templayer obspoly (line 1559), so FILLOBSPOLY labels attach to whichever Magic type has obspoly's CIF index. IHP has the same pattern at lines 176-177 and 193-194, later overridden. The intermediate assignments can also trigger the "supersedes prior attachment" TechError with a bogus type name. If the index is greater than or equal to DBNumTypes, DBTypeLongNameTbl[idx] is NULL (printf of %s NULL is undefined behaviour) and the label carries an invalid type.
- Options: replicate | fix (reject labels under templayer) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. magic-compat ON: replicate. The label type is the Magic type whose number equals the templayer's CIF-layer index, which keeps the sky130 FILLOBSPOLY read and round-trip identical to Magic. When the index is not a valid type (index greater than or equal to DBNumTypes), Hefesto applies the OFF behaviour in both modes, under X-2 tier 1. OFF: a `labels` statement under a templayer is ignored with a tech-load warning, so the CIF layer keeps its earlier mapping or space. Rejecting it with an error, as the entry proposed, would fail stock sky130 and IHP tech loads. (corrected 2026-11-13, was fix)

### Q-MASK-312: CIF layer name buffer overflow at 5 characters
- Kind: bug
- Magic behavior: `L` reads up to 5 chars into a 5-byte buffer then writes the terminator out of bounds; names longer than 5 are truncated silently.
- Evidence: cif/CIFrdpt.c:766-800
- Options: replicate truncation at 5 | fix (read whole name)
- Recommendation: replicate truncation to 5 chars without the overflow.
- Resolution: fix — the terminator is never written out of bounds; names are still truncated to 5 characters as Magic reads them (X-2 tier 1, 2026-11-07).

### Q-MASK-313: unbounded name buffer in 9/91/94/95
- Kind: bug
- Magic behavior: Names are copied into a 128-byte static buffer without bound checks.
- Evidence: cif/CIFrdcl.c:1260-1300
- Options: fix (unbounded string)
- Recommendation: fix; accept arbitrary length.
- Resolution: fix — names of any length are accepted, without the 128-byte buffer overflow (X-2 tier 1, 2026-11-07).

### Q-MASK-314: 95 rescale does not update the symbol scale
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: When a 95 label corner is odd, the input style is rescaled by 2 but the symbol numerator is not, so later coordinates in the same symbol are interpreted at half size.
- Evidence: cif/CIFrdcl.c:1166-1172 vs cif/CIFrdutils.c:620-626 (corrected 2026-11-13: The behaviour is correct, but the lines moved: the odd-corner branch calls CIFInputRescale(2, 1) at cif/CIFrdcl.c:1583-1588 without `cifReadScale1 *= 2`. CIFParsePoint does update cifReadScale1 when it rescales (cif/CIFrdutils.c:609-614 and 632-637). Every later point in the same DS lands at half size against the doubled planes. Magic's writer emits size=2w and center=xbot+xtop (CIFwrite.c:419-445), which normally gives even corners, so Magic-written CIF rarely triggers this.)
- Options: replicate | fix (also double the symbol numerator)
- Recommendation: Fix in both modes under X-2 tier 1. After the 95 rescale, multiply the symbol numerator by 2 as CIFParsePoint does, so later geometry in the symbol keeps its true size. Magic silently corrupts all following geometry in the symbol, which is import data loss. Golden output differs only for foreign CIF with odd 95 corners. Such cases go in tests/golden/deviations.toml.

### Q-MASK-315: odd box lengths are truncated asymmetrically
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `B 3 2 10 0` gives x 8..11 (true 8.5..11.5); truncation toward zero shifts boxes toward the origin; no read-unit subdivision is triggered.
- Evidence: cif/CIFrdpt.c:160-170 (corrected 2026-11-13: The lines moved: the division is at cif/CIFrdpt.c:144-147, in CIFParseBox, which starts at line 68. The entry's example is right (B 3 2 10 0 gives 8..11), but "shifts toward the origin" is incomplete. C truncation also changes the width of boxes that straddle 0: B 3 2 0 0 gives x -1..1, a width of 2. B 3 2 -10 0 gives -11..-8. Magic's writer always emits doubled lengths (CIFwrite.c:690-694), so Magic-written CIF is never affected.)
- Options: replicate | fix (subdivide read units by 2) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. X-2 lists rounding asymmetries as tier 2. magic-compat ON: truncate as Magic does. OFF: when the center doubled plus or minus the half-length is odd, subdivide the read units by 2 (the same mechanism as the center rescale) so the box is exact. Hefesto's and Magic's own CIF output gives the same result in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-316: round flash read as a square
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `R d x y` paints the bounding square of the circle.
- Evidence: cif/CIFrdpt.c:225-232 (corrected 2026-11-13: The lines moved: the square is built at cif/CIFrdpt.c:215-220 (CIFParseFlash starts at 179). The header comment "We approximate a roundflash by a box" shows this is intended. Magic never writes `R`.)
- Options: replicate | fix (polygonal approximation)
- Recommendation: Replicate in both modes: paint the bounding square of the flash, with the same truncation. This is Magic's documented design, not a bug. A polygon approximation would need an invented segment count with no Magic output to match. The spec already says this.

### Q-MASK-317: "9 name" outside a definition renames the edit cell
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The current cell at top level is the edit cell, so a top-level `9` renames it.
- Evidence: cif/CIFrdcl.c:1302-1316
- Verified 2026-11-13: Confirmed at cif/CIFrdcl.c:1302-1316. Outside a definition, cifReadCellDef is the edit cell (set at CIFrdcl.c:135 and reset after DF at 1190). Magic's writer emits `9` only inside DS (CIFwrite.c:367-379), so Magic-written CIF never triggers this.
- Options: replicate | fix (ignore at top level) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. magic-compat ON: a top-level `9 NAME` renames the edit cell (the cell receiving top-level geometry), as Magic does. OFF: ignore it with a warning, because renaming the user's open cell changes the file it saves to. Magic-written CIF behaves the same in both modes. (corrected 2026-11-13, was fix)

### Q-MASK-318: DD/DS reuse and redefinition of renamed symbols
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: After `DD n`, `DS n` maps again to the existing cell named "n" and clears it (affecting earlier calls). If symbol n was renamed by `9`, a second `DS n` clears and overwrites the renamed cell instead of creating a new one.
- Evidence: cif/CIFrdcl.c:199-290, cif/CIFrdcl.c:1218-1240
- Verified 2026-11-13: Both cases are confirmed at cif/CIFrdcl.c:165-290 (cifForgetCell, cifUniqueCell, cifFindCell) and CIFParseDelete at 1212-1236. Case 2: after `9`, DBCellLookDef("n") fails, so cifUniqueCell returns without renaming and cifFindCell returns the renamed def, which DBCellClearDef then clears. Concatenating two Magic-written CIF files therefore silently replaces the first file's cells. The entry omits two further deviations. First, cifFindCell also picks up any in-memory cell named "n" that is not from this file, such as a user cell "12", and clears it on DS. Second, `DD n` forgets only symbol n, while CIF 2.0 deletes every symbol numbered n or higher.
- Verified 2026-11-13 (review): The clearing is at CIFParseStart, cif/CIFrdcl.c:484-486 (cifUniqueCell, cifFindCell, DBCellClearDef). A forward call goes through the same lookup: `C n` before `DS n` calls cifFindCell (CIFrdcl.c:1355), which binds n to an existing in-memory cell named "n". The later `DS n` then finds that cell available, renames it "n_1" with the "reused" warning (cifUniqueCell, :199-233) and defines a new cell "n", so the earlier call still points at the user's renamed cell. CifCellTable lives for one read (initialised at :134, killed at :1840).
- Options: replicate | fix (always fresh cell) | compat (replicate with `magic-compat` ON, fresh cell with it OFF)
- Recommendation: fix, in both modes. Clearing a cell the user already had, or one defined earlier in the same read, is data loss, so X-2 tier 1 ("always fix") applies; this is not a judgement item. Within one read, symbol n, whether it first appears in a `C n` or in a `DS n`, always maps to a cell created by that read. An in-memory cell is never looked up by the name "n", reused or cleared. Name collisions are resolved as "n_k" with the smallest k >= 1 not in use: a cell defined earlier in the same read is renamed "n_k" with the "reused" warning, as Magic does, and a cell from before the read keeps its name while the new cell becomes "n_k". A forward `C n` and the later `DS n` name the same new cell. A second `DS n` (after `DD n`, or after `9` renamed the symbol) defines a fresh cell and leaves the earlier one and its callers intact. `DD n` is compat: with magic-compat ON it forgets only symbol n, as Magic does; with OFF it forgets every symbol numbered n or higher, as CIF 2.0 specifies. That is a parse quirk that changes which cell a later `C` names (X-2 tier 2). Magic never writes DD, so Magic-written CIF reads the same in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-319: "cif polygon subcells" has no effect
- Kind: doc-mismatch
- Magic behavior: The flag is stored but never consulted by the CIF reader (the GDS reader has its own option).
- Evidence: cif/CIFrdutils.c:103, commands/CmdCD.c:2014-2034
- Options: replicate (no-op) | fix (implement)
- Recommendation: replicate as no-op; document.
- Resolution: n/a — spec follows Magic's code; documentation is wrong; `cif polygon subcells` is stored and has no effect (spec conventions §2.7, 2026-11-07).

### Q-MASK-320: ".cif" always appended to file names [platform]
- Kind: platform
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `cif write chip.cif` writes `chip.cif.cif`; read likewise searches `NAME.cif`.
- Evidence: utils/path.c:686-710, commands/CmdCD.c:2075-2087 (corrected 2026-11-13: Behaviour is right; line numbers are wrong. utils/path.c:686-710 is correct: PaOpen always appends the extension, and the 'skip it if already present' test is commented out at 696-706 so that a.mag and a.mag.mag stay distinct. The cif read open is at commands/CmdCD.c:2100-2105, and the cif write/flat open is at CmdCD.c:2246-2250 (not 2075-2087). Magic's own docs (doc/html/cif.html:178-181) say read looks for `file` or `file.cif`. tcltk/wrapper.tcl:142-146 (promptsave) passes the full name from tk_getSaveFile, so Magic's own GUI writes chip.cif.cif. Precedent: GDS strips a known extension first (CmdCD.c:1016-1065).)
- Options: replicate | fix (append only if missing) | platform alternative (file picker on iPadOS) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: always append `.cif` on read, write and flat, as Magic does (`cif write chip.cif` writes chip.cif.cif). OFF: append `.cif` only when the name does not already end in `.cif`, matching the GDS commands and Magic's own docs. In both modes the GUI and the iPadOS document picker pass the picked file name without its extension, so picking a file never doubles the extension (the read path of Magic's wrapper already does this). This is not tier 1 because nothing is lost: the file is written under a predictable name. (corrected 2026-11-13, was "append only when missing; on iPadOS use document picker")

### Q-MASK-321: flat CIF named __CIF2__ and without labels
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Flat output writes `9 __CIF2__;` and drops all cell labels (only paint is flattened).
- Evidence: cif/CIFwrite.c:729-792, cif/CIFhier.c:136-139
- Verified 2026-11-13: Confirmed at CIFwrite.c:729-792 and CIFhier.c:136-139. The flat symbol name comes from cifOutFunc (CIFwrite.c:372-380), so it gets the `cif prefix` prefix (`9 PREFIX/__CIF2__;`). Labels are dropped because cifHierCopyFunc copies paint only and __CIF2__ has no label list. Label-layer (CIF_LABEL) tiles generated from paint are still written.
- Options: replicate | fix (root name, flattened labels) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: write symbol 1 as `__CIF2__` (with any `cif prefix`), with no cell labels and only label-layer output, byte-identical to Magic. OFF: name the symbol after the root cell, with the same prefix and UNNAMED rules as hierarchical output. Also write the labels that Magic's `flatten` copies by default (FlatCopyAllLabels, commands/CmdFI.c:1933-1950), transformed to root coordinates, in the same 94/95 format. This mirrors Magic's own flatten precedent, so it is not a new format choice. (corrected 2026-11-13, was replicate)

### Q-MASK-322: label text not escaped in CIF output
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Label text containing spaces or `;` is written verbatim, producing CIF that reads back differently.
- Evidence: cif/CIFwrite.c:450-478 (corrected 2026-11-13: Behaviour is right. The label printfs are at cif/CIFwrite.c:431-462 (95 at 437/443, 94 at 451/457), not 450-478. The reader stops a name at whitespace or ';' (Q-MASK-313), so `a b` reads back as `a`, followed by a parse error. Label-layer lines (CIFwrite.c:619/624) write tech layer names and are safe. Precedent: the GDS writer maps illegal characters to '_' through calmaMapTableStrict/Permissive and warns 'characters changed in string' (calma/CalmaWrite.c:208-222, 3158-3240).)
- Options: replicate | fix (substitute characters) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: write label text verbatim, as Magic does, and print a warning naming each label that contains whitespace or ';', since the file will not read back. OFF: replace whitespace and ';' in the written text with '_' and print a warning in the GDS writer's form ('characters changed in string ...; modified string is ...'), so the file always parses. The layout's own labels are never changed. (corrected 2026-11-13, was replicate)

### Q-MASK-323: CIF symbol numbering depends on instance enumeration order
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Child numbers follow the database's instance enumeration order (spatial index traversal), not a documented order.
- Evidence: cif/CIFwrite.c:234-258, cif/CIFwrite.c:340-352 (corrected 2026-11-13: Behaviour is right; line ranges are imprecise. Numbering is in cifWriteMarkFunc (cif/CIFwrite.c:234-242, cifCellNum decrements from -2). cifOut (CIFwrite.c:309-338) calls DBCellEnum at line 333. Calls are written by DBCellEnum(def, cifWriteUseFunc) at CIFwrite.c:467. DBCellEnum calls DBSrCellPlaneArea → BPEnum over the bin plane (database/DBcellsrch.c:1263-1281, 86-109). This is the same 'use enumeration order' that gds-write uses for SREF and structure order (Q-MASK-204).)
- Options: replicate the traversal | define a deterministic order (e.g. instance creation order) | compat (follows Q-MASK-204)
- Recommendation: compat, following Q-MASK-204. CIF child numbering, stack order and `C` call order use the current mode's 'use enumeration order', as gds-write defines it; no separate CIF order is defined. With magic-compat ON that is bplane order, taken at the points where Magic takes it: numbering when the parent is processed (cif/CIFwrite.c:333, before the cell's own CIFGenSubcells, so unbinned unless an ancestor's search already reached the cell's plane) and calls after CIFGenSubcells (:467, binned for a cell with 10 or more uses once a partial search ran). The two can therefore differ for the same cell, as in Magic. With OFF both use the canonical reverse-insertion order. Byte-identical CIF needs the ON order. (corrected 2026-11-13, was "capture Magic's order on golden cases and decide; byte equality of CIF depends on it")

### Q-MASK-324: CIF header contains host/time fields [platform]
- Kind: platform
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: @@user, @@machine from USER/HOSTNAME env, @@date from local time, @@tool "Magic x.y".
- Evidence: cif/CIFwrite.c:260-300 (corrected 2026-11-13: Behaviour is right. The preamble is cifOutPreamble at cif/CIFwrite.c:260-289 (not 260-300). Besides @@user (USER), @@machine (HOSTNAME, often unexported, so '?'), @@tool ('Magic %s.%s') and @@date (ctime with the newline stripped, line 273), @@source (cd_file path) and @@compiled (MagicCompileTime) are also host or build dependent.)
- Options: replicate | platform alternative (app-provided user/host; "Hefesto" tool name)
- Recommendation: platform. Write the same lines in the same order and format in both modes. USER and HOSTNAME come from the environment on desktop ('?' when unset). On iPadOS, @@user and @@machine are '?' unless app-provided. @@tool is 'Hefesto <version>' and @@compiled is Hefesto's build time: Hefesto does not claim to be Magic. @@date is local-time ctime. Golden comparisons mask @@user, @@machine, @@source, @@tool, @@compiled and @@date. Tests run with TZ=UTC, matching the GDS time-field rule.

### Q-MASK-325: cif coverage counts triangles as rectangles
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Split tiles contribute their full bounding-box area to the coverage sum.
- Evidence: cif/CIFsee.c:515-530
- Verified 2026-11-13: Confirmed at cif/CIFsee.c:517-530: cifCoverageFunc adds the full TiToRect area of every solid tile, split tiles included, as long long.
- Options: replicate | fix (half area) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, which changes the entry's 'fix'. The standing rule applies: this is a value-changing quirk, and tapeout-fill-and-density reuses the number for density checks that should match Magic. ON: each split tile counts as its full rectangle. OFF: each split tile counts as half its rectangle. Sum twice the area in 64-bit integers and halve at the end, so the result stays exact. 'Layer Bounding Area' and the coverage fraction are unchanged otherwise. (corrected 2026-11-13, was fix)

### Q-MASK-326: calma number list of exactly 255 entries rejected
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The parser fails when the list has 255 or more entries.
- Evidence: cif/CIFrdtech.c:1388-1430
- Verified 2026-11-13: Confirmed at cif/CIFrdtech.c:1388-1431. The loop fills at most numNums = CALMA_LAYER_MAX = 255 entries, into arrays of size 255 (line 482; callers at 833-834). Reaching 255 entries reports 'Too many layer/type numbers in line; maximum = 255' even at end of string, so the statement is ignored. Lists of 255 or more entries are rejected, including the legitimate full list 0..255 (256 values).
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, which changes the entry's 'fix' to follow the standing rule (a parse quirk that changes results). ON: a list of 255 or more entries is an error, with Magic's message, and the statement is ignored. OFF: lists of any length are accepted, and only bad characters or out-of-range values are errors. No known PDK tech file is affected, so PDK output is the same in both modes. (corrected 2026-11-13, was fix)

### Q-MASK-327: default 94 label sticky flag uses a mismatched layer search
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: For a 94 label without layer, the sticky decision searches CIF layer indices < number of rules for one whose label type equals the current label type.
- Evidence: cif/CIFrdcl.c:1479-1497
- Verified 2026-11-13: Confirmed at cif/CIFrdcl.c:1479-1497. For a 94 with no layer, type = cifCurLabelType, set by the last `L` (cif/CIFrdpt.c:798-802). The loop runs i < crs_nLayers, the count of layer/templayer rules (CIFrdtech.c:734-748), but indexes crs_labelLayer and crs_labelSticky by CIF-layer-name index (CIFReadNameToType, CIFrdtech.c:135-175). It takes the first such index whose label type equals the type. The sticky flag can therefore come from a different CIF layer, or be missed when that layer's index is >= crs_nLayers. 95 labels behave the same way (CIFrdcl.c:1653). Magic's own comment ('Should do this better, by defining cifCurLabelFlags') marks the intent.
- Options: replicate | fix (use the current L layer's kind) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, which changes the entry's 'fix'. ON: replicate the search over indices 0..crs_nLayers-1 by label type exactly. OFF: use the label kind of the CIF layer selected by the current `L`. When there is no valid current layer, the label is not sticky. This is what Magic's comment says was intended. (corrected 2026-11-13, was fix)

### Q-MASK-328: statements before the first style are ignored
- Kind: doc-mismatch
- Magic behavior: The manual says a style named "default" is assumed; the code ignores lines until the first `style`.
- Evidence: cif/CIFrdtech.c:622-624, doc/latexfiles/maint2.tex:1712-1714
- Options: replicate | fix (implicit "default" style)
- Recommendation: replicate with a warning.
- Resolution: n/a — spec follows Magic's code; documentation is wrong; statements before the first `style` are ignored (spec conventions §2.7, 2026-11-07).

### Q-MASK-329: GDS text on unmapped layer indexes kind table at -1
- Kind: bug
- Magic behavior: When ignore-unknown-layer-labels is off, crs_labelSticky[cifnum] is read with cifnum = -1.
- Evidence: calma/CalmaRdpt.c:1015, calma/CalmaRdpt.c:1038
- Options: fix (treat kind as NONE)
- Recommendation: fix.
- Resolution: fix — the label kind of an unmapped layer is treated as NONE, without indexing the table at -1 (X-2 tier 1, 2026-11-07).

### Q-MASK-330: not-square drops non-manhattan geometry
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Split tiles are never copied to the result of not-square.
- Evidence: cif/CIFgen.c:4738
- Verified 2026-11-13: Confirmed at cif/CIFgen.c:4738 (`if (IsSplit(tile)) return 0;`). The comment there reads 'Non-Manhattan tiles are never square', so the intent was to keep them. The same function (CIFGenLayer, CIFgen.c:5420-5431) serves both cifinput and cifoutput, so this is the same question as Q-MASK-7. The four neighbour tests all read TiGetBottomType of the neighbour (4747-4780).
- Options: replicate | fix (keep triangles) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, which changes the entry's 'replicate'. Resolve together with Q-MASK-7, so input and output behave the same. ON: split tiles are dropped. OFF: split tiles are kept and copied into the result with their diagonal, since by the code's own comment a triangle is never square. Square-isolated manhattan tiles are removed in both modes. sky130/gf180/IHP input styles use not-square only on contact layers, which are manhattan, so PDK reads are the same in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-331: warning redirect not honored for CIF input
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `cif warning redirect FILE` only redirects GDS read messages.
- Evidence: calma/CalmaRead.c:195-205, cif/CIFrdutils.c:132-182
- Verified 2026-11-13: Confirmed. `cif warning redirect FILE` sets the shared CIFWarningLevel and CIFErrorFilename (commands/CmdCD.c:2199-2225). Only CalmaReadFile opens the file (calma/CalmaRead.c:195-202). CIFReadError/CIFReadWarning (cif/CIFrdutils.c:131-176) print everything for redirect and align. Also: `cif warning` and `gds warning` set the same global, and CIF message bodies go to raw stderr via Vfprintf while the prefix goes through TxError.
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, which changes the entry's 'fix' to follow the standing rule. ON: under `redirect`, CIF read messages print to the console as Magic prints them, and only GDS read uses the file. OFF: CIF read errors and warnings, prefix and body, go to the redirect file, opened for writing at the start of each read as GDS does. The console gets nothing when no file name was given, as for GDS. In both modes the prefix and the body go to the same stream; Magic's split of TxError and raw stderr is not reproduced. (corrected 2026-11-13, was fix)

### Q-MASK-332: flat CIF write lacks reducer check
- Kind: bug
- Magic behavior: CIFWriteFlat does not refuse styles with reducer 0 (division by zero).
- Evidence: cif/CIFwrite.c:729-792 vs cif/CIFwrite.c:142-147
- Options: fix (same refusal as hierarchical)
- Recommendation: fix.
- Resolution: fix — flat CIF write refuses a style with reducer 0, like the hierarchical write, instead of dividing by zero (X-2 tier 1, 2026-11-07).

### Q-MASK-333: CIF wires use square end extensions
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: CIF wire ends are extended by half width (square), while the CIF standard specifies round ends.
- Evidence: cif/CIFrdpt.c:705-764 (corrected 2026-11-13: Behaviour is right. CIFParseWire is at cif/CIFrdpt.c:702-747 and passes endcap = TRUE at line 744. The half-width extension along the end segment is applied in CIFPaintWirePath (cif/CIFrdpt.c:339 ff., ~403-420: xmin -= width*cos(theta)). The square end is a deliberate parameter (also used by PaintWireList), not an accident.)
- Options: replicate | fix
- Recommendation: replicate, in both modes. Magic always uses square half-width extensions, and the endcap parameter shows this is intended, so there is no 'intended' Magic behaviour for an OFF mode to restore. Round ends would require Hefesto to invent a polygon approximation of the CIF round flash that no reference tool produces. Document the difference from the CIF standard.

### Q-MASK-334: grow-grid does not snap upper edges to the grid
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Edges move by |x| mod G (xtop += |xtop| mod G; xbot -= |xbot| mod G), so 7 becomes 9 for G=5 and -7 becomes -9.
- Evidence: cif/CIFgen.c:526-533
- Verified 2026-11-13: Confirmed at cif/CIFgen.c:526-533, after cifScale scaling at 519-522. This is the same function as the output operator (Q-MASK-1). No reference PDK cifinput uses grow-grid.
- Options: replicate | fix (round outward to multiples of G) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. Resolved together with Q-MASK-1, with the same answer for input styles. With magic-compat ON, edges move by |x| mod G. With it OFF, edges round outward to multiples of G (floor for lower edges, ceil for upper edges). Infinite edges are unchanged in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-335: malformed CIF call transformation is used unscaled
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: On a transform parse error the call is still placed, with the partial transform's translation still in read units.
- Evidence: cif/CIFrdutils.c:1540-1595, cif/CIFrdcl.c:1349 (corrected 2026-11-13: Behaviour is right. CIFParseTransform is cif/CIFrdutils.c:1515-1587. Its error returns (1531-1575) skip the scaling at 1580-1585, so the partial transform's translation stays in CIF units. CIFParseCall ignores the result at cif/CIFrdcl.c:1353 (not 1349) and still places the use with that transform, using the pending 91 id.)
- Options: replicate | fix (skip the call) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, which changes the entry's 'fix' to follow the standing rule. Malformed input placing a use at a wrong position is wrong geometry, not a crash or a loss of user data. ON: report the error and place the call with the partial, unscaled transform, as Magic does. OFF: report the error plus 'call ignored', skip the call, and clear the pending 91 id, matching how other malformed commands are ignored. (corrected 2026-11-13, was fix)

### Q-MASK-336: CIF output tile order
- Kind: ambiguity
- Magic behavior: B/P lines follow the corner-stitched plane area-search order and maximal-horizontal-strip decomposition.
- Evidence: cif/CIFwrite.c:400-418
- Options: replicate the plane enumeration order | define canonical order
- Recommendation: replicate (needed for byte equality); share definition with gds-write.
- Resolution: compat — tile order bullet: B/P lines follow Magic's maximal-horizontal-strip decomposition and area-search order, byte-identical (X-1 as amended by D-1, 2026-11-07).

### Q-MASK-337: area label default differs from documentation
- Kind: doc-mismatch
- Magic behavior: `cif arealabels` defaults to no; doc/html/cif.html says default yes.
- Evidence: cif/CIFwrite.c:52, doc/html/cif.html
- Options: replicate code default
- Recommendation: replicate (default no).
- Resolution: n/a — spec follows Magic's code; documentation is wrong; `cif arealabels` defaults to no (spec conventions §2.7, 2026-11-07).

### Q-MASK-110: Structure with trailing garbage leaves input planes uncleared
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: When the record after a structure's elements is not ENDSTR, the reader skips to ENDSTR without converting or clearing the input-layer planes. The stale geometry then leaks into the next structure that is processed.
- Evidence: calma/CalmaRdcl.c:612-613, calma/CalmaRdcl.c:715-720 (corrected 2026-11-13: Behaviour confirmed, lines moved: the ENDSTR check is calma/CalmaRdcl.c:610 (`if (!calmaSkipExact(CALMA_ENDSTR)) goto syntaxerror;`) and the error path is CalmaRdcl.c:703-706. The planes are cleared only by CIFPaintCurrent (cif/CIFrdcl.c:985-986) or moved out by calmaExact. calmaParseStructure does not clear cifSubcellPlanes when a structure starts (CalmaRdcl.c:493-497), so the stale geometry is painted into the next cell that reaches CIFPaintCurrent. The error path also skips label cleanup, DBAdjustLabelsNew, unique-id generation and CDPROCESSEDGDS.)
- Options: replicate | fix (clear the input planes on syntax error) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: same as Magic. Report the unexpected record, skip to ENDSTR and leave the input planes uncleared, so their geometry ends up in the next structure that is painted. OFF: clear the input planes, report the error and drop the malformed structure's geometry. Only malformed files are affected, and the ON golden output is still defined. Tier 1 would also be defensible, because the leak corrupts a different, well-formed cell; the standing rule settles it as compat. (corrected 2026-11-13, was fix)

### Q-MASK-111: "Rewound" flag is session-global and never reset
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Once any read rewinds the file during look-ahead, duplicate-definition errors are suppressed for every later GDS read in the session.
- Evidence: calma/CalmaRdcl.c:56, calma/CalmaRdcl.c:170, calma/CalmaRdcl.c:439
- Options: replicate | fix (reset per read) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: once any read rewinds, the flag stays set for the rest of the session and suppresses the 'already defined' and 'Ignoring duplicate definition' errors for processed duplicates (CalmaRdcl.c:56, 170, 439). OFF: reset the flag at the start of each `gds read`. Only diagnostics change; the geometry is the same in both modes. (corrected 2026-11-13, was fix)

### Q-MASK-112: noduplicates ignored when the cell is first seen as a reference
- Kind: bug
- Magic behavior: The "predefined" flag is set only if the first occurrence of the name in the file is its definition. For top-down files, an in-memory cell referenced before its definition is overwritten despite `noduplicates yes`, even though "Using pre-existing cell definition" is printed.
- Evidence: calma/CalmaRdcl.c:1758-1840 (was_called/predefined logic), calma/CalmaRdcl.c:1192
- Options: replicate | fix (remember predefined status per name)
- Recommendation: fix. The printed message already shows the intent.
- Resolution: fix — the predefined status is remembered per name, so `noduplicates yes` never overwrites an in-memory cell (silent data loss) (X-2 tier 1, 2026-11-07).

### Q-MASK-113: Round path ends (PATHTYPE 1) read as flush
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: PATHTYPE 1 is reported as unsupported and treated as flush (no extension), although a code comment says round ends are treated as square.
- Evidence: calma/CalmaRdpt.c:553-563
- Verified 2026-11-13: Confirmed at calma/CalmaRdpt.c:552-562 (warning, then pathtype = SQUAREFLUSH). The comment at :584-589 claims that round ends are treated as square, which the code does not do.
- Options: replicate | fix (treat as half-width square, or approximate the round cap) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: warn 'pathtype 1 unsupported (ignored)' and treat the path as flush (type 0), as Magic does. OFF: treat PATHTYPE 1 as type 2 (half-width square extension) and warn that the round cap was approximated, which is what Magic's own comment says it intends. Hefesto does not approximate a true round cap. (corrected 2026-11-13, was replicate)

### Q-MASK-114: Path end extensions applied per axis
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: BGNEXTN/ENDEXTN move the end point by E in x and by E in y independently. On a diagonal end segment this extends by E·√2 along the diagonal and skews the end. Negative extensions are ignored.
- Evidence: calma/CalmaRdpt.c:641-672
- Verified 2026-11-13: Confirmed at calma/CalmaRdpt.c:641-672. The extensions are doubled first (:598-620), so each coordinate moves by E per axis, and extensions <= 0 are ignored.
- Options: replicate | fix (extend along the segment direction; support negative extensions) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: same as Magic. The end point moves by E independently in x and in y, and negative or zero extensions are ignored. OFF: extend along the end segment's direction by E, rounding to the doubled grid, and apply negative extensions as shortening. Manhattan paths with E >= 0 give identical results in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-115: Negative (absolute) path widths dropped
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: GDSII defines a negative WIDTH as an absolute width that is not affected by magnification. Magic drops paths with width <= 0 without a message.
- Evidence: calma/CalmaRdpt.c:675-679
- Verified 2026-11-13: Confirmed at calma/CalmaRdpt.c:675-679: width <= 0 frees the path with no message. The width was already scaled at :578-582.
- Options: replicate | fix (use |width|) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: drop paths with W <= 0 silently, as Magic does. OFF: for W < 0, use |W| with the unit conversion but without the SREF magnification (GDSII absolute width), and warn. W == 0 is dropped in both modes. The golden test MASK-GR-016 checks the ON behaviour. (corrected 2026-11-13, was fix)

### Q-MASK-116: Duplicate-point removal skips the head pair
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Consecutive duplicate points are removed only from position 1 onward. A duplicate of the first point stays, which gives a zero-length first segment with atan2(0,0) = 0.
- Evidence: cif/CIFrdpt.c:355-375
- Verified 2026-11-13: Confirmed at cif/CIFrdpt.c:355-372. The loop starts at the head's successor and removes P_i when P_i == P_{i+1}, so P_0 == P_1 survives and the first segment has theta = atan2(0,0) = 0 (:397-398). The start cap and offset are then aligned to +x, and a following westward segment triggers the 'duplicate point in path' warning (:470-473).
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: keep the head duplicate, as Magic does, which is needed for identical geometry on such inputs. OFF: also remove the duplicate at the head, so the first real segment sets the cap direction. Savepaths properties are recorded before this step (CalmaRdpt.c:724), so they keep the duplicate in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-117: Mitre-limit truncation drops the pending polygon
- Kind: bug
- Magic behavior: At a bend with |cos(alpha)| in [1e-10, 0.25) the routine stops. The polygon accumulated since the start, or since the last direction reversal, is never painted, so the whole path portion disappears rather than being truncated.
- Evidence: cif/CIFrdpt.c:459-495
- Options: replicate | fix (paint the accumulated polygon up to the bend)
- Recommendation: fix. Missing geometry is worse than an approximate end. Log the decision.
- Resolution: fix — at a mitre-limit stop the accumulated polygon is painted up to the bend instead of being dropped (silent data loss) (X-2 tier 1, 2026-11-07).

### Q-MASK-118: Path outline uses H = 1.5708 and savetheta = 0 for the first segment
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The perpendicular offset uses 1.5708 rather than π/2, which introduces a relative error of ~3.7e-6. The inner-corner correction of the first joint uses savetheta = 0 instead of the first segment's angle.
- Evidence: cif/CIFrdpt.c:395-410, cif/CIFrdpt.c:540-575
- Verified 2026-11-13: Lines are slightly off. 1.5708 is used at cif/CIFrdpt.c:411-412 and :497. theta starts at 0 (:392) and savetheta = theta at :397, so the first iteration's joint correction (:540-571) uses savetheta = 0. The behaviour is as described.
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: use exactly 1.5708 and savetheta = 0 for the first joint, so vertices round as in Magic for very wide paths and short inner corners. OFF: use M_PI_2 and the first segment's own angle for the first joint correction. (corrected 2026-11-13, was replicate)

### Q-MASK-119: savepaths stores input units in a dimension property
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The recorded centerline values are input-unit coordinates (halved from doubled), not internal units. If the style scale factor is not 1, the property values do not match layout coordinates, although they are typed as dimensions.
- Evidence: cif/CIFrdpt.c:239-330, calma/CalmaRdpt.c:723
- Verified 2026-11-13: Confirmed at cif/CIFrdpt.c:239-300 (PROPERTY_TYPE_DIMENSION, value = doubled coordinate >> 1). The call site is calma/CalmaRdpt.c:724, not 723. The 'input units' are the CIF-scaled units after calmaReadScale1/2, not raw GDS units, and no division by the style scale factor is applied.
- Options: replicate | fix (divide by S) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: store the values exactly as Magic does, in CIF input units, because downstream scripts may rely on that and MASK-GR-021 checks it. OFF: convert the values to internal units, the same way CIFPaintCurrent converts geometry (divide by the style scale factor), so the dimension property matches the layout coordinates. When the scale factor is 1 the two modes are identical. (corrected 2026-11-13, was replicate)

### Q-MASK-120: TEXT WIDTH honored only right after PRESENTATION
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Without PRESENTATION, PATHTYPE and WIDTH are skipped. With PRESENTATION followed by PATHTYPE, WIDTH is also skipped.
- Evidence: calma/CalmaRdpt.c:828-880
- Verified 2026-11-13: Confirmed at calma/CalmaRdpt.c:826-882 (ignore set {PATHTYPE, WIDTH} at :769). The GDSII record order is PRESENTATION, PATHTYPE, WIDTH, STRANS, so a conforming file that includes PATHTYPE loses its WIDTH. Size only matters when PRESENTATION is present, because font < 0 uses DBPutLabel (:1088-1092). Magic's writer emits PRESENTATION and MAG but never WIDTH (CalmaWrite.c:2900-2906).
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: same as Magic, so WIDTH is honored only when it directly follows PRESENTATION. OFF: honor a WIDTH record anywhere in the PATHTYPE/WIDTH run before STRANS, so standard-ordered files keep their text size. Files without PRESENTATION are unaffected in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-121: Label positions truncated toward zero
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The text point and the pin-shape rectangle are divided by the scale factor with C truncation. Negative off-grid positions therefore round up and positive ones round down. Instance origins, in contrast, use exact or rescaled conversion.
- Evidence: calma/CalmaRdpt.c:938-941, calma/CalmaRdpt.c:330-334 (corrected 2026-11-13: Behaviour confirmed, lines moved. The text point is divided at calma/CalmaRdpt.c:951-952 (not 938-941) and the pin-shape rectangle at CalmaRdpt.c:333-336 (not 330-334), both by crs_scaleFactor with C truncation.)
- Options: replicate | fix (floor or nearest) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: truncate toward zero, as Magic does. OFF: use floor division for both the text point and the pin rectangle, so off-grid negative and positive positions are treated alike. Floor is chosen to match the placement of other geometry. On-grid labels give identical results in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-122: Inconsistent font-size units between DRC default, 1 µm cap and MAG
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The default size from the DRC width is scaled by 8/10. The cap is 800·m/S. MAG gives 1000·mag·magscale·m/S. A MAG of 0.8 therefore equals the default cap. The formula for the DRC-derived size multiplies by den/num, which makes it look inverted.
- Evidence: calma/CalmaRdpt.c:800-820, calma/CalmaRdpt.c:898-912
- Verified 2026-11-13: Confirmed at calma/CalmaRdpt.c:805-819 (DRC default: size *= Scale2*m*8; size /= Scale1*S*10; cap 800*m/S) and :905-914 (MAG: (int)(0.5 + mag*1000*magscale*m/S), which rounds). There is a third unit as well: TEXT WIDTH gives size = CIFScaleCoord(width) with no 8/10 factor (:866-882).
- Options: replicate | fix
- Recommendation: replicate in both modes, using the exact integer and double formulas, including the +0.5 rounding for MAG and the CIFScaleCoord path for WIDTH. The font size is stored in .mag files. No intended unit is documented, so a 'corrected' formula would be Hefesto's own invention and could not be tested against any reference. The DRC-default scaling by den/num looks inverted but has no specified target.

### Q-MASK-123: Pin placeholder converts text-kind labels to ports
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: When a TEXT consumes an empty placeholder label created by a pin shape on a `text`/`sticky` layer, the label gets all four port directions and a computed port index (possibly 0), so it becomes a port-like label.
- Evidence: calma/CalmaRdpt.c:1053-1090
- Verified 2026-11-13: Confirmed. Placeholders are created for any label kind other than NONE at calma/CalmaRdpt.c:338-356, and get port data only for the `port` kind (:357-385). When a TEXT consumes a placeholder (:1051-1086) it sets all four PORT_DIR flags and a recomputed index (0 when no other labels exist) whatever the kind, so a `text` (sticky) label becomes a port.
- Options: replicate | fix (only set port data for `port` layers) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: same as Magic; MASK-GR-030 confirms this. OFF: when a TEXT consumes a placeholder, set port directions and an index only if the placeholder already had PORT_DIR flags (that is, it came from a `port` layer). Otherwise take only its rectangle. (corrected 2026-11-13, was replicate)

### Q-MASK-124: AREF row spacing rounding discarded
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: For columns, a non-divisible displacement is rounded with (p+(cols+1)/2)/cols. For rows, the rounded value is computed and then overwritten by the truncated p/rows (a missing `else`).
- Evidence: calma/CalmaRdcl.c:1490-1500
- Verified 2026-11-13: Confirmed at calma/CalmaRdcl.c:1488-1499. The `else` is missing before `ysep = p.p_y / rows`, so the rounded n is printed in the error message ('-> n') but the truncated value is used.
- Options: replicate | fix (round like columns) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: use the truncated ysep, as Magic does, and report the rounded value in the message. OFF: use the rounded value (p+(rows+1)/2)/rows, the same as for columns, which also matches the value the message prints. Well-formed GDS gives identical results in both modes, and the error is reported in both. (corrected 2026-11-13, was replicate)

### Q-MASK-125: Unknown instance property stops property parsing
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: A PROPATTR other than 61/98/99 ends the property loop. Its PROPVALUE and any following 61/98/99 properties are skipped.
- Evidence: calma/CalmaRdcl.c:1385-1445
- Verified 2026-11-13: Confirmed at calma/CalmaRdcl.c:1384-1442. An unknown PROPATTR is consumed, the next PROPVALUE header ends the loop, and that value and all later properties are discarded by calmaSkipTo(CALMA_ENDEL) at CalmaRdcl.c:780. The known attributes are 61, 98 and 99 (calmaInt.h:102-104).
- Options: replicate | fix (skip the unknown pair and continue) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: stop at the first unknown attribute, as Magic does, so the default use ids, and therefore the written output, match. OFF: skip the unknown PROPATTR/PROPVALUE pair and keep parsing, so 61, 98 and 99 properties that follow are honored and instance names from third-party tools are kept. (corrected 2026-11-13, was fix)

### Q-MASK-126: Look-ahead seek assumes minimal STRNAME padding
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: After matching a STRNAME, the reader seeks back by BGNSTR length + 4 + strlen rounded to even. If a writer pads with more NULs, the position is wrong and parsing desynchronizes.
- Evidence: calma/CalmaRdcl.c:148-160
- Verified 2026-11-13: Confirmed at calma/CalmaRdcl.c:148-156. The reader seeks back by -(nbytes_BGNSTR + even(strlen) + 4). Because strlen stops at the first NUL, a STRNAME with extra NUL padding, or with an odd unpadded length, makes the seek land at the wrong position and the parse desynchronizes.
- Options: replicate | fix (seek back using the actual STRNAME record length)
- Recommendation: fix in both modes: seek back using the STRNAME record's actual length from its header, or save the BGNSTR offset before reading it. Conforming files give byte-identical results. For over-padded files Magic produces only a desynchronized, garbage parse, which is no meaningful output to match (tier 1 / no Magic output).

### Q-MASK-127: Temporary polygon-cell flattening matches any "polygon*" cell
- Kind: bug
- Magic behavior: In `temporary` mode, every use of a cell whose name starts with "polygon" is flattened and the cell deleted, including genuine GDS structures named e.g. "polygon_ring".
- Evidence: calma/CalmaRdcl.c:323-350
- Options: replicate | fix (track created polygon cells explicitly)
- Recommendation: fix.
- Resolution: fix — only polygon cells created by the reader are flattened and deleted; genuine structures named `polygon*` are kept (silent data loss) (X-2 tier 1, 2026-11-07).

### Q-MASK-128: Home-directory substitution in GDS_FILE uses the wrong string
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The "~" substitution tests and copies the cell's .mag file path instead of the GDS path. A cell loaded from a .mag under $HOME therefore gets GDS_FILE set to "~/<mag path>". Freshly read cells have no .mag path, so no substitution happens.
- Evidence: database/DBio.c:6095-6108
- Verified 2026-11-13: Confirmed at database/DBio.c:6097-6109. The test is pathstart starting with '/', but the HOME prefix check and the copied text use cellDef->cd_file. The GDS reader calls it at calma/CalmaRdcl.c:549, and the GDS writer compares library names through the same function at calma/CalmaWrite.c:533. The .mag `use` caller (DBio.c:6184) is unaffected, because there pathstart lies inside cd_file.
- Options: replicate | fix (substitute on the GDS path)
- Recommendation: fix in both modes: apply the '~' substitution to the GDS path itself, and only when that path is absolute and lies under $HOME. Applying it to the path in both the reader's GDS_FILE and the writer's library-name comparison keeps them consistent. Replicating Magic stores a .mag path as GDS_FILE, so a later GDS write cannot find the vendor GDS and loses that cell's contents (tier 1, data loss).

### Q-MASK-129: GDS_FILE stores the path as found on the search path
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: GDS_FILE is the name returned by the path search (possibly relative to the current directory). It is not canonicalized.
- Evidence: commands/CmdCD.c:1017-1030, calma/CalmaRdcl.c:544-570
- Verified 2026-11-13: Confirmed. PaZOpen with the search path returns namep at commands/CmdCD.c:1017-1030, the substitution is at calma/CalmaRdcl.c:549, and the property is stored at :565-570. A relative name stays relative to the cwd at read time, and the PDK and '~' substitutions only apply when the stored string starts with the matching absolute prefix.
- Options: replicate | fix (store an absolute path) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: store the name as returned by the search, as Magic does, so the .mag files match Magic's. OFF: store the absolute, canonical path before substitution. GDS_FILE then still resolves after a change of directory, and the PDK and '~' substitutions apply consistently. (corrected 2026-11-13, was replicate)

### Q-MASK-130: `gds rescale no` also forces warning level "limit"
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Disabling rescale silently changes the global warning level to `limit`, which is also shared with CIF.
- Evidence: commands/CmdCD.c:912-914
- Verified 2026-11-13: Confirmed at commands/CmdCD.c:912-914. CIFWarningLevel is the global level that CIF read also uses.
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: `gds rescale no` also sets the shared warning level to `limit`, as Magic does. OFF: leave the warning level unchanged. This affects diagnostics only, and the ON log output still matches Magic. (corrected 2026-11-13, was replicate)

### Q-MASK-131: "read into edit cell" documentation mismatch
- Kind: doc-mismatch
- Magic behavior: The help says "read ... into edit cell", but no geometry is placed in the edit cell. Structures become cells, and the cell named by LIBNAME is loaded into the window.
- Evidence: commands/CmdCD.c:170-172, calma/CalmaRead.c:295-310
- Options: replicate behavior | document
- Recommendation: replicate the behavior and fix the documentation.
- Resolution: n/a — spec follows Magic's code; documentation is wrong; the read places no geometry in the edit cell (spec conventions §2.7, 2026-11-07).

### Q-MASK-132: Host dependencies (time zone, PDK variables, gzip)
- Kind: platform
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: GDS dates are converted with mktime (host local time zone, so results differ between hosts). PDK prefixes come from Tcl globals. $HOME is used for "~". Files are opened through zlib with search paths.
- Evidence: calma/CalmaRdio.c:335-370, database/DBio.c:6060-6111
- Verified 2026-11-13: Confirmed. mktime with tm_isdst = -1 at calma/CalmaRdio.c:335-353 gives host local time. PDK variables come from Tcl globals and '~' from getenv("HOME") at database/DBio.c:6060-6111. Files are opened with PaZOpen (zlib) and the search path at commands/CmdCD.c:1017-1030.
- Options: replicate (local time) | fix (UTC) | platform alternative (configurable PDK variables on iPadOS)
- Recommendation: platform. Convert GDS dates in the host's local time zone by default, so .mag timestamps match Magic on the same host, and provide a time-zone override setting (e.g. UTC) that golden captures and CI pin. Resolve PDK_PATH/PDKPATH/PDK_ROOT/PDKROOT from Tcl globals, since X-3 embeds Tcl on every platform, with the X-4 settings store as the source on iPadOS. Use the platform-services home directory for '~'. Bundle zlib so gzip input works on every platform.

### Q-MASK-133: Cell-id label test reads layer index -1
- Kind: bug
- Magic behavior: For a TEXT on an unmapped layer, the "cellid" check indexes the label-kind table with -1 (undefined behavior) before the unknown-layer branch.
- Evidence: calma/CalmaRdpt.c:1015
- Options: replicate (impossible to replicate deterministically) | fix
- Recommendation: fix. Treat unmapped pairs as not cell-id (the spec states this).
- Resolution: fix — a TEXT on an unmapped layer is treated as not cell-id, without indexing the kind table at -1 (X-2 tier 1, 2026-11-07).

### Q-MASK-134: BGNSTR year interpreted as years since 1900
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The year field is used directly as the C tm_year (years since 1900), consistent with Magic's own writer. Files from tools that write the full year (e.g. 2024) yield dates around year 3924, and the 32-bit cast may overflow.
- Evidence: calma/CalmaRdio.c:340-368
- Verified 2026-11-13: Confirmed at calma/CalmaRdio.c:340-353. READI2 reads the year straight into tm_year, and the result is cast `(int)mktime(...)`, so a full year (e.g. 2024 becomes 3924) overflows 32 bits and is truncated. Magic's writer emits localtime() fields, i.e. years since 1900 (CalmaWrite.c:3127).
- Options: replicate | fix (treat values >= 1900 as full years) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: use the year as tm_year and keep the 32-bit truncation of the result (defined as the low 32 bits, two's complement) so that cd_timestamp matches Magic. OFF: treat year >= 1900 as a full year and smaller values as years since 1900, and store the timestamp without overflow. Magic only checks timestamps for equality, so the OFF values stay internally consistent. (corrected 2026-11-13, was replicate)

### Q-MASK-200: Permissive GDS character table swaps '%' and '&'
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: With `options calma-permissive-labels`, string bytes 0x25 '%' are written as '&' and 0x26 '&' as '%' (table entries transposed).
- Evidence: calma/CalmaWrite.c:219 (corrected 2026-11-13: Behaviour is right, but the line is wrong. The transposed entries are at calma/CalmaWrite.c:234 (row SP-'), not :219. The same table is also used for structure names in calmaOutStructName (CalmaWrite.c:1888-1902), so a cell named 'a%b' is written as 'a&b' with a warning. IHP sg13g2 sets `options calma-permissive-labels` (ihp-sg13g2-GDS.tech:219), so a real PDK hits this. Magic tree HEAD 9c83a9a6 has no source diff from fc5daa9e under calma/, cif/, commands/, database/ or bplane/.)
- Options: replicate | fix (identity mapping for all printable characters) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto writes 0x25 as '&' and 0x26 as '%' in STRING, PROPVALUE and STRNAME records, with Magic's 'characters changed' warning. Layout data is never mutated (Q-MASK-208 resolution). With it OFF, the permissive table maps every printable character 0x20-0x7E to itself. This is the standing rule for an output-value quirk, and ON keeps IHP output byte-identical. The swap does corrupt label and net text on a round trip, so tier 1 'fix' is the alternative if that is treated as data loss. (corrected 2026-11-13, was fix)

### Q-MASK-201: UNITS values stick after an angstrom write
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The UNITS values are function-static and are only ever set to the angstrom values. After one write with `units angstroms`, every later write in the same session emits 0.0001/1e-10, even for nanometer styles, while coordinates are scaled for nanometers.
- Evidence: calma/CalmaWrite.c:3066-3102
- Options: replicate | fix (compute UNITS from the current style on every write)
- Recommendation: fix (tier 1). `static double useru = 0.001; static double mum = 1.0e-9;` (CalmaWrite.c:3066-3067) are only ever assigned the angstrom values (:3097, :3100). After one angstrom write, every later nanometer write in the session declares 1e-10 m/dbu while its coordinates are scaled x10 for nm (dbunits, :1454). The file is then silently 10x too small, which is data corruption. In both modes Hefesto computes UNITS from the current style on every write: 0.001/1e-9 for nm and 0.0001/1e-10 for angstrom. The golden cases use one write per session, so no golden case shows the difference.

### Q-MASK-202: Modification date is current time, documentation says zero
- Kind: doc-mismatch
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The second date in BGNLIB and BGNSTR is always the current time. gds.html says it is set to zero. As a result, two writes are never byte-identical.
- Evidence: calma/CalmaWrite.c:1444, calma/CalmaWrite.c:3078; doc/html/gds.html ("datestamp") (corrected 2026-11-13: Behaviour is right, but the lines are shifted. The current-time modification date is written at CalmaWrite.c:1445 (BGNSTR in calmaOutFunc) and :3079 (BGNLIB), and also at :443 (full-dump BGNSTR) and :1300 (vendor-cell BGNSTR). gds.html:256-261 documents 'sets the modification date stamp to zero'.)
- Options: replicate | fix (write zero, or the creation stamp) | add an option for reproducible output | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the modification date in BGNLIB and every BGNSTR is the current time, as in Magic. The M5 exit criterion already masks these fields. With it OFF, Hefesto follows gds.html: the modification date is time 0, encoded the same way `gds datestamp no` encodes the creation date (so its calendar fields follow Q-MASK-203). Writes then become reproducible without a new option. The tier is 2: only output values change. (corrected 2026-11-13, was replicate)

### Q-MASK-203: Dates depend on the host time zone
- Kind: platform
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Dates are converted with localtime(). `datestamp 0` writes 1970-01-01 00:00:00 only under UTC, and 1969-12-31 in western time zones.
- Evidence: calma/CalmaWrite.c:3127-3134
- Verified 2026-11-13: localtime() at CalmaWrite.c:3127 (calmaOutDate :3122-3135); fields tm_year, tm_mon+1, tm_mday, tm_hour, tm_min, tm_sec. Range in the entry is accurate.
- Options: replicate (use the host local zone) | fix (always UTC) | platform alternative (explicit zone setting; iPadOS sandbox has the device zone)
- Recommendation: replicate, in both modes. Dates are converted with the host's local time zone, as Magic does. On iPadOS that is the device zone, so no platform alternative is needed. This is the GDS convention and not a defect, and a UTC-only rule would make Hefesto differ from Magic on every non-UTC host. The golden cases and CI run with TZ=UTC so that t = 0 encodes as 1970-01-01 00:00:00. A test also pins one non-UTC zone, to prove that the conversion uses the host zone.

### Q-MASK-204: Order of cell references and structures depends on the spatial-index binning state
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Child uses are enumerated by the bin-plane index. For unbinned planes this is LIFO (reverse insertion). Once bins are built (after enough uses and a partial-area search), the order follows bin layout and can change with editing history. This order determines both SREF order and the post-order of structures.
- Evidence: database/DBcellsrch.c:86-109, 1263-1281; bplane/bpMain.c:114-170; bplane/bpBins.c:645-707; bplane/bpEnum.c:85-120
- Verified 2026-11-13: The behaviour is confirmed: DBCellEnum (DBcellsrch.c:1263-1281) calls DBSrCellPlaneArea (:86-109) with BPE_OVERLAP over TiPlaneRect. BPAdd (bpMain.c:114-170) pushes onto the head of bp_inBox (LIFO) unless bins exist and the element fits bp_binArea. A non-surrounding BPE_OVERLAP/TOUCH enumeration calls bpBinsUpdate (bpEnum.c:89-118), which rebuilds the bins when the inBox holds at least bpMinBAPop = 10 elements (bpBins.c:49, :651). So any cell with 10 or more uses that has had a partial-area search (display, DRC, extraction, select) changes order, and the order depends on history.
- Verified 2026-11-13 (review): When the order changes depends on the writer and the session. CIFGenSubcells searches parts of the cell's own plane: DRCFindInteractions once per step square (drc/DRCsubcell.c:451) and, at each interaction, cifGenHierArea (cif/CIFhier.c:885-902, called at :1082), whose DBTreeSrTiles also reaches every descendant's plane. In CIF, `C` calls are written after that search (CIFGenSubcells at cif/CIFwrite.c:394, calls at :467), so a cell with 10 or more uses gets binned call order. Symbol numbering is done earlier (DBCellEnum at :333) and is unbinned, unless an ancestor's search already reached the cell's plane (CIF is written top-down). In GDS, SREFs are written before the search (calma/CalmaWrite.c:1486, then CIFGenSubcells at :1510-1514), and structures are written bottom-up, so the first `gds write` of a fresh session is unbinned. A second write in the same session, or one after `cif write`, DRC or extraction, can differ. DBReComputeBbox removes and re-places every use of a cell whose bounding box changed (database/DBcellbox.c:728-764), which moves those uses to the head of the parent's inBox; this happens, for example, when a child is read after its parent. `.mag` output sorts uses alphabetically (database/DBio.c:4775-4791), so the .mag `use` order, and the OFF order derived from it, do not depend on history.
- Options: replicate (reimplement the bin-plane exactly) | fix (define a canonical order, e.g. reverse insertion order or sorted by use id) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. The standing compat rule and the byte-identical exit criterion of M5 decide it, so it is not a judgement item. With magic-compat ON, Hefesto's cell-use index reproduces bplane's observable enumeration order: LIFO inBox, the re-placement of uses by DBReComputeBbox, the bin rebuild at 10 uses on a non-surrounding search, bin geometry and the bin walk order. Each writer enumerates at the same points as Magic (GDS SREFs before CIFGenSubcells, CIF numbering before it and CIF calls after it), so it sees the same index state. With OFF, the order is a canonical one (reverse insertion, that is, the reverse of the .mag `use` order or the GDS SREF order), independent of history. Replicating only the unbinned order in both modes is not enough: it fails CIF goldens for any cell with 10 or more uses that has an interaction (or is larger than one step square), and GDS goldens for a second write. A canonical order in both modes gives up the byte-identical criterion for those cells. The golden harness pins Magic's session state: each case does one write in a fresh session with no DRC or extraction before it, unless the case tests history on purpose. Cases with 10 or more uses and an interaction verify CIF and GDS. This decides the SREF order, the structure post-order and the CIF order (Q-MASK-323); OASIS inherits it. The bplane emulation is the main cost (Effort and scope notes in docs/decisions/m5-decision-table.md). (corrected 2026-11-13, was replicate)

### Q-MASK-205: Order of port labels with equal port index
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Port labels are collected into a list in reverse label order and sorted with the C library qsort by port index. The order of equal indices depends on the libc implementation.
- Evidence: calma/CalmaWrite.c:1548-1600
- Verified 2026-11-13: In CalmaWrite.c:1544-1592, port labels are prepended to a list, so the list is in reverse label order. The list is copied in that order to an array, and the array is sorted with qsort(compport). compport (:1387-1398) compares pl_port only, so equal port indices compare equal. glibc's qsort uses merge sort (stable) for arrays of this size. macOS/BSD qsort is not stable.
- Options: replicate (glibc behavior) | fix (stable sort preserving label-list order)
- Recommendation: replicate, in both modes. Hefesto stable-sorts the reversed label list by port index, so labels with equal port indices are written in reverse label-list order. This matches the Linux/glibc Magic that produces the reference output, and it does not depend on the libc Hefesto runs on. The reference platform is Linux: Magic's golden outputs are captured only in the pinned Debian trixie image (tools/golden/container/Containerfile:27, pinned in tools/golden/pins.toml:24-26; trixie ships glibc 2.41), built and run by .github/workflows/golden-capture.yml on ubuntu-24.04 (:57, :155-177). The `local` capture runner builds only the hash harness, not Magic (tools/golden/goldenlib/refrunners.py:145-146), so no macOS Magic output is ever a reference. A golden case with two port labels that share an index confirms it against the reference Magic build.

### Q-MASK-206: Port labels dropped when the type has no `labels ... port` declaration
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Port labels are written only if the output style declared `labels <type> port` for their type. `labels <type>` alone sets the port text layer but not the port layer, and the writer requires the port layer, so the port is silently omitted from GDS.
- Evidence: calma/CalmaWrite.c:1586-1591; cif/CIFtech.c:933-955
- Verified 2026-11-13: The behaviour is confirmed: The writer (CalmaWrite.c:1584-1590) sets type = cs_portLayer[...] and writes a port label only if type >= 0. In CIFtech.c:938-958, `labels <type>` with no argument sets cs_labelLayer and cs_portText but not cs_portLayer. `labels <type> noport` sets neither port field. Magic's own comment (CIFtech.c:948-952) says a later `port` statement makes the data type separate from the text type. That implies the intended default is the same layer. sky130A and IHP declare `labels X port` for their pin layers.
- Options: replicate | fix (fall back to the label layer when no port layer exists) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, with a warning in both modes. magic-compat ON: a port label whose type has no port layer is omitted, as in Magic, and Hefesto warns once per type and write. magic-compat OFF: when the port layer is missing but a port-text layer exists, the label's rectangle is written on the port-text layer, which is the behaviour the CIFtech comment describes. Types with neither layer are still skipped. PDK tech files that declare `port` are unaffected in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-207: string-limit truncation of odd-length strings keeps 31 characters
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The length is rounded up to even before truncation, so for an odd length > 32 the kept tail is 31 characters plus a NUL pad.
- Evidence: calma/CalmaWrite.c:3197-3208 (corrected 2026-11-13: Behaviour is right, but the lines are shifted to CalmaWrite.c:3194-3202. `if (len & 01) len++;` comes before the CWF_STRING_LIMIT test. Then locstr = str + len - 32 and len = 32, so for an odd original length greater than 32 the output is 31 characters plus the NUL terminator. Magic's own comment (:3181-3185) says it outputs the last CALMANAMELENGTH characters.)
- Options: replicate | fix (keep 32 real characters) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. magic-compat ON: with `string-limit`, Hefesto pads to even length and then keeps the last 32 bytes, so an odd-length string keeps 31 characters plus 0x00, as in Magic. magic-compat OFF: Hefesto keeps the last 32 real characters (offset n_original - 32, length 32, already even), which is what Magic's comment states. The truncation warning is the same in both modes. The flag is legacy-only and no current PDK sets it. (corrected 2026-11-13, was replicate)

### Q-MASK-208: Control characters become NUL bytes, and strings are mutated in place
- Kind: bug
- Magic behavior: Bytes 0x01-0x1F and 0x7F map to 0x00 and are written inside the STRING/PROPVALUE data. The encoder also overwrites the label text or use id in memory with the mapped characters, which can truncate it at the new NUL.
- Evidence: calma/CalmaWrite.c:3218-3243
- Options: replicate output bytes but do not mutate data | fix (map to '_' or 'X')
- Recommendation: replicate the output bytes, and never mutate layout data (already stated as normative in the spec).
- Resolution: fix — layout data (label text, use ids) is never mutated by the encoder (data loss); the bytes written stay as Magic writes them (X-2 tier 1, 2026-11-07).

### Q-MASK-209: Sanitized structure names are not checked for uniqueness
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Cells `a-b` and `a_b` both produce STRNAME `a_b` under the strict table. The file then has duplicate structure definitions.
- Evidence: calma/CalmaWrite.c:1895-1918
- Verified 2026-11-13: The behaviour is confirmed in calmaOutStructName, CalmaWrite.c:1880-1918, which carries the comment 'We really should ensure that the new name is unique' (:1904). Also: With `gds lower no` (CalmaDoLower false), calmaOutStringRecord upper-cases the output (:3229-3232), so 'a' and 'A' also collide. (corrected 2026-11-13 (review): `string-limit` does not truncate structure names. calmaOutStructName sends a name longer than 32 characters to `XXXXX<calmanum>` (CalmaWrite.c:1907-1922), and calmaOutStringRecord pads odd lengths before its length test, so a name of 32 characters or fewer is never truncated (:3193-3201). Truncation collisions are possible only for the vendor names that a full dump writes directly (STRNAME and SNAME at :612 and :629).)
- Options: replicate | fix (uniquify with a suffix and warn)
- Recommendation: fix (tier 1: data loss), in both modes. Two STRNAMEs that collide make every reader keep one definition, so the other cell's geometry is lost from the output. Hefesto keeps Magic's mapping, so names without a collision are byte-identical. When a mapped name equals a name already given to a different cell in this write, the later cell in write order gets `XXXXX<calmanum>`, Magic's fallback form for bad names, with the cell's Magic structure number (cd_client, as at CalmaWrite.c:1917-1920). That name is itself checked: if it is already used in this write, `_k` is appended with the smallest k >= 1 that is free. Its SREFs use the same name, and Hefesto warns. This matches the oasis-write rule (Q-OASIS-61). The alternative is compat (ON: write the duplicate and report an error). That is only worth it if a golden case depends on the broken output. (corrected 2026-11-13, was replicate)

### Q-MASK-210: Default extension test looks for '.' anywhere in the path
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `.gds`/`.gds.gz` is appended only if the path contains no '.', including in directory names (`./out/top` gets no extension).
- Evidence: commands/CmdCD.c:1046, commands/CmdCD.c:1066
- Verified 2026-11-13: commands/CmdCD.c:1046 `dotptr = strrchr(namep, '.')`; :1065-1066 PaZOpen appends gdsExt only if dotptr == NULL. Confirmed.
- Options: replicate | fix (test only the final path component) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. magic-compat ON: `.gds` or `.gds.gz` is appended only if the whole path contains no '.', as in Magic. `./out/top` gets no extension. magic-compat OFF: only the final path component is tested, so `./out/top` becomes `./out/top.gds`. This is the standing rule for a quirk that changes which file is written. Scripts that pass full paths get the same name in both modes. (corrected 2026-11-13, was fix)

### Q-MASK-211: Vendor full-dump prefix uses random()
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The two-character library prefix comes from the process-global random() sequence, which is never seeded. It depends on every earlier random() call in the session, so output is not reproducible across sessions with different histories.
- Evidence: calma/CalmaWrite.c:747-770 (corrected 2026-11-13: The prefix loop is at CalmaWrite.c:753-769 (random() % 26, then random() % 36, retried until unique in calmaPrefixHash). 'Never seeded' is wrong: the `random seed [value]` command calls srandom (commands/CmdRS.c:115). The only other consumers of random() are the `random` command itself (CmdRS.c:100/102) and the plow random tests (plow/PlowRandom.c:269). In a batch session the sequence is therefore deterministic: glibc random() with default seed 1, advanced by `random` calls and by earlier dumps.)
- Options: replicate (glibc random() with seed 1, counting calls) | fix (deterministic prefix derived from a hash of the file name) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. magic-compat ON: Hefesto implements glibc's random() (TYPE_3 additive-feedback generator, default seed 1) itself, so the sequence does not depend on the host libc. One generator per session (per project interpreter, per Q-SCRIPT-41) is shared by the full-dump prefix, the `random` command and `random seed`. OpenLane-style stream-outs with abstract (LEFview) cells then get Magic's prefixes and stay byte-identical. magic-compat OFF: the prefix is derived from a hash of the resolved library file name, and collisions are resolved deterministically, so the output does not depend on session history. This settles Q-OASIS-68 the same way. (corrected 2026-11-13, was fix)

### Q-MASK-212: Full-dump unit check depends on the `gds readonly` setting at write time
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The vendor library's units are checked only if `gds readonly` is currently yes. Otherwise a library with any units is dumped verbatim and may be at the wrong scale.
- Evidence: calma/CalmaRead.c:849-880; calma/CalmaWrite.c:722-730 (corrected 2026-11-13: Behaviour is right, but the CalmaWrite.c lines are wrong. calmaFullDump calls calmaParseUnits at CalmaWrite.c:715 (error and return at :715-719), not :722-730. In CalmaRead.c, the check at :857-880 runs only `if (CalmaReadOnly == TRUE)` against CIFCurStyle's angstrom flag. When the check fails, calmaFullDump returns before it records the library in calmaLibHash, so each later abstract cell from the same file retries and reports the error again.)
- Options: replicate | fix (always check units against the output style)
- Recommendation: fix (tier 1: silent wrong-scale output), in both modes. A full dump always checks that the library's meters per database unit round to the current style's unit (1e-9 for nm, 1e-10 for angstrom), whatever the current `gds readonly` value. On a mismatch the dump is aborted with Magic's error, the library is recorded so the error is reported once per write, and the write's status follows the `undefined` rule. The check is already done whenever readonly is yes, so normal readonly flows produce the same output as Magic. It only changes the case where Magic would copy a library at the wrong scale.

### Q-MASK-213: Empty vendor library writes a stray record header
- Kind: bug
- Magic behavior: If the dumped library has no structure, the next record header (normally ENDLIB) is copied to the output with data type forced to 2, producing a premature `00 04 04 02` inside the stream.
- Evidence: calma/CalmaWrite.c:425-433
- Options: replicate | fix (write nothing)
- Recommendation: fix.
- Resolution: fix — an empty vendor library writes nothing, so no premature ENDLIB header truncates the stream for readers (silent data loss) (X-2 tier 1, 2026-11-07).

### Q-MASK-214: Forward-reference renaming in full dumps
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: An SNAME naming a structure not yet seen is renamed prefix+name, or prefix+name+"[[0]]" if a database cell with the prefixed name exists. Unresolved references are written at the end of the write as magic cells under their unprefixed names, which do not match the prefixed SNAME.
- Evidence: calma/CalmaWrite.c:599-625, calma/CalmaWrite.c:345-366, calma/CalmaWrite.c:788-801
- Verified 2026-11-13: The behaviour is confirmed, with lines shifted slightly: SNAME forward-reference renaming is at CalmaWrite.c:602-624 (`0`+prefix+name, or +`[[0]]` when DBCellLookDef finds the prefixed name). Unresolved references move to calmaUndefHash at :783-799. At the end of the write, :346-371 looks up the unprefixed name and writes it with calmaProcessDef, whose STRNAME is the Magic cell name (calmaOutStructName, or the vendor path at :1290-1303). It therefore does not match the prefixed SNAME, and the output has a dangling reference. A later dump of a library that defines the name reuses the recorded prefix (:445-472).
- Options: replicate | fix (resolve two-pass: scan the library for definitions before copying) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, with a warning in both modes. magic-compat ON: Hefesto reproduces Magic's renaming exactly, including the end-of-write structure written under its unprefixed name. Hefesto warns that the SREF to `prefix+name` is left dangling. magic-compat OFF: a structure appended at the end of the write for an unresolved forward reference is written under the name recorded for that reference (`prefix+name`, or the `[[0]]` form), so every SREF resolves. The same addendum-library golden cases that would decide between the options test both modes. If dangling references are judged data loss, the alternative is fix in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-215: All structures of a full dump get the triggering cell's creation date
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Each dumped structure's BGNSTR creation date is the session stamp or the timestamp of the vendor cell that caused the dump, not the date in the source file.
- Evidence: calma/CalmaWrite.c:446-451 (corrected 2026-11-13: Behaviour is correct, but the lines have moved. calmaDumpStructure writes BGNSTR with *CalmaDateStamp or def->cd_timestamp at calma/CalmaWrite.c:438-443, where def is the vendor cell passed in by calmaFullDump at calma/CalmaWrite.c:776. Lines 446-451 are the calmaUndefHash lookup.)
- Verified 2026-11-13 (review): The source structure's BGNSTR record, with both its dates, is read and discarded by calmaSkipExact(CALMA_BGNSTR) (calma/CalmaWrite.c:432; calma/CalmaRdio.c:543-569) before the new BGNSTR is written at :438-443.
- Options: replicate | fix (copy the original BGNSTR dates) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: each dumped structure's creation date is the session `datestamp` if one is set, otherwise the triggering vendor cell's timestamp, as Magic writes it (CalmaWrite.c:438-443). OFF: the creation date is copied from the source file's BGNSTR, unless `datestamp` is set, in which case the datestamp is used as for every other structure. A full dump is meant to copy the vendor structure, and Magic discards the date only because calmaSkipExact skips the whole record (:432), so the corrected behaviour is clear. The modification date is the write time in both modes. Flows that set `datestamp` produce the same bytes in both modes. (corrected 2026-11-13, was replicate)

### Q-MASK-216: Copied SNAME records are not updated for renamed cells
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The bytes of a new-style vendor copy (GDS_START..GDS_END) are copied verbatim. If subcells were renamed on read (e.g. `gds unique`) or afterwards, the SNAME inside the copy still names the original structure, while the subcell structure is written under its magic name.
- Evidence: calma/CalmaWrite.c:1085-1335
- Verified 2026-11-13: Confirmed. The new-style copy (calma/CalmaWrite.c:1086-1330) writes its own BGNSTR and STRNAME with the magic name (1292-1303), then fwrites the GDS_START..GDS_END buffer verbatim (1305), so SNAMEs inside are untouched. Note that `gds unique` (CalmaUnique, calma/CalmaRead.c:109-112) renames the cell already in memory, not the newly read one. The dangling or wrong reference therefore appears when that renamed cell is itself referenced by another vendor copy, or after `cellname rename`.
- Options: replicate | fix (parse the copied records and rewrite SNAME) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, copy the bytes verbatim as Magic does and warn when a referenced child's current name differs from the SNAME in the copy. With it OFF, parse the copied records and rewrite each SNAME to the child's current output name, adjusting record lengths. This requires Hefesto to keep each vendor cell's original GDS name. A stale SNAME silently binds to the wrong structure or to none, so the corrected output is clearly the intended one. (corrected 2026-11-13, was replicate)

### Q-MASK-217: Merged polygons may contain slits and may be split arbitrarily
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Each tile is spliced into at most one existing polygon at one shared edge. Other shared edges remain as zero-width slits, and connected parts already in different bounds are never joined. The 201-point check splits components. Collinear removal is a single pass and can leave a collinear point at the ring start.
- Evidence: calma/CalmaWrite.c:2187-2257, calma/CalmaWrite.c:2089-2172
- Verified 2026-11-13: Confirmed. calmaMergeSegments (calma/CalmaWrite.c:2187-2257) returns after the first antiparallel match, and skips a bound when bt_points + num_points > 201 (2214). calmaRemoveColinear (2089-2160) is a single pass.
- Options: replicate | fix (proper polygon union with hole cut-lines)
- Recommendation: Replicate in both modes. The output is valid GDS covering the same area: slits and abutting splits are a representation choice, not wrong geometry. Byte identity with Magic depends on porting this exact merge and collinear pass. A proper polygon-union writer for OFF mode would be a separate feature with no Magic output to check it against, and is not needed for M5.

### Q-MASK-218: Tile structure of non-Manhattan mask planes
- Kind: ambiguity
- Magic behavior: For Manhattan geometry, maximal-horizontal-strip tiling is canonical. With split tiles, the decomposition depends on the order of paint operations in mask generation, and this affects element order and triangle splitting in the output.
- Evidence: database/DBtiles.c:521-618; cif/CIFgen.c (paint order)
- Options: replicate (reimplement the corner-stitch paint algorithm exactly) | fix (canonical decomposition)
- Recommendation: replicate the corner-stitched plane implementation. This is shared with capability `mask-output-operators`.
- Resolution: compat — tile order bullet: Hefesto reproduces Magic's corner-stitched paint algorithm, including split-tile decomposition, so element order and triangle splitting match (X-1 as amended by D-1, 2026-11-07).

### Q-MASK-219: Write with no output style leaves an empty file
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The output file is created before the output-style check. When no style is set, an empty file remains.
- Evidence: commands/CmdCD.c:1066-1076; calma/CalmaWrite.c:300-304 (corrected 2026-11-13: Behaviour is correct, but the lines have moved. The file is opened with PaZOpen at commands/CmdCD.c:1068-1074 (mode 'wT', or 'w<z>', chosen at 1049-1066), and the style check is in CalmaWrite at calma/CalmaWrite.c:299-303. With compression on, zlib's gzclose probably leaves an empty gzip member (about 20 bytes) rather than a 0-byte file; this was not run. Either way, an existing file of that name is truncated.)
- Options: replicate | fix (check before creating the file)
- Recommendation: fix (tier 1, data loss) in both modes. Check that an output style is set before opening or creating the output file, so an existing file of the same name is not truncated. Report the same 'No CIF/GDS output style set!' error and return failure. Opening the file first truncates an existing GDS file and leaves no useful output, so this is data loss, not an output-value quirk.

### Q-MASK-220: Option query messages are inverted or wrong
- Kind: doc-mismatch
- Magic behavior: `gds abstract` (query) prints the `undefined` flag. `gds lower` prints "lowercase only" when mixed case is allowed. `gds unique` prints "will not be renamed" when renaming is on. The help text says compress is 0-6, but 0-9 is accepted.
- Evidence: commands/CmdCD.c:337-341, 620-623, 773-776, 168
- Options: replicate | fix
- Recommendation: fix (messages are informative).

### Q-MASK-221: Text magnification scale is read-only
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `gds magscale` changes how MAG is interpreted on read, but on write MAG is always size/800 × S/E (1.0 = 1 micron), so a read/write round trip with magscale ≠ 1 changes text size.
- Evidence: calma/CalmaWrite.c:2913-2916; commands/CmdCD.c:454-473 (corrected 2026-11-13: Behaviour is correct, but the lines have moved. MAG is written as lab_size/800 * scaleFactor/expander at calma/CalmaWrite.c:2906-2909. `gds magscale` sets CalmaMagScale at commands/CmdCD.c:432-450, and only the reader uses it (calma/CalmaRdpt.c:912-914).)
- Options: replicate | fix (divide by magscale on write) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, write MAG as Magic does and ignore magscale. With it OFF, divide the written MAG by magscale, so that reading and writing with the same magscale keeps text size. With the default magscale of 1.0 both modes give identical output, so library round-trips are unaffected. (corrected 2026-11-13, was replicate)

### Q-MASK-222: 32-bit overflow in coordinate scaling
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Coordinates are multiplied by Wscale/Pscale in C `int` arithmetic, with no overflow check (undefined behavior, which wraps in practice).
- Evidence: calma/CalmaWrite.c:1826-1850, 2337-2340, 2741-2744 (corrected 2026-11-13: Behaviour is correct, but the cited lines are wrong (2337-2340 is a debug print). The int multiplications by calmaWriteScale or calmaPaintScale are at calma/CalmaWrite.c:1757-1758 and 1800-1801 (SREF/AREF translation), 1814-1824 (AREF row and column points), 2307-2314 (boundary XY), 2736-2739 (rectangles), 2918-2919 (label XY, where (xbot+xtop)*Wscale overflows at half the range), 2953-2956 (label rectangle) and 3030-3031.)
- Options: replicate wrap | fix (report an error when a value exceeds the I4 range)
- Recommendation: fix (tier 1, undefined behaviour) in both modes. Compute every scaled coordinate, including the label-midpoint sum, in 64-bit. Fail the write with an error that names the cell and the value when a result is outside the I4 range, instead of writing a wrapped value. Valid designs never get near the limit, so byte identity on real libraries is unaffected.

### Q-MASK-223: gzip byte stream depends on the zlib build
- Kind: platform
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Compressed output is produced by zlib gzopen/gzwrite with a final Z_SYNC_FLUSH. The header OS byte, the deflate blocks, and the empty stored block depend on the zlib version and platform.
- Evidence: commands/CmdCD.c:1049-1060; calma/CalmaWrite.c:369-372 (corrected 2026-11-13: Behaviour is correct, but the lines have moved. The writeMode ('w<z>' or transparent 'wT') and PaZOpen are at commands/CmdCD.c:1049-1074. ENDLIB is written at calma/CalmaWrite.c:374, followed by gzflush(f, Z_SYNC_FLUSH) at 376.)
- Options: replicate (bundle the same zlib) | platform alternative (any conforming gzip)
- Recommendation: platform. Any conforming gzip writer, producing a single member at level z, is acceptable in both modes, and conformance compares decompressed bytes only. Bundling Magic's exact zlib build to match the compressed bytes is not worth it: the bytes already differ between Magic builds.

### Q-MASK-224: `datestamp yes` documented as current time but uses the cell timestamp
- Kind: doc-mismatch
- Magic behavior: The option help says "use current time or value". The code clears the session stamp, so each structure's creation date is the cell's own timestamp.
- Evidence: commands/CmdCD.c:125, 932-944; calma/CalmaWrite.c:1440-1443
- Options: replicate | fix (doc)
- Recommendation: replicate the code. Document it correctly in Hefesto.
- Resolution: n/a — spec follows Magic's code; documentation is wrong; `datestamp yes` uses each cell's timestamp (spec conventions §2.7, 2026-11-07).

### Q-MASK-225: Incompatible expander writes mis-scaled output
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: If 10 (or 100 with angstrom units) is not divisible by the expander, an error is printed, but the file is written with Wscale = S and Pscale = 1, which is wrong by the expander ratio.
- Evidence: calma/CalmaWrite.c:1450-1478
- Verified 2026-11-13: Confirmed at calma/CalmaWrite.c:1454-1479. Two additions. The `good` flag is not cleared, so the write reports success apart from the TxError text. The case is reachable through scalegrid, which multiplies cs_expander (cif/CIFtech.c:2490).
- Options: replicate | fix (refuse to write) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, print Magic's errors and write with Wscale = S and Pscale = 1. With it OFF, refuse the write before the output file is opened, with an error suggesting `units angstroms` (or a coarser scalegrid), and return failure. A mis-scaled mask file that reports success is dangerous in a tapeout flow, so the OFF behaviour is the intended one. (corrected 2026-11-13, was fix)

### Q-MASK-226: Abstract-view failure leaves a partial but terminated file
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: When an abstract cell is disallowed, the already-written structures stay, the failing cell's ancestors are skipped, ENDLIB is still written, and only an error status is returned.
- Evidence: calma/CalmaWrite.c:1037-1049, 1077-1080, 357-388 (corrected 2026-11-13: Mostly correct, but the lines have moved. The abstract check is at calma/CalmaWrite.c:1036-1047, the child failure propagates at 1082-1083, and ENDLIB is still written at 374 (the root call is at 344). Correction: calmaProcessUse returns calmaProcessDef's result (calma/CalmaWrite.c:920-925), so DBCellEnum stops in every ancestor. Every cell not yet written is skipped, not only the ancestors; the exception is the calmaUndefHash cells, which the trailing loop (348-370) still writes.)
- Options: replicate | fix (validate the whole tree before writing, or delete the file) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, reproduce Magic: the structures already written stay, all remaining cells are skipped (except the undefined-reference cells handled by the trailing loop), ENDLIB is appended and the write fails. With it OFF, check the whole tree for disallowed abstract views before opening the output file, and write nothing if one is found. The ON behaviour is a well-formed but incomplete library, not loss of design data, so tier 2 fits the standing rule. (corrected 2026-11-13, was fix)

### Q-MASK-227: bridge expansion takes square roots of negative values
- Kind: bug
- Magic behavior: the bridge expansion computes `sqrtf(w² − x²)` without checking the sign. When the gap along the chosen axis exceeds w (possible when d > w), the argument is negative, the result is NaN, and `ceilf(NaN)` is converted to int. That conversion is undefined in C: x86-64 yields INT_MIN, AArch64 yields 0. The bridge rectangle therefore differs between platforms, and on x86-64 can overflow to a huge rectangle when NaN is the governing term.
- Evidence: cif/CIFgen.c:1735-1765
- Options: replicate the x86-64 result | fix (clamp the argument to 0)
- Recommendation: fix by clamping to 0. Capture x86-64 golden output first to see whether any real PDK hits it (verify against Magic).
- Resolution: fix — the square-root argument is clamped to 0, so no NaN is converted to int (undefined behaviour) (X-2 tier 1, 2026-11-07).

### Q-MASK-228: bridge-lim passes 1 and 2 modify the plane they are enumerating
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: after pass 0, the plane being searched for corners is also the plane that bridges are painted into. Tiles created or merged by a bridge can be visited, revisited or skipped depending on the enumeration state, so the result depends on the tile-plane traversal algorithm and not only on the geometry.
- Evidence: cif/CIFgen.c:5243-5267
- Verified 2026-11-13: Confirmed at cif/CIFgen.c:5243-5266. After pass 0, curPlane = nextPlane = cifPlane, and cifBridgeLimFunc1/2 enumerate curPlane while painting into cifPlane (the same plane).
- Options: replicate (port the enumeration literally) | fix (enumerate a snapshot and paint into a copy) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, port the enumeration literally: passes 1 and 2 search the plane they paint into. This is deterministic, provided Hefesto's corner-stitched plane (src/core/src/db/tile_plane.cpp) reproduces Magic's strip splitting and merging and DBSrPaintArea's behaviour when tiles change during a search, which byte identity needs anyway. With it OFF, enumerate a snapshot of the pass-0 result and paint into a copy, so the result depends only on geometry. Golden captures should include layouts where two bridges interact. (corrected 2026-11-13, was fix)

### Q-MASK-229: bridge-lim erases whole limiting tiles, not only their part inside the bridge
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: when no limit-free bridge shape exists, every limiting tile that overlaps the bridge is erased from the result over its full extent (unscaled), which can also remove original geometry far from the bridge.
- Evidence: cif/CIFgen.c:4379-4405
- Verified 2026-11-13: Confirmed. bridgeErase (cif/CIFgen.c:4379-4407) runs cifPaintFunc with CIFEraseTable on every limiting tile that overlaps the area. cifPaintFunc (cif/CIFgen.c:104-121) erases the whole tile rectangle scaled by cifScale, which is 1 here, so database tiles are erased unscaled. The early 'return 0' never happens, because cifPaintFunc always returns 0.
- Options: replicate | fix (erase only the intersection with the bridge, in mask units) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, erase every overlapping limiting tile over its full extent, unscaled, as Magic does. With it OFF, erase only the intersection of each limiting tile with the bridge, converted to mask units, consistently with the OFF answer to Q-MASK-5. Removing original geometry far from the bridge is clearly not the intended behaviour. (corrected 2026-11-13, was replicate)

### Q-MASK-230: bloat-all leaves contact-image trigger tiles marked
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: with d = 0, a trigger tile is marked as processed. At the end of the operation, marks are cleared only on the T2 plane. A trigger found on another plane (the image of a contact on a second plane) stays marked, so a later `bloat-all` that uses the same tile as a trigger does not push it and produces nothing for it.
- Evidence: cif/CIFgen.c:1427-1432, cif/CIFgen.c:1473-1477, cif/CIFgen.c:5313-5328
- Verified 2026-11-13: Confirmed. With pmask != 0 the trigger is pushed (cif/CIFgen.c:1475-1478) and set to CIF_PROCESSED (1507). With d = 0 it is never put on ResetStack (1517-1532). The final reset (5313-5328) covers only the temp layers or cd_planes[bl_plane]. Additional detail: CIF_UNPROCESSED == CLIENTDEFAULT (1228), so the stale value 1 stays in ti_client of that database plane until the tile is rebuilt. Magic normally generates on flattened scratch defs, so in practice the effect is mainly within one generation pass.
- Options: replicate | fix (clear marks on every plane searched for triggers) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, keep the stale marks for the rest of the layer generation of that cell, so a later bloat-all that uses the same trigger produces nothing for it, as in Magic. Do not let marks leak past the generation pass or into the editable database. With it OFF, clear the marks on every plane searched for triggers at the end of the operation. Golden captures with two bloat-all ops sharing a contact trigger should confirm the ON behaviour. (corrected 2026-11-13, was fix)

### Q-MASK-231: bloat-all seeding depends on trigger order when d = 0
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: for triggers that are not on the T2 plane, only the first unprocessed T2 tile overlapping the trigger seeds the flood. With d = 0, tiles stay processed across triggers, so a later trigger whose first overlapping region is already done seeds a different, unprocessed region instead. The union of painted regions therefore depends on the trigger enumeration order.
- Evidence: cif/CIFgen.c:1332-1344, cif/CIFgen.c:1458-1478
- Verified 2026-11-13: Confirmed. cifFoundFunc (cif/CIFgen.c:1334-1346) pushes the first CIF_UNPROCESSED tile and stops. It is called for pmask == 0 at 1462-1473. One addition: even a single trigger that overlaps two separate T2 regions floods only one of them.
- Options: replicate | fix (seed every region overlapping the trigger) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat (rounding and ordering quirk, tier 2). With magic-compat ON, seed only the first unprocessed overlapping tile, in Magic's enumeration order. With it OFF, seed every connect-type tile overlapping the trigger, so the result is the union of all regions touching any trigger, independent of order. (corrected 2026-11-13, was replicate)

### Q-MASK-232: bloat-all converts mask-layer triggers to internal units with truncation toward zero
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: a mask-layer trigger used with database T2 types is divided by the scale factor with C integer division, which truncates toward zero. Both the seed area and the clip base (multiplied back by the scale) shift toward the origin for negative or off-grid coordinates.
- Evidence: cif/CIFgen.c:1420-1428, cif/CIFgen.c:1480-1490 (corrected 2026-11-13: Behaviour is correct, but the lines have moved slightly. The truncating division is at cif/CIFgen.c:1418-1424 (area /= locScale, with clipArea = area at 1426). The clip is multiplied back by locScale at 1481-1486.)
- Options: replicate | fix (floor/ceil outward) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat (a rounding asymmetry, the X-2 tier-2 example). With magic-compat ON, use C truncation toward zero for both the seed area and the clip base. With it OFF, round outward (floor the lower bounds, ceil the upper bounds) so off-grid and negative triggers are not shifted toward the origin. Both modes give identical output for on-grid mask geometry. (corrected 2026-11-13, was replicate)

