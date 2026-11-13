# Open Questions — DRC

Items found while reverse-engineering Magic 8.3.694 (fc5daa9e) DRC for the capabilities `drc-rule-language`, `drc-rule-semantics`, `drc-engine`, `drc-cif-rules` and `drc-commands`. Unless decided otherwise, the specs require Hefesto to replicate the Magic behavior described here.

### Q-DRC-1: Scale remainder stored in one byte
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: `drcScaleDown` divides each distance by the reduced DRC factor N with ceiling and keeps the remainder in an `unsigned char` (`drcc_mod`, `drcc_cmod`); `drcScaleUp` subtracts 1 when the stored remainder is non-zero, multiplies back and adds it. For N > 256 the remainder wraps, and one that wraps to 0 also skips the subtract-1. For area rules the stored value is `(area % N²) / N`, so the restored area is `area − area % N`; when `0 < area % N² < N` the stored 0 skips the subtract-1 and the restored area is `ceil(area/N²)·N²` (e.g. N = 10, area 1001 → 1100). Wrong values appear only after a grid rescale (`DRCTechScale`, run by `scalegrid` and by reading a `.mag`/GDS/CIF on a finer grid) while N > 1; with N = 1 nothing is stored and rescaling is exact.
- Evidence: drc/DRCtech.c:drcScaleDown (4068-4120), drc/DRCtech.c:drcScaleUp (4137-4189), drc/DRCtech.c:DRCTechScale (4550-4592), drc/drc.h:33,35 (corrected 2026-11-10: added the area `< N` case and the wrap-to-0 case)
- Options: replicate | compat (replicate under `magic-compat`; native keeps the tech-file values and rescales from them exactly) | fix
- Recommendation: compat — the restored distances drive every later check, so this is a value-changing quirk (X-2 tier 2); native recomputes each distance from the tech-file value and the current D/N. Confirm with golden DRC-SCALE-3 plus an area case (`scalefactor 10`, `area m1 1001 …`, `scalegrid 1 2`).
- Resolution: compat — the restored distances drive every later check, so this is a value-changing quirk (X-2 tier 2); native recomputes each distance from the tech-file value and the current D/N. Confirm with golden DRC-SCALE-3 plus an area case (`scalefactor 10`, `area m1 1001 …`, `scalegrid 1 2`). Approved 2026-11-10.

### Q-DRC-2: Mask-layer rule halo scaled twice
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: `drcCifWidth`, `drcCifSpacing`, `drcCifArea` and `drcCifMaxwidth` return `ceil(dist / cs_scaleFactor)`, already in internal units; `DRCTechAddRule` folds that into `DRCTechHalo` together with the tech-file-unit distances of ordinary rules, and `drcTechFinalStyle` then multiplies the whole halo by D/N with ceiling. With `scalefactor N` (N > 1) a halo set by a CIF rule is divided by N, so check squares, subcell interaction areas and array overlap areas can be too small and mask-layer errors near their boundaries are missed. Without a `scalefactor` (D = N = 1) the halo is correct.
- Evidence: drc/DRCcif.c:drcCifWidth (223), drcCifSpacing (378), drcCifArea (1220), drcCifMaxwidth (1290); drc/DRCtech.c:DRCTechAddRule (1164-1174); drc/DRCtech.c:drcTechFinalStyle (4228-4233)
- Options: replicate | compat (replicate under `magic-compat`; native scales only tech-file-unit contributions by D/N and takes the maximum with the CIF ones) | fix
- Recommendation: compat — the halo decides which areas are rechecked and so which error tiles exist (X-2 tier 2); native keeps CIF contributions out of the D/N scaling.
- Resolution: compat — the halo decides which areas are rechecked and so which error tiles exist (X-2 tier 2); native keeps CIF contributions out of the D/N scaling. Approved 2026-11-10.

### Q-DRC-3: Multiplane spacing compiles only one plane pair
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: When either type list of a `spacing` rule spans several planes (types partitioned across planes), `drcSpacing` loops over (plane1, plane2) pairs but `return`s the result of `drcMaskSpacing` for the first pair that matches, in ascending plane order; the other combinations get no rules, although the comment says "Loop over independent plane/layer combinations".
- Evidence: drc/DRCtech.c:drcSpacing (2810-2835)
- Options: replicate | compat (replicate under `magic-compat`; native compiles every combination) | fix
- Recommendation: compat — missing rules mean missing errors, a value-changing bug (X-2 tier 2); in both modes Hefesto also emits a tech-file lint warning naming the dropped plane pairs.
- Resolution: compat — missing rules mean missing errors, a value-changing bug (X-2 tier 2); in both modes Hefesto also emits a tech-file lint warning naming the dropped plane pairs. Approved 2026-11-10.

