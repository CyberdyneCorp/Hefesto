# MASK open questions

Area MASK: mask output styles and operators, hierarchical mask generation, mask input styles, GDS read/write, CIF format I/O. ID ranges: 1–99 output styles/operators, 110–199 gds-read, 200–299 gds-write, 300–399 mask-input-styles and cif-format-io, 400–499 hierarchical generation. Q-MASK-334 duplicates Q-MASK-1 (grow-grid rounding); resolve together.

### Q-MASK-1: grow-grid rounds by |c| mod g on both sides
- Kind: bug
- Magic behavior: lower/left edges move down by `|c| mod g`, upper/right edges move up by `|c| mod g`; correct only for non-negative lower edges and non-positive upper edges (e.g. upper edge 12 with g=5 becomes 14, not 15; lower edge -7 becomes -9, not -10).
- Evidence: cif/CIFgen.c:533-541
- Options: replicate | fix (snap lower edges to floor, upper edges to ceil multiples of g)
- Recommendation: replicate by default for GDS identity; offer a corrected mode behind a compatibility flag.

### Q-MASK-2: close threshold is an area but normalised as a length
- Kind: bug
- Magic behavior: `close a` compares a summed area (mu²) against `a`, but unit normalisation multiplies/divides `a` linearly by the scale factors, so the effective area threshold depends on the reduction factor.
- Evidence: cif/CIFgen.c:2205-2215, cif/CIFtech.c:2562-2567, cif/CIFtech.c:2655-2656
- Options: replicate | fix (scale quadratically)
- Recommendation: replicate; document that `close` values are in final mask units².

### Q-MASK-3: bridge/bridge-lim test the wrong bottom neighbour for the SE corner
- Kind: bug
- Magic behavior: the loop meant to find the bottom neighbour at the tile's right end tests the right neighbour's edge instead and never advances, so the bottom-LEFT neighbour is used.
- Evidence: cif/CIFgen.c:1894-1895, cif/CIFgen.c:4509-4510
- Options: replicate | fix
- Recommendation: replicate (affects only which SE corners are bridged).

### Q-MASK-4: bridge width/grid expansion is a no-op (withdrawn)
- Kind: doc-mismatch
- Magic behavior: withdrawn. The helper that computes the minimum-width, grid-snapped bridge rectangle stores its result back through the rectangle pointer (`*area = r`), so the expansion IS applied. The `bridge` requirement now specifies the expansion exactly.
- Evidence: cif/CIFgen.c:1703-1803 (line 1802)
- Options: n/a
- Recommendation: none; kept for traceability.
- Resolution: n/a — withdrawn; the spec follows Magic's code and applies the expansion (spec conventions §2.7, 2026-11-07).

### Q-MASK-5: bridge-lim searches database planes with mask-unit coordinates
- Kind: bug
- Magic behavior: limiting database types are searched on internal-unit planes using mask-unit rectangles (no division by scaleFactor); erasure of limiting geometry likewise uses whatever scale is current.
- Evidence: cif/CIFgen.c:4170-4228, cif/CIFgen.c:4379-4426
- Options: replicate | fix (convert units)
- Recommendation: replicate when scaleFactor = 1 is the common case; flag mismatch in a lint.

### Q-MASK-6: tagged uses internal-unit label coordinates against mask planes
- Kind: bug
- Magic behavior: the label rectangle (internal units) is divided by scaleFactor for database planes and used unscaled on mask-unit planes; tags only work correctly when scaleFactor = 1.
- Evidence: cif/CIFgen.c:5548-5570, cif/CIFgen.c:4078-4110
- Options: replicate | fix
- Recommendation: decide after golden capture with scaleFactor ≠ 1.

### Q-MASK-7: not-square drops all non-Manhattan geometry
- Kind: quirk
- Magic behavior: split tiles are skipped (not copied), so triangles disappear; the four neighbour checks all use the bottom kind of the neighbour.
- Evidence: cif/CIFgen.c:4740-4790
- Options: replicate | fix (copy triangles)
- Recommendation: replicate.

### Q-MASK-8: gridlimit is never rescaled and has two interpretations
- Kind: ambiguity
- Magic behavior: operators apply `gridLimit` directly to final mask-unit coordinates (after reduction), while the grid-scale guard multiplies it by the expander (treating it as nanometres-like units). Neither rescales it when lambda changes.
- Evidence: cif/CIFtech.c:466-490, cif/CIFgen.c:163-237, cif/CIFtech.c:775-790
- Options: replicate | fix (store in nm and convert)
- Recommendation: replicate; techs normally use gridlimit with styles reducing to 1 nm or 10 nm units.

### Q-MASK-9: slots with exactly 6 tokens reads past the argument list
- Kind: bug
- Magic behavior: the 6-token form reads `argv[6]` before rejecting the line.
- Evidence: cif/CIFtech.c:1567-1585, cif/CIFtech.c:1611-1612
- Options: replicate (reject) | fix
- Recommendation: reject the line with a wrong-argument error without undefined behaviour.
- Resolution: fix — the 6-token `slots` line is rejected with a wrong-argument error without reading past the argument list (X-2 tier 1, 2026-11-07).

### Q-MASK-10: style name prefix shadowing
- Kind: bug
- Magic behavior: a new `style` name is treated as existing if an earlier name begins with it (`style gds` after `style gdsfoo` is not registered).
- Evidence: cif/CIFtech.c:594-602
- Options: replicate | fix (exact comparison)
- Recommendation: fix with a warning; it never matters for well-formed techs.

### Q-MASK-11: `labels ... text` has no effect
- Kind: quirk
- Magic behavior: the `text` option is parsed into a flag that is never used, so it behaves like no option.
- Evidence: cif/CIFtech.c:905-955
- Options: replicate | implement intended "text only" semantics
- Recommendation: replicate.

### Q-MASK-12: interacting-family flags treated as bloat data in reducer computation
- Kind: bug
- Magic behavior: for `nonoverlapping`, `interacting`, `noninteracting` (non-zero flag stored in the client field) the CIF reducer loop falls into the bloat branch and dereferences the small integer as a pointer (undefined behaviour; likely crash) unless the reducer already reached 1.
- Evidence: cif/CIFtech.c:1996-2037, cif/CIFtech.c:1131-1146
- Options: replicate (impossible) | fix (skip these operators)
- Recommendation: fix: skip; verify against a tech using `interacting` to confirm observed reducer.
- Resolution: fix — the reducer computation skips `nonoverlapping`, `interacting` and `noninteracting` instead of dereferencing their flag as a pointer (X-2 tier 1, 2026-11-07).

### Q-MASK-13: bloat-all prevents unit reduction
- Kind: quirk
- Magic behavior: bloat-all stores the value 1 per selected type; it enters the GCD in unit normalisation so the expander/scale are never reduced when d = 1, and these values are multiplied but never divided.
- Evidence: cif/CIFtech.c:2620-2635, cif/CIFtech.c:2690-2700
- Options: replicate | fix (exclude bloat-all)
- Recommendation: replicate: the reduction granularity changes integer rounding in squares/grow-min centring.

### Q-MASK-14: maxrect tie-breaking, overflow and split tiles
- Kind: ambiguity
- Magic behavior: the largest rectangle is chosen by strict `>` in candidate-list order, which depends on area-search enumeration order; areas are computed in 32-bit int (overflow for large regions); split tiles of the region count as fully inside.
- Evidence: utils/maxrect.c:80-160, utils/maxrect.c:324-333
- Options: replicate | define deterministic tie-break
- Recommendation: replicate enumeration order (top-to-bottom, left-to-right area search) and use 64-bit areas; capture golden ties.
- Resolution: compat — candidates are taken in Magic's tile enumeration order (tile order bullet) and ties keep the first by strict `>`; areas are computed in 64 bits, since the 32-bit overflow is undefined behaviour (X-1 as amended by D-1; X-2 tier 1, 2026-11-07).

