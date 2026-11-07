# Open Questions — DRC

Items found while reverse-engineering Magic 8.3.694 (fc5daa9e) DRC for the capabilities `drc-rule-language`, `drc-rule-semantics`, `drc-engine`, `drc-cif-rules` and `drc-commands`. Unless decided otherwise, the specs require Hefesto to replicate the Magic behavior described here.

### Q-DRC-1: Scale remainder stored in one byte
- Kind: bug
- Magic behavior: When a rule distance is scaled by the DRC scalefactor N, the remainder `dist % N` is stored in an unsigned char and used to restore the original value when the internal grid is later rescaled. For N > 256 the remainder wraps. For area rules the stored remainder is `(area % N²) / N`, so `area % N` is lost on restore. Results are only wrong after a grid rescale (`scalegrid`, reading finer-grid GDS/CIF/.mag).
- Evidence: drc/DRCtech.c:4068-4204
- Options: replicate | fix (keep exact rationals; recompute from tech-file values on every rescale)
- Recommendation: fix (store the original tech-file values); differences only appear after rescales with large scalefactors — add a golden case to confirm.

### Q-DRC-2: Mask-layer rule halo scaled twice
- Kind: bug
- Magic behavior: `cifwidth/cifspacing/cifarea/cifmaxwidth` return a halo already converted to internal units (ceil(dist/σ)); the final halo computation then multiplies all contributions by the DRC scalefactor D/N as if they were tech-file units. With `scalefactor 10` a CIF-only halo is divided by 10 and CIF errors near check boundaries may be missed.
- Evidence: drc/DRCcif.c:234, drc/DRCtech.c:4228-4232
- Options: replicate | fix (exclude CIF contributions from the D/N scaling)
- Recommendation: replicate (halo affects which errors are regenerated, i.e. observable error planes).

### Q-DRC-3: Multiplane spacing compiles only one plane pair
- Kind: bug
- Magic behavior: When a spacing type list spans several planes (types partitioned across planes), the compiler loops over plane pairs but returns after compiling the first (plane1, plane2) combination; other combinations get no rules.
- Evidence: drc/DRCtech.c:2836-2852
- Options: replicate | fix (compile all combinations)
- Recommendation: replicate for compatibility; log a tech-file lint warning in Hefesto.

### Q-DRC-4: `angles 45` reads past the token
- Kind: bug
- Magic behavior: After parsing the number, the code compares `endptr + 1` with `"only"`. For the token `45`, `endptr` points at the terminating NUL so the comparison reads one byte past the string (undefined behavior; in practice usually not "only").
- Evidence: drc/DRCtech.c:1858-1861
- Options: replicate (treat `45` as 45-with-90) | fix
- Recommendation: fix: `45` means 45+90 degrees; `45` followed by exactly one separator character and `only` means 45-only.
- Resolution: fix — the token is never read past its end: `45` means 45 and 90 degrees, `45` followed by one separator and `only` means 45 only (X-2 tier 1, 2026-11-07).

### Q-DRC-5: Covered-rule elimination conditions
- Kind: bug
- Magic behavior: A rule X is removed if a later rule Y "covers" it. The code skips Y when Y has BOTHCORNERS and X does not — the opposite of the documented intent ("if A is BOTHCORNERS then B must be, too") — so a BOTHCORNERS rule can be removed in favor of a single-corner rule, losing corner checks. Edge plane, MANHATTAN, RUNLENGTH and DERIVED flags are not compared.
- Evidence: drc/DRCtech.c:4352-4420
- Options: replicate | fix (require Y BOTHCORNERS when X is; compare edge plane and the other flags)
- Recommendation: replicate (it determines error geometry and why-text); provide golden cases DRC-OPT-*.

### Q-DRC-6: No edge-end test on horizontal edges
- Kind: quirk
- Magic behavior: For vertical edges a corner extension is applied only if the edge actually ends (the type pair changes beyond the end). Horizontal edges apply the corner extension based only on the corner-type sample, even if the same edge continues in the next tile.
- Evidence: drc/DRCbasic.c:1140-1180 vs drc/DRCbasic.c:1570-1650
- Options: replicate | fix
- Recommendation: replicate.