### Q-DRC-4: `angles 45` reads past the token
- Kind: bug
- Magic behavior: After parsing the number, the code compares `endptr + 1` with `"only"`. For the token `45`, `endptr` points at the terminating NUL so the comparison reads one byte past the string (undefined behavior; in practice usually not "only").
- Evidence: drc/DRCtech.c:1858-1861
- Options: replicate (treat `45` as 45-with-90) | fix
- Recommendation: fix: `45` means 45+90 degrees; `45` followed by exactly one separator character and `only` means 45-only.
- Resolution: fix — the token is never read past its end: `45` means 45 and 90 degrees, `45` followed by one separator and `only` means 45 only (X-2 tier 1, 2026-11-07).

### Q-DRC-5: Covered-rule elimination conditions
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: With rule optimization on, a non-trigger rule A is removed when a later rule B on the same (i,j) list has dist ≥, cdist ≥, OK mask ⊆ A's, the same corner mask, check plane, exception index, REVERSE and SPLITTILE flags, and (only when the distances are equal) the same OUTSIDE flag. B is skipped when B has BOTHCORNERS and A does not — the inverse of the comment "if A is BOTHCORNERS then B must be, too" — so a BOTHCORNERS rule can be removed in favor of a single-corner rule and lose its second corner check. The edge plane and the MANHATTAN, RUNLENGTH and DERIVED_EDGE/DERIVED_TARGET flags are not compared.
- Evidence: drc/DRCtech.c:drcTechFinalStyle (4352-4440, condition at 4414-4416)
- Options: replicate | compat (replicate under `magic-compat`; native requires B BOTHCORNERS when A is and compares edge plane and those flags) | fix
- Recommendation: compat — the coded condition contradicts its documented intent and changes error geometry and why-text (X-2 tier 2); confirm with golden DRC-OPT-2/3.
- Resolution: compat — the coded condition contradicts its documented intent and changes error geometry and why-text (X-2 tier 2); confirm with golden DRC-OPT-2/3. Approved 2026-11-10.

### Q-DRC-6: No edge-end test on horizontal edges
- Kind: quirk
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: On vertical edges a corner extension is applied only if the edge ends there (the type on either side changes past the segment end). Horizontal edges sample only the corner square. In a canonical tile plane horizontally adjacent tiles always differ in type, so a horizontal segment's type pair always changes at a tile end and the missing test only matters where the sampled square is inside the edge's own outside tile — the segment clipped by the check rectangle or a triggered sub-range — and that tile's type is in the corner mask, or next to split tiles. Compiled spacing, width and surround rules never put the edge's own outside type in the corner mask, so only hand-written `edge`/`edge4way` rules or split tiles can show a difference.
- Evidence: drc/DRCbasic.c:drcTile (vertical 1139-1180 and 1208-1256; horizontal 1563-1648)
- Options: replicate | compat (native adds the continuation test)
- Recommendation: replicate in both modes — not observable for compiled rules on Manhattan geometry, and a second code path is not worth it; golden DRC-EDGE-1 with an `edge` rule whose corner types include the outside type covers it.
- Resolution: replicate in both modes — not observable for compiled rules on Manhattan geometry, and a second code path is not worth it; golden DRC-EDGE-1 with an `edge` rule whose corner types include the outside type covers it. Approved 2026-11-10.

### Q-DRC-7: Run-length rules asymmetric between edge orientations
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: For `widespacing` rules with a run-length, the triggered spacing rule stores the run-length in `drcc_cdist` with DRC_RUNLENGTH. Horizontal edges set cdist = 0 for such rules; vertical edges use cdist = run-length both to widen the triggered sub-range (then clipped to the edge) and as the corner extension, also feeding the Euclidean radius. Vertical-edge checks therefore cover a larger region than horizontal ones. `areaCheck` applies the run-length filter in both orientations.
- Evidence: drc/DRCbasic.c:drcTile (967 vs 1427; corner use 1147-1180, 1222-1256), drc/DRCbasic.c:areaCheck (312-315), drc/DRCtech.c:drcMaskSpacing (2346-2352)
- Options: replicate | compat (replicate under `magic-compat`; native uses 0 on both orientations, as the horizontal branch does) | fix
- Recommendation: compat — the horizontal branch's explicit `RUNLENGTH ? 0` shows the vertical one is an oversight, and the region changes error geometry (X-2 tier 2); confirm with golden DRC-WS-RL-V/H.
- Resolution: compat — the horizontal branch's explicit `RUNLENGTH ? 0` shows the vertical one is an oversight, and the region changes error geometry (X-2 tier 2); confirm with golden DRC-WS-RL-V/H. Approved 2026-11-10.