### Q-MASK-15: grow-min geometric formula is incomplete
- Kind: quirk
- Magic behavior: the code comment states the algorithm does not cover many cases; `changed` is only set when both width and height are short; the vertical extension uses `(int)sqrt(g² - (g+W)²/4 + 0.5)` (0.5 added inside the root, truncating conversion), which is not a symmetric rounding; the neighbour-column pass can repaint the same rectangle several times.
- Evidence: cif/CIFgen.c:300-485
- Options: replicate | fix
- Recommendation: replicate exactly using IEEE double sqrt and truncation.

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
- Magic behavior: unit normalisation returns early when the common factor is ≤ 1, skipping the radius recomputation even though scaleFactor and distances changed (e.g. after lambda rescaling).
- Evidence: cif/CIFtech.c:2640-2650, cif/CIFtech.c:2716-2718
- Options: replicate | fix (always recompute)
- Recommendation: replicate; radius only affects hierarchical correctness margins but also output in rare cases — verify.

### Q-MASK-19: GDS layer range documented as 0..63
- Kind: doc-mismatch
- Magic behavior: code accepts 0..255; the tech manual says 0..63.
- Evidence: calma/calmaInt.h:117, doc/latexfiles/maint2.tex (Calma layers subsection)
- Options: follow code
- Recommendation: follow code (0..255).
- Resolution: n/a — spec follows Magic's code; documentation is wrong; GDS layer and datatype numbers are 0..255 (spec conventions §2.7, 2026-11-07).

### Q-MASK-20: bloat-or on split tiles ignores corner extensions
- Kind: quirk
- Magic behavior: diagonal tiles are bloated only along the hypotenuse direction by the distance of the type across the diagonal; leg-side corner extensions are not handled (comment says so).
- Evidence: cif/CIFgen.c:951-1040
- Options: replicate | fix
- Recommendation: replicate.

### Q-MASK-21: interacting tests against database types use truncating division
- Kind: quirk
- Magic behavior: mask-unit tile rectangles (grown by 1 for touching forms) are divided by scaleFactor with truncation toward zero before searching database planes, so touching tests can be lost or asymmetric when coordinates are not multiples of scaleFactor.
- Evidence: cif/CIFgen.c:4078-4083, cif/CIFgen.c:4868-4882
- Options: replicate | fix (floor/ceil outward)
- Recommendation: replicate.

### Q-MASK-22: net with no layer list adds nothing
- Kind: ambiguity
- Magic behavior: `net name` without layers ORs no geometry because the type mask is empty.
- Evidence: cif/CIFtech.c:1365-1372, cif/CIFgen.c:5446-5452
- Options: replicate | default to all types
- Recommendation: replicate; warn in lint.

### Q-MASK-23: bbox operator uses the database cell bounding box
- Kind: ambiguity
- Magic behavior: the cell definition bounding box (which in Magic includes subcells and may include label extents) is used rather than mask geometry extents.
- Evidence: cif/CIFgen.c:5665-5675
- Options: replicate
- Recommendation: replicate; ensure layout-database defines the same bbox.

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
- Magic behavior: in hierarchical mode the layer stops at net/tagged/boundary/bbox and at squares/slots not followed by grow, returning the pre-operator geometry, which is then used in interaction processing.
- Evidence: cif/CIFgen.c:5332-5380, cif/CIFgen.c:5714-5718
- Options: replicate
- Recommendation: replicate (see mask-hierarchical-generation).

### Q-MASK-27: squares grid-limit fix diverges for negative coordinates
- Kind: bug
- Magic behavior: for `squares`, `δ = |cx| mod gridLimit` always shrinks the upper bound by 2δ; for negative cx this moves the cut further off-grid and repeats until no columns remain (slots handles the sign correctly).
- Evidence: cif/CIFgen.c:3823-3861, cif/CIFgen.c:3686-3697
- Options: replicate | fix (sign-aware like slots)
- Recommendation: replicate by default; contacts left of the origin may lose cuts — capture golden case.

### Q-MASK-28: squares placed per region bounding box, not per tile
- Kind: doc-mismatch
- Magic behavior: modern code centres cut arrays on each connected region's bounding box (after strip extraction); the tech manual describes per-tile placement.
- Evidence: cif/CIFgen.c:2738-2990, doc/latexfiles/maint2.tex (squares paragraph)
- Options: follow code
- Recommendation: follow code.
- Resolution: n/a — spec follows Magic's code; documentation is wrong; cuts are placed per region bounding box (spec conventions §2.7, 2026-11-07).

### Q-MASK-400: Phantom interaction area around the origin
- Kind: bug
- Magic behavior: When interaction areas are found, each subcell that is not fully surrounded by nearby parent paint is checked for siblings. The accumulator for that check starts as the rectangle (0,0)-(0,0). If no sibling is found, this empty accumulator is still expanded by the radius r, giving (-r,-r)-(r,r). It is then clipped to the subcell's halo and added to the interaction bounding box. So whenever a subcell's halo covers the origin, a spurious region of up to 2r×2r around (0,0) is added. That region can enlarge the interaction rectangle, which is then recomputed flat.
- Evidence: drc/DRCsubcell.c:460-476, utils/geometry.c:77
- Options: replicate (the interaction rectangle boundary can change the output of non-distributive operators) | fix (skip the include when no sibling was found)
- Recommendation: replicate for exact GDS compatibility, and add a golden test (MASK-HG-005). This code is shared with hierarchical DRC, so decide it jointly with the DRC area.

### Q-MASK-401: Vendor-cell exclusion during flattening never applies
- Kind: bug
- Magic behavior: The flattening copy is meant to skip the paint of vendor (read-only GDS) cells unless the style option `see-no-vendor` changes that. The test is written as `if (!CIFCurStyle) if (!(CIFCurStyle->cs_flags & CWF_SEE_NO_VENDOR)) return 0;`. When a style is loaded, the first condition is false, so vendor paint is always copied. When no style is loaded, the second condition dereferences NULL. The style option `see-no-vendor` therefore has no effect on generation. Its intended meaning is also unclear: the flag comment says "Hide magic's GDS from vendor cells", while the code comment says "unless overridden".
- Evidence: cif/CIFhier.c:396-405, cif/CIFint.h:338, cif/CIFtech.c:1055-1056
- Options: replicate (always include vendor paint) | fix (honour the flag with a clarified meaning)
- Recommendation: replicate. Always including vendor paint is the behavior that existing GDS output reflects.

### Q-MASK-402: Hierarchy-stopping operators leave a partial layer result
- Kind: quirk
- Magic behavior: During hierarchical evaluation, processing of a layer stops at `squares`, `squares-grid` or `slots` not followed by `grow`, and at `net`, `tagged`, `boundary` or `bbox`. The partial result computed before that operator then takes part in the residual and is ORed into the parent. A layer such as `bloat-or ...; squares` can therefore output an uncut bloated region in the parent at a cell seam.
- Evidence: cif/CIFgen.c:5332-5376, cif/CIFgen.c:5433-5462, cif/CIFgen.c:5601-5636, cif/CIFgen.c:5700-5705
- Options: replicate | fix (drop the layer from the residual when it stopped early)
- Recommendation: replicate, and flag the case with a warning in Hefesto's style loader.

### Q-MASK-403: Layers that depend on bbox/boundary layers stay hierarchical
- Kind: quirk
- Magic behavior: Layers that contain `bbox` or `boundary` are removed from the hierarchical set after dependencies have been propagated. A hierarchical layer that reads such a templayer therefore sees it as empty during interaction processing, in both the total and the components. The residual is computed against the wrong operand.
- Evidence: cif/CIFtech.c:2222-2238
- Options: replicate | fix (also remove the dependents, or generate the dependency non-hierarchically)
- Recommendation: replicate. Document it in the style-writing guide.

### Q-MASK-404: Halo radius ignores several operators
- Kind: ambiguity
- Magic behavior: The radius counts only grow, grow-min, grow-grid, shrink and bloat-or distances, and stops at bbox/net/tagged/mask-hints. The reach of bloat-all, bloat-max, bloat-min, bridge, bridge-lim, close, slots, squares and interact is not counted. For styles that use those operators, interaction areas and yank windows may be too small, so hierarchical output differs from flat output near cell boundaries.
- Evidence: cif/CIFtech.c:1668-1760
- Options: replicate | fix (count all reach-bearing operators)
- Recommendation: replicate, because the radius also sets the generation clip area and therefore changes the output.

