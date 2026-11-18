# Open Questions — MODEL (layout database)

Capabilities: `layout-geometry-model`, `layout-paint-semantics`, `layout-hierarchy`, `layout-labels-ports`, `layout-properties`, `layout-connectivity`, `layout-undo`. Magic reference: `fc5daa9e`.

### Q-MODEL-1: Label rotation sense under transforms
- Kind: ambiguity
- Magic behavior: transforming a label rotation through the clockwise-90 transform `(0,1,-1,0)` adds +90 degrees, while label text boxes are rotated counter-clockwise by `lab_rotate`. A clockwise placement therefore appears to rotate text counter-clockwise.
- Evidence: utils/geometry.c:154-189, database/DBlabel.c:1440-1460, commands/CmdCD.c:2279-2345
- Options: replicate | fix (subtract for clockwise) | verify rendering first
- Recommendation: capture golden output (MODEL-G-012) before deciding; replicate for file data.
- Resolution: compat — label rotations transform as in Magic (+90 for the clockwise-90 transform), because the values are written to files; golden case MODEL-G-012 confirms the rendering; with `magic-compat` OFF, a clockwise transform subtracts 90 (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-2: Transformed angle can be 360
- Kind: bug
- Magic behavior: the angle sum is reduced only when `> 360`, so 270 rotated by +90 yields 360 instead of 0 (non-flipped transforms).
- Evidence: utils/geometry.c:179
- Options: replicate | fix (reduce `>= 360`)
- Recommendation: fix internally but normalize 360 -> 0 only where it is not written to files; verify whether `flabel` output can contain 360.
- Resolution: compat — a sum of exactly 360 is kept, as in Magic, because it can reach `flabel` output; with `magic-compat` OFF, the sum is reduced when it is 360 or more (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-3: Asymmetric rounding of non-Manhattan subdivision
- Kind: quirk
- Magic behavior: for `/` diagonals crossings are rounded half-up; for `\` the code computes the rounding term on a negative value (C `%` and left shift of a negative number, the latter undefined behaviour) which always yields 0, so crossings are truncated.
- Evidence: database/DBpaint.c:1657-1745
- Options: replicate | fix (symmetric round-half-up)
- Recommendation: replicate (geometry is written to files); golden cases MODEL-G-020/021.
- Resolution: compat — `/` crossings round half-up and `\` crossings truncate, as in Magic, because the geometry is written to files; Hefesto computes the truncation without Magic's undefined shift; with `magic-compat` OFF, both diagonals round half-up (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-4: Opposite-diagonal overlap on 1-unit areas fills the rectangle
- Kind: quirk
- Magic behavior: painting a triangle over a triangle with the opposite diagonal quarters the area recursively; at width or height 1 the whole rectangle is painted, over-filling by half a unit square.
- Evidence: database/DBpaint.c:1572-1590
- Options: replicate | fix
- Recommendation: replicate.
- Resolution: n/a — the union of two opposite-diagonal triangles in a 1-unit square is not a single split tile, so Magic's fill is a limit of the representation rather than a defect; the spec keeps it (D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-5: Non-unique representation of non-Manhattan geometry
- Kind: ambiguity
- Magic behavior: split tiles are merged only under specific collinearity conditions; the same region can end up as one large triangle or several smaller ones plus rectangles depending on painting history, which changes `.mag` `tri` lines.
- Evidence: database/DBpaint.c:1018-1320, 788-1012
- Options: replicate history-dependent behaviour | canonicalize triangles on output
- Recommendation: replicate operations exactly; accept history dependence; golden tests compare after identical command sequences.
- Resolution: compat — tile order: X-1 requires Magic's decomposition and fracturing/merging rules to be reproduced, so the same command history gives the same `tri` records; golden tests compare after identical command sequences (X-1, 2026-11-07).

### Q-MODEL-6: Font-label bounding box rounding mask
- Kind: bug
- Magic behavior: when converting 1/8 units to database units the code tests `(v & 0x7f)` instead of `(v & 0x7)`, so exact multiples of 8 that are not multiples of 128 are still expanded by one unit.
- Evidence: database/DBlabel.c:1484-1499
- Options: replicate | fix (`& 0x7`)
- Recommendation: replicate (it affects extended bounding boxes, which appear in `.mag` and LEF outputs); verify.
- Resolution: compat — the `& 0x7f` test is kept, as in Magic, because extended bounding boxes reach `.mag` and LEF output; with `magic-compat` OFF, the test is `& 0x7` (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-7: Inconsistent array subscript order
- Kind: quirk
- Magic behavior: element names print as `[y,x]` and parse `[i1][i2]` as `[y][x]`, but in-place flattening prefixes labels with `[x][y]`.
- Evidence: database/DBcellbox.c:99-104, database/DBlabel2.c:410-440, database/DBcellcopy.c:650-662
- Options: replicate | fix (use `[y][x]`)
- Recommendation: replicate; document.
- Resolution: compat — in-place flattening prefixes labels with `[x][y]`, as in Magic; with `magic-compat` OFF, it uses `[y][x]`, like element names (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-8: Default identifier base differs between link and read
- Kind: quirk
- Magic behavior: linking uses the part of the child name after the last `/`; generating unique ids on read uses the full child name, producing ids containing `/`.
- Evidence: database/DBcellname.c:2425-2440, 2758-2770
- Options: replicate | fix (always use basename)
- Recommendation: fix (ids with `/` break hierarchical names); keep reading such ids.
- Resolution: compat — ids generated on read use the full child name, as in Magic, because they appear in written files and netlists; such ids are always read back; with `magic-compat` OFF, the part after the last `/` is used, as when linking (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-9: Natural-sort comparator not antisymmetric
- Kind: bug
- Magic behavior: `strcmpbynum` returns 1 both for (prefix, longer) and (longer, prefix); qsort order of such pairs is undefined.
- Evidence: database/DBcellname.c:676-713
- Options: replicate (impossible deterministically) | fix (shorter first)
- Recommendation: fix with shorter-first; affects port renumbering, `.mag` use order and property order (cell-* owners should align).
- Resolution: fix — same comparator as Q-CELL-12: the total natural order of `cell-file-format`, with a proper prefix sorting first (D-9, 2026-11-07).

### Q-MODEL-10: Duplicate detection in port renumbering is case-sensitive
- Kind: quirk
- Magic behavior: the sort is case-insensitive but index sharing uses `strcmp`, so `vdd` and `VDD` adjacent get different indices while equal texts separated by a case variant do not share.
- Evidence: commands/CmdLQ.c:2100-2112
- Options: replicate | fix
- Recommendation: replicate.
- Resolution: compat — index sharing compares case-sensitively, as in Magic (it changes port indices); with `magic-compat` OFF, it compares case-insensitively, like the sort (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-11: Label size adjustment threshold
- Kind: doc-mismatch
- Magic behavior: the comment says a label is shrunk only if material covers more than half of it; the code shrinks whenever any material remains, clipping against other tiles in enumeration order (not choosing the largest remaining rectangle).
- Evidence: database/DBlabel.c:672-718
- Options: replicate code | implement comment
- Recommendation: replicate code.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-MODEL-12: Undo of paint does not mark the cell modified
- Kind: bug
- Magic behavior: after saving a cell and undoing an earlier paint step, the cell geometry differs from disk but the modified flag is not set by paint replay.
- Evidence: database/DBundo.c:306-420
- Options: replicate | fix (mark modified on any replay)
- Recommendation: fix; verify with MODEL-G-040.
- Resolution: fix — the cell differs from disk while its modified flag is clear, so the change can be lost on exit without a prompt (silent data loss); undo and redo replays mark the affected cells modified (X-2 tier 1, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-13: Lock/unlock does not mark the parent modified
- Kind: bug
- Magic behavior: the lock flag is persisted in `.mag` but changing it does not set the modified flag, so the change can be lost on exit without a prompt.
- Evidence: database/DBcellname.c:1260-1300, database/DBio.c:6247
- Options: replicate | fix
- Recommendation: fix.
- Resolution: fix — the lock flag is saved in `.mag`, so an unflagged change can be lost on exit without a prompt (silent data loss); locking or unlocking marks the parent modified (X-2 tier 1, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-14: Property changes are not undoable
- Kind: quirk
- Magic behavior: no undo events for properties.
- Evidence: database/DBprop.c:58-125
- Options: replicate | add undo
- Recommendation: add undo in Hefesto (behavioral improvement, no file impact).
- Resolution: compat — decided by D-10 (2026-11-07). Not recorded with `magic-compat` ON (Magic's behavior); recorded and undoable with `magic-compat` OFF. Hefesto compound operations (agent proposals, `sim` edits) always record them (capability `layout-undo`).

### Q-MODEL-15: Renamed cell itself not flagged modified
- Kind: ambiguity
- Magic behavior: `DBCellRenameDef` marks parents modified but not the renamed definition (the command layer may handle saving under the new name).
- Evidence: database/DBcellname.c:2122-2155
- Options: replicate | fix
- Recommendation: verify the `cellname rename` command behaviour (cmd owner) before deciding.
- Resolution: fix — `cellname rename` leaves the renamed cell unflagged, so quitting loses the cell under its new name while saved parents reference it (silent data loss); the renamed definition is marked modified (X-2 tier 1, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-16: Mask-hint plane lookup uses the wrong key on CIF/GDS read
- Kind: bug
- Magic behavior: when adding hints from a cifinput templayer, the existing plane is looked up under the bare layer name instead of `MASKHINTS_<layer>`, so a second batch replaces rather than extends the first.
- Evidence: cif/CIFrdcl.c:740-755
- Options: replicate | fix
- Recommendation: fix; coordinate with `mask-hints` owner.

### Q-MODEL-17: Flat connectivity cross-plane step picks a single tile
- Kind: bug
- Magic behavior: from a contact tile the flat search looks in a 1-expanded area on each other plane and continues from only the first matching tile, possibly a diagonal neighbour, possibly skipping the contact's own image.
- Evidence: database/DBconnect.c:615-650 (dbSrConnectFunc), database/DBconnect.c:348-358 (dbcFindTileFunc); callers lef/defWrite.c:780,829,2631, netmenu/NMwiring.c:174,240,555,803
- Options: replicate | fix (continue from the contact image and its edge neighbours)
- Recommendation: fix; affects only DEF/netlist-menu flat searches.
- Resolution: replicate in magic-compat, fix in native — evidence corrected (the code is now at database/DBconnect.c:615-650, and its callers include the DEF writer, lef/defWrite.c:780,829,2631, as well as the netlist menu), so the result reaches written DEF; on DRC-clean layouts the single-tile step gives the same nets as a full search, because a tile within one unit that is not edge-adjacent to the contact image must touch it only at a corner, which spacing rules forbid, so only DRC-dirty layouts differ (verified against database/DBconnect.c:dbSrConnectFunc and database/DBconnect.c:dbcFindTileFunc on 2026-11-09; approved 2026-11-09).

### Q-MODEL-18: Port uses reset/scan/tieoff are not persisted
- Kind: bug
- Magic behavior: the data model has `reset`, `scan`, `tieoff` uses but the `.mag` writer emits nothing for them.
- Evidence: database/database.h.in:299-301, database/DBio.c:4845-4862
- Options: replicate (lose on save) | fix (write names; reader must accept them)
- Recommendation: decide with cell-* owner; default replicate for byte-identical output.
- Resolution: fix — data loss (named in X-2): reset, scan and tieoff are written and read back, as decided for Q-CELL-6 (X-2 tier 1, 2026-11-07).

### Q-MODEL-19: Bounding box includes DRC error and hint planes
- Kind: quirk
- Magic behavior: bounding-box computation scans every plane >= 1 except the DRC check plane, so DRC error tiles and router hint paint extend the box.
- Evidence: database/DBcellbox.c:575-585
- Options: replicate | fix (paint planes only)
- Recommendation: replicate (box appears in `.mag` and drives array/overlap computations).
- Resolution: compat — DRC error and hint planes extend the bounding box, as in Magic, because the box is written to `.mag` and drives array and overlap computations; with `magic-compat` OFF, only paint planes count (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-20: Flattened labels keep untransformed font attributes
- Kind: bug
- Magic behavior: flatten copies outline-font rotation and offset without transforming them (marked "to-do" in the source).
- Evidence: commands/CmdFI.c:1990-2005
- Options: replicate | fix (transform like hierarchical copies do)
- Recommendation: fix; record as intentional difference.
- Resolution: compat — flatten copies font rotation and offset untransformed, as in Magic, because the values are written to the flattened cell; with `magic-compat` OFF, they are transformed as hierarchical copies do (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-21: Undo history limit
- Kind: ambiguity
- Magic behavior: fixed 1000 steps.
- Evidence: utils/undo.c:58-59,811-836
- Options: replicate | configurable larger limit
- Recommendation: configurable, default 1000.
- Resolution: other — a configurable limit with Magic's 1000-step default, never fewer; no script can observe the cap beyond running out of undo steps, so a larger user setting changes no output (verified against utils/undo.c:undoFreeHead on 2026-11-09; approved 2026-11-09).

### Q-MODEL-22: Erase-valid ignores stacked types in the mask
- Kind: quirk
- Magic behavior: paint-valid decomposes stacked types into residues; erase-valid does not, so erasing a stacked type name through the user path does nothing.
- Evidence: database/DBpaint2.c:272-277,380-417
- Options: replicate | fix
- Recommendation: replicate until a command-level test shows the effect.
- Resolution: compat — erase-valid ignores stacked types in its mask, as in Magic; with `magic-compat` OFF, it decomposes them into residues, like paint-valid (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-23: Paint/erase always mark modified
- Kind: quirk
- Magic behavior: the modified flag is set before checking whether anything changes.
- Evidence: database/DBpaint2.c:65,177
- Options: replicate | fix
- Recommendation: replicate (users rely on the prompt-on-exit behaviour).
- Resolution: n/a — intended behaviour: every paint and erase marks the cell modified, and users rely on the resulting prompt on exit; the spec keeps it (D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-24: Geometry beyond the plane limits
- Kind: ambiguity
- Magic behavior: no clipping to `±(2^30-6)`; behaviour is undefined (tile plane corruption, overflow in transforms).
- Evidence: tiles/tile.h:149-165, tiles/tile.c:67
- Options: clip | reject with error
- Recommendation: reject with an error message.
- Resolution: fix — geometry beyond ±(2^30−6) corrupts the tile plane and overflows transforms (undefined behaviour); it is rejected with an error (X-2 tier 1, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-25: Property listing order
- Kind: ambiguity
- Magic behavior: `property` with no arguments lists properties in hash-table order (implementation dependent); only `.mag` output is sorted.
- Evidence: database/DBprop.c:288-321, commands/CmdLQ.c:2386-2397
- Options: replicate hash order (impossible) | natural-sort order
- Recommendation: natural-sort order.

### Q-MODEL-26: Label removal by content or re-justification does not mark modified
- Kind: quirk
- Magic behavior: low-level label erase-by-content, erase-by-reference and re-orientation do not set the modified flag (callers usually do).
- Evidence: database/DBlabel.c:431-580
- Options: replicate | fix (always mark)
- Recommendation: fix at the data-model level; harmless for files.

### Q-MODEL-27: Interruptible paint and search
- Kind: platform
- Magic behavior: long operations poll an interrupt flag (signals / Tcl events) and may stop half-way, leaving partial paint.
- Evidence: database/DBpaint.c:282-286, database/DBtiles.c:556-558
- Options: replicate | atomic operations with cancellation before commit
- Recommendation: atomic operations; on iPadOS there are no signals, use cooperative cancellation.
- Resolution: platform — paint and search are atomic, with cooperative cancellation before commit on every platform (no signals on iPadOS); an interrupted operation leaves no partial paint (X-4, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-28: Flatten in place is not undoable
- Kind: quirk
- Magic behavior: undo recording is disabled for the whole operation, including deleting the flattened use, so neither the added paint nor the deletion can be undone.
- Evidence: commands/CmdFI.c:2116-2140
- Options: replicate | make undoable
- Recommendation: make undoable (behavioral improvement).
- Resolution: compat — decided by D-10 (2026-11-07). Not recorded with `magic-compat` ON; recorded as one undo step with `magic-compat` OFF (capability `layout-undo`).

### Q-MODEL-29: Erasing "space" is a no-op
- Kind: bug
- Magic behavior: erase with operand space uses the paint table of space, all of whose entries are no-ops, contrary to the comment "erasing space is the same as erasing everything".
- Evidence: database/DBpaint2.c:179-192, database/DBtpaint.c:199-218
- Options: replicate | fix (clear all planes)
- Recommendation: replicate at the data-model level; commands use explicit masks.
- Resolution: compat — erasing space is a no-op at the data-model level, as in Magic; commands use explicit masks; with `magic-compat` OFF, erasing space clears every type in the area (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MODEL-30: `undo 0` enables and `redo 0` disables undo recording
- Kind: quirk
- Magic behavior: a count of 0 is not a no-op. `undo 0` resumes recording by one nesting level and `redo 0` suspends it by one level, with no message. This is undocumented in the help text (`undo [count]`, `redo [count]`).
- Evidence: windows/windCmdSZ.c:686-691, windows/windCmdNR.c:382-385
- Options: replicate | fix (treat 0 as a no-op; keep `undo enable|disable` only)
- Recommendation: replicate behind `magic-compat` (scripts may rely on `redo 0`/`undo 0` as a disable/enable idiom); with the setting off, treat 0 as a no-op.
- Resolution: compat — decided by D-11 (2026-11-07). With `magic-compat` ON, `undo 0` resumes and `redo 0` suspends recording, silently; with it OFF, both are no-ops (capability `layout-undo`).


### Q-MODEL-31: In-place flatten label renaming overflows its buffer and leaves labels unprefixed
- Kind: bug
- Magic behavior: `DBFlattenInPlace` allocates `strlen(text) + strlen(id) + 2` bytes for the new label text, then `sprintf`s `<id>[x][y]/<text>` (or `<id>[i]/<text>`) into it, a heap buffer overflow of up to the length of the index suffix plus its terminator. In the captured run (golden case MODEL-G-035, a 2x2 array flattened with `flatten -doinplace`) the four copied labels keep the plain text `out`, contrary to the documented `<id>[x][y]/<text>` naming.
- Evidence: database/DBcellcopy.c:640-670, commands/CmdFI.c:2112-2140
- Options: replicate | fix (size the buffer for the prefix; apply the documented names)
- Recommendation: fix. The documented names are what the code intends; replicating a result that depends on heap corruption is not possible reliably.
- Resolution: fix — undefined behavior (X-2 tier 1, "buffer overflows"): labels are renamed `<id>[x][y]/<text>` as `layout-hierarchy` "Flattening an instance in place" specifies (recorded 2026-11-09 from the MODEL-G-035 capture).

### Q-MODEL-32: A parent keeps its old bounding box when its child was shown in a window before being placed
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: When a definition's box changes, `dbReComputeBboxFunc` walks `cd_parents` (most recent use first) and recomputes a parent only when the next use has a different parent; the pending parent is recomputed after the loop by `if ((last != NULL) && (parent != NULL)) (*recurseProc)(parent)`, where `parent` is the parent of the *last* use in the list, not `last`. `DBWloadWindow` gives every window load a root use with no parent (`DBCellNewUse(def, NULL)`, "Topmost cell in the window") that is prepended to `cd_parents` and never freed. So when the child was loaded in a window before every placement that still uses it (e.g. `load child`, then `load top`, then edit the child), the oldest use, last in the list, is the window's root use, the final recursion is skipped, and the last pending parent keeps its old bounding box (the use box itself is updated) until something else recomputes that parent. With a placement older than the window load the parent follows normally.
- Evidence: database/DBcellbox.c:dbReComputeBboxFunc (757-775, final recursion at 775), database/DBcellname.c:DBCellNewUse (2066-2097, prepend to `cd_parents`), dbwind/DBWprocs.c:DBWloadWindow (545-547). Hefesto: src/core/src/db/hierarchy.cpp:recompute_bbox and window_use_is_oldest (433-488), src/core/src/view/window.cpp:250 (`note_window_root`); regression test tests/unit/db/hierarchy_test.cpp "a parent keeps its box when the child was shown in a window before its placement" (both modes) found by goldens DRC-ENG-1 and CELL-TS-02 (M3 round 1). The `layout-hierarchy` requirement "Bounding box and extended bounding box" says only that the computation "propagates upward" and does not state this case.
- Options: replicate | compat (replicate under `magic-compat`; with it off every parent is recomputed, as round 1 implements) | fix
- Recommendation: compat — the stale box is an output value: it bounds the parent's DRC check areas (`<< checkpaint >>` in saved `.mag`, golden DRC-ENG-1), the deferred timestamp processing (CELL-TS-02), the use boxes of the parent's own parents and the boxes written by LEF/GDS, so it is a value-changing quirk (X-2 tier 2), not a crash or data loss; with `magic-compat` OFF the parent is always recomputed. Round 1 already implements this; the decision is to confirm it and add the case to `layout-hierarchy`.
- Resolution: compat — the stale box is an output value: it bounds the parent's DRC check areas (`<< checkpaint >>` in saved `.mag`, golden DRC-ENG-1), the deferred timestamp processing (CELL-TS-02), the use boxes of the parent's own parents and the boxes written by LEF/GDS, so it is a value-changing quirk (X-2 tier 2), not a crash or data loss; with `magic-compat` OFF the parent is always recomputed. Round 1 already implements this; the decision is to confirm it and add the case to `layout-hierarchy`. Approved 2026-11-10.

### Q-MODEL-33: A restricting flatten copies nothing when the flattened cell itself fails the descend test
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: `flatten` with a restricting mode (`-doproperty`, `-nosubcircuit`, `-novendor`) runs `DBCellCopyAllPaint`, `DBFlatCopyMaskHints`, the label copy and `DBCellCopyAllCells` from the edit cell's own use, and every one of them applies `DBDescendSubcell` to that root use first (`dbCellPlaneSrFunc`, the label and cell copy functions). So the flattened cell itself must pass the test: with `-doproperty` and no `flatten` property on the top cell, or with `-nosubcircuit` on a top cell that has ports, the new cell is empty (no paint, no labels, no hints, no kept instances). Seen in the first capture of MISC-MH-11 (2026-11-10, PR #30): `flatten -doproperty flat` of a top cell without the property wrote an empty `flat.mag`.
- Evidence: commands/CmdFI.c:CmdFlatten (2206-2228), database/DBcellsrch.c:dbCellPlaneSrFunc (245), database/DBcellcopy.c (1359, 1435), database/DBcellsubr.c:DBDescendSubcell (61-108). Hefesto: src/core/src/db/flatten.cpp:flatten_to_new always copies the root level and applies the mode only to the uses below it. `layout-hierarchy` "Flattening into a new cell" says "every descended level" without saying whether the root is tested.
- Options: replicate | compat (replicate under `magic-compat`; with it off the root level is always copied) | fix
- Recommendation: compat. An empty result is clearly unintended (a `-nosubcircuit` flatten of any cell with ports produces nothing), but it is a value difference, not a crash or data loss of the source (X-2 tier 2). MISC-MH-11 now sets the property on its top cell so that it records only Q-MISC-15; a separate golden case should record this one once decided.
- Resolution: compat — with `magic-compat` ON a restricting flatten (and `xor`) applies the descend test to the flattened cell itself and copies nothing when it fails, as in Magic (the top labels of `-dotoplabels` are still copied, `DBCellCopyAllLabels` passes the root with `CU_DESCEND_SPECIAL`); with it OFF the root level is always copied (X-2 tier 2, a value difference). `layout-hierarchy` "Flattening into a new cell" states the rule; golden MODEL-G-060 records Magic's empty result. Approved 2026-11-10.

### Q-MODEL-34: Triangle painting over opposite-diagonal triangles recurses without bound
- Kind: bug
- Magic behavior: `DBNMPaintPlane0` calls itself for every piece the diagonal still crosses. Over existing triangles on the opposite diagonal, rounding leaves one more piece on the diagonal at each level, so the depth grows linearly with the triangle's size in internal units (about 0.4 levels per unit of `k` for the triangles `(0,0)-(30k,10k)` `/` and `(0,0)-(30k,17k)` `\`) and the work with its square. Found by `fuzz_mag_reader` on PR #53: a `.mag` file with `magscale 8888882 2` (k = 4444441) recursed about two million levels and overflowed the stack, in Hefesto's port as it would in Magic.
- Evidence: database/DBpaint.c:DBNMPaintPlane0 (1476-1800, recursive call at 1779). Hefesto: src/core/src/db/nm_paint.cpp (NMPainter); regression tests tests/unit/db/plane_test.cpp "a huge triangle over crossed triangles does not overflow the stack" and the seed tests/fuzz/corpus/fuzz_mag_reader/regression-26fd2417b438ec59bd431e90dbaa325dc30efeb2.
- Options: replicate | fix (explicit work stack) | fix (explicit work stack and a limit on the pieces of one paint)
- Recommendation: fix with both — a crash is X-2 tier 1. The work stack alone removes the overflow but leaves the quadratic time (the seed would take hours); the limit bounds it.
- Resolution: fix (X-2 tier 1, both modes) — the pieces live on a heap-allocated stack processed in Magic's order, so every triangle that stays under the limit is painted exactly as before. One paint processes at most 4096 straddling pieces (512 since Q-MODEL-35); each further one is painted as a rectangle and the paint is reported off grid, as Magic does for a piece it cannot quarter (Q-MODEL-4). `layout-paint-semantics` "Non-Manhattan painting" states the limit.

### Q-MODEL-35: Painting a huge triangle over crossed triangles is quadratic up to the piece limit
- Kind: bug
- Magic behavior: below the stack overflow of Q-MODEL-34, every level of `DBNMPaintPlane0`'s chain over opposite-diagonal triangles is only slightly smaller than its parent and fractures (`DBFracturePlane`), enumerates and paints next to the tiles all the levels above it left, which border the same large tiles, so a chain of n levels costs about n^2 tile steps and leaves about 4n tiles. `DBMergeNMTiles0` then leaves the plane's hint on the tile its walk ends at (`nmdone`), beside a staircase often a large tile one unit outside the paint, and `dbResolveImages` repaints the contact images one tile at a time through `DBPaint`, each point location walking the staircase again from that hint: quadratic in the tiles the paint left. Found by `fuzz_mag_reader` (CI run 43648522848, 60 s job, 10 s timeout): with Q-MODEL-34's limit of 4096 pieces, a `.mag` file with `magscale 8888882 2` and crossed triangles painted on several types and planes took about 10 s to read in a debug build (18 s on the CI host); Magic overflows the stack on the same file (Q-MODEL-34) and would take hours past it.
- Evidence: database/DBpaint.c:DBNMPaintPlane0 (1476-1800), DBMergeNMTiles0 (1047-1336, `PlaneSetHint` at 1332), database/DBpaint2.c:DBPaint (48-109), dbResolveImages (122-135). Hefesto: src/core/src/db/nm_paint.cpp (`kMaxSubAreas`, NMMerger::run); regression tests tests/unit/db/plane_test.cpp "repainting a huge triangle over crossed triangles stays within the work bound", tests/unit/cell/mag_io_test.cpp "a huge magscale over crossed triangles reads with bounded work" and the seed tests/fuzz/corpus/fuzz_mag_reader/regression-0ecdaeced894ac48779e9e0fac70fc254120e458.
- Options: replicate | fix (lower the piece limit so its square stays small) | fix (lower the limit and keep the hint where the paint left it)
- Recommendation: fix with both — a fuzz timeout on a 338-byte file is X-2 tier 1. Ordinary triangles descend into at most 16 sub-areas over the unit and golden suites, so a limit of 512 leaves them painted exactly as before; the hint only speeds up point location.
- Resolution: fix (X-2 tier 1, both modes) — one paint processes at most 512 straddling pieces; the merge after a paint no longer moves the plane's hint, so the image repaints locate their strips from the last painted tile. The seed reads in 0.3 s in a debug build; golden replay is unchanged. `layout-paint-semantics` "Non-Manhattan painting" states the limit and the work bound.

### Q-MODEL-36: Contact image repaints and type-list records multiply the piece limit
- Kind: bug
- Magic behavior: `dbResolveImages` repaints every contact image tile under a paint through its own `DBPaint`, and each of those `DBNMPaintPlane0` calls is a fresh paint. After Q-MODEL-35 bounded the pieces of one triangle paint, a paint over the staircase of contact images a huge triangle left over crossed triangles still repainted every image strip with a whole budget of its own: one paint of fuzz_mag_reader's timeout input (CI fuzz job, 60 s per target, 10 s timeout) descended into 11664 pieces over 604 triangle paints and left about 70000 tiles. A record of a type-list section (`<< ~metal1 >>`) paints every type, so each record repeats that, and every later paint and image repaint walks the tiles: the 354-byte file took about 12 s to read in a debug build. Past that, a few kilobytes of huge triangles in type-list sections, each paint within its own budget, still left tens of thousands of tiles for the rest of the read to walk (48 records: about 38000 tiles and over two minutes in a debug build). Magic overflows the stack on these files first (Q-MODEL-34).
- Evidence: database/DBpaint2.c:DBPaint (48-109), dbResolveImages (122-135), database/DBpaint.c:DBNMPaintPlane0 (1476-1800). Hefesto: src/core/src/db/paint.cpp (paint_type_within, resolve_images, repaint_images), src/core/src/db/nm_paint.cpp (NMPainter::descend), src/core/src/cell/mag_reader.cpp (`kPiecesPerRecord`, paint_record); regression tests tests/unit/cell/mag_io_test.cpp "a paint shares its non-Manhattan budget with the contact images it repaints", "the paints of a .mag read share one non-Manhattan budget", "crossed triangles under a huge magscale read with bounded work (image repaints)" and the seed tests/fuzz/corpus/fuzz_mag_reader/regression-3b720185126474ca4033c22ea793e51f2711551e.
- Options: replicate | fix (lower the per-paint limit again) | fix (one budget per DBPaint shared with its image repaints, and a pool per `.mag` read that grows with the paint records)
- Recommendation: fix with shared budgets — a fuzz timeout is X-2 tier 1. Lowering the limit only shrinks the constant: the image repaints still multiply it by the image strips and the records by the types of their list. Sharing bounds both. Every piece leaves at most about four tiles, so a read leaves at most about 4 * (512 + 16 * records) tiles from non-Manhattan pieces, whatever the type lists and image strips. Ordinary triangles descend into at most 16 pieces per paint, so the 16 per record never runs short for them, and the 512 at the start of a read keeps a single large triangle exact as before.
- Resolution: fix (X-2 tier 1, both modes) — a cell paint shares one NMBudget of 512 pieces between its planes and the contact images it repaints; a `.mag` read shares a pool that starts at 512 and grows by 16 per paint record, each paint drawing at most min(512, pool). Pieces past either are painted as rectangles and reported off grid, as in Q-MODEL-34. The timeout input reads in 0.03 s and the worst variant constructed in 0.12 s in a debug build; golden replay is unchanged. `layout-paint-semantics` "Non-Manhattan painting" states both budgets.

### Q-MODEL-37: The 512-piece limit changes the tiling of real GDS reads
- Kind: bug
- Status: approved (drafted 2026-11-17 in round 7 from MASK-RT-gf180mcu_fd_io-f02, approved by the user 2026-11-17 as fix)
- Magic behavior: `DBNMPaintPlane0` has no limit. A triangle painted beside a nearly parallel diagonal, a few units off, descends into a chain of small pieces along the two diagonals, about as long as the triangle divided by the offset. gf180mcu_fd_io's `POWER_RAIL_COR_1` has a 45-degree PPLUS path over a boundary whose diagonal edge is 3 read units away (after an earlier structure rescaled the read units by 5): its triangles of 1824 units descend into 910 pieces each, 926 such paints over the library.
- Evidence: database/DBpaint.c:DBNMPaintPlane0 (1476-1800), cif/CIFrdutils.c:CIFMakeManhattanPath (1052-1170). With Q-MODEL-35's 512 pieces per paint, the rest of each chain was filled as a rectangle, which covered space above both diagonals until later pieces of the path painted it; the area came out right, but the read plane's tiling did not, so the `psd` rule's and/and-not results and the psubdiff painted from them were split differently (13091 triangles in Magic, 12873 in Hefesto) and out.gds had 678 fewer boundaries in layers 22/0 and 31/0. Found with a native build of Magic 8.3.694 instrumented to dump the read plane after every paint (round 7); with an unbounded budget the plane, the saved `.mag` and the library's out.gds equal Magic's. Regression test: `tests/unit/mask/input/engine_test.cpp` "a read plane of nearly parallel 45-degree edges is tiled as Magic tiles it" (data `tests/data/mask/input/gf180_pplus_chain.txt`).
- Options: keep 512 (exact only below it) | raise the limit for every paint | a larger budget for the read planes only
- Recommendation: a larger budget for the read planes only, in both modes (X-2 tier 1 stays: the bound remains). A triangle that a CIF or GDS read paints into its read planes descends into at most 1024 pieces, drawn from a pool of the read that starts at 65536 and grows by 1024 for every triangle the read paints, at most 65536 per polygon (`mask::ReadTriangleBudget`). That covers the polygon reader (`mask::paint_read_triangle`) and the repaints of read-plane split tiles: the read-unit rescale (GDS `scale_plane`, CIF `input_detail::scale_plane`) and the flatten copies (GDS `copy_tiles`, the CIF reader's copy of a saved cell), each repainted triangle adding its own 1024 (`mask::repaint_read_triangle`). Database paints, `.mag` reads and the mask generators keep Q-MODEL-35 and Q-MODEL-36 unchanged. 1024 is above the 910 the gf180mcu_fd_io chains need, and the pool's growth per triangle matches it, so a library of such triangles stays exact however they cluster.
- Cost, the numbers to approve: the pieces of a read stay linear in the triangles it paints, but the work does not. Each piece of a chain works next to the tiles the pieces before it left, so one paint of n pieces costs about n^2 tile steps (`layout-paint-semantics` "Non-Manhattan painting"). Per paint that is at most 1024^2, about 1.05M tile steps, against 512^2, about 0.26M, for a database paint (4 times more). Over a read the pool bounds the pieces, so the non-Manhattan work stays below 1024 x (65536 + 1024 x triangles) tile steps: about 1.05M per triangle read, and 67M before the first triangle's grant. (Round 7 had proposed 4096 pieces per paint: 16.8M tile steps per paint and about 4.2M per triangle; the round 7 review lowered it to 1024.) A crafted file of many triangles beside nearly parallel diagonals reaches the bound; the fuzz seed `tests/fuzz/corpus/fuzz_cif_reader/regress-nm-near-parallel-diagonals` (the POWER_RAIL_COR_1 chain) replays in under a second on a debug build.
- Implemented in round 7 (`m5-r7-iort`; repaints and the 1024 cap in the round 7 review, test "a rescale or flatten copy repaints read-plane triangles with the read's budget"): `src/core/include/hefesto/mask/input/engine.hpp` `ReadTriangleBudget`, `src/core/src/mask/input/engine.cpp`, `src/core/src/gds/read/shapes.cpp`, `src/core/src/gds/read/reader.cpp`, `src/core/src/gds/read/sref.cpp`, `src/core/src/mask/input/scale.cpp`; `mask-input-styles` "Polygon to region conversion" and `layout-paint-semantics` "Non-Manhattan painting" state it.
- Resolution: fix — in both modes a triangle that a GDS or CIF read paints into its read planes (a polygon's, or a split tile a read-unit rescale or flatten copy repaints) descends into at most 1024 pieces, drawn from a pool of the read that starts at 65536 and grows by 1024 for every triangle the read paints, at most 65536 per polygon (`mask::ReadTriangleBudget`); a piece past it is painted as a rectangle. Cost as stated above: at most about 1.05M tile steps per read paint (0.26M for a database paint, which keeps Q-MODEL-35's 512) and about 1.05M per triangle over a read. Database paints, `.mag` reads and the mask generators keep Q-MODEL-35 and Q-MODEL-36. `mask-input-styles` "Polygon to region conversion", `layout-paint-semantics` "Non-Manhattan painting" and security.md state it. Approved 2026-11-17.