### Q-DRC-8: Area rule aborted at exact check-rectangle boundary
- Kind: quirk
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: The area flood fill gives up (no error) as soon as any visited tile has an edge exactly on the check rectangle `dCD_rect` (the clip area expanded by the halo), or the running sum reaches the limit. In flattened interaction buffers geometry is clipped to the buffer, so regions reaching the buffer edge are never flagged there; in unflattened checks tiles are not clipped, so the fill only aborts when a tile edge happens to coincide with the check rectangle.
- Evidence: drc/DRCextend.c:drcCheckArea (138-178)
- Options: replicate | compat (native aborts only for regions actually clipped by a flattened buffer)
- Recommendation: replicate in both modes — the abort is how Magic avoids flagging regions it cannot see whole; a native variant would need clip tracking for a rare coincidence. Confirm with golden DRC-AREA-2.
- Resolution: replicate in both modes — the abort is how Magic avoids flagging regions it cannot see whole; a native variant would need clip tracking for a rare coincidence. Confirm with golden DRC-AREA-2. Approved 2026-11-10.

### Q-DRC-9: Off-grid snapping toward zero
- Kind: quirk
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: The off-grid error rectangle is the degenerate edge rectangle clipped to the clip area and then expanded so each coordinate reaches `trunc(v/p)·p`, i.e. toward zero rather than toward −∞; for negative coordinates the error extends toward the origin. An edge that lies on a grid line (vertical or horizontal) keeps zero width and is never flagged, even if its endpoints are off-grid; those are flagged by the perpendicular edges. The error is not re-clipped after expansion. Pitch ≤ 1 never errs.
- Evidence: drc/DRCextend.c:drcCheckOffGrid (87-115) (corrected 2026-11-10: the zero-width case applies to both orientations, not just horizontal edges)
- Options: replicate | compat (replicate under `magic-compat`; native uses floor so negative coordinates mirror positive ones) | fix
- Recommendation: compat — a rounding asymmetry in error geometry, which X-2 tier 2 names explicitly.
- Resolution: compat — a rounding asymmetry in error geometry, which X-2 tier 2 names explicitly. Approved 2026-11-10.

### Q-DRC-10: Diagonal-edge checks may never report
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: For a split tile, `drcTile` searches the triangle offset by the Euclidean rule distance; `areaNMCheck` skips the initiating tile and accepts another candidate only if `DBSrPaintNMArea`, searching the initiating tile's own rectangle (`dCD_rlist`), finds that candidate. Tiles do not overlap, so only the initiating tile is found there, `areaNMReject` never returns 1, and diagonal spacing/width checks never report. Orthogonal sides of split tiles are still checked by the normal edge loops. Trigger, area, maxwidth, rectangle and off-grid rules are skipped on diagonals.
- Evidence: drc/DRCbasic.c:areaNMReject (460-470), drc/DRCbasic.c:areaNMCheck (492-545), drc/DRCbasic.c:drcTile (731-840), database/DBtiles.c:DBSrPaintNMArea (377-483)
- Options: replicate (no diagonal errors) | compat (replicate under `magic-compat`; native implements the intended Euclidean diagonal check) | fix
- Recommendation: compat, once golden DRC-NM-1..3 confirm that Magic reports nothing across diagonals; standard cells are Manhattan, so the M3 exit criterion does not depend on it.
- Resolution: compat, once golden DRC-NM-1..3 confirm that Magic reports nothing across diagonals; standard cells are Manhattan, so the M3 exit criterion does not depend on it. Approved 2026-11-10.