### Q-MASK-405: Non-distributive operators do not force hierarchical processing
- Kind: quirk
- Magic behavior: Only and, and-not, interact, shrink, close, bridge, bridge-lim and the bloat-family operators force a layer into the hierarchical set. `grow-min`, `not-square`, `orthogonal`/`manhattan`, `squares`, `slots`, `net` and `tagged` do not, although their result on a union of geometries generally differs from the union of their results.
- Evidence: cif/CIFtech.c:2085-2121
- Options: replicate | fix
- Recommendation: replicate.

### Q-MASK-406: Array element filter with reversed index ranges
- Kind: bug
- Magic behavior: The low-corner filter is `x - xlo < 2 && y - ylo < 2` and the high-corner filter is `xhi - x < 2 && yhi - y < 2`. When an array is declared with xlo > xhi (descending indices), the difference is negative for every element, so every element that overlaps the region's halo is accepted. For small radii this matches the intended 2×2 corner. With a large radius, more elements are included in the total and in the components.
- Evidence: cif/CIFhier.c:1137-1180
- Options: replicate | fix (use absolute index distance from the geometric corner)
- Recommendation: replicate. The effect on the residual is usually nil, but test with MASK-HG-013.

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
- Magic behavior:
  - `min-width` is stored in output units and is never rescaled when the style scale changes.
  - Growth uses (m-h) div 2 on each side, so an odd difference grows one unit less than m.
  - The early return in the first branch skips the second branch.
  - The function prints `got sliver ...` and `created sliver ...` to stdout.
  - Sliver growth applies only to hierarchical residual pieces, never to own-paint output.
- Evidence: cif/CIFhier.c:1268-1305, cif/CIFtech.c:979-989
- Options: replicate | fix
- Recommendation: replicate the geometry and drop the stdout prints.

### Q-MASK-410: CIFhier property ignored by CIF writer
- Kind: doc-mismatch
- Magic behavior: The documentation says the `CIFhier` property overrides `*hier write disable` and `*array write disable` "for that cell and its descendents". Only the GDS writer honours it. Within GDS it affects only descendants first written while the cell's subcells are processed.
- Evidence: doc/html/cif.html:200-220, calma/CalmaWrite.c:1026, cif/CIFwrite.c:385-396
- Options: replicate | fix (honour it in CIF too)
- Recommendation: fix in CIF, because it only matters when a user disabled hierarchy. Keep GDS behavior.

### Q-MASK-411: Flat CIF output loses labels and mask hints, and names the cell `__CIF2__`
- Kind: quirk
- Magic behavior: `cif flat` flattens paint only. Labels and `MASKHINTS_*` properties are not copied, so mask-hints layers are empty. The single symbol is named after the internal scratch cell, `__CIF2__`.
- Evidence: cif/CIFwrite.c:729-790
- Options: replicate | fix (copy labels and hints, and name the symbol after the root cell)
- Recommendation: replicate the geometry. Decide the naming together with the cif-format-io area.

### Q-MASK-412: `net` operator in own-paint generation uses a stale dummy cell
- Kind: bug
- Magic behavior: The `net` operator looks up the named net through an internal dummy use. That use's cell is only set by hierarchical, array or flat-view processing. During own-paint generation of cell D it still points at whatever cell was processed before (or at the internal scratch cell). If no such processing has happened yet in the session, the pointer may be NULL. The net may therefore be searched in the wrong cell, or not found.
- Evidence: cif/CIFgen.c:5433-5446, cif/CIFhier.c:157-162, cif/CIFwrite.c:388-394, calma/CalmaWrite.c:1509-1516
- Options: replicate | fix (search D)
- Recommendation: fix, and verify against Magic with MASK-HG-020 before deciding.

### Q-MASK-413: `cif unfracture` needs a dummy argument
- Kind: quirk
- Magic behavior: The yes/no value is read from the fourth word (`cif unfracture <x> yes`). `cif unfracture yes` only reports the current state.
- Evidence: commands/CmdCD.c:2056-2080
- Options: replicate | fix (accept the value as the third word)
- Recommendation: accept both forms.

### Q-MASK-414: Progress reporting and event blocking during interaction processing
- Kind: platform
- Magic behavior: Magic uses a 5-second signal timer, prints `Completed N%`, and restricts Tk events so that the database cannot change during a write.
- Evidence: cif/CIFhier.c:974-988, cif/CIFhier.c:1035-1041, cif/CIFhier.c:1110-1124
- Options: platform alternative (progress callback with cancellation, and a read-only snapshot of the database during the write)
- Recommendation: use a platform alternative on all targets, including iPadOS.

### Q-MASK-300: cifinput "nanometers"/"angstroms" overwrites the decimal multiplier
- Kind: bug
- Magic behavior: In `cifinput`, `scalefactor 2.5 nanometers` parses 2.5 as S=5, M=2, then SETS M=10 (output side multiplies the expander by 10). The resulting internal unit is 5 nm instead of 2.5 nm.
- Evidence: cif/CIFrdtech.c:638-644 vs cif/CIFtech.c:731-735
- Options: replicate | fix (multiply M by 10/100 like the output side)
- Recommendation: replicate by default for compatibility; warn when a decimal scalefactor is combined with a unit keyword.

### Q-MASK-301: labels kind "sticky" identical to "text"
- Kind: quirk
- Magic behavior: `labels L sticky` stores LABEL_TYPE_TEXT, the same as `text`; both make labels sticky.
- Evidence: cif/CIFrdtech.c:870-873
- Options: replicate | fix (distinct kind)
- Recommendation: replicate (no observable difference is defined).

### Q-MASK-302: style statement prefix-matches existing style names
- Kind: bug
- Magic behavior: `style NAME` is considered an existing style when any listed style starts with NAME (strncmp with strlen(NAME)); `style fo` after `style foo` adds nothing and its statements are unreachable.
- Evidence: cif/CIFrdtech.c:509-514
- Options: replicate | fix (exact match)
- Recommendation: fix with exact match, but warn; existing PDKs are unlikely to rely on the merge.

### Q-MASK-303: istyle selection ambiguous even with an exact match
- Kind: quirk
- Magic behavior: `cif istyle gds` with styles `gds` and `gds2` reports ambiguity although `gds` is exact.
- Evidence: cif/CIFrdtech.c:1339-1352
- Options: replicate | fix (exact match wins, like Lookup)
- Recommendation: fix (exact match wins); it only makes previously failing commands succeed.

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
- Magic behavior: CIFGenLayer picks Euclidean vs square grow from the current OUTPUT style's grow-euclidean flag even when evaluating cifinput rules.
- Evidence: cif/CIFgen.c:5081-5082
- Options: replicate | fix (square grow on input always)
- Recommendation: replicate for exactness; flag in diagnostics when the output style is Euclidean.

### Q-MASK-307: boundary/mask-hints operators on regular input layers use output semantics
- Kind: quirk
- Magic behavior: Only templayer rules (without copyup) apply the input meaning of `boundary` (define FIXED_BBOX) and `mask-hints` (store hints). In a `layer` rule, these ops OR in existing FIXED_BBOX / MASKHINTS geometry scaled by the output scalefactor. With several boundary tiles, the last enumerated tile wins.
- Evidence: cif/CIFrdcl.c:342-527, cif/CIFgen.c:5601-5633, cif/CIFgen.c:5682-5700
- Options: replicate | fix (error on boundary/mask-hints in layer rules)
- Recommendation: replicate and warn at tech load.

### Q-MASK-308: mask-hints input property lookup uses wrong key
- Kind: bug
- Magic behavior: The existing plane is looked up under `NAME` but stored under `MASKHINTS_NAME`; each templayer creates a fresh plane that replaces any earlier hints of the same name in the same cell.
- Evidence: cif/CIFrdcl.c:736-753
- Options: replicate | fix (look up MASKHINTS_NAME and merge)
- Recommendation: fix (merge); only differs when two templayers target the same hint name.
- Canonical entry for Q-MISC-11.