### Q-DRC-7: Run-length rules asymmetric between edge orientations
- Kind: bug
- Magic behavior: For widespacing rules with a run-length, the run-length value is stored in cdist. Horizontal edges use cdist = 0, but vertical edges use cdist = runlength both for expanding the triggered range and for corner extensions, so vertical-edge checks cover a larger region than horizontal ones.
- Evidence: drc/DRCbasic.c:967, drc/DRCbasic.c:1427
- Options: replicate | fix (use 0, or the spacing distance, on both)
- Recommendation: replicate; confirm with golden case DRC-WS-RL-V/H.

### Q-DRC-8: Area rule aborted at exact check-rectangle boundary
- Kind: quirk
- Magic behavior: The area flood fill gives up (no error) when any visited tile has an edge exactly equal to the check rectangle's edge. In flattened interaction buffers, geometry is clipped to the buffer area, so regions reaching the buffer edge are never flagged there; in unflattened checks tiles are not clipped and the fill runs over the whole region.
- Evidence: drc/DRCextend.c:150-158
- Options: replicate | fix (bound the search by area instead)
- Recommendation: replicate.

### Q-DRC-9: Off-grid snapping toward zero
- Kind: quirk
- Magic behavior: The off-grid error rectangle is the clipped edge expanded to `trunc(v/p)·p`, i.e. toward zero rather than floor; for negative coordinates the error extends toward the origin. A horizontal edge on a grid line is never flagged even if its endpoints are off-grid (they are flagged by the vertical edges). The error is not re-clipped after expansion.
- Evidence: drc/DRCextend.c:87-125
- Options: replicate | fix (floor)
- Recommendation: replicate.

### Q-DRC-10: Diagonal-edge checks may never report
- Kind: bug
- Magic behavior: For split tiles the offset-triangle search excludes the initiating tile and then accepts a candidate only if a search over the initiating tile's own rectangle finds the candidate tile. Since no other tile can overlap that rectangle, the acceptance test appears to always fail, so non-Manhattan spacing/width checks across diagonals would never report errors (orthogonal sides of split tiles are still checked).
- Evidence: drc/DRCbasic.c:492-545, drc/DRCbasic.c:731-850
- Options: replicate (no diagonal errors) | fix (implement the intended Euclidean check)
- Recommendation: verify with golden cases DRC-NM-1..3 before deciding; spec currently requires replicating observed output.

### Q-DRC-11: Exception/exemption inside test boundaries
- Kind: ambiguity
- Magic behavior: The "edge inside hint area" test searches the hint plane with a zero-width rectangle. For vertical edges this includes hints whose left boundary equals the edge x but excludes those whose right boundary does. For horizontal edges the search starts from the row just below the edge and only proceeds if the tile there spans across the edge, so edges on a hint's bottom or top boundary are generally treated as outside.
- Evidence: drc/DRCbasic.c:935-950, drc/DRCbasic.c:1395-1410, database/DBtiles.c:520-600
- Options: replicate | fix (closed-interval overlap test)
- Recommendation: replicate; capture golden cases DRC-EXC-3/4.

### Q-DRC-12: Background checker and scripting results without Tcl
- Kind: platform
- Magic behavior: The checker runs from the Tcl idle loop (or before blocking on terminal input), processes Tcl events between squares, and returns list results via the Tcl interpreter (`drc list ...`). Under X-3, Hefesto embeds sandboxed Tcl on every platform, including iPadOS, so the scripting-results part is settled (capability `scripting-api`); only the background scheduling remains open. iPadOS has no fork/exec.
- Evidence: drc/DRCcontin.c:481-560, commands/CmdCD.c:4197-4274
- Options: platform alternative (background worker thread or run-loop idle source with cancellation on edit; scripting API returning native list objects)
- Recommendation: implement cancellation-safe background checking on a worker with a snapshot or edit-generation counter; expose list results through Hefesto's scripting layer with identical structure.

### Q-DRC-13: Mask error coordinate mapping truncates toward zero
- Kind: quirk
- Magic behavior: Mask-layer error rectangles are converted to internal units with C integer division (toward zero), so negative coordinates round up and positive round down; zero-width results are widened by one unit away from zero. Area/maxwidth errors are not widened.
- Evidence: drc/DRCcif.c:1052-1068
- Options: replicate | fix (floor/ceil outward)
- Recommendation: replicate.