### Q-DRC-11: Exception/exemption inside test boundaries
- Kind: ambiguity
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: The inside test searches the hint plane with a zero-width rectangle. Vertical edge (X,[yb,yt]): a hint tile covering a unit square (X, y) with y ∈ [yb,yt) makes it inside, so hints with left boundary X count and hints with right boundary X do not. Horizontal edge (Y,[xl,xr]): `DBSrPaintArea` starts at the hint-plane tile (hint or space) containing (xl, Y−1) and stops at once unless that tile extends above Y; if it does, every tile containing a unit square (x, Y−1) with x ∈ [xl,xr) is tested. So a horizontal edge is inside only if the row just below it touches a hint and the starting tile spans Y. The result depends on how the hint plane is split into tiles, and an edge on a hint's bottom or top boundary is normally outside.
- Evidence: drc/DRCbasic.c:drcTile (933-950 vertical, 1393-1408 horizontal), database/DBtiles.c:DBSrPaintArea (521-600)
- Options: replicate | compat (replicate under `magic-compat`; native uses the vertical rule transposed — a hint covering a unit square on one fixed side of the edge) | fix (closed-interval overlap test)
- Recommendation: compat — the horizontal result depends on tile structure, a clear defect, but it decides which rules apply (X-2 tier 2); the replica needs Hefesto's hint plane to keep Magic's maximal-horizontal-strip tiling. Confirm with golden DRC-EXC-3/4.
- Resolution: compat — the horizontal result depends on tile structure, a clear defect, but it decides which rules apply (X-2 tier 2); the replica needs Hefesto's hint plane to keep Magic's maximal-horizontal-strip tiling. Confirm with golden DRC-EXC-3/4. Approved 2026-11-10.
- Note: the native (magic-compat OFF) inside test is set by Q-MISC-13 — a hint touching or straddling the edge from either side makes it inside — not by the transposed vertical rule above (user decision 2026-11-10, M3 plan).

### Q-DRC-12: Background checker and scripting results without Tcl
- Kind: platform
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: In the Tcl build `DRCContinuous` runs from the idle loop. It processes one check square per `drcCheckTile` call and drains Tcl events between squares; on `DRC_BREAK_PENDING` it returns, keeping the remaining check tiles. It schedules `magic::drcstate busy` / `idle` and sets the prompt to `]` / `%`. Pending cells are appended, so children are checked before the parents that copy their errors up. List results (`drc list …`, `drc count` in list mode) are Tcl objects. Under X-3 Hefesto embeds sandboxed Tcl everywhere, including iPadOS, so scripting results are settled (`scripting-api`); only the scheduling mechanism remains. iPadOS has no fork/exec.
- Evidence: drc/DRCcontin.c:DRCContinuous (481-570), drc/DRCcontin.c:DRCCheckThis (184-300), commands/CmdCD.c:CmdDrc (4197-4274)
- Options: platform alternative (worker thread on a snapshot with an edit-generation counter; or run-loop idle source)
- Recommendation: platform — check one square at a time on a worker over an immutable snapshot, in Magic's pending order (children first, check tiles in area-enumeration order). Commit a square's errors only if the edit generation is unchanged, otherwise leave its check tile for a retry. `drc catchup` runs synchronously. Emit `magic::drcstate busy|idle` at the same transitions, and return list results as Tcl objects with Magic's structure.
- Resolution: platform — check one square at a time on a worker over an immutable snapshot, in Magic's pending order (children first, check tiles in area-enumeration order). Commit a square's errors only if the edit generation is unchanged, otherwise leave its check tile for a retry. `drc catchup` runs synchronously. Emit `magic::drcstate busy|idle` at the same transitions, and return list results as Tcl objects with Magic's structure. Approved 2026-11-10.
- Note: "immutable snapshot" is realized as a read of the live database under the reader/writer lock, not a copy: a copy would re-split tiles, and some DRC results depend on tile structure. A square's errors are committed only if the edit generation is unchanged (user decision 2026-11-10, M3 design).