### Q-MASK-309: copyup ignored by the CIF reader
- Kind: quirk
- Magic behavior: copyup planes are saved but CIF `C` calls never copy them into parents; only the GDS SREF/AREF handler does.
- Evidence: cif/CIFrdcl.c:342-406, cif/CIFrdcl.c:1334-1399, calma/CalmaRdcl.c:1525-1565
- Options: replicate | fix (apply in CIF calls too)
- Recommendation: fix if cheap; low risk because CIF input with copyup is rare.

### Q-MASK-310: tagged on input mixes internal and read units
- Kind: bug
- Magic behavior: The label rectangle (internal units) is used directly as a search area on read planes (read units).
- Evidence: cif/CIFgen.c:5560-5580, cif/CIFgen.c:4078-4105
- Options: replicate | fix (scale label rect by S)
- Recommendation: fix (multiply by S) — the unscaled behavior is never meaningful unless S=1.

### Q-MASK-311: labels statement under a templayer stores a CIF index as a Magic type
- Kind: bug
- Magic behavior: For a templayer rule, crl_magicType holds the CIF layer index, which `labels` then stores as a Magic tile type.
- Evidence: cif/CIFrdtech.c:856-925, cif/CIFrdtech.c:787-800
- Options: replicate | fix (reject labels under templayer)
- Recommendation: fix (reject with error).

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
- Magic behavior: When a 95 label corner is odd, the input style is rescaled by 2 but the symbol numerator is not, so later coordinates in the same symbol are interpreted at half size.
- Evidence: cif/CIFrdcl.c:1166-1172 vs cif/CIFrdutils.c:620-626
- Options: replicate | fix (also double the symbol numerator)
- Recommendation: fix.

### Q-MASK-315: odd box lengths are truncated asymmetrically
- Kind: quirk
- Magic behavior: `B 3 2 10 0` gives x 8..11 (true 8.5..11.5); truncation toward zero shifts boxes toward the origin; no read-unit subdivision is triggered.
- Evidence: cif/CIFrdpt.c:160-170
- Options: replicate | fix (subdivide read units by 2)
- Recommendation: replicate (Magic-written CIF never produces odd sizes).

### Q-MASK-316: round flash read as a square
- Kind: quirk
- Magic behavior: `R d x y` paints the bounding square of the circle.
- Evidence: cif/CIFrdpt.c:225-232
- Options: replicate | fix (polygonal approximation)
- Recommendation: replicate.

### Q-MASK-317: "9 name" outside a definition renames the edit cell
- Kind: quirk
- Magic behavior: The current cell at top level is the edit cell, so a top-level `9` renames it.
- Evidence: cif/CIFrdcl.c:1302-1316
- Options: replicate | fix (ignore at top level)
- Recommendation: fix (ignore with warning).

### Q-MASK-318: DD/DS reuse and redefinition of renamed symbols
- Kind: quirk
- Magic behavior: After `DD n`, `DS n` maps again to the existing cell named "n" and clears it (affecting earlier calls). If symbol n was renamed by `9`, a second `DS n` clears and overwrites the renamed cell instead of creating a new one.
- Evidence: cif/CIFrdcl.c:199-290, cif/CIFrdcl.c:1218-1240
- Options: replicate | fix (always fresh cell)
- Recommendation: replicate.

### Q-MASK-319: "cif polygon subcells" has no effect
- Kind: doc-mismatch
- Magic behavior: The flag is stored but never consulted by the CIF reader (the GDS reader has its own option).
- Evidence: cif/CIFrdutils.c:103, commands/CmdCD.c:2014-2034
- Options: replicate (no-op) | fix (implement)
- Recommendation: replicate as no-op; document.
- Resolution: n/a — spec follows Magic's code; documentation is wrong; `cif polygon subcells` is stored and has no effect (spec conventions §2.7, 2026-11-07).

### Q-MASK-320: ".cif" always appended to file names [platform]
- Kind: platform
- Magic behavior: `cif write chip.cif` writes `chip.cif.cif`; read likewise searches `NAME.cif`.
- Evidence: utils/path.c:686-710, commands/CmdCD.c:2075-2087
- Options: replicate | fix (append only if missing) | platform alternative (file picker on iPadOS)
- Recommendation: append only when missing; on iPadOS use document picker.

### Q-MASK-321: flat CIF named __CIF2__ and without labels
- Kind: quirk
- Magic behavior: Flat output writes `9 __CIF2__;` and drops all cell labels (only paint is flattened).
- Evidence: cif/CIFwrite.c:729-792, cif/CIFhier.c:136-139
- Options: replicate | fix (root name, flattened labels)
- Recommendation: replicate for byte compatibility; consider an option.

### Q-MASK-322: label text not escaped in CIF output
- Kind: bug
- Magic behavior: Label text containing spaces or `;` is written verbatim, producing CIF that reads back differently.
- Evidence: cif/CIFwrite.c:450-478
- Options: replicate | fix (substitute characters)
- Recommendation: replicate; warn on such labels.

### Q-MASK-323: CIF symbol numbering depends on instance enumeration order
- Kind: ambiguity
- Magic behavior: Child numbers follow the database's instance enumeration order (spatial index traversal), not a documented order.
- Evidence: cif/CIFwrite.c:234-258, cif/CIFwrite.c:340-352
- Options: replicate the traversal | define a deterministic order (e.g. instance creation order)
- Recommendation: capture Magic's order on golden cases and decide; byte equality of CIF depends on it.

### Q-MASK-324: CIF header contains host/time fields [platform]
- Kind: platform
- Magic behavior: @@user, @@machine from USER/HOSTNAME env, @@date from local time, @@tool "Magic x.y".
- Evidence: cif/CIFwrite.c:260-300
- Options: replicate | platform alternative (app-provided user/host; "Hefesto" tool name)
- Recommendation: same line structure; values may differ; golden comparisons exclude these lines.

### Q-MASK-325: cif coverage counts triangles as rectangles
- Kind: bug
- Magic behavior: Split tiles contribute their full bounding-box area to the coverage sum.
- Evidence: cif/CIFsee.c:515-530
- Options: replicate | fix (half area)
- Recommendation: fix.

### Q-MASK-326: calma number list of exactly 255 entries rejected
- Kind: bug
- Magic behavior: The parser fails when the list has 255 or more entries.
- Evidence: cif/CIFrdtech.c:1388-1430
- Options: replicate | fix
- Recommendation: fix (no realistic tech file is affected).