### Q-DRC-14: `drc count` prints a zero total
- Kind: bug
- Magic behavior: Without the `total` argument and with background DRC not off, after the per-cell lines the command prints `Total DRC errors found: 0` regardless of the counts.
- Evidence: commands/CmdCD.c:4253-4273
- Options: replicate | fix (print the real total or nothing)
- Recommendation: fix in interactive output (informative text), keep list results identical.

### Q-DRC-15: First `drc find` shows error #2
- Kind: bug
- Magic behavior: The persistent index starts at 1 and is incremented before use, so the first argument-less `drc find` in a session jumps to the second error (wrapping to #1 if there is only one).
- Evidence: commands/CmdCD.c:4058, commands/CmdCD.c:4304-4314
- Options: replicate | fix (start at 0)
- Recommendation: fix (start at error #1); behavior is interactive only.

### Q-DRC-16: Unknown `%` escape in why-text
- Kind: bug
- Magic behavior: For `%` followed by a character other than d, c, a, the output pointer is advanced two bytes without copying, leaving whatever bytes of the original string happen to be at that offset (correct only if no earlier substitution changed the length).
- Evidence: drc/DRCmain.c:250-256
- Options: replicate | fix (copy `%x` literally)
- Recommendation: fix (copy literally); differs from Magic only after an earlier substitution in the same string.

### Q-DRC-17: Last why-text cannot be ignored or searched
- Kind: bug
- Magic behavior: `drc ignore text` and `drc find text` scan why indices `1 .. size−1`, excluding the last registered why-text.
- Evidence: commands/CmdCD.c:4326, commands/CmdCD.c:4501
- Options: replicate | fix (include the last index)
- Recommendation: fix.

### Q-DRC-18: exact_width surround only for same-plane layers
- Kind: quirk
- Magic behavior: `surround ... exact_width` adds the maximum-surround (OUTSIDE) rules only for edge pairs where the inner and surrounding types share a plane; for layers on different planes (the usual via/metal case) it behaves as `absence_illegal`.
- Evidence: drc/DRCtech.c:3530-3565
- Options: replicate | fix
- Recommendation: replicate.

### Q-DRC-19: maxwidth exclude types ignored for bend_illegal/both
- Kind: bug
- Magic behavior: Exclude types given in `maxwidth layers w bends exclude why` are honored only by the `bend_ok` (canonical) algorithm; `bend_illegal` and `both` ignore them (and the rule's check plane is set to the exclude plane).
- Evidence: drc/DRCtech.c:1795-1802, drc/DRCextend.c:260-300
- Options: replicate | fix
- Recommendation: replicate.

### Q-DRC-20: Euclidean corner distance limited to 4095 units
- Kind: bug
- Magic behavior: The corner distance used by the Euclidean exemption is packed into 12 bits; corner distances above 4095 internal units are taken modulo 4096.
- Evidence: drc/DRCbasic.c:1300-1304, drc/DRCbasic.c:331
- Options: replicate | fix (full width)
- Recommendation: fix only if golden cases show no PDK depends on it; otherwise replicate.

### Q-DRC-21: Area/maxwidth/rectangle evaluated from the first left-edge segment only
- Kind: quirk
- Magic behavior: These per-region rules run only from the lowest segment of a tile's left edge whose two types differ. If that segment's edge pair does not carry the rule (e.g. left neighbor is another type of the same layer set), the tile is never a start tile, so only some tiles of an offending region are flagged.
- Evidence: drc/DRCbasic.c:900-912, drc/DRCbasic.c:980-987, drc/DRCbasic.c:1330-1334
- Options: replicate | fix (flag the whole region)
- Recommendation: replicate (error geometry is observable).

### Q-DRC-22: `drc list` without option
- Kind: bug
- Magic behavior: `drc list` with no further argument prints `No option given` but then looks up a missing argument (null pointer).
- Evidence: commands/CmdCD.c:4111-4135
- Options: fix (print help)
- Recommendation: fix.
- Resolution: fix — `drc list` with no option prints the help instead of dereferencing a missing argument (X-2 tier 1, 2026-11-07).

### Q-DRC-23: Check-plane operation type ignored
- Kind: quirk
- Magic behavior: Marking always paints `checkpaint`; `checksubcell` is never painted although callers request it for subcell changes. Results are unaffected.
- Evidence: drc/DRCcontin.c:280-286
- Options: replicate | fix
- Recommendation: replicate (it affects `.mag` file contents of the check plane).