### Q-DRC-13: Mask error coordinate mapping truncates toward zero
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: Mask-layer error rectangles are converted to internal units with C integer division (toward zero), so negative coordinates round up and positive round down; zero-width results are widened by one unit away from zero. Area/maxwidth errors are not widened.
- Evidence: drc/DRCcif.c:1052-1068 (corrected 2026-11-13: The spacing/width path is confirmed at drc/DRCcif.c:1052-1068: C division, and a zero extent is widened by xbot-- when the coordinate is negative, otherwise by xtop++. Correction for area and maxwidth: drcCheckCifArea (DRCcif.c:1364-1367) and drcCheckCifMaxwidth (1468-1471, 1497-1500, 1554-1557) truncate with no widening and then skip the error if GEO_RECTNULL holds. A violation whose start tile collapses to zero width in internal units is therefore not reported at all, not just placed differently. All three PDK DRC sections use cifwidth, cifspacing and cifmaxwidth, and gf180mcuD and IHP also use cifarea.)
- Options: replicate | fix (floor/ceil outward) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto replicates Magic: truncation toward zero, the one-unit widening on the spacing/width path, and dropping collapsed area and maxwidth errors. The DRC error plane, `drc count` and `drc find` then match Magic for the PDK decks. With it OFF, mask rectangles map to internal units with outward rounding (floor for lower edges, ceil for upper edges) on every path. Error rectangles are never empty, so area and maxwidth violations narrower than one internal unit are reported. In both modes, Hefesto counts collapsed area and maxwidth errors in a diagnostic, so a missed error is visible even with magic-compat ON. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON mask-layer error rectangles map to internal units by truncation toward zero, zero-width spacing/width errors are widened by one unit away from zero, and area and maxwidth errors that collapse to zero width are dropped, as in Magic; with it OFF every path rounds outward (floor for lower edges, ceil for upper edges), so no error rectangle is empty and sub-unit area and maxwidth violations are reported. In both modes a diagnostic counts the collapsed area and maxwidth errors (X-2 tier 2). Approved 2026-11-13.

### Q-DRC-14: `drc count` prints a zero total
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: Without `total`, outside list mode and with background DRC not off, after the per-cell lines the command sets the total to 0 and prints `Total DRC errors found: 0` whatever the per-cell counts. With background DRC off nothing follows the per-cell lines. `drc count total` and `drc list count` are correct.
- Evidence: commands/CmdCD.c:CmdDrc (4196-4273, reset at 4258-4259)
- Options: replicate | compat (replicate under `magic-compat`; native prints the real sum) | fix
- Recommendation: compat — the line is console output that golden captures compare, so magic-compat keeps it; native prints the sum of the per-cell counts. List results are identical in both modes.
- Resolution: compat — the line is console output that golden captures compare, so magic-compat keeps it; native prints the sum of the per-cell counts. List results are identical in both modes. Approved 2026-11-10.

### Q-DRC-15: First `drc find` shows error #2
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: The static counter `drc_nth` starts at 1 and an argument-less `drc find` increments it before use, so the first such call in a session moves the box to error #2 (wrapping to #1 if there is only one). After `drc find n` the next argument-less call shows n+1, as intended.
- Evidence: commands/CmdCD.c:CmdDrc (4058, 4304-4314, 4347-4352)
- Options: replicate | compat (replicate under `magic-compat`; native starts at 0) | fix
- Recommendation: compat — the box position and the `Error area #N` line are observable and scripts loop on `drc find` (X-2 tier 2); native starts the counter at 0 so the first call shows #1.
- Resolution: compat — the box position and the `Error area #N` line are observable and scripts loop on `drc find` (X-2 tier 2); native starts the counter at 0 so the first call shows #1. Approved 2026-11-10.

### Q-DRC-16: Unknown `%` escape in why-text
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: `drcSubstitute` first copies the whole why-string into a buffer of `strlen + 20·(number of %)` bytes. For `%` followed by anything but d, c, a it advances the output pointer two bytes without copying. The output keeps whatever bytes lie there: the original `%x` only if no earlier substitution changed the length. If an earlier substitution shortened the output, the bytes come from a shifted spot in the original. If it lengthened the output, they come from uninitialized memory. If the escape directly follows a substitution, the NUL written by `snprintf` is skipped and the string ends there. A trailing `%` reads past the string's NUL.
- Evidence: drc/DRCmain.c:drcSubstitute (182-261, default case 250-253) (corrected 2026-11-10: added the uninitialized, truncation and trailing-`%` cases)
- Options: replicate | fix (copy `%x` literally)
- Recommendation: fix in both modes — it reads uninitialized memory and past the string end (X-2 tier 1). Copying `%x` literally matches Magic wherever Magic's output is defined; check the sky130/gf180 why-strings for a `%` that is not `%d/%c/%a`.
- Resolution: fix in both modes — it reads uninitialized memory and past the string end (X-2 tier 1). Copying `%x` literally matches Magic wherever Magic's output is defined; check the sky130/gf180 why-strings for a `%` that is not `%d/%c/%a`. Approved 2026-11-10.