### Q-MASK-327: default 94 label sticky flag uses a mismatched layer search
- Kind: bug
- Magic behavior: For a 94 label without layer, the sticky decision searches CIF layer indices < number of rules for one whose label type equals the current label type.
- Evidence: cif/CIFrdcl.c:1479-1497
- Options: replicate | fix (use the current L layer's kind)
- Recommendation: fix (use current layer).

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
- Magic behavior: Split tiles are never copied to the result of not-square.
- Evidence: cif/CIFgen.c:4738
- Options: replicate | fix (keep triangles)
- Recommendation: replicate.

### Q-MASK-331: warning redirect not honored for CIF input
- Kind: quirk
- Magic behavior: `cif warning redirect FILE` only redirects GDS read messages.
- Evidence: calma/CalmaRead.c:195-205, cif/CIFrdutils.c:132-182
- Options: replicate | fix
- Recommendation: fix (redirect both).

### Q-MASK-332: flat CIF write lacks reducer check
- Kind: bug
- Magic behavior: CIFWriteFlat does not refuse styles with reducer 0 (division by zero).
- Evidence: cif/CIFwrite.c:729-792 vs cif/CIFwrite.c:142-147
- Options: fix (same refusal as hierarchical)
- Recommendation: fix.
- Resolution: fix — flat CIF write refuses a style with reducer 0, like the hierarchical write, instead of dividing by zero (X-2 tier 1, 2026-11-07).

### Q-MASK-333: CIF wires use square end extensions
- Kind: quirk
- Magic behavior: CIF wire ends are extended by half width (square), while the CIF standard specifies round ends.
- Evidence: cif/CIFrdpt.c:705-764
- Options: replicate | fix
- Recommendation: replicate.

### Q-MASK-334: grow-grid does not snap upper edges to the grid
- Kind: bug
- Magic behavior: Edges move by |x| mod G (xtop += |xtop| mod G; xbot -= |xbot| mod G), so 7 becomes 9 for G=5 and -7 becomes -9.
- Evidence: cif/CIFgen.c:526-533
- Options: replicate | fix (round outward to multiples of G)
- Recommendation: replicate (affects mask output equality); owner of output semantics to confirm.

### Q-MASK-335: malformed CIF call transformation is used unscaled
- Kind: bug
- Magic behavior: On a transform parse error the call is still placed, with the partial transform's translation still in read units.
- Evidence: cif/CIFrdutils.c:1540-1595, cif/CIFrdcl.c:1349
- Options: replicate | fix (skip the call)
- Recommendation: fix (skip call with error).

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
- Magic behavior: When the record after a structure's elements is not ENDSTR, the reader skips to ENDSTR without converting or clearing the input-layer planes. The stale geometry then leaks into the next structure that is processed.
- Evidence: calma/CalmaRdcl.c:612-613, calma/CalmaRdcl.c:715-720
- Options: replicate | fix (clear the input planes on syntax error)
- Recommendation: fix. Clear the planes and report the error; this only affects malformed files.

### Q-MASK-111: "Rewound" flag is session-global and never reset
- Kind: bug
- Magic behavior: Once any read rewinds the file during look-ahead, duplicate-definition errors are suppressed for every later GDS read in the session.
- Evidence: calma/CalmaRdcl.c:56, calma/CalmaRdcl.c:170, calma/CalmaRdcl.c:439
- Options: replicate | fix (reset per read)
- Recommendation: fix (reset at the start of each read). The geometry is unaffected; only diagnostics change.

### Q-MASK-112: noduplicates ignored when the cell is first seen as a reference
- Kind: bug
- Magic behavior: The "predefined" flag is set only if the first occurrence of the name in the file is its definition. For top-down files, an in-memory cell referenced before its definition is overwritten despite `noduplicates yes`, even though "Using pre-existing cell definition" is printed.
- Evidence: calma/CalmaRdcl.c:1758-1840 (was_called/predefined logic), calma/CalmaRdcl.c:1192
- Options: replicate | fix (remember predefined status per name)
- Recommendation: fix. The printed message already shows the intent.
- Resolution: fix — the predefined status is remembered per name, so `noduplicates yes` never overwrites an in-memory cell (silent data loss) (X-2 tier 1, 2026-11-07).

### Q-MASK-113: Round path ends (PATHTYPE 1) read as flush
- Kind: quirk
- Magic behavior: PATHTYPE 1 is reported as unsupported and treated as flush (no extension), although a code comment says round ends are treated as square.
- Evidence: calma/CalmaRdpt.c:553-563
- Options: replicate | fix (treat as half-width square, or approximate the round cap)
- Recommendation: replicate for compatibility. Optionally offer a non-default option.

### Q-MASK-114: Path end extensions applied per axis
- Kind: quirk
- Magic behavior: BGNEXTN/ENDEXTN move the end point by E in x and by E in y independently. On a diagonal end segment this extends by E·√2 along the diagonal and skews the end. Negative extensions are ignored.
- Evidence: calma/CalmaRdpt.c:641-672
- Options: replicate | fix (extend along the segment direction; support negative extensions)
- Recommendation: replicate. Manhattan paths (the common case) are unaffected.

### Q-MASK-115: Negative (absolute) path widths dropped
- Kind: quirk
- Magic behavior: GDSII defines a negative WIDTH as an absolute width that is not affected by magnification. Magic drops paths with width <= 0 without a message.
- Evidence: calma/CalmaRdpt.c:675-679
- Options: replicate | fix (use |width|)
- Recommendation: fix with |width| and a warning, but mark it as a compatibility deviation. Decide with the golden test MASK-GR-016.

### Q-MASK-116: Duplicate-point removal skips the head pair
- Kind: quirk
- Magic behavior: Consecutive duplicate points are removed only from position 1 onward. A duplicate of the first point stays, which gives a zero-length first segment with atan2(0,0) = 0.
- Evidence: cif/CIFrdpt.c:355-375
- Options: replicate | fix
- Recommendation: replicate. It is needed for byte-identical geometry on such inputs.

### Q-MASK-117: Mitre-limit truncation drops the pending polygon
- Kind: bug
- Magic behavior: At a bend with |cos(alpha)| in [1e-10, 0.25) the routine stops. The polygon accumulated since the start, or since the last direction reversal, is never painted, so the whole path portion disappears rather than being truncated.
- Evidence: cif/CIFrdpt.c:459-495
- Options: replicate | fix (paint the accumulated polygon up to the bend)
- Recommendation: fix. Missing geometry is worse than an approximate end. Log the decision.
- Resolution: fix — at a mitre-limit stop the accumulated polygon is painted up to the bend instead of being dropped (silent data loss) (X-2 tier 1, 2026-11-07).

### Q-MASK-118: Path outline uses H = 1.5708 and savetheta = 0 for the first segment
- Kind: quirk
- Magic behavior: The perpendicular offset uses 1.5708 rather than π/2, which introduces a relative error of ~3.7e-6. The inner-corner correction of the first joint uses savetheta = 0 instead of the first segment's angle.
- Evidence: cif/CIFrdpt.c:395-410, cif/CIFrdpt.c:540-575
- Options: replicate | fix
- Recommendation: replicate exactly. These constants affect rounding of vertices for very wide paths and for short inner corners.

### Q-MASK-119: savepaths stores input units in a dimension property
- Kind: ambiguity
- Magic behavior: The recorded centerline values are input-unit coordinates (halved from doubled), not internal units. If the style scale factor is not 1, the property values do not match layout coordinates, although they are typed as dimensions.
- Evidence: cif/CIFrdpt.c:239-330, calma/CalmaRdpt.c:723
- Options: replicate | fix (divide by S)
- Recommendation: replicate. Downstream scripts may depend on it; confirm with the golden test MASK-GR-021.

### Q-MASK-120: TEXT WIDTH honored only right after PRESENTATION
- Kind: quirk
- Magic behavior: Without PRESENTATION, PATHTYPE and WIDTH are skipped. With PRESENTATION followed by PATHTYPE, WIDTH is also skipped.
- Evidence: calma/CalmaRdpt.c:828-880
- Options: replicate | fix
- Recommendation: replicate. Plain labels have no size anyway.

### Q-MASK-121: Label positions truncated toward zero
- Kind: quirk
- Magic behavior: The text point and the pin-shape rectangle are divided by the scale factor with C truncation. Negative off-grid positions therefore round up and positive ones round down. Instance origins, in contrast, use exact or rescaled conversion.
- Evidence: calma/CalmaRdpt.c:938-941, calma/CalmaRdpt.c:330-334
- Options: replicate | fix (floor or nearest)
- Recommendation: replicate.

### Q-MASK-122: Inconsistent font-size units between DRC default, 1 µm cap and MAG
- Kind: quirk
- Magic behavior: The default size from the DRC width is scaled by 8/10. The cap is 800·m/S. MAG gives 1000·mag·magscale·m/S. A MAG of 0.8 therefore equals the default cap. The formula for the DRC-derived size multiplies by den/num, which makes it look inverted.
- Evidence: calma/CalmaRdpt.c:800-820, calma/CalmaRdpt.c:898-912
- Options: replicate | fix
- Recommendation: replicate the integer formulas exactly. The font size is stored in .mag files.

### Q-MASK-123: Pin placeholder converts text-kind labels to ports
- Kind: quirk
- Magic behavior: When a TEXT consumes an empty placeholder label created by a pin shape on a `text`/`sticky` layer, the label gets all four port directions and a computed port index (possibly 0), so it becomes a port-like label.
- Evidence: calma/CalmaRdpt.c:1053-1090
- Options: replicate | fix (only set port data for `port` layers)
- Recommendation: replicate pending a golden test (MASK-GR-030).

### Q-MASK-124: AREF row spacing rounding discarded
- Kind: bug
- Magic behavior: For columns, a non-divisible displacement is rounded with (p+(cols+1)/2)/cols. For rows, the rounded value is computed and then overwritten by the truncated p/rows (a missing `else`).
- Evidence: calma/CalmaRdcl.c:1490-1500
- Options: replicate | fix (round like columns)
- Recommendation: replicate. It is exact for well-formed GDS, and an error is reported either way.

### Q-MASK-125: Unknown instance property stops property parsing
- Kind: quirk
- Magic behavior: A PROPATTR other than 61/98/99 ends the property loop. Its PROPVALUE and any following 61/98/99 properties are skipped.
- Evidence: calma/CalmaRdcl.c:1385-1445
- Options: replicate | fix (skip the unknown pair and continue)
- Recommendation: fix. Instance names from third-party tools are otherwise lost.

### Q-MASK-126: Look-ahead seek assumes minimal STRNAME padding
- Kind: bug
- Magic behavior: After matching a STRNAME, the reader seeks back by BGNSTR length + 4 + strlen rounded to even. If a writer pads with more NULs, the position is wrong and parsing desynchronizes.
- Evidence: calma/CalmaRdcl.c:148-160
- Options: replicate | fix (seek back using the actual STRNAME record length)
- Recommendation: fix. The result is identical for conforming files.

### Q-MASK-127: Temporary polygon-cell flattening matches any "polygon*" cell
- Kind: bug
- Magic behavior: In `temporary` mode, every use of a cell whose name starts with "polygon" is flattened and the cell deleted, including genuine GDS structures named e.g. "polygon_ring".
- Evidence: calma/CalmaRdcl.c:323-350
- Options: replicate | fix (track created polygon cells explicitly)
- Recommendation: fix.
- Resolution: fix — only polygon cells created by the reader are flattened and deleted; genuine structures named `polygon*` are kept (silent data loss) (X-2 tier 1, 2026-11-07).

### Q-MASK-128: Home-directory substitution in GDS_FILE uses the wrong string
- Kind: bug
- Magic behavior: The "~" substitution tests and copies the cell's .mag file path instead of the GDS path. A cell loaded from a .mag under $HOME therefore gets GDS_FILE set to "~/<mag path>". Freshly read cells have no .mag path, so no substitution happens.
- Evidence: database/DBio.c:6095-6108
- Options: replicate | fix (substitute on the GDS path)
- Recommendation: fix. Replicating produces a wrong file reference.

### Q-MASK-129: GDS_FILE stores the path as found on the search path
- Kind: ambiguity
- Magic behavior: GDS_FILE is the name returned by the path search (possibly relative to the current directory). It is not canonicalized.
- Evidence: commands/CmdCD.c:1017-1030, calma/CalmaRdcl.c:544-570
- Options: replicate | fix (store an absolute path)
- Recommendation: replicate. The .mag files written by Magic contain these strings.

### Q-MASK-130: `gds rescale no` also forces warning level "limit"
- Kind: quirk
- Magic behavior: Disabling rescale silently changes the global warning level to `limit`, which is also shared with CIF.
- Evidence: commands/CmdCD.c:912-914
- Options: replicate | fix
- Recommendation: replicate (behavioral; messages are informative).

### Q-MASK-131: "read into edit cell" documentation mismatch
- Kind: doc-mismatch
- Magic behavior: The help says "read ... into edit cell", but no geometry is placed in the edit cell. Structures become cells, and the cell named by LIBNAME is loaded into the window.
- Evidence: commands/CmdCD.c:170-172, calma/CalmaRead.c:295-310
- Options: replicate behavior | document
- Recommendation: replicate the behavior and fix the documentation.
- Resolution: n/a — spec follows Magic's code; documentation is wrong; the read places no geometry in the edit cell (spec conventions §2.7, 2026-11-07).

### Q-MASK-132: Host dependencies (time zone, PDK variables, gzip)
- Kind: platform
- Magic behavior: GDS dates are converted with mktime (host local time zone, so results differ between hosts). PDK prefixes come from Tcl globals. $HOME is used for "~". Files are opened through zlib with search paths.
- Evidence: calma/CalmaRdio.c:335-370, database/DBio.c:6060-6111
- Options: replicate (local time) | fix (UTC) | platform alternative (configurable PDK variables on iPadOS)
- Recommendation: Convert in local time to match Magic on the same host, and allow a UTC override for reproducible tests. On iPadOS, provide PDK variables through app configuration.

### Q-MASK-133: Cell-id label test reads layer index -1
- Kind: bug
- Magic behavior: For a TEXT on an unmapped layer, the "cellid" check indexes the label-kind table with -1 (undefined behavior) before the unknown-layer branch.
- Evidence: calma/CalmaRdpt.c:1015
- Options: replicate (impossible to replicate deterministically) | fix
- Recommendation: fix. Treat unmapped pairs as not cell-id (the spec states this).
- Resolution: fix — a TEXT on an unmapped layer is treated as not cell-id, without indexing the kind table at -1 (X-2 tier 1, 2026-11-07).

### Q-MASK-134: BGNSTR year interpreted as years since 1900
- Kind: quirk
- Magic behavior: The year field is used directly as the C tm_year (years since 1900), consistent with Magic's own writer. Files from tools that write the full year (e.g. 2024) yield dates around year 3924, and the 32-bit cast may overflow.
- Evidence: calma/CalmaRdio.c:340-368
- Options: replicate | fix (treat values >= 1900 as full years)
- Recommendation: replicate. Timestamps of read cells must match Magic; only equality is checked.

### Q-MASK-200: Permissive GDS character table swaps '%' and '&'
- Kind: bug
- Magic behavior: With `options calma-permissive-labels`, string bytes 0x25 '%' are written as '&' and 0x26 '&' as '%' (table entries transposed).
- Evidence: calma/CalmaWrite.c:219
- Options: replicate | fix (identity mapping for all printable characters)
- Recommendation: fix, and add an opt-in compatibility switch if byte-identical output with Magic is required for labels containing '%' or '&'.

### Q-MASK-201: UNITS values stick after an angstrom write
- Kind: bug
- Magic behavior: The UNITS values are function-static and are only ever set to the angstrom values. After one write with `units angstroms`, every later write in the same session emits 0.0001/1e-10, even for nanometer styles, while coordinates are scaled for nanometers.
- Evidence: calma/CalmaWrite.c:3066-3102
- Options: replicate | fix (compute UNITS from the current style on every write)
- Recommendation: fix. The Magic behavior produces files at the wrong scale by a factor of 10.

### Q-MASK-202: Modification date is current time, documentation says zero
- Kind: doc-mismatch
- Magic behavior: The second date in BGNLIB and BGNSTR is always the current time. gds.html says it is set to zero. As a result, two writes are never byte-identical.
- Evidence: calma/CalmaWrite.c:1444, calma/CalmaWrite.c:3078; doc/html/gds.html ("datestamp")
- Options: replicate | fix (write zero, or the creation stamp) | add an option for reproducible output
- Recommendation: replicate by default (exact match except for documented time fields), and offer a reproducible-build option that writes the creation stamp in both fields.

### Q-MASK-203: Dates depend on the host time zone
- Kind: platform
- Magic behavior: Dates are converted with localtime(). `datestamp 0` writes 1970-01-01 00:00:00 only under UTC, and 1969-12-31 in western time zones.
- Evidence: calma/CalmaWrite.c:3127-3134
- Options: replicate (use the host local zone) | fix (always UTC) | platform alternative (explicit zone setting; iPadOS sandbox has the device zone)
- Recommendation: replicate local time by default. Golden tests run with TZ=UTC.

### Q-MASK-204: Order of cell references and structures depends on the spatial-index binning state
- Kind: ambiguity
- Magic behavior: Child uses are enumerated by the bin-plane index. For unbinned planes this is LIFO (reverse insertion). Once bins are built (after enough uses and a partial-area search), the order follows bin layout and can change with editing history. This order determines both SREF order and the post-order of structures.
- Evidence: database/DBcellsrch.c:86-109, 1263-1281; bplane/bpMain.c:114-170; bplane/bpBins.c:645-707; bplane/bpEnum.c:85-120
- Options: replicate (reimplement the bin-plane exactly) | fix (define a canonical order, e.g. reverse insertion order or sorted by use id)
- Recommendation: replicate reverse insertion order for the unbinned case, and capture golden cases with many uses to decide whether the full bin-plane emulation is worth it.

### Q-MASK-205: Order of port labels with equal port index
- Kind: ambiguity
- Magic behavior: Port labels are collected into a list in reverse label order and sorted with the C library qsort by port index. The order of equal indices depends on the libc implementation.
- Evidence: calma/CalmaWrite.c:1548-1600
- Options: replicate (glibc behavior) | fix (stable sort preserving label-list order)
- Recommendation: use a stable sort on the reversed list. That matches glibc's merge-sort behavior in practice. Verify against Magic on Linux.

### Q-MASK-206: Port labels dropped when the type has no `labels ... port` declaration
- Kind: quirk
- Magic behavior: Port labels are written only if the output style declared `labels <type> port` for their type. `labels <type>` alone sets the port text layer but not the port layer, and the writer requires the port layer, so the port is silently omitted from GDS.
- Evidence: calma/CalmaWrite.c:1586-1591; cif/CIFtech.c:933-955
- Options: replicate | fix (fall back to the label layer when no port layer exists)
- Recommendation: replicate, because PDK tech files are written against this behavior. Emit a warning.

### Q-MASK-207: string-limit truncation of odd-length strings keeps 31 characters
- Kind: quirk
- Magic behavior: The length is rounded up to even before truncation, so for an odd length > 32 the kept tail is 31 characters plus a NUL pad.
- Evidence: calma/CalmaWrite.c:3197-3208
- Options: replicate | fix (keep 32 real characters)
- Recommendation: replicate. The option is legacy-only.

### Q-MASK-208: Control characters become NUL bytes, and strings are mutated in place
- Kind: bug
- Magic behavior: Bytes 0x01-0x1F and 0x7F map to 0x00 and are written inside the STRING/PROPVALUE data. The encoder also overwrites the label text or use id in memory with the mapped characters, which can truncate it at the new NUL.
- Evidence: calma/CalmaWrite.c:3218-3243
- Options: replicate output bytes but do not mutate data | fix (map to '_' or 'X')
- Recommendation: replicate the output bytes, and never mutate layout data (already stated as normative in the spec).
- Resolution: fix — layout data (label text, use ids) is never mutated by the encoder (data loss); the bytes written stay as Magic writes them (X-2 tier 1, 2026-11-07).

### Q-MASK-209: Sanitized structure names are not checked for uniqueness
- Kind: bug
- Magic behavior: Cells `a-b` and `a_b` both produce STRNAME `a_b` under the strict table. The file then has duplicate structure definitions.
- Evidence: calma/CalmaWrite.c:1895-1918
- Options: replicate | fix (uniquify with a suffix and warn)
- Recommendation: replicate the naming, and report an error when a collision occurs.

### Q-MASK-210: Default extension test looks for '.' anywhere in the path
- Kind: quirk
- Magic behavior: `.gds`/`.gds.gz` is appended only if the path contains no '.', including in directory names (`./out/top` gets no extension).
- Evidence: commands/CmdCD.c:1046, commands/CmdCD.c:1066
- Options: replicate | fix (test only the final path component)
- Recommendation: fix (test the basename only). It is UI-level behavior (behavioral class).

### Q-MASK-211: Vendor full-dump prefix uses random()
- Kind: ambiguity
- Magic behavior: The two-character library prefix comes from the process-global random() sequence, which is never seeded. It depends on every earlier random() call in the session, so output is not reproducible across sessions with different histories.
- Evidence: calma/CalmaWrite.c:747-770
- Options: replicate (glibc random() with seed 1, counting calls) | fix (deterministic prefix derived from a hash of the file name)
- Recommendation: fix with a deterministic hash-based prefix, and document it as a known deviation.

### Q-MASK-212: Full-dump unit check depends on the `gds readonly` setting at write time
- Kind: quirk
- Magic behavior: The vendor library's units are checked only if `gds readonly` is currently yes. Otherwise a library with any units is dumped verbatim and may be at the wrong scale.
- Evidence: calma/CalmaRead.c:849-880; calma/CalmaWrite.c:722-730
- Options: replicate | fix (always check units against the output style)
- Recommendation: fix (always check).

### Q-MASK-213: Empty vendor library writes a stray record header
- Kind: bug
- Magic behavior: If the dumped library has no structure, the next record header (normally ENDLIB) is copied to the output with data type forced to 2, producing a premature `00 04 04 02` inside the stream.
- Evidence: calma/CalmaWrite.c:425-433
- Options: replicate | fix (write nothing)
- Recommendation: fix.
- Resolution: fix — an empty vendor library writes nothing, so no premature ENDLIB header truncates the stream for readers (silent data loss) (X-2 tier 1, 2026-11-07).

### Q-MASK-214: Forward-reference renaming in full dumps
- Kind: quirk
- Magic behavior: An SNAME naming a structure not yet seen is renamed prefix+name, or prefix+name+"[[0]]" if a database cell with the prefixed name exists. Unresolved references are written at the end of the write as magic cells under their unprefixed names, which do not match the prefixed SNAME.
- Evidence: calma/CalmaWrite.c:599-625, calma/CalmaWrite.c:345-366, calma/CalmaWrite.c:788-801
- Options: replicate | fix (resolve two-pass: scan the library for definitions before copying)
- Recommendation: replicate the naming. Collect golden cases with addendum-style vendor libraries before deciding.

### Q-MASK-215: All structures of a full dump get the triggering cell's creation date
- Kind: quirk
- Magic behavior: Each dumped structure's BGNSTR creation date is the session stamp or the timestamp of the vendor cell that caused the dump, not the date in the source file.
- Evidence: calma/CalmaWrite.c:446-451
- Options: replicate | fix (copy the original BGNSTR dates)
- Recommendation: replicate.

### Q-MASK-216: Copied SNAME records are not updated for renamed cells
- Kind: ambiguity
- Magic behavior: The bytes of a new-style vendor copy (GDS_START..GDS_END) are copied verbatim. If subcells were renamed on read (e.g. `gds unique`) or afterwards, the SNAME inside the copy still names the original structure, while the subcell structure is written under its magic name.
- Evidence: calma/CalmaWrite.c:1085-1335
- Options: replicate | fix (parse the copied records and rewrite SNAME)
- Recommendation: replicate. Warn when a referenced child name differs from the GDS name.

### Q-MASK-217: Merged polygons may contain slits and may be split arbitrarily
- Kind: quirk
- Magic behavior: Each tile is spliced into at most one existing polygon at one shared edge. Other shared edges remain as zero-width slits, and connected parts already in different bounds are never joined. The 201-point check splits components. Collinear removal is a single pass and can leave a collinear point at the ring start.
- Evidence: calma/CalmaWrite.c:2187-2257, calma/CalmaWrite.c:2089-2172
- Options: replicate | fix (proper polygon union with hole cut-lines)
- Recommendation: replicate. The algorithm is cheap to port, and byte identity depends on it.

### Q-MASK-218: Tile structure of non-Manhattan mask planes
- Kind: ambiguity
- Magic behavior: For Manhattan geometry, maximal-horizontal-strip tiling is canonical. With split tiles, the decomposition depends on the order of paint operations in mask generation, and this affects element order and triangle splitting in the output.
- Evidence: database/DBtiles.c:521-618; cif/CIFgen.c (paint order)
- Options: replicate (reimplement the corner-stitch paint algorithm exactly) | fix (canonical decomposition)
- Recommendation: replicate the corner-stitched plane implementation. This is shared with capability `mask-output-operators`.
- Resolution: compat — tile order bullet: Hefesto reproduces Magic's corner-stitched paint algorithm, including split-tile decomposition, so element order and triangle splitting match (X-1 as amended by D-1, 2026-11-07).

### Q-MASK-219: Write with no output style leaves an empty file
- Kind: quirk
- Magic behavior: The output file is created before the output-style check. When no style is set, an empty file remains.
- Evidence: commands/CmdCD.c:1066-1076; calma/CalmaWrite.c:300-304
- Options: replicate | fix (check before creating the file)
- Recommendation: fix.

### Q-MASK-220: Option query messages are inverted or wrong
- Kind: doc-mismatch
- Magic behavior: `gds abstract` (query) prints the `undefined` flag. `gds lower` prints "lowercase only" when mixed case is allowed. `gds unique` prints "will not be renamed" when renaming is on. The help text says compress is 0-6, but 0-9 is accepted.
- Evidence: commands/CmdCD.c:337-341, 620-623, 773-776, 168
- Options: replicate | fix
- Recommendation: fix (messages are informative).

### Q-MASK-221: Text magnification scale is read-only
- Kind: quirk
- Magic behavior: `gds magscale` changes how MAG is interpreted on read, but on write MAG is always size/800 × S/E (1.0 = 1 micron), so a read/write round trip with magscale ≠ 1 changes text size.
- Evidence: calma/CalmaWrite.c:2913-2916; commands/CmdCD.c:454-473
- Options: replicate | fix (divide by magscale on write)
- Recommendation: replicate.

### Q-MASK-222: 32-bit overflow in coordinate scaling
- Kind: ambiguity
- Magic behavior: Coordinates are multiplied by Wscale/Pscale in C `int` arithmetic, with no overflow check (undefined behavior, which wraps in practice).
- Evidence: calma/CalmaWrite.c:1826-1850, 2337-2340, 2741-2744
- Options: replicate wrap | fix (report an error when a value exceeds the I4 range)
- Recommendation: fix (error). Valid designs never reach the limit.

### Q-MASK-223: gzip byte stream depends on the zlib build
- Kind: platform
- Magic behavior: Compressed output is produced by zlib gzopen/gzwrite with a final Z_SYNC_FLUSH. The header OS byte, the deflate blocks, and the empty stored block depend on the zlib version and platform.
- Evidence: commands/CmdCD.c:1049-1060; calma/CalmaWrite.c:369-372
- Options: replicate (bundle the same zlib) | platform alternative (any conforming gzip)
- Recommendation: compare decompressed content only. Any conforming gzip writer is acceptable.

### Q-MASK-224: `datestamp yes` documented as current time but uses the cell timestamp
- Kind: doc-mismatch
- Magic behavior: The option help says "use current time or value". The code clears the session stamp, so each structure's creation date is the cell's own timestamp.
- Evidence: commands/CmdCD.c:125, 932-944; calma/CalmaWrite.c:1440-1443
- Options: replicate | fix (doc)
- Recommendation: replicate the code. Document it correctly in Hefesto.
- Resolution: n/a — spec follows Magic's code; documentation is wrong; `datestamp yes` uses each cell's timestamp (spec conventions §2.7, 2026-11-07).

### Q-MASK-225: Incompatible expander writes mis-scaled output
- Kind: quirk
- Magic behavior: If 10 (or 100 with angstrom units) is not divisible by the expander, an error is printed, but the file is written with Wscale = S and Pscale = 1, which is wrong by the expander ratio.
- Evidence: calma/CalmaWrite.c:1450-1478
- Options: replicate | fix (refuse to write)
- Recommendation: fix (refuse with an error suggesting `units angstroms`).

### Q-MASK-226: Abstract-view failure leaves a partial but terminated file
- Kind: quirk
- Magic behavior: When an abstract cell is disallowed, the already-written structures stay, the failing cell's ancestors are skipped, ENDLIB is still written, and only an error status is returned.
- Evidence: calma/CalmaWrite.c:1037-1049, 1077-1080, 357-388
- Options: replicate | fix (validate the whole tree before writing, or delete the file)
- Recommendation: fix (validate first). Behavioral class.

### Q-MASK-227: bridge expansion takes square roots of negative values
- Kind: bug
- Magic behavior: the bridge expansion computes `sqrtf(w² − x²)` without checking the sign. When the gap along the chosen axis exceeds w (possible when d > w), the argument is negative, the result is NaN, and `ceilf(NaN)` is converted to int. That conversion is undefined in C: x86-64 yields INT_MIN, AArch64 yields 0. The bridge rectangle therefore differs between platforms, and on x86-64 can overflow to a huge rectangle when NaN is the governing term.
- Evidence: cif/CIFgen.c:1735-1765
- Options: replicate the x86-64 result | fix (clamp the argument to 0)
- Recommendation: fix by clamping to 0. Capture x86-64 golden output first to see whether any real PDK hits it (verify against Magic).
- Resolution: fix — the square-root argument is clamped to 0, so no NaN is converted to int (undefined behaviour) (X-2 tier 1, 2026-11-07).

### Q-MASK-228: bridge-lim passes 1 and 2 modify the plane they are enumerating
- Kind: bug
- Magic behavior: after pass 0, the plane being searched for corners is also the plane that bridges are painted into. Tiles created or merged by a bridge can be visited, revisited or skipped depending on the enumeration state, so the result depends on the tile-plane traversal algorithm and not only on the geometry.
- Evidence: cif/CIFgen.c:5243-5267
- Options: replicate (port the enumeration literally) | fix (enumerate a snapshot and paint into a copy)
- Recommendation: fix (snapshot), after golden captures show whether common layouts differ (verify against Magic).

### Q-MASK-229: bridge-lim erases whole limiting tiles, not only their part inside the bridge
- Kind: bug
- Magic behavior: when no limit-free bridge shape exists, every limiting tile that overlaps the bridge is erased from the result over its full extent (unscaled), which can also remove original geometry far from the bridge.
- Evidence: cif/CIFgen.c:4379-4405
- Options: replicate | fix (erase only the intersection with the bridge, in mask units)
- Recommendation: replicate until golden output confirms the effect; then decide with Q-MASK-5.

### Q-MASK-230: bloat-all leaves contact-image trigger tiles marked
- Kind: bug
- Magic behavior: with d = 0, a trigger tile is marked as processed. At the end of the operation, marks are cleared only on the T2 plane. A trigger found on another plane (the image of a contact on a second plane) stays marked, so a later `bloat-all` that uses the same tile as a trigger does not push it and produces nothing for it.
- Evidence: cif/CIFgen.c:1427-1432, cif/CIFgen.c:1473-1477, cif/CIFgen.c:5313-5328
- Options: replicate | fix (clear marks on every plane searched for triggers)
- Recommendation: fix (verify against Magic first).

### Q-MASK-231: bloat-all seeding depends on trigger order when d = 0
- Kind: quirk
- Magic behavior: for triggers that are not on the T2 plane, only the first unprocessed T2 tile overlapping the trigger seeds the flood. With d = 0, tiles stay processed across triggers, so a later trigger whose first overlapping region is already done seeds a different, unprocessed region instead. The union of painted regions therefore depends on the trigger enumeration order.
- Evidence: cif/CIFgen.c:1332-1344, cif/CIFgen.c:1458-1478
- Options: replicate | fix (seed every region overlapping the trigger)
- Recommendation: replicate.

### Q-MASK-232: bloat-all converts mask-layer triggers to internal units with truncation toward zero
- Kind: quirk
- Magic behavior: a mask-layer trigger used with database T2 types is divided by the scale factor with C integer division, which truncates toward zero. Both the seed area and the clip base (multiplied back by the scale) shift toward the origin for negative or off-grid coordinates.
- Evidence: cif/CIFgen.c:1420-1428, cif/CIFgen.c:1480-1490
- Options: replicate | fix (floor/ceil outward)
- Recommendation: replicate; it only matters for off-grid mask geometry.