### Q-DRC-17: Last why-text cannot be ignored or searched
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: Why-texts are stored at indices 1..`DRCWhySize`, but `drc ignore text` and `drc find text` scan `1 .. DRCWhySize−1`, so the last registered why-text can never be ignored or matched.
- Evidence: drc/DRCtech.c:drcWhyCreate (368-398), commands/CmdCD.c:CmdDrc (4325, 4500)
- Options: replicate | compat (replicate under `magic-compat`; native includes the last index) | fix
- Recommendation: compat — an off-by-one that changes which errors `drc why`/`drc find` report (X-2 tier 2); native scans 1..`DRCWhySize`.
- Resolution: compat — an off-by-one that changes which errors `drc why`/`drc find` report (X-2 tier 2); native scans 1..`DRCWhySize`. Approved 2026-11-10.

### Q-DRC-18: exact_width surround only for same-plane layers
- Kind: quirk
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: `surround … exact_width` adds the maximum-surround rule (FORWARD/REVERSE, BOTHCORNERS|OUTSIDE) only for type pairs (A inner, B outer) that share a plane in the inner types' coincident planes, and then always adds the absence_illegal rules. Contact types are on their residue planes, so the usual via/metal case does get the maximum rule. Only pairs with no common plane (e.g. a well-plane outer layer around an active-plane inner layer) behave as plain `absence_illegal`, with no warning.
- Evidence: drc/DRCtech.c:drcSurround (3499-3528, 3532-3610), drc/DRCtech.c:CoincidentPlanes (113-125) (corrected 2026-11-10: the via/metal case was wrongly given as the different-plane case)
- Options: replicate | fix (add a cross-plane OUTSIDE rule)
- Recommendation: replicate in both modes plus a tech-file lint warning for cross-plane `exact_width` — a missing feature rather than a clear bug, and a native check would invent errors Magic never reports.
- Resolution: replicate in both modes plus a tech-file lint warning for cross-plane `exact_width` — a missing feature rather than a clear bug, and a native check would invent errors Magic never reports. Approved 2026-11-10.

### Q-DRC-19: maxwidth exclude types ignored for bend_illegal/both
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: For `maxwidth layers w bends exclude why`, `drcMaxwidth` stores the exclude types as the corner mask and their plane as the check plane. Only the `bend_ok` path (`drcCanonicalMaxwidth`/`MaxRectsExclude`) reads them; `bend_illegal` and `both` run `drcCheckMaxwidth`, which floods the edge plane with the rule's OK mask and ignores both fields. Exclude types on the layer's own plane only warn "cannot be checked".
- Evidence: drc/DRCtech.c:drcMaxwidth (1741-1800), drc/DRCextend.c:drcCheckMaxwidth (260-335), drc/DRCextend.c:drcCanonicalMaxwidth (595-612), drc/DRCbasic.c:drcTile (1067-1081)
- Options: replicate | fix (define exclude semantics for the flood-fill variants)
- Recommendation: replicate in both modes plus a tech-file lint warning when exclude types are combined with `bend_illegal`/`both`; the flood fill has no defined exclude semantics to fix toward.
- Resolution: replicate in both modes plus a tech-file lint warning when exclude types are combined with `bend_illegal`/`both`; the flood fill has no defined exclude semantics to fix toward. Approved 2026-11-10.

### Q-DRC-20: Euclidean corner distance limited to 4095 units
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: When a corner is extended under `drc euclidean on`, the corner distance is packed into the low 12 bits of `dCD_radial` (`0xfff & cdist`) and unpacked in `areaCheck`, so corner distances above 4095 internal units are taken modulo 4096. On vertical run-length edges cdist is the run-length (Q-DRC-7), so it is reached more easily there.
- Evidence: drc/DRCbasic.c:drcTile (1277-1281, 1669-1673), drc/DRCbasic.c:areaCheck (331)
- Options: replicate | compat (replicate under `magic-compat`; native uses the full width) | fix
- Recommendation: compat — a packing truncation that changes the error set (X-2 tier 2); golden DRC-EUC-5 covers it.
- Resolution: compat — a packing truncation that changes the error set (X-2 tier 2); golden DRC-EUC-5 covers it. Approved 2026-11-10.

### Q-DRC-21: Area/maxwidth/rectangle evaluated from the first left-edge segment only
- Kind: quirk
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: Area, maxwidth `bend_illegal`/`both` and rectangle rules run only while `firsttile` is set: on the lowest segment of the tile's left edge whose two types differ and that lies within the check rectangle. `firsttile` is cleared after that segment whether or not its edge pair carries the rule. A tile whose first such segment borders another type of the same layer set is never a start tile. Each region's leftmost tile always is one, so every region is evaluated, but only start tiles are reported (one error per start tile, clipped). Rectangle rules also require the left and bottom neighbors to be outside the rule's types.
- Evidence: drc/DRCbasic.c:drcTile (890-912, 980-987, 1067-1091, 1305), drc/DRCtech.c:drcArea (1541-1553)
- Options: replicate | fix (flag the whole region)
- Recommendation: replicate in both modes — this is how Magic chooses error tiles, not a defect, and the error geometry is observable.
- Resolution: replicate in both modes — this is how Magic chooses error tiles, not a defect, and the error geometry is observable. Approved 2026-11-10.

### Q-DRC-22: `drc list` without option
- Kind: bug
- Magic behavior: `drc list` with no further argument prints `No option given` but then looks up a missing argument (null pointer).
- Evidence: commands/CmdCD.c:4111-4135
- Options: fix (print help)
- Recommendation: fix.
- Resolution: fix — `drc list` with no option prints the help instead of dereferencing a missing argument (X-2 tier 1, 2026-11-07).

### Q-DRC-23: Check-plane operation type ignored
- Kind: quirk
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: `DRCCheckThis` always paints `checkpaint`; its `operation` argument is unused, so `checksubcell` is never painted although callers (parent propagation, cell placement, undo, plowing, timestamp fixes) pass it. Checking is unaffected (`drcCheckTile` searches all non-space check types), but `.mag` files only ever get `<< checkpaint >>` sections; a `checksubcell` section read from a file is still painted and checked.
- Evidence: drc/DRCcontin.c:DRCCheckThis (184-300, paint at 267-273), database/DBtpaint.c:235-237
- Options: replicate | fix (paint `checksubcell` when requested)
- Recommendation: replicate in both modes — it changes `.mag` contents only, and painting `checksubcell` would break `.mag` round-trip with Magic for no benefit.
- Resolution: replicate in both modes — it changes `.mag` contents only, and painting `checksubcell` would break `.mag` round-trip with Magic for no benefit. Approved 2026-11-10.

### Q-DRC-24: `drc *stepsize` accepts 0 and negative step sizes
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: `drc *stepsize d` stores `cmdScaleCoord(w, d, FALSE, TRUE, 1)` in `DRCStepSize` with no range check and prints `DRC step size is now 0 internal units (0 DRC units))`; an unparsable value also gives 0 (`Coordinate value cannot be parsed:  assuming 0`). The checker then divides by the step size: `drcCheckTile` computes the step square as `(LEFT(tile)/DRCStepSize) * DRCStepSize`, and `DRCInteractionCheck` (used by the background checker, `drc check`, `drc catchup`, `drc why`, `drc count`, `drc find`) does the same and steps its square loops by `DRCStepSize`. With 0 this is an integer division by zero: on x86-64 a SIGFPE, which Magic's signal handler turns into "Floating Point Exception" and `exit(12)`; on arm64 the quotient is 0, the square is empty, the check tile is never erased and the checker never finishes. A negative step makes the `for (… ; square.r_xbot < area->r_xtop; square.r_xbot += DRCStepSize)` loops run forever. The tech-file `stepsize` statement rejects the same values (`Step size must be a positive integer.`, then the default 16 × halo); only the command accepts them. A golden capture cannot record it (Magic crashes or hangs).
- Evidence: commands/CmdCD.c:CmdDrc (4570-4590), commands/CmdSubrs.c:cmdScaleCoord (123-150), drc/DRCcontin.c:drcCheckTile (667-672), drc/DRCsubcell.c:DRCInteractionCheck (789-799), drc/DRCtech.c:drcStepSize (3996-4006) and drcTechFinalStyle (4244-4256), utils/signals.c (586, exit at 621). Hefesto (M3 round 1): src/core/src/drc/commands/control_options.cpp:drc_stepsize stores the value as given and prints Magic's message, and src/core/src/drc/engine/context.cpp:current_step_size silently uses 1 for any value ≤ 0; the query then prints `DRC step size is 1 internal units`, not the value set, and a step of 1 makes every check one square per unit of area, a practical hang on real layouts. No unit test or golden covers 0 or a negative value.
- Options: replicate (impossible to do faithfully: crash or endless loop) | fix (reject values ≤ 0: print `Step size must be a positive integer.` as the tech file does and keep the current step size) | fix (0 restores the style's step size, as the tech file's 0 does; negative values rejected)
- Recommendation: fix — division by zero is undefined behavior and the result is a crash or a hang (X-2 tier 1, "crashes"). Reject any value ≤ 0 with `Step size must be a positive integer.`, keep the current step size and print nothing else, in both modes; drop the silent clamp to 1 in `current_step_size` (it can then assert). `drc style` and `tech load` already restore the style's step size, so no "reset" spelling is needed. Add a unit test for 0, a negative value and an unparsable value.
- Resolution: fix — division by zero is undefined behavior and the result is a crash or a hang (X-2 tier 1, "crashes"). Reject any value ≤ 0 with `Step size must be a positive integer.`, keep the current step size and print nothing else, in both modes; drop the silent clamp to 1 in `current_step_size` (it can then assert). `drc style` and `tech load` already restore the style's step size, so no "reset" spelling is needed. Add a unit test for 0, a negative value and an unparsable value. Approved 2026-11-10.


### Q-DRC-25: A grid rescale or a large CIF scalefactor can leave a step size of 0
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: `DRCTechScale` rescales `DRCStepSize` and the style's step size with `*= scaled; /= scalen` (rounding down), so a rescale that makes the internal grid coarser by more than the step size leaves 0; the default step of a style without rules (`DRCTechHalo == 0`) is `6400 / CIFCurStyle->cs_scaleFactor`, which is 0 for a scale factor above 6400. A step size of 0 then divides by zero in `drcCheckTile` and `DRCInteractionCheck`, as in Q-DRC-24.
- Evidence: drc/DRCtech.c:DRCTechScale (4586-4593), drc/DRCtech.c:drcTechFinalStyle (4244-4256), drc/DRCcontin.c:drcCheckTile (667-672), drc/DRCsubcell.c:DRCInteractionCheck (789-799). Hefesto: src/core/src/drc/rules/style.cpp (rescale of the style step and of the `*stepsize` override), src/core/src/drc/rules/cif_catalog.cpp:default_step_size; src/core/src/drc/engine/context.cpp:current_step_size keeps a guard that uses 1 for any value ≤ 0 (found while fixing Q-DRC-24, 2026-11-10).
- Options: replicate (impossible: crash or hang) | fix (a rescaled or default step size never goes below 1) | fix (a step size that would be 0 falls back to 16 × halo, or 64 without a halo)
- Recommendation: fix — never let a rescaled or default step size go below 1 (X-2 tier 1, "crashes"), clamped where it is computed (rescale and `default_step_size`) rather than at use, in both modes; `current_step_size` can then assert. No golden case (Magic crashes).
- Resolution: fix — never let a rescaled or default step size go below 1 (X-2 tier 1, "crashes"), clamped where it is computed (the style and `drc *stepsize` rescale in src/core/src/drc/rules/style.cpp and `CifCatalog::default_step_size`), in both modes; `current_step_size` asserts a positive step. No golden case (Magic crashes). Approved 2026-11-10.

### Q-DRC-26: DRC-PDK-3 uses IHP's `cap_cmim` generator (no `cap_cmomf` in IHP-Open-PDK)
- Kind: other
- Status: approved (recorded 2026-11-12 from m4-scripting task 12.3; approved 2026-11-12)
- Magic behavior: n/a. The M4 plan (m4-scripting proposal and design, task 12.3) named an IHP `cap_cmomf` generator for DRC-PDK-3, but IHP-Open-PDK 5e6d592e has no such generator: its Tcl capacitor generators are `cap_cmim` and `cap_rfcmim` (MiM), and `cap_cmomf` exists only as an OSDI compact model (Q-SIMADV-5).
- Evidence: IHP-Open-PDK 5e6d592e `ihp-sg13g2/libs.tech/magic/ihp-sg13g2.tcl`; tests/golden/DRC/cases/DRC-PDK-3 (golden-capture run 43214374466)
- Options: use `cap_cmim` (two default MiM capacitors 0.4 µm apart, MIM.b asks 0.6 µm) | keep the box/paint stand-in until IHP ships a MOM generator
- Recommendation: `cap_cmim`: it exercises the PDK's own generator and rules.
- Resolution: other (user decision 2026-11-12) — DRC-PDK-3 places two `sg13g2::cap_cmim` capacitors; the case is captured, committed and replays. m4-scripting task 12.3 and its proposal and design name `cap_cmim`. Approved 2026-11-12.
