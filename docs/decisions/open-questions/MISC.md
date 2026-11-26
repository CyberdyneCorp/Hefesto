# Open Questions — MISC area

Capabilities: `antenna-check`, `mask-hints`, `font-labels`, `view-3d`, `plot-output`.
Magic reference: `RTimothyEdwards/magic` @ `fc5daa9e`.

## antenna-check

### Q-MISC-1: Cumulative antenna model never reports violations
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: The per-layer sums S_metal and S_via are only accumulated when the PARTIAL model flag is set. With `model cumulative` both sums stay 0, so no gate ever fails, although the areas are accumulated over all lower planes.
- Evidence: extflat/EFantenna.c:574-590, extflat/EFantenna.c:647-668
- Verified 2026-11-13: Confirmed at extflat/EFantenna.c:574-584 (no-diffusion branch) and :637-645 (diffusion branch). anttotal and conttotal are accumulated only under ANTENNAMODEL_PARTIAL. In the cumulative model both stay 0, so no gate ever fails, although areas are accumulated over all planes with order ≤ pos (:551-561). All three PDKs use `model partial` (sky130A-extract.tech:75, gf180mcuD-extract.tech:44, ihp-sg13g2), so none is affected.
- Options: replicate | fix (accumulate a[t]/ratio for all non-contact types in the cumulative model) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: in the cumulative model, report nothing, as Magic does. OFF: in the cumulative model, sum a[t]/ratio over all non-contact types into the metal total, and over contact types into the via total, for every plane with order ≤ the check plane, using the same gate and diffusion-ratio formulas as partial. Either total over the gate area is a violation. A check that can never fire is a bug that changes results, so the compat rule applies. The release notes should say clearly that ON reports no cumulative violations.
- Resolution: compat — with `magic-compat` ON the cumulative antenna model reports nothing, as Magic does; with it OFF it accumulates a[t]/ratio over every plane with order ≤ the check plane into metal and via totals, with the partial model's gate and diffusion formulas. Release notes say that ON reports no cumulative violations. Approved 2026-11-13.

### Q-MISC-2: Antenna model flags are uninitialised
- Kind: bug
- Magic behavior: The style's model flag byte is never initialised (the style is allocated without zeroing) and `model` lines only OR flags in. Without a `model` line the model is undefined; reloading a style can keep stale flags.
- Evidence: extract/ExtTech.c:816-823, extract/ExtTech.c:942-951, extract/ExtTech.c:3086-3105
- Options: replicate (impossible deterministically) | fix (initialise to 0 on style init)
- Recommendation: fix — start empty; equals Magic's behaviour whenever malloc returns zeroed memory.
- Resolution: fix — the antenna model flags start empty for every style and on reload (X-2 tier 1, 2026-11-07).

### Q-MISC-3: Antenna line without calc type and no default
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: The error is reported but the (uninitialised) calc type is still stored and the ratios are stored, so the layer may or may not count.
- Evidence: extract/ExtTech.c:3015-3028
- Verified 2026-11-13: Confirmed at extract/ExtTech.c:3017-3027. With no calc type and no SURFACE or SIDEWALL model bit, TechError is reported, but the uninitialised local `areaType` is still stored in exts_antennaRatio[t].areaType (:3025-3027). The ratios are stored afterwards. antennaAccumFunc (extflat/EFantenna.c:837 and the following lines) then branches on those bits, so whether the layer counts depends on stack garbage. No PDK line omits the calc type.
- Options: replicate | fix (store "no calculation type")
- Recommendation: fix. Both modes: after the error, store 'no calculation type', so the layer contributes no antenna area while its ratios are still parsed. Magic reads an uninitialised value here, which is undefined behaviour and therefore X-2 tier 1. Magic has no reproducible output to match.
- Resolution: fix — in both modes an antenna line with no calculation type and no default stores 'no calculation type': the layer contributes no antenna area while its ratios are still parsed (X-2 tier 1; Magic reads an uninitialised value). Approved 2026-11-13.

### Q-MISC-4: Contact cut count mixes internal and output units
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: nx = (w − 2·border)/(size + sep) uses tile width in internal units but cut parameters in cifoutput units (scaled by scalefactor), so cut counts are wrong unless scalefactor = 1; additionally the cut area is truncated to an integer before multiplying. If the contact has no squares/slots rule the parameters are uninitialised.
- Evidence: extflat/EFantenna.c:853-863, extflat/EFantenna.c:974-985, cif/CIFtech.c:2339-2343
- Verified 2026-11-13: Confirmed. The SIDEWALL path is at extflat/EFantenna.c:853-863 and the SURFACE path at :974-985. nx = (w − 2·border)/(size+sep), with w in internal units and the cut parameters in CIF style units; a zero result is replaced by 1. The SURFACE area is nx·ny·(dlong)(f·f) with f = size/cs_scaleFactor, so the area is truncated. For sky130A (scalefactor 10 nm, MCON `squares-grid 0 170 190`), most tiles count as 1 cut. Two cases are worse than the entry says. In cif/CIFtech.c:2339-2343, with no CIF style `edge = spacing = border = 0;` assigns the local pointers, so the caller's values stay uninitialised; with no squares or slots rule it returns without writing them. Separately, size+sep == 0 divides by zero.
- Options: replicate | fix (convert w, h to output units first; use 0 area when no rule) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. Both modes (tier 1): a contact type with no CIF style or no squares/slots rule contributes cut area 0, and size+sep == 0 counts one cut, with no uninitialised read and no division by zero. ON: otherwise compute as Magic does, with the unit mix and the truncated f², which keeps sky130 and gf180 antenna results identical. OFF: convert w, h and border to the same units as the cut parameters before counting, and use the untruncated cut area. The unit mix is a quirk that changes results, so the compat rule applies; the undefined-behaviour parts are fixed in both modes. (corrected 2026-11-13, was replicate by default with a fixed mode; the undefined-behaviour cases are now fixed in both modes)
- Resolution: compat — in both modes a contact type with no CIF style or no squares/slots rule contributes cut area 0, and size+sep == 0 counts one cut (tier 1); otherwise with `magic-compat` ON the cut count uses Magic's unit mix and truncated f², and with it OFF w, h and border are converted to the cut parameters' units and the cut area is not truncated. Approved 2026-11-13.

### Q-MISC-5: `antenna` line with calc type and a single ratio reads past arguments
- Kind: bug
- Magic behavior: `antenna m1 surface 400` passes the 4-argument minimum but the parser then reads argv[4] (NULL).
- Evidence: extract/ExtTech.c:156-157, extract/ExtTech.c:3044-3046
- Options: replicate (crash) | fix (report "Malformed line")
- Recommendation: fix — report an error and ignore the line.
- Resolution: fix — an `antenna` line with a calc type and a single ratio reports an error and is ignored, instead of reading a missing argument (X-2 tier 1, 2026-11-07).

### Q-MISC-6: Missing plane-order index reuses previous check plane
- Kind: bug
- Magic behavior: If no plane has order k, the check plane variable keeps its previous value (uninitialised on the first iteration).
- Evidence: extflat/EFantenna.c:492-494
- Options: replicate | fix (skip k)
- Recommendation: fix — skip order indices with no plane.
- Resolution: fix — plane-order indices with no plane are skipped, instead of reusing a previous or uninitialised check plane (X-2 tier 1, 2026-11-07).

### Q-MISC-7: Connectivity restriction uses plane numbers and one contact type
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: Only the first contact type whose home plane is the check plane is restricted, and the restriction keeps types whose plane *number* (not plane order) is ≤ the check plane's number. Other via types on that plane still connect upward.
- Evidence: extflat/EFantenna.c:504-517
- Verified 2026-11-13: Confirmed at extflat/EFantenna.c:504-517. Only the first contact type whose DBPlane equals the check plane is restricted (`break`). It keeps the types j with DBPlane(j) <= pNum2, which compares plane numbers. The table is restored at :740-741. This affects a PDK: in gf180mcuD the metal4 plane declares mimcc (contact mimcap↔metal5) before via4 in the types section. At the metal4 check only mimcc is restricted, and via4 still connects through metal5, so metal4 reached through metal5 jumpers is counted. With the partial model that can produce false violations. sky130A and ihp-sg13g2 have one via per metal plane that matters.
- Options: replicate | fix (restrict all contacts on the plane, compare plane order) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: restrict only the first contact type whose home plane is the check plane, and compare plane numbers, as Magic does (gf180mcuD's metal4 results included). OFF: restrict every contact type whose home plane is the check plane, and keep only the types whose plane order is ≤ the check plane's order. This is a bug that changes antenna results, so the compat rule applies. The release notes should name gf180mcuD's metal4 check as the case where the modes differ.
- Resolution: compat — with `magic-compat` ON the connectivity restriction applies to the first contact type on the check plane and compares plane numbers, as Magic does (gf180mcuD's metal4 check included); with it OFF it applies to every contact type on the check plane and compares plane order. Release notes name gf180mcuD's metal4 check. Approved 2026-11-13.

### Q-MISC-91: `antennacheck run` becomes the cell name
- Kind: bug
- Status: approved 2026-11-22 (proposed in M7 round R1, 2026-11-21; approved with the recommended option)
- Magic behavior: antennacheck's option table has a `[run] [options]` entry that is never matched; the word becomes the cell name, as for `ext2spice` (Q-NETLIST-13) and `ext2sim` (Q-NETLIST-49).
- Evidence: extflat/EFantenna.c (CmdAntennaCheck); antenna-check "Antenna check command" (says `run` is matched by prefix, which holds only with magic-compat OFF)
- As implemented (round R1, scaffold): compat, as Q-NETLIST-49.
- Options: compat | replicate | fix
- Recommendation: compat; the spec text should say that the prefix match holds with magic-compat OFF.
- Resolution: compat — as Q-NETLIST-13 and Q-NETLIST-49: with `magic-compat` ON `run` and any ambiguous first word are run arguments, so `antennacheck run` checks a cell named `run`, as Magic does; with it OFF the first word is matched by unique prefix against `run`, `debug` and `help`, `run` is a keyword and an ambiguous prefix prints `Ambiguous option: <w>`. In both modes a word that is not an option prefix is the cell name. Spec: `antenna-check` "Antenna check command". Approved 2026-11-22.

### Q-MISC-92: `antennacheck <cell>` with no layout window
- Kind: bug
- Status: pending user confirmation (fixed in both modes in M7 round R4 by ANTENNA under the standing rule)
- Magic behavior: CmdAntennaCheck dereferences the NULL window (`editUse = (CellUse *)w->w_surfaceID`) when a cell is named and no layout window exists: a crash.
- Evidence: extflat/EFantenna.c:216-221
- As implemented (round R4), both modes: Hefesto prints `Point to a window or specify a cell name.` Tests: `tests/unit/antenna/run_test.cpp`.
- Options: fix (as implemented)
- Recommendation: fix. Spec on approval: `antenna-check` "Antenna check command".

### Q-MISC-93: antennacheck devices with no style device line or no terminals
- Kind: bug
- Status: pending user confirmation (fixed in both modes in M7 round R4 by ANTENNA under the standing rule)
- Magic behavior: a device whose model names no style device line makes extGetDevType return -1, then DBPlane(-1) is read; a device with no terminals reads `dev_terms[0]`.
- Evidence: extflat/EFantenna.c:251-255, 410, 427-430
- As implemented (round R4), both modes: the device is skipped. Tests: `tests/unit/antenna/run_test.cpp` "a device whose model has no style device line is skipped" (added in the R4 fix pass; without the guard it aborts under libc++ hardening). The no-terminal guard has no test: Hefesto's `.ext` reader gives every fet, mosfet and asymmetric device at least one terminal (the record needs 11 tokens and whole terminal triples), so no input reaches it; it stays as a defensive check.
- Options: fix (as implemented)
- Recommendation: fix.


## mask-hints

### Q-MISC-10: Mask hints do not round-trip with `property compat false`
- Kind: bug
- Magic behavior: With compatibility off, hints are written as `dimension MASKHINTS_x ...`, but on read a `dimension` property is stored as an integer list, not a region; later users of the hint expect a region (assert / undefined behaviour).
- Evidence: database/DBio.c:5053-5060, database/DBio.c:3604-3660, cif/CIFgen.c:5695
- Options: replicate | fix (detect `MASKHINTS_` prefix for any declared type)
- Recommendation: fix — always parse `MASKHINTS_` keys as regions.
- Resolution: fix — `MASKHINTS_` property keys are always parsed as regions, so hints round-trip with `property compat false` (X-2 tier 1, 2026-11-07).

### Q-MISC-11: cifinput templayer hints look up the wrong key
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: When adding a templayer hint, the existing property is looked up under the bare layer name instead of `MASKHINTS_<name>`; normally not found, so a new region replaces any existing hint of the same name instead of adding to it (and a property named exactly `<name>` is misused as a plane). See Q-MASK-308.
- Evidence: cif/CIFrdcl.c:CIFPaintCurrent (:736-754)
- Options: replicate | fix (look up `MASKHINTS_<name>` and union)
- Recommendation: fix (union) everywhere, as drafted in Q-MASK-308.
- Duplicate: tracked in Q-MASK-308, which is the canonical entry. Record the resolution there. (Consolidation audit, 2026-11-06.)
- Resolution: fix (union) everywhere, as drafted in Q-MASK-308. Approved 2026-11-10.

### Q-MISC-12: `plane` property type keyword not accepted in .mag
- Kind: quirk
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: `dbReadProperties` looks the type keyword up with `Lookup` in `string|integer|dimension|double` (unique prefix, case-insensitive: `str`, `STRING` are accepted). `plane` is missing from that table, so a `plane ...` line ends the properties section; the main reader then takes it for a use line and reports `Expected "use" line but saw: plane MASKHINTS_X ...` and `File <cell> contained format error`, keeping what was read so far (paint, earlier properties) and dropping the rest of the file. The switch's `case PROPERTY_TYPE_PLANE` (parse `MASKHINTS_*` as a hint, otherwise report `Plane type specified for property "<key>" but property is not a mask hint!`) is unreachable. Magic never writes `plane` (only `string` or `dimension`). Checked with Magic 8.3.694 (`plane`, `PLANE`, `str` lines).
- Evidence: database/DBio.c:dbReadProperties (:3210-3243 type table and lookup, :3309-3320 dead `plane` case), database/DBio.c:dbCellReadDef (:852-857, :882-885, badfile at :1058-1062), database/DBio.c:dbReadUse ("Expected use line"), utils/lookup.c:Lookup
- Options: replicate (format error, rest of file dropped) | fix (accept `plane` as the dead case intends) | replicate under magic-compat, fix with it off
- Recommendation: replicate under magic-compat (X-2 tier 2: only files Magic never writes are affected, and the format error is observable); with magic-compat off accept `plane` exactly as the dead case does. Hefesto never writes `plane`.
- Resolution: replicate under magic-compat (X-2 tier 2: only files Magic never writes are affected, and the format error is observable); with magic-compat off accept `plane` exactly as the dead case does. Hefesto never writes `plane`. Approved 2026-11-10.

### Q-MISC-13: DRC exception edge touching the hint boundary
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: The inside test passes the zero-width edge segment to `DBSrPaintArea`, whose contract excludes degenerate areas, so the result follows the tile walk, not geometry. Vertical edge at x over (b, t): inside iff the hint covers part of the unit column [x, x+1) × (b, t), so a hint touching from the right (or straddling) counts and one touching only from the left does not. Horizontal edge at y over (l, r): the walk starts at the hint-plane tile holding (l, y−1); if that tile's top is y the edge is outside, otherwise it is inside iff a hint tile covers part of the row [y−1, y) within (l, r). So a hint touching from above never counts, and whether a straddling hint counts depends on the strip decomposition: an L-shaped hint that fully covers the edge misses it when its strips break at y at x = l, and an unrelated hint far to the left with a corner at height y can break the start tile. Checked with Magic 8.3.694 (`width` under `exception X`, 2-unit wires; e.g. hint `5 -5 15 5` on bottom edge y=0, x 0..20: error; adding hint `-100 0 -90 10`: no error).
- Evidence: drc/DRCbasic.c:drcTile (:921-957 vertical, :1381-1417 horizontal), database/DBtiles.c:DBSrPaintArea (start point (xbot, ytop−1), `while (TOP(tp) > rect->r_ybot)`), tiles/tile.h:GOTOPOINT
- Options: replicate (emulate the walk on Magic's maximal-horizontal-strip hint plane) | fix (inside iff the closed hint region contains a positive-length part of the edge segment, i.e. the hint touches or straddles from either side)
- Recommendation: replicate under magic-compat (X-2 tier 2: changes DRC results; needs the canonical strip decomposition), fix with it off; capture golden MISC-MH-08 with the cases above (left/right/above/below touch, L-shape, far corner).
- Resolution: replicate under magic-compat (X-2 tier 2: changes DRC results; needs the canonical strip decomposition), fix with it off; capture golden MISC-MH-08 with the cases above (left/right/above/below touch, L-shape, far corner). Approved 2026-11-10.

### Q-MISC-14: `flatten -doinplace` drops the hints of the flattened instance's descendants
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: `DBFlattenInPlace` (used by `flatten -doinplace <instance>` and, through `dbCellFlattenCellsFunc`, by `flatten -doinplace` on the selection) copies paint and labels with the search mask, so with the default `CU_DESCEND_ALL` every level below the instance is flattened into the parent, but it copies hints with `DBCellCopyMaskHints(scx.scx_use, …)`, which enumerates only the properties of the instance's own definition. The `MASKHINTS_*` regions of every flattened grandchild and deeper cell are lost: the parent gets their paint without the hints that scope its `exception`/`exemption` rules and feed the `mask-hints` output operator. With a restricting mask (`-nosubcircuit`, `-novendor`, `-dopropflat`) the kept instances are copied as instances and keep their hints in their own cells; only the hints of descended levels are lost. One-level instances (golden MISC-MH-07) are unaffected.
- Evidence: database/DBcellcopy.c:DBFlattenInPlace (580-690, hints at 674), database/DBcellcopy.c:DBCellCopyMaskHints (483-495), database/DBcellcopy.c:dbCellFlattenCellsFunc (740-760), commands/CmdFI.c:CmdFlatten (2112-2143). Hefesto (M3 round 1) replicates this in both modes: src/core/src/db/flatten.cpp:flatten_element (307); test tests/unit/hints/copy_test.cpp "flatten in place copies only the flattened instance's own hints". `mask-hints` "Copying, flattening and transforming hints" (b) states only the instance's own hints, so it matches Magic, while (a) promises every flattened descendant's hints for plain `flatten`.
- Options: replicate | compat (replicate under `magic-compat`; with it off copy the hints of every flattened level, transformed and unioned) | fix
- Recommendation: fix — the hints are dropped while the geometry they describe is kept, which is data loss (X-2 tier 1), and it is the same kind of loss that Q-MISC-11/Q-MASK-308 fixed everywhere. Copy the hints of every level that is flattened (each descended instance and array element, with its full transform to the parent), never those of kept instances, and state it in `mask-hints` (b). A new golden case (two-level hierarchy with hints at both levels, `flatten -doinplace`) records the difference from Magic as a Q-MISC-14 deviation.
- Resolution: fix — the hints are dropped while the geometry they describe is kept, which is data loss (X-2 tier 1), and it is the same kind of loss that Q-MISC-11/Q-MASK-308 fixed everywhere. Copy the hints of every level that is flattened (each descended instance and array element, with its full transform to the parent), never those of kept instances, and state it in `mask-hints` (b). A new golden case (two-level hierarchy with hints at both levels, `flatten -doinplace`) records the difference from Magic as a Q-MISC-14 deviation. Approved 2026-11-10.

### Q-MISC-15: `flatten` with a restricting mode copies the hints of kept instances and drops those of flattened ones
- Kind: bug
- Status: approved (recommendation drafted and approved 2026-11-10)
- Magic behavior: `CmdFlatten` copies hints into the new cell with `DBFlatCopyMaskHints(&scx, xMask, …)`, i.e. `DBTreeSrCells`. `dbTreeCellSrFunc` calls the copy function on a use that is *not* descended, and on a descended one only when the mask is `CU_DESCEND_ALL`; it then searches the descended use's children. With the default mask (`CU_DESCEND_ALL`) every instance's hints are copied, which is what the comment ("copy information in mask-hint properties from flattened children") intends. With `-nosubcircuit`, `-novendor` or `-dopropflat` the test is inverted: the hints of instances that are kept (copied into the new cell as instances by `DBCellCopyAllCells`, with their own hints) are also painted into the new cell's hints, and the hints of the instances whose paint is flattened, and of all their descendants, are lost. `flatten -dobox` takes the same path. The DRC interaction flattening uses mask 0 (`CU_DESCEND_ALL`) and is unaffected.
- Evidence: commands/CmdFI.c:CmdFlatten (2084-2097 options, 2206-2220), database/DBcellcopy.c:DBFlatCopyMaskHints (551-560) and dbFlatCopyMaskHintsFunc (517-532), database/DBcellsrch.c:DBTreeSrCells (825-860) and dbTreeCellSrFunc (871-903), database/DBcellsubr.c:DBDescendSubcell, database/database.h.in:531-537. Hefesto (M3 round 1) replicates this in both modes: src/core/src/hints/hints.cpp:FlatHintCopier::visit (141-148), src/core/src/db/flatten.cpp:flatten_to_new (282); test tests/unit/hints/copy_test.cpp "hint flattening follows Magic's descend rules". This contradicts `mask-hints` "Copying, flattening and transforming hints" (a) ("the hints of every flattened descendant instance").
- Options: replicate | compat (replicate under `magic-compat`; with it off copy only the flattened instances' hints) | fix (copy the flattened instances' hints in both modes; the extra copies of kept instances' hints follow the compat setting)
- Recommendation: fix the dropped hints in both modes (X-2 tier 1, data loss, as Q-MISC-14 and Q-MISC-11); keep the extra copy of kept instances' hints under `magic-compat` and omit it with the setting off (X-2 tier 2: it adds hint geometry that the `mask-hints` output operator writes for the new cell as well as for the kept child, a value change but no loss). A new golden case (`flatten -novendor` or `-dopropflat` over a two-level hierarchy with hints at each level) records the difference from Magic as a Q-MISC-15 deviation.
- Resolution: fix the dropped hints in both modes (X-2 tier 1, data loss, as Q-MISC-14 and Q-MISC-11); keep the extra copy of kept instances' hints under `magic-compat` and omit it with the setting off (X-2 tier 2: it adds hint geometry that the `mask-hints` output operator writes for the new cell as well as for the kept child, a value change but no loss). A new golden case (`flatten -novendor` or `-dopropflat` over a two-level hierarchy with hints at each level) records the difference from Magic as a Q-MISC-15 deviation. Approved 2026-11-10.

## font-labels

### Q-MISC-20: Bounding-box rounding tests 7 bits instead of 3
- Kind: bug
- Magic behavior: Conversion from 1/8 units uses `(v & 0x7f) != 0` to decide whether to round outward, so exact multiples of 8 that are not multiples of 128 are still expanded by one unit.
- Evidence: database/DBlabel.c:1481-1492
- Options: replicate | fix (`& 0x7`)
- Recommendation: replicate — affects extended bboxes saved to `.mag` timestamps/bboxes and redraw areas only marginally.
- Resolution: compat — same as Q-MODEL-6: the `& 0x7f` test is kept because the extended bounding box is saved; with `magic-compat` OFF, the test is `& 0x7` (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MISC-21: `flatten` copies font label rotation/offset untransformed
- Kind: bug
- Magic behavior: The non-inplace flatten path passes the original rotation and offset (marked "To-do" in source) while other copy paths transform them.
- Evidence: commands/CmdFI.c:1996-2011
- Options: replicate | fix (transform like other copies)
- Recommendation: fix.
- Resolution: compat — same as Q-MODEL-20: plain `flatten` copies rotation and offset untransformed, because the values are written to the flattened cell; with `magic-compat` OFF, they are transformed like the other copies (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MISC-22: Default font label size equals lambda numerator in eighth units
- Kind: quirk
- Magic behavior: When a font is applied to a label of size 0, or `label` is used with a font and no size and no defaults, size = lambda numerator (often 1), i.e. 1/8 internal unit — practically invisible.
- Evidence: commands/CmdRS.c:2104, commands/CmdLQ.c:255-256
- Options: replicate | fix (8 × numerator)
- Recommendation: replicate for data compatibility; GUI may suggest a size.
- Resolution: compat — the default size is the lambda numerator in 1/8 units, as in Magic, because it is stored in `.mag`; the GUI may suggest a size; with `magic-compat` OFF, the default is 8 × the numerator (one lambda) (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MISC-23: `setlabel offset` query prints x twice; size/offset query scaling inconsistent
- Kind: bug
- Magic behavior: The per-label offset query prints offset.x/8 for both coordinates; the `-default` queries print unscaled values while per-label queries divide by 8.
- Evidence: commands/CmdRS.c:1992-1993, commands/CmdRS.c:2380-2384, commands/CmdRS.c:2420-2424
- Options: replicate | fix
- Recommendation: fix (informative output only).
- Canonical entry for Q-CMDEDIT-90.
- Resolution: fix — query output is informative: the per-label offset query prints x and y, and the `-default` queries are scaled like the per-label ones (÷8) (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-MISC-24: Rotation transform direction mismatch
- Kind: bug
- Magic behavior: The angle transform adds 90° for a clockwise 90° instance transform, while corners are rotated counter-clockwise, so copied rotated font labels (and their bboxes) are mirrored in angle relative to the geometric image.
- Evidence: utils/geometry.c:154-189, database/DBlabel.c:1440-1458
- Options: replicate | fix
- Recommendation: replicate (stored angles in `.mag`/GDS must match Magic); flag in UI.
- Resolution: compat — same as Q-MODEL-1: stored angles in `.mag` and GDS must match Magic; the UI may flag the mismatch; with `magic-compat` OFF, a clockwise 90° transform subtracts 90 (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-MISC-25: Unclosed glyph path overflows point buffer
- Kind: bug
- Magic behavior: When a glyph path's last point differs from its first, the point count is one larger than the allocated array.
- Evidence: database/DBlabel.c:1745-1757
- Options: fix (allocate count points)
- Recommendation: fix — semantics unchanged (all distinct points kept).
- Resolution: fix — the glyph point buffer holds every distinct point of an unclosed path; semantics unchanged (X-2 tier 1, 2026-11-07).

### Q-MISC-26: Loading a font by file name duplicates fonts
- Kind: quirk
- Magic behavior: `setlabel font FreeSans.pt3 0.58` looks up "FreeSans.pt3" (not found), loads the file, and the new font is named "FreeSans"; repeating the command loads a second copy with a new index.
- Evidence: commands/CmdRS.c:2287-2300, database/DBlabel.c:1680-1688
- Options: replicate | fix (skip loading if the file's font name already exists)
- Recommendation: fix — font indices are not persisted (`.mag` stores names; GDS stores index & 3, see Q-MISC-27).
- Resolution: replicate — the file is loaded again and gets a new index, as in Magic, in both modes: `setlabel fontlist` output and the GDS font index (index & 3, Q-MISC-27) then match Magic's for the same startup and scripts (golden case MISC-FL-04); recorded 2026-11-09 while implementing the `setlabel` font family; approved 2026-11-09.

### Q-MISC-27: GDS stores only two bits of font index
- Kind: quirk
- Magic behavior: Font index is written modulo 4 in PRESENTATION bits; on read the index refers to the reader's load order.
- Evidence: calma/CalmaWrite.c:2860-2867, calma/CalmaRdpt.c:831
- Options: replicate
- Recommendation: replicate; Hefesto must load FreeSans, FreeSerif, FreeMono in that order by default.

### Q-MISC-28: `setlabel font <index>` resets the selected labels to the default font
- Kind: bug
- Magic behavior: `setlabel font N` with an integer N prints the name of font N (or reports an out-of-range index), but the index is held in a variable declared inside that block; the outer `font` variable is still -1, so the command then applies font -1 to every selected label (or to the `-default` label). The same happens for `setlabel font N scale`. The query prints the name; the change silently turns the labels into default-font labels (size, rotation and offset are kept).
- Evidence: commands/CmdRS.c:2276-2330
- Options: replicate | fix (apply font N, or only print and change nothing)
- Recommendation: fix (only print and change nothing): the documented use is "see the font name corresponding to a font number".
- Resolution: replicate in magic-compat (scripts written for Magic change labels the same way); fix in native mode, where `setlabel font <index>` only prints the font name and changes no label (approved 2026-11-09).

## plot-output

### Q-MISC-41: PostScript drops rectangles that start left of / below the area
- Kind: bug
- Magic behavior: Outline/cross rectangles (`mr`, `mx`) are emitted only if their lower-left corner is inside the plotted area, contrary to the comment ("Depend on PostScript to do the clipping"); cell outlines straddling the area's left/bottom edge disappear.
- Evidence: plot/plotPS.c:405-420
- Options: replicate | fix (emit when any part overlaps)
- Recommendation: replicate for golden compatibility, offer fix.

### Q-MISC-42: PostScript label/cell text not escaped
- Kind: bug
- Magic behavior: Text is written inside `( )` verbatim; parentheses or backslashes in labels or cell names produce invalid PostScript.
- Evidence: plot/plotPS.c:995-997, plot/plotPS.c:1040-1055
- Options: replicate | fix (escape `(`, `)`, `\`)
- Recommendation: fix — only affects otherwise-broken files.

### Q-MISC-43: Label margin estimate mixes relative and absolute coordinates
- Kind: bug
- Magic behavior: The anchor is relative to the area origin but is compared with absolute area bounds, so margins are only correct when the area's lower-left corner is (0,0).
- Evidence: plot/plotPS.c:873-878
- Options: replicate | fix
- Recommendation: replicate (BoundingBox and scale depend on it).

### Q-MISC-44: Printer spooling, temp files and library-path resources
- Kind: platform
- Magic behavior: Raster plots are written to `/tmp` (parameter `directory`) via mkstemp and printed with `system("lp -d <printer> <file>")`; `magicps.pro` and style/colour files are found on the system library path.
- Evidence: plot/plotVers.c:95-110, plot/plotVers.c:1220-1235, plot/plotVers.c:1475-1485, plot/plotPS.c:1180-1192
- Options: platform alternative (embed resources; share sheet / print API instead of spooling)
- Recommendation: embed prologue byte-exact; no process spawning on iPadOS.

### Q-MISC-45: colorversatec stipple byte swap uses wrong index
- Kind: bug
- Magic behavior: On little-endian hosts the replicated stipple words are byte-swapped from `vs_stipple[i]` instead of `[j+i]`, corrupting patterns given with fewer than 16 words.
- Evidence: plot/plotVers.c:470-476
- Options: replicate | fix
- Recommendation: fix (legacy device, informative).

### Q-MISC-46: PNM plots ignore labels and cell outlines
- Kind: doc-mismatch
- Magic behavior: The default plot mask includes labels and subcells, and the routine's documentation says they are plotted, but PNM rendering draws paint only.
- Evidence: plot/plotPNM.c:560-575, plot/plotPNM.c:880-905
- Options: replicate | fix (render labels)
- Recommendation: replicate (byte-identical images).
- Resolution: n/a — spec follows Magic's code (PNM draws paint only); documentation is wrong (X-2 tier 3, 2026-11-07).

### Q-MISC-47: PNM downsampling message computes required memory oddly
- Kind: quirk
- Magic behavior: The reported "Required for non-downsampled image" divides by 4^(original downsample) after adding 1023 inside an int cast of a float expression.
- Evidence: plot/plotPNM.c:675-685
- Options: replicate (informative message)
- Recommendation: message text is informative; do not replicate exactly.

### Q-MISC-48: A plotted area of zero width or height divides by zero
- Kind: bug
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan, `docs/decisions/m10-plot-route-decision-table.md`)
- Magic behavior: `plot postscript` computes the scale as min((W − 2M)/X, (H − 2M)/Y). A box of zero width or height in the root cell, or an empty root cell with a degenerate bounding box, makes a divisor 0. The scale becomes infinite or NaN, and the later float-to-int conversions (the BoundingBox width `trunc(X·s)` and the label margins) are undefined behaviour; the header written is meaningless. `plot pnm` on the same area reports "Empty region, no plot" because it uses the bounding box of the painted tiles, so only PostScript is affected.
- Evidence: plot/plotPS.c:1165-1192 (scale and margin recomputation), plot/plotPNM.c:672-676 (the PNM empty-region check)
- Options: replicate (not possible: the result is undefined) | fix (refuse the plot with a diagnostic and write no file)
- Recommendation: fix in both modes (X-2 tier 1: undefined behaviour). When X or Y is 0, `plot postscript` reports `PLOT-PS-001` "Nothing to plot: the plotted area has zero width or height." and writes no file. No golden case; a unit test per mode covers it.

### Q-MISC-49: PNM memory arithmetic overflows and can hang
- Kind: bug
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: three defects in the PNM strip logic. (1) `pnmmaxmem` is an `int` in KB and is multiplied by 1024 in `int`, which overflows above 2 097 151 (undefined behaviour). (2) The strip height `tile_ysize = maxmem·1024 / (3·tile_xsize)` ignores the antialiasing halo; when it is at most 2·scale_over_2 the strip step `tile_ydelta` is zero or negative and the strip loop never ends. (3) The forced-downsampling loop shifts by `ds·2` with no upper bound, which is undefined for very large images.
- Evidence: plot/plotPNM.c:685-699 (forced downsampling), :705-729 (strip size and step), :686 and :705 (`PlotPNMmaxmem*1024`)
- Options: replicate (not possible: overflow and hang) | fix
- Recommendation: fix in both modes (X-2 tier 1). The arithmetic is 64-bit; `pnmmaxmem` accepts 1..4 194 304 KB (4 GiB) and reports "pnmmaxmem must be an integer 1-4194304." otherwise; the strip height is at least 2·scale_over_2 + 1 rows, letting a strip exceed `pnmmaxmem` by at most that halo when needed (the strips are Magic's whenever Magic's step is positive, so every golden case is unchanged); downsampling stops at 2^15, and a plot that still does not fit reports `PLOT-PNM-002` and writes no file. Design D18 caps the output at 65 535 pixels per side.

### Q-MISC-50: Malformed `plot` style lines
- Kind: bug
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: (1) a bare `map` line in `style pnm` reads `argv[1]` without checking `argc` (NULL dereference). (2) `style postscript` parses its numeric fields with `sscanf` without checking the result, so a garbage index or colour component is silently accepted with an indeterminate value. (3) A layer style that names a colour or pattern index never defined is not checked; the PostScript file then calls an undefined `colN` or pattern and fails when rendered. (4) A `draw` line with more than three fields and an unknown pnm keyword are silently ignored. (5) Repeating `color` or `dstyle` leaks the earlier table (not observable).
- Evidence: plot/plotPNM.c:1231 (bare `map`), :1132-1255 (keyword prefixes, `draw` handles argc == 3 only), plot/plotPS.c:240-323 (unchecked `sscanf`, list prepends)
- Options: replicate | fix (report and ignore the line; report undefined references at plot time)
- Recommendation: fix (1)–(3) in both modes (X-2 tier 1: crash, indeterminate values, invalid PostScript). A bare `map` reports `"map" lines need at least one type` at technology load and is ignored; a non-numeric index or colour component reports `"ps" line field "<f>" is not an integer` and the line is ignored; a layer style naming an undefined colour or pattern is reported once per plot (`PLOT-PS-002`) and skipped, so the file stays valid PostScript. Replicate (4) in both modes (silently ignored, as Magic; scmos and the three PDKs load with no new message). (5) needs no decision.

### Q-MISC-51: Plot devices and parameters Hefesto offers
- Kind: platform
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: `plot/plotInt.h:28` defines `VERSATEC` unconditionally, and `plotCmd.c`, `plotMain.c`, `plotPNM.c` and `plotVers.c` all include that header, so the default Magic 8.3.694 build has the PostScript, PNM and Versatec devices: `plot versatec [scale [layers]]` rasterises the area and prints it by running the `spoolcommand` (default `lp -d %s %s`) on a temporary file in `directory`; `plot pnm` sends its output to an HP RTL/HPGL2 plotter when `pnmplotRTL` is true; `plot parameters` lists and sets the HP/Versatec parameters (`cellidfont`, `cellnamefont`, `labelfont`, `directory`, `dotsperinch`, `printer`, `spoolcommand`, `swathheight`, `width`, `plottype`) and `pnmplotRTL`, printed as the `pnmplotRTL` line of the PNM group and the "HP/Versatec plotting parameters are:" group; and the `versatec` and `colorversatec` tech lines are parsed. `svg` is added when Magic is built with Cairo and the Tcl wrapper (`HAVE_LIBCAIRO && MAGIC_WRAPPER`). Only `gremlin` (`GREMLIN`) and `pixels` (`LLNL`) are compiled out; their tech lines are accepted and ignored. The comments that say every device except PostScript and PNM is removed (`plotMain.c:50-53`, `plot/Makefile.in:19-22`) are stale. The living `plot-output` spec describes this correctly.
- Evidence: plot/plotInt.h:28, plot/plotCmd.c:42 and :70-125, :250-268, :300-330 (`versatec` and the RTL width), plot/plotMain.c:306-320 (listing), :359-372 and :401-413 (parameter table), :511-600 (setters), plot/plotPNM.c:81, :139, :590, :745, :895-954 (RTL path), plot/plotVers.c:51 and :1063-1490, plot/plotGremln.c:131-217, plot/plotPixels.c:149-183
- Options: (a) Hefesto deviates (platform): options `postscript`, `pnm`, `svg`, `parameters`, `help`; no `plot versatec`, no HP RTL output, no HP/Versatec parameters (unknown names); the `versatec` and `colorversatec` tech lines accepted and ignored | (b) surface parity without a device: the option list and the HP/Versatec parameters are as Magic (listed, set, checked and stored), and `plot versatec`, or `plot pnm` with `pnmplotRTL` true, reports `PLOT-DEV-001` "Raster printer output is not supported; use plot postscript or plot pnm." and writes nothing | (c) implement Versatec and HP RTL rasterisation to a file, and spool through a subprocess on the desktop only
- Recommendation: (a), in both modes, as a documented platform deviation. Raster printers and HP plotters are obsolete; spooling means starting a process, which Hefesto does on no platform (Q-MISC-44); and (a) keeps no settings that do nothing. The cost is three golden deviations, each a masked part of the Magic output: MISC-PLOT-05 (the `pnmplotRTL` line, the HP/Versatec group and the sets of those parameters), MISC-PLOT-13 (the `versatec` lines of the help list) and MISC-PLOT-17 (the same listing lines printed after `plot parameters nosuch 1`). `plot versatec` is not run in any capture, since it would try to spool through `lp`. (b) avoids the three deviations at the price of eleven stored parameters with no effect; (c) is out of proportion for obsolete devices. The `plot-output` requirements "Plot command and plotted region", "Plot parameters" and "Raster printer and legacy plot devices" change accordingly (m10-plot-route deltas, worded as Hefesto deviations).

### Q-MISC-52: SVG content and its golden reference
- Kind: platform
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: `plot svg` redraws the window into a Cairo SVG 1.2 surface of the window's screen area, so the file is whatever Cairo emits for Magic's display drawing (stipple patterns, outlines, labels as glyph paths). It needs the `-d XR` display; the golden capture image builds Magic `--with-cairo=no` and runs `-dnull`, so MISC-PLOT-12 cannot be captured there. Hefesto has no Cairo (P-3), and its headless sessions have a window too (`view::kNullScreen`, 512 × 484 pixels).
- Evidence: graphics/grTCairo1.c:354-404, plot/plotCmd.c:270-287; tools/golden/container/Containerfile:99-105
- Options: (a) Hefesto writes its own SVG from the display scene (the layers, stipples, outlines, labels and cell outlines the window draws) at the window's pixel size; a `magic-cairo` capture variant (Magic built with Cairo under Xvfb, `-d XR`) captures MISC-PLOT-12 and `plot help`, and MISC-PLOT-12 is compared only on the file name, `width`, `height` and well-formedness | (b) as (a) without the Cairo variant; MISC-PLOT-12 becomes a Hefesto-only case | (c) imitate Cairo's output element by element
- Recommendation: (a). The drawing inside Magic's SVG is a display rendering (informative), so only the observable contract (name, size, an SVG 1.2 document) is compared. MISC-PLOT-12 sets the layout window's drawing area to 512 × 484 pixels in both tools (Hefesto's headless default; in Magic the Tk geometry of the layout window, applied and checked before `plot svg`), and `svg_shape` compares `width` and `height` as numbers after removing the unit, because Cairo's SVG surface writes them in points (`512pt`) while Hefesto writes plain pixels. The variant follows the `magic-gl` precedent (m10-3d-view design D13) and also captures the `plot help` listing (Q-MISC-51; its `versatec` lines are a documented deviation under Q-MISC-51 (a)).
- `magic-compat` ON deviation: Magic without Cairo or the Tcl wrapper treats `svg` as an invalid option, and Magic with Cairo but another display (for example `-dnull`) reports `Must be running in mode "-d XR" (CAIRO) to get SVG output.` and writes nothing. Hefesto writes the SVG with every display and in headless sessions, with `magic-compat` ON as well. This is a platform decision (P-3: no Cairo display exists in Hefesto to require), recorded as an ON-mode deviation in the `plot-output` "SVG plot" requirement; no golden case runs `plot svg` outside the Cairo variant.

### Q-MISC-53: Interrupted and failed plots leave partial files
- Kind: bug
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: both writers open the output file first and write as they go. An interrupt during `plot pnm` prints " *** interrupted ***" and leaves a truncated image; write errors are not checked, so a full disk leaves a truncated PostScript or PNM file and no message.
- Evidence: plot/plotPNM.c:819-877 (open, strips, interrupt), plot/plotPS.c:1126-1334 (no write checks)
- Options: replicate | fix (write to a temporary file in the same directory and rename on success)
- Recommendation: fix in both modes (X-2 tier 1: a truncated file that looks like a plot is corrupt output). On interrupt or write error the temporary file is removed and an existing file of that name is left unchanged; the interrupt message is Magic's; a write error reports `PLOT-IO-001`. Golden cases do not interrupt plots.

### Q-MISC-54: Byte-identical PostScript and PNM on every platform
- Kind: platform
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: the PostScript header prints single-precision values with `%.3f`; the PNM antialiasing builds a 2 049-entry Lanczos table from `double sin()` cast to `float` and accumulates in `float`. libm implementations (glibc, Apple) can differ in the last bit of `sin`, and FMA contraction changes float sums, so the bytes may differ between platforms.
- Evidence: plot/plotPS.c:1240-1262, plot/plotPNM.c:62, :90, :244-266, :984-995
- Options: (a) embed the Lanczos table as constants generated once from glibc in the capture image and checked by a unit test; compile the plot sources with `-ffp-contract=off`; format floats from the `float` value with a correctly rounded formatter | (b) compute at run time and accept per-platform differences with a tolerance
- Recommendation: (a). `plot-output` marks both formats exact, and `hefesto-output-determinism` requires byte identity on Linux x86_64, Linux aarch64 and macOS arm64.

### Q-MISC-55: `.pnm` is appended only when the whole path has no dot
- Kind: quirk
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: `plot pnm` tests the whole path for a `.`, so `plot pnm ./out` and `plot pnm dir.v2/out` write a file named `out` with no extension, while `plot pnm out` writes `out.pnm`. `plot svg` has the same rule.
- Evidence: plot/plotPNM.c:819-821, graphics/grTCairo1.c:363-368
- Options: replicate | compat (ON tests the whole path; OFF tests the last path component only)
- Recommendation: compat (standing rule: the name of the file written is observable). With `magic-compat` ON the whole path is tested, as Magic; with it OFF only the file name after the last `/`.

## view-3d

### Q-MISC-70: 3D view clips subcell tiles in the wrong coordinate system
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: Tiles are transformed to root coordinates but clipped against the search area expressed in the subcell's coordinates, so transformed subcells render clipped incorrectly (only visible with a cut box or partial area).
- Evidence: graphics/W3Dmain.c:253-278, graphics/W3Dmain.c:589 (corrected 2026-11-13: Confirmed. w3dRenderVolume transforms the tile to root coordinates (W3Dmain.c:253) and clips it with GeoClip against `cliprect` (:264 split, :278 Manhattan); :589 passes `&scx->scx_area`, which is in the subcell's own coordinates because DBTreeSrTiles applies GEOTRANSRECT(&tinv, ...) for each child (database/DBcellsrch.c:1201, :1223). The same child-coordinate rectangle is also used by GrClipTriangle for split tiles (:266), for the side segments, which are transformed and then tested and clipped against it (:316-321), and for closing cut faces, which compares root coordinates with it (:362-372). The root `scx_area` is the cut box or the window surface area grown by 1, in root coordinates (:1668-1680). The parenthesis in "Magic behavior" is wrong: without a cut box the clip area is the window's surface area, about the cell bbox fitted to the square window, so any subcell whose placement moves its tiles outside that rectangle as expressed in child coordinates is clipped too. Example: root (0,0)-(2000,100) with an instance at x+1000; the window area x ≈ -1..2001 is x ≈ -1001..1001 in child coordinates, so the instance's top and bottom faces vanish and spurious side faces appear (GeoClip on a disjoint rectangle leaves an inverted one, which :300-309 extrudes). `magic::render3d` runs `cutbox box` (tcltk/wrapper.tcl:1595), which sets a cut box only when a box exists in the rendered cell (W3Dmain.c:729-736); without one the window-area case above applies, so the usual entry point is affected either way. Mask-layer mode is not affected: :441 passes NULL, and its planes are generated flat in root coordinates.)
- Options: replicate | fix (clip in root coordinates)
- Recommendation: fix, in both modes (X-2 tier 3: display only). Hefesto transforms each tile and its outline segments to root coordinates, then clips them against the root-coordinate clip area (the cut box, or the window area grown by 1). The picture is wrong and no command returns it, so there is no Magic output to match (view-3d "Viewing transform and shading" is informative); copying it would mean porting inverted-rectangle artefacts into the mesh builder. (corrected 2026-11-13, was fix (display only); both modes and the corrected behaviour statement are now explicit)
- Resolution: fix, in both modes (X-2 tier 3: display only) — tiles of subcells and their outline segments are transformed to root coordinates and then clipped against the root-coordinate clip area (the cut box, or the window area grown by 1), including split-tile triangles, side faces and closed cut faces. Magic clips against the area in the subcell's coordinates, also without a cut box; the picture is wrong and no command returns it, so there is no Magic output to match. Approved 2026-11-13.

### Q-MISC-71: One visibility mask for two layer namespaces
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: `see` sets bits by tile type in layout mode and by mask-layer index in mask mode, using the same bit set; toggling `cif` reinterprets the bits.
- Evidence: graphics/W3Dmain.c:788-803, graphics/W3Dmain.c:1690-1695, graphics/W3Dmain.c:1767-1769 (corrected 2026-11-13: Confirmed. w3dSeeLayers uses CIFNameToMask in mask mode, which sets every mask-layer index with that name (cif/CIFmain.c:339-341), and CmdParseLayers in layout mode, both writing `crec->visible`. It is read by tile type at :1694 (TTMaskAndMask3) and by mask-layer index at :1769 (TTMaskHasType). Also: the `cif` toggle keeps the set as it is (:879-890), and `defaults` sets every bit (:1370-1371), resetting both meanings. Example: `see no MET1` in mask mode clears bit i; after `cif`, the tile type numbered i is hidden.)
- Options: replicate | fix (separate sets)
- Recommendation: fix, in both modes (X-2 tier 3). Hefesto keeps two visibility sets: tile types for layout-layer mode, and mask-layer indices of the current output style for mask-layer mode. `see` changes only the current mode's set (`see [no] name` in mask mode acts on every mask layer with that name, as CIFNameToMask does). `defaults` makes everything visible in both sets, as Magic's all-bits reset does. Because mask-layer indices change meaning when the current output style changes, the mask set is reset to all visible on a style change. No command returns the visibility set, so only pixels differ from Magic. (corrected 2026-11-13, was fix; the per-mode rules are now explicit)
- Resolution: fix, in both modes (X-2 tier 3) — two visibility sets: tile types for layout-layer mode and mask-layer indices of the current output style for mask-layer mode. `see` changes only the current mode's set (in mask-layer mode, every mask layer with that name); `defaults` makes everything visible in both sets; the mask set is reset to all visible when the current output style changes. No command returns the visibility set. Approved 2026-11-13.

### Q-MISC-72: Units of `render` heights versus extract heights
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: `render` height/thickness are used unscaled while the view is rescaled by the output scalefactor in mask mode; extract heights are converted to internal units. PDK files give render values in micrometres, so the vertical exaggeration differs between the two modes.
- Evidence: cif/CIFtech.c:1016-1026, graphics/W3Dmain.c:428-433, graphics/W3Dmain.c:847-857 (corrected 2026-11-13: the CIFtech.c lines are :1015-1027 (atof at :1015-1016, stored at :1024-1026). W3Dmain.c:428-433 (w3dRenderCIF uses cl_height and cl_thick raw) and :847-857 (w3dRescale: s_z *= f, p_z /= f, s_xy /= f) are correct. Also: extract/ExtTech.c:3963-3965 (with `units microns` extract heights are divided by dscale, giving internal units), :4090-4093 (rescaled when the internal grid changes), W3Dmain.c:396-400 (layout mode) and :1239-1241 (Magic's comment: render values "are independent of any extraction or other physical meaning"). The f rescale only cancels the change of xy unit, so a numeric h is drawn as 25·h internal units in both modes; extract h is in internal units while render h is used as written, and the PDKs write it in µm. For sky130 (scalefactor 10 nm, 0.01 µm per internal unit) metal1 shows at 25× its true height in layout mode and 0.25× in mask mode, 100 times flatter, and the factor changes with the internal grid. PDK check: sky130A cifoutput `render` values equal its extract `height` values (DIFF 0.2062/0.12 = alldiff, MET1 1.3761/0.36 = allm1), so they are µm; ihp-sg13g2 has 19 `render` lines with the same values and no extract `height` lines; gf180mcuD has no `render` lines, so its mask-layer view is flat (h = t = 0).)
- Options: replicate | fix (interpret render values as µm and convert) | compat (replicate with `magic-compat` ON, convert with it OFF)
- Recommendation: compat (standing rule; Magic produces a consistent, if flat, picture and copying it costs one multiplier). With magic-compat ON, `render` height and thickness are used as stored, as Magic does. With it OFF, they are taken as µm and converted to internal units at draw time by dividing by the µm per internal unit of the current output style, cs_scaleFactor / (100 · cs_expander) (cif/CIFint.h:264-276), so both modes show the same exaggeration. Reading `render` values as µm rests on PDK convention only (Magic's comment at W3Dmain.c:1239-1241 gives them no physical meaning; sky130A and ihp-sg13g2 write µm). A mask layer with no `render` line has no physical height: it is drawn flat (h = t = 0) in both settings, and the µm-based extensions report it (a warning naming the layers) instead of inventing a height. In both modes the `render` statement stores the values as given, `render name` returns them unchanged, and `zoom` returns the specified s_z (the conversion applies to h and t, not to s_z), so the exact "Default view" scenarios hold. Hefesto's extensions (cross-section heights, glTF/STL export) always use physical µm, whatever the mode. (corrected 2026-11-13, was replicate numerically for now; revisit with visual comparison)
- Resolution: compat (X-2 tier 2) — with `magic-compat` ON `render` height and thickness are used as stored, as Magic does; with it OFF they are taken as µm and converted to internal units at draw time by the current output style's µm per internal unit, cs_scaleFactor / (100 · cs_expander), so both modes show the same exaggeration. A mask layer with no `render` line is drawn flat (h = t = 0) in both settings and the µm-based extensions report it instead of inventing a height. The `render` statement stores values as given, `render name` returns them unchanged and `zoom` returns the specified s_z in both modes. Hefesto's extensions (cross-sections, glTF/STL export) always use physical µm. Approved 2026-11-13.

### Q-MISC-73: `cif` toggle with no output style
- Kind: bug
- Status: approved (resolved 2026-11-07; amendment drafted 2026-11-13, approved 2026-11-13; resolution changed by the user 2026-11-13)
- Magic behavior: The window starts in mask mode even if no cifoutput style exists; toggling back dereferences the missing style.
- Evidence: graphics/W3Dmain.c:879-890, graphics/W3Dmain.c:1502-1503 (corrected 2026-11-13: both are correct, but the same missing style is dereferenced on paths the Resolution does not cover: opening the window, Set3DDefaults :1374-1376 (`crec->cif` is TRUE from :1503, so a window opened with no output style crashes before any toggle); the first redraw, W3DCIFredisplay :1745 (`CIFCurStyle->cs_radius`); and the `render` command, w3dRenderValues :1257 and :1297.)
- Options: fix (start in layout mode when no style; ignore toggle)
- Recommendation: fix.
- Proposed amendment (drafted 2026-11-13, approved 2026-11-13; still X-2 tier 1, NULL dereference): the window opens in layout-layer mode when there is no current output style; `cif` keeps layout mode and reports it; `render` reports "No CIF output style set!" (the message CIFNameToMask already prints, cif/CIFmain.c:334).
- Resolution: fix (user decision 2026-11-13; amends the resolution of 2026-11-07, which covered only the toggle) — with no current output style the 3D window opens in layout-layer mode, `cif` keeps layout-layer mode and reports it, and `render` reports "No CIF output style set!", instead of dereferencing the missing style when the window opens (Set3DDefaults), on the first redraw or in `render` (X-2 tier 1, NULL dereference). Approved 2026-11-13.

### Q-MISC-74: 3D window size fixed at 500 x 500
- Kind: bug
- Status: approved 2026-11-13; aspect rule for tall windows amended, approved 2026-11-21
- Magic behavior: The aspect correction and viewport use the initial 500×500 size even after the window is resized.
- Evidence: graphics/W3Dmain.c:1496-1497, graphics/W3Dmain.c:532-533, graphics/W3Dmain.c:955-958 (corrected 2026-11-13: Confirmed: initial size :1496-1497, the aspect glScalef and glViewport :532-533, the refresh area :955-958. Also: Tk_GeometryRequest 500×500 (:1566), TOGLEventProc handles StructureNotify (:1579), and graphics/grTOGL1.c:851-876 (ConfigureNotify calls WindReframe on the MagWindow but never updates `crec->width`/`height`). `render3d` packs the window with `-expand true -fill both` (tcltk/wrapper.tcl:1613), so a resize is routine; after one, drawing and refresh stay in the lower-left 500×500 area and the aspect stays 1.)
- Options: fix (use actual size)
- Recommendation: fix, in both modes (X-2 tier 3; a fixed viewport is meaningless in Raven or Metal hosts and on iPadOS). Hefesto uses the drawable's current size for the viewport and the refresh area, with A = H_w / W_w from that size. The initial size request stays 500 × 500. (corrected 2026-11-13, was fix; scope made explicit)
- Amendment (found by CAMERA in M10 round 1; approved 2026-11-21): Magic's A = diag(H_w/W_w, 1, 1) is right for W_w ≥ H_w only; for a window taller than wide it leaves the x extent at 1/1.1 of the height, so the plan view overflows the width. As built (view3d/camera), A = diag(H_w/W_w, 1, 1) when W_w ≥ H_w, else diag(1, W_w/H_w, 1): units stay 1:1 and the limiting dimension is fitted (about 91 % of it in the default view). No command returns A, and Magic's own window is square (500 × 500) until it is resized, which is the case this entry fixes.
- Resolution: fix, in both modes (X-2 tier 3) — the viewport, the refresh area and the aspect use the drawable's current size W_w × H_w: A = diag(H_w/W_w, 1, 1) when W_w ≥ H_w, else diag(1, W_w/H_w, 1); the initial size request stays 500 × 500. Approved 2026-11-13; the aspect rule for tall windows (the second case) amended and approved 2026-11-21 (J1; was "the aspect A = H_w / W_w").

### Q-MISC-75: 3D view depends on OpenGL, Tk and GLX
- Kind: platform
- Magic behavior: Available only with the OGL display and Tk; refused otherwise.
- Evidence: graphics/W3Dmain.c:1474-1480, graphics/W3Dmain.c:1540-1586
- Verified 2026-11-13: :1474-1480 is correct; the Tk window creation starts at about :1538. Nothing to change.
- Options: platform alternative (Metal/Vulkan/OpenGL abstraction; touch gestures)
- Recommendation: implement natively on all platforms including iPadOS.
- Resolution: platform — the 3D view is implemented natively on every platform, iPadOS included, through the rendering interface (Vulkan on desktop, Metal on iPadOS) (P-3, P-13, `view-3d`, 2026-11-07).

### Q-MISC-76: 3D redisplay mode is global while the mode flag is per window
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the `cif` toggle sets the client's `w_redisplay` procedure, which is registered once as W3DCIFredisplay and never reset when a window opens, while each new window starts with `cif = TRUE` and a mask-scaled view. After a 3D window is closed in layout-layer mode, the next one draws layout layers with a mask-mode (×f) view until `cif` is toggled.
- Evidence: graphics/W3Dmain.c:881, :887 (toggle sets w_redisplay), :1845-1846 (registration), :1503 and :1374-1376 (a new window starts in mask mode with the mask rescale)
- Options: replicate | fix (the mode belongs to the window)
- Recommendation: fix, in both modes (X-2 tier 3: display only; no command returns the redisplay procedure). The rendering mode is per-window state, and a new window draws in the mode it reports.
- Resolution: fix, in both modes (X-2 tier 3: display only) — the rendering mode is per-window state, and a new window draws in the mode it reports. Approved 2026-11-13.

### Q-MISC-77: 3D view has no depth test
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: GL_DEPTH_TEST is never enabled and w3dUnlock disables it, although the depth buffer is cleared. Occlusion follows draw order (style-table order in layout-layer mode, mask-layer order in mask-layer mode) plus back-face culling, so in tilted views lower layers can paint over higher ones.
- Evidence: graphics/W3Dmain.c:105 (glDisable(GL_DEPTH_TEST)), :465 (depth buffer cleared); graphics/W3D*.c never enables GL_DEPTH_TEST
- Options: replicate | fix (depth-buffered rendering)
- Recommendation: fix, in both modes (X-2 tier 3). Faces are depth-tested. Pixels are informative in view-3d ("Viewing transform and shading"), no command observes the draw order, and the stack view of `hefesto-3d-visualization` needs correct occlusion. A tile type that belongs to several display styles is drawn once per style, in style-table order, as Magic does (W3Dmain.c:1692-1702; in sky130A, contacts such as `pc` and `mcon` are in two styles); the coplanar duplicates use a less-or-equal depth test, so the last style drawn wins, as in Magic's draw order. (amended 2026-11-13 after review: the multi-style rule)
- Resolution: fix, in both modes (X-2 tier 3) — faces are depth-tested. A tile type in several display styles is drawn once per style in style-table order, as Magic does, with a less-or-equal depth test, so the last style drawn wins on coplanar faces. Approved 2026-11-13.

### Q-MISC-78: 3D view rescale uses the current output style's scalefactor
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the `cif` toggle rescales the view by `CIFCurStyle->cs_scaleFactor`, the current style at the time of the toggle, not the one applied when the window entered mask-layer mode. After `cif ostyle` changes to a style with another scalefactor while the 3D window is in mask-layer mode, toggling to layout-layer mode divides by f_new instead of f_old, so the values `view`, `scroll` and `zoom` return (trans, s_xy, s_z) drift by f_old/f_new, and so does the drawn picture. `defaults` in mask-layer mode also uses the current f.
- Evidence: graphics/W3Dmain.c:883, :890 (rescale by the current style's f), :1374-1376 (Set3DDefaults), :847-857 (w3dRescale)
- Options: replicate | compat (replicate ON; remember the applied f OFF)
- Recommendation: compat (standing rule; the drift is visible in command outputs, so ON copies it). With magic-compat ON, `cif` rescales by the current output style's f, as Magic. With it OFF, the window records the f it applied when it entered mask-layer mode and divides by that same f when it leaves, and a change of output style while in mask-layer mode rescales the view from f_old to f_new so the picture keeps its size.
- Resolution: compat (X-2 tier 2) — with `magic-compat` ON `cif` and `defaults` rescale by the current output style's scalefactor, as Magic does, so `view`, `scroll` and `zoom` drift after `cif ostyle` changes the style; with it OFF the window records the f it applied on entering mask-layer mode and divides by that f on leaving it, and a change of output style while in mask-layer mode rescales the view from f_old to f_new. Approved 2026-11-13.

### Q-MISC-79: 3D view draws a split tile with its left type's height, also when the left half is space
- Kind: quirk
- Status: approved 2026-11-21 (J3; found by MESH in M10 round 1, PR #77 review)
- Magic behavior: w3dRenderVolume takes h and t from ExtGetZAxis, which reads the tile's left type only (`TiGetLeftType`, "Ignore non-Manhattan for now"), and draws whichever triangle is being painted with them. When the left half of a split tile is space and the right half is, say, metal1, the metal1 triangle gets space's h = t = 0: it is drawn flat at z = 0 with no side faces (W3Dmain.c:288, `ztop == zbot`), below the rest of the metal1 layer. When both halves are material, the right triangle is drawn at the left type's height.
- Evidence: graphics/W3Dmain.c:383-412 (w3dRenderVolume, ExtGetZAxis at :396, no sides when ztop == zbot at :288), extract/ExtTech.c:584-602 (ExtGetZAxis uses TiGetLeftType)
- Options: replicate | compat (replicate ON; each triangle's own type OFF)
- Recommendation: compat (standing rule; the picture is consistent, and the view-3d requirement "Layer height and thickness for the layout-layer view" states the left-type rule as Magic's). With magic-compat ON, both triangles of a split tile take the left type's h and t, as Magic; a split tile whose left half is space draws its right triangle flat at z = 0 with no side faces. With it OFF, each triangle takes the h and t of its own type, which decide its height, thickness and side faces.
- Resolution: compat (X-2 tier 2) — with `magic-compat` ON a split tile's triangles are drawn with the h and t of its left type, as Magic does (a left half of space gives a flat triangle at z = 0 with no side faces); with it OFF each triangle uses the h and t of its own type for its height, thickness and side faces. No command returns the heights. Approved 2026-11-21.

### Q-MISC-80: reopening the 3D window after `closewindow`
- Kind: bug
- Status: approved 2026-11-22 (J5, revised after the MISC-3D-12 capture; found by CAMERA/WIND3D planning in M10 round 1); the Magic behaviour below was disproved by the MISC-3D-12 capture, see "Capture" and the revised recommendation
- Magic behavior (superseded: disproved by the MISC-3D-12 capture, see "Capture"; kept for the record): W3Ddelete frees the client record and clears `w3dWindow`, but leaves the Tk window alive (`Tk_DestroyWindow` is commented out) and does not delete the `.magic3d` Tcl command made by MakeWindowCommand. A later `specialopen wind3d` on the default path calls Tk_CreateWindowFromPath for `.magic3d`, which fails because the path exists, so Magic reports "Could not create a new Tk window" and opens nothing. Meanwhile `.magic3d <cmd>` reaches the freed client record (a use-after-free).
- Evidence: graphics/W3Dmain.c:1617-1631 (W3Ddelete), :1548-1551 (Tk_CreateWindowFromPath of `.magic3d`), :1584 (MakeWindowCommand), :1595-1596 (the refusal); what actually runs on close: windows/windMove.c:228 (WindDelete calls GrDeleteWindowPtr after the client's delete), graphics/grTOGL1.c:1027 and :1256-1268 (GrTOGLDelete: Tcl_DeleteCommand on the path, Tk_DestroyWindow)
- Options: replicate | compat (refuse ON; reopen OFF) | fix
- Recommendation (superseded by the revised recommendation below): compat for reopening (the refusal is an observable command result, so ON copies it); the use-after-free is fixed in both modes (standing rule: crashes and undefined behaviour are fixed in both modes). With magic-compat ON, after `.magic3d closewindow`, `specialopen wind3d` on the default path reports "Could not create a new Tk window" and opens nothing, and `.magic3d <cmd>` no longer reaches a freed window: it is refused and changes nothing (Magic has no defined output there to match; WIND3D chooses the message and lists it in `docs/diagnostics/codes/view3d.md`). With it OFF, closing removes the toolkit path and its command, and `specialopen wind3d` opens a new window.
- Open point (M10 round 2, GOLDEN and INTEGRATION): the Magic behaviour above may be wrong. `.magic3d closewindow` runs w3dClose → WindDelete (windows/windMove.c:228), which after W3Ddelete calls GrDeleteWindowPtr = GrTOGLDelete under the Tk OGL display (graphics/grTOGL1.c:1027); GrTOGLDelete runs Tcl_DeleteCommand on the path and Tk_DestroyWindow (graphics/grTOGL1.c:1256-1268). The commented-out Tk_DestroyWindow in W3Ddelete is therefore not the last word: the command is deleted, no use-after-free is reachable through it, and a second `specialopen wind3d` on the same path probably succeeds. MISC-3D-12 captures what Magic does; if it reopens, ON should reopen too and this entry becomes a replicate (no compat).
- Capture (M10 round 2, golden-capture run 44965506225, MISC-3D-12 on `.render.magic`): after `closewindow`, `info commands .render.magic` is empty, a second `specialopen wind3d c3d .render.magic` opens the window (its result is the same `isn't a top-level window` text as the first open, and `info commands` lists the path again), and a third `specialopen` on `.render.magic2` is refused with "Only one 3D window allowed.". Magic does not refuse, and no use-after-free is reachable through the command.
- Revised recommendation (INTEGRATION, M10 round 2; approved by the user 2026-11-22): replicate, in both modes. Closing deletes the toolkit-path command and a later `specialopen wind3d` on the same path opens a new window. As implemented on `m10-3d-round2` (view3d/client/client.cpp `retire_path`); MISC-3D-12 replays with no deviation. MISC-3D-12 opens the window at `.render.magic`, not at the default `.magic3d` the view-3d scenario "Reopening after closewindow" names: with `.magic3d` (an unmapped toplevel) the magic-gl captures crashed Magic at random (exit 139, tools/golden/authoring/m10_cases.py). Both paths end in the same GrTOGLDelete; the `.magic3d` sequence is covered by the unit test "a closed 3D window's toolkit path is no command and reopens in both modes".
- Resolution: replicate, in both modes (X-2; revised after the MISC-3D-12 capture, was compat for reopening and fix for a use-after-free) — `closewindow` deletes the toolkit-path command, as Magic's GrTOGLDelete does, so `info commands <path>` is empty and calling it is "invalid command name"; a later `specialopen wind3d` on the same path opens a new window. No refusal and no reachable use-after-free. Approved 2026-11-22 (J5 revised).

### Q-MISC-81: depth-tested 3D faces put lower layers in front in the top view
- Kind: quirk
- Status: approved 2026-11-22 (R1; found by RENDER3D in M10 round 2, PR #81 review)
- Magic behavior: Magic emits each face counter-clockwise seen from outside with z_top = −h·s_z, culls back faces and never enables a depth test (Q-MISC-77), so draw order decides what shows: the later display style, usually metal over poly. The face orientation puts the viewer of the top view on the physical underside (higher layers have more negative z), so with the depth test Q-MISC-77 adds, Magic's orientation and depth sense show lower layers in front of higher ones where they overlap, the opposite of Magic's usual picture.
- Evidence: graphics/W3Dmain.c:101-105 (culling and depth test off at setup), :526-528 (glEnable(GL_CULL_FACE), glCullFace(GL_BACK)), :234-446 (faces, z = −h·s_z), :471-544 (viewing transform); tests/data/render/mesh3d/top.txt (round-2 reference pick map: lower layers in front)
- Options: replicate (Magic's orientation with the depth test: lower layers in front) | compat (ON keep, OFF flip) | fix (flip the depth and culling sides in both modes)
- Recommendation: fix, in both modes (X-2 tier 3: pixels are informative and no command returns the draw order; Q-MISC-77 already leaves Magic's draw-order picture in both modes, so a per-mode split would copy a picture Magic never shows). The depth comparison and the culled side are flipped so higher layers are in front in the top view; RENDER3D updates the reference pick maps.
- Resolution: fix, in both modes (X-2 tier 3) — the 3D view flips the depth and culling sides so that higher layers are in front in the top view (metal1 over poly where they overlap), as Magic's draw order usually shows; Magic's face orientation with Q-MISC-77's depth test would show lower layers in front. No command output changes. Approved 2026-11-22.

### Q-MISC-82: layout commands a script runs after `specialopen wind3d` go to the 3D window
- Kind: quirk
- Status: approved 2026-11-22 (N1; found by GOLDEN and INTEGRATION in M10 round 2 replaying MISC-3D-05 and MISC-3D-10)
- Magic behavior: once the 3D window is open, the commands a script runs are dispatched with the current window ID, which is then the 3D window's, so layout commands reach the 3D client: `box` and `load` fail with an empty error (W3Dcreate clears WIND_COMMANDS, so the 3D window knows only its own commands), and `cif ostyle half` reaches the 3D window's `cif`.
- Evidence: textio/txCommands.c:1491-1540 (TxTclDispatch: `tx_wid = txCurrentWindowID`, WindSendCommand), windows/windSend.c:186 (WindCurrentWindow); golden-capture run 44965506225 (MISC-3D-05, MISC-3D-10; `m10_cases.REPLAY_GAPS`)
- Options: replicate | compat (ON route to the 3D window as Magic; OFF to the layout window)
- Recommendation: compat (standing rule; the command results differ, so ON copies them). With magic-compat ON, once a 3D window is open the layout commands a script runs (`box`, `load`, `cif ostyle`, ...) go to the 3D window, as Magic does; with it OFF they go to the layout window. WIND3D implements it; MISC-3D-05 and MISC-3D-10 then replay with magic-compat ON.
- Resolution: compat (X-2 tier 2) — with `magic-compat` ON, after a command sent through the 3D window's toolkit path (Magic's `_tk_dispatch` `TxSetPoint`) and while that window is open, the layout commands a script runs (`box`, `load`, `cif ostyle`) go to the 3D window, as Magic does (`box` and `load` fail there; `cif ostyle half` reaches the 3D `cif`); with it OFF they go to the layout window. Opening the window alone does not route them (`specialopen` never calls `TxSetPoint`; MISC-3D-05 sends `.render.magic cutbox ...` before `box`). A command on a layout window's path, a key or button in a layout window (Magic's `TxSetPoint` on every layout event) and closing the 3D window route them back; `setpoint x y <window>` makes the named window current (windCmdSZ.c:286, standing rule, PR #86 review). Cited by MISC-3D-05 and MISC-3D-10. Approved 2026-11-22.

### Q-MISC-83: a split tile cut by the clip area is closed along its clipped bounding rectangle
- Kind: bug
- Status: approved 2026-11-22 (N2; found by MESH in M10 round 2, PR #81 review; already implemented)
- Magic behavior: where the clip area (the cut box, or the window area grown by 1) cuts a split tile, w3dRenderVolume closes the cut along the edges of the tile's bounding rectangle clipped to the area, because it tests the rectangle `r`, not the triangle, so the cut face also spans the part of the rectangle the triangle does not cover and the solid is not closed there.
- Evidence: graphics/W3Dmain.c:360-378
- Options: replicate | compat (ON the clipped bounding rectangle; OFF the triangle's own cut)
- Recommendation: compat (standing rule; the face is part of the geometry the GL capture compares, design D13). With magic-compat ON the cut of a split tile is closed along its bounding rectangle clipped to the area, as Magic; with it OFF along the triangle's own cut.
- Resolution: compat (X-2 tier 2) — with `magic-compat` ON a split tile cut by the clip area is closed along the edges of its bounding rectangle clipped to the area, as Magic does (W3Dmain.c:360-378); with it OFF along the triangle's own cut. Implemented in M10 round 2 (view3d/mesh `rect_clip_edges`). Approved 2026-11-22.

### Q-MISC-84: the desktop 3D tab uses Magic's GL-state lighting and a top-down default
- Kind: platform (product)
- Status: approved 2026-11-24 (V1; user request of 2026-11-24 after the round-3 desktop capture of `sky130_fd_sc_hd__dfxtp_1`, sky130A, magic-compat ON)
- Magic behavior: Magic's 3D window clears to the background colour of the display styles (`STYLE_TRANSPARENT`) and lights every face with fixed-function GL state: no normal is ever set, so every face has the normal (0, 0, 1); light #0 is positional at (50, 50, 50) in model coordinates, plus ambient 0.6 (GL's global 0.2 and light #0's 0.4); lighting is per vertex. It opens top-down, with the view angles (0, 0, 0). With R2 (approved 2026-11-22) Hefesto copied this lighting with magic-compat ON. In the desktop tab the result is far from the approved board (`docs/design/mockups/View3D.dc.html`, render `docs/images/ui/view3d.png`): a light grey background, pastel faces that fade to white across a face (the positional light on a constant normal), side faces as light as top faces so edges do not read, and a plan view in which the stack's depth is not visible.
- Evidence: graphics/W3Dmain.c:449-466 (w3dClear: the `STYLE_TRANSPARENT` colour), :505-523 (glEnable(GL_LIGHTING), light #0 positional, light #1 directional, no glNormal anywhere in W3D*.c), :1332-1380 (default view angles (0, 0, 0)); the user's screenshot of 2026-11-24 (desktop 3D tab, magic-compat ON); `docs/images/ui/view3d.png`
- Options: keep (R2 as approved: ON Magic's lighting, OFF directional; top-down on opening) | platform (the desktop tab renders the board look in both modes and opens on the isometric preset; Magic's lighting is not used by the desktop tab)
- Recommendation: platform, in both modes. view-3d "Viewing transform and shading" is `Compatibility: informative`, no command returns a pixel, and the GL face log (MISC-3D-13..16) logs geometry only, so the look of the desktop tab is a product decision, not a compat one; the board is the approved look.
- Resolution: platform, in both modes (user decision V1, 2026-11-24, recommended option) — the desktop 3D tab renders with the approved board look regardless of magic-compat: the background is the desktop theme's canvas token (`ThemeTokens::canvas`, dark in both themes); faces get flat per-face directional shading evaluated on each face's own outward normal in the displayed scene, so top faces are lighter and side faces darker and edges read; colours are the display style's colour, saturated, never washed to white (no lit channel exceeds the style colour by more than the top-face factor); on opening, the tab shows the isometric preset from above, framed to the cell, with an x/y/z axis gizmo and the presets Isometric, Top, Front and Side. Magic's GL-state lighting (normal (0, 0, 1), positional light at (50, 50, 50), ambient 0.6) is no longer used by the desktop tab; RENDER may keep it only where a headless test needs it (nothing in the golden face log depends on lighting). Magic's pixels stay informative. The window's `ViewState` still reports what the user sees: the tab applies the isometric view by running the same `view`/`zoom`/`scroll` commands a preset runs, so `view` prints the isometric angles; `defaults`, a fresh `specialopen wind3d` from a script or hefesto-cli and the golden MISC-3D cases keep Magic's top-down default view (view-3d "Default view" unchanged). As landed (round-4 FIX): the tab applies the isometric camera only to a window on `render3d`'s toolkit path `.render.magic`, which is how the 3D tab and Options > 3D Display open it; a window on any other path keeps the top-down view. Amends R2 (approved 2026-11-22, "Round-3 decisions without a Q row") for the desktop tab. Approved 2026-11-24.
- Amended 2026-11-25: the user approved the round-4 choices that were outside this Resolution (items A4, A5 and A6 of "Round-5 decisions without a Q row"): the opening z scale is s_z = 1 in layout-layer mode and s_z = f in mask-layer mode (`kBoardScaleZ`; see the 2026-11-26 amendment for mask-layer mode with magic-compat ON); the renderer draws the displayed scene, the model mirrored in z, with CAMERA's matrix, so tz moves along the displayed height, while Magic's `view`, `scroll` and `zoom` outputs, the window's view state and the GL face log are unchanged; the isometric opening applies to any window the desktop opens with `render3d` (toolkit path `.render.magic`), and `specialopen wind3d`, scripts and hefesto-cli keep the top-down default.
- Amended 2026-11-26: in the desktop tab only, in mask-layer mode with magic-compat ON, the board z scale the tab sets is additionally multiplied by 1 / (µm per internal unit), because Q-MISC-72 keeps the `render` heights as stored while x and y are in mask units, which drew sky130A's mask-layer stack almost flat; with magic-compat OFF it stays `kBoardScaleZ` × f. Magic's own s_z values (the window state Magic's commands set, `defaults`, the golden cases, scripts and hefesto-cli windows) are untouched (item B1 of "Round-6 decisions without a Q row").

### Q-MISC-85: wells, implants and other non-conducting area layers hide the devices in the 3D view
- Kind: platform (product)
- Status: approved 2026-11-24 (V2; same user request as Q-MISC-84)
- Magic behavior: Magic draws every visible type opaque (the translucent styles blend over the background, not over other faces, and R3 draws them opaque), so well, deep-well and other large area layers are drawn as opaque slabs; in Hefesto's depth-tested view (Q-MISC-77, Q-MISC-81) the nwell slab hides the devices in it from most angles. The layer panel's transparency control (100 / 50 / 20 %, outside Magic's state) starts at 100 % for every layer. The board draws the substrate ghosted.
- Evidence: graphics/W3Dmain.c:406, :439 (every solid or stippled style drawn the same way); the user's screenshot of 2026-11-24 (the green nwell slab over the cell); `docs/design/mockups/View3D.dc.html` (substrate row "ghosted")
- Options: keep (every layer opaque by default) | platform (non-conducting area layers ghosted by default in the desktop tab, per-row override)
- Recommendation: platform, in both modes (a Hefesto view default outside Magic's state; `see` and every command output unchanged).
- Resolution: platform, in both modes (user decision V2, 2026-11-24, recommended option) — in the desktop 3D tab a stack layer whose class is non-conducting area is ghosted by default: drawn with alpha 0.2 (the transparency control's 20 % step) in the transparency pass, with no depth write and no pick id, so the devices under it show and a click picks them. Layer-class rule (the board's reading): a layer of the layout-layer stack is ghosted by default when its tile type is not a contact type and lies on a plane that holds no residue of any contact type of the technology (the `contact` section; a stacked contact counts with its full residue set); every other layer (diffusions, poly, local interconnect, metals, MiM caps, contacts and vias) is opaque. For sky130A this ghosts the dwell plane (dnwell, isosubstrate, photodiode), the well plane (nwell, pwell, rpw, obswell, pbase, nbase) and the comment plane; its block plane stays opaque because it holds `ubm`, a residue of the `pi2` contact. For gf180mcuD it ghosts the dwell, well, poly and comment planes (its `poly` plane holds only the fill type `fillpoly`, which no contact touches; poly itself is on the active plane). The user changes a layer with the row's transparency control (100 / 50 / 20 %); a value the user set is kept for the window across rebuilds and outside Magic's state, so `see` and the visibility sets are unaffected. In phase 1 the tab draws the layout-layer stack in both window modes (mask-layer mode scales x and y by f), so mask-layer mode ghosts the same layers; a mask-layer stack (phase 2) has no planes and ghosts nothing by default. The board's synthetic substrate slab is not built (deferred). Approved 2026-11-24.
- Amended 2026-11-25: the user approved the round-4 readings of the class rule (items A7, A8 and A9 of "Round-5 decisions without a Q row"): in phase 1, mask-layer mode ghosted the layout layers of the layout-layer stack the tab then drew (historic: superseded by the 2026-11-26 amendment); gf180mcuD's `poly` plane, which holds only `fillpoly`, is ghosted by the class rule; a technology with no contact type ghosts nothing.
- Amended 2026-11-26: mask-layer mode ghosts nothing (the tab draws the mask-layer stack, which has no planes; item B2); the deferred substrate slab is built and on by default in the desktop tab, with magic-compat ON and OFF (items B3, B7, B16: thickness 0.2 of the drawn stack's height, colour #2A2F37, not pickable, never exported, none for a flat stack) ("Round-6 decisions without a Q row").

### Q-MISC-86: `setpoint x y <window>` naming the 3D window makes it current for script commands
- Kind: quirk
- Status: approved 2026-11-25 (item A1; found in the PR #86 review, implemented in the round-3 review fix pass, PR #91, under the standing rule)
- Magic behavior: `setpoint x y <window ID|name>` calls `TxSetPoint` with the named window, which makes it the current window, so the commands a script runs next are dispatched to that window's client; naming the 3D window's toolkit path sends later layout commands to the 3D client, where `box` and `load` fail (the trigger Q-MISC-82 describes for a command on the 3D window's path). Magic also accepts a numeric window ID there.
- Evidence: windows/windCmdSZ.c:214-289 (windSetpointCmd; `TxSetPoint(x, y, wid)` at :286; usage `[x y [window ID|name]]`), textio/txCommands.c:1491-1540 (TxTclDispatch dispatches with the current window ID); tests `tests/unit/view3d/client/routing_test.cpp` "setpoint naming a window switches where script commands go with magic-compat ON" and "setpoint naming the 3D window leaves script commands in the layout window with magic-compat OFF", `tests/unit/ui/tools_test.cpp` "setpoint forces the point and pushbutton clicks there", "setpoint into a color window sends the next commands to the color client"
- Options: replicate | compat (ON route as Magic; OFF never route to the 3D window)
- Recommendation: compat (standing rule; the command results differ, as for Q-MISC-82).
- Resolution: compat (X-2 tier 2) — with `magic-compat` ON, `setpoint x y <window>` naming the open 3D window's toolkit path makes it current, so the layout commands a script runs next go to the 3D window, as Magic's `TxSetPoint` does (windCmdSZ.c:286); naming any other window (a layout window's path or ID, a color window) makes that window current and sends them back; `setpoint` without a window argument keeps the current window. With it OFF `setpoint` never routes script commands to the 3D window. This is the `setpoint` trigger of Q-MISC-82's routing (same route, `Session::script_target_3d`); Q-MISC-82 keeps the toolkit-path trigger. Known gap (item A2): a numeric window ID naming the 3D window is not supported (Hefesto's 3D window has no layout-window ID; such an ID names no window and the route goes back to the layout window), in both modes. Already implemented (round-3 review fix pass, PR #91; `ui/tool_commands.cpp`) and tested per mode (view-3d "3D view commands", scenario "setpoint names the window script commands go to"). Approved 2026-11-25.

### Q-MISC-87: without a cut box, the 3D mask layers are generated over the cell's bounding box
- Kind: quirk
- Status: approved 2026-11-26 (item B18; found by MASKSRC in M10 round 5, already implemented and tested per mode)
- Magic behavior: W3DCIFredisplay generates the CIF planes over the cut box or, without one, over the cell's bounding box: it grows only the search area by the style's interaction radius (`scx.scx_area`) and passes the ungrown rectangle to CIFGen as the clip, so a layer grown past the cell's edge (a well around a device at the edge) is cut at the bounding box.
- Evidence: graphics/W3Dmain.c:1744-1745 (clipRect = cut box or `cd_bbox`; GEO_EXPAND into the search area only), :1757 (CIFGen with clipRect); test `tests/unit/view3d/client/mask_source_test.cpp` "without a cut box magic-compat ON clips the mask layers to the cell's bounding box" and "without a cut box magic-compat OFF generates the bounding box grown by the halo"
- Options: replicate | compat (ON the cell bounding box; OFF the bounding box grown by the interaction radius)
- Recommendation: compat (standing rule; the drawn and exported geometry differs at the cell edge).
- Resolution: compat (X-2 tier 2) — without a cut box, the 3D view's mask layers are generated over the cell's bounding box with `magic-compat` ON, as Magic (W3Dmain.c:1744), so a layer grown past the cell's edge is cut there; with it OFF over the bounding box grown by the output style's interaction radius, so nothing is cut at the edge. With a cut box both modes generate over the cut box; a cut box outside the cell gives an empty model. Already implemented (round 5, `view3d::MaskSource`) and tested per mode (view-3d "Rendered geometry", scenario "Grown mask layer at the cell edge"). Approved 2026-11-26.

### Q-MISC-88: the 3D view draws temporary (`templayer`) mask layers
- Kind: quirk
- Status: approved 2026-11-26 (item B19; found by MASKSRC in M10 round 5, already implemented and tested per mode)
- Magic behavior: W3DCIFredisplay draws every CIF layer of the current output style whose visibility bit is set, with no test for `CIF_TEMP`, so the temporary layers a style defines with `templayer` (intermediate results, not masks) are drawn like mask layers.
- Evidence: graphics/W3Dmain.c:1767-1774 (the loop over `cs_nLayers`, no `CIF_TEMP` test); tests `tests/unit/view3d/client/mask_source_test.cpp` "temporary layers are drawn with magic-compat ON, as Magic, and left out OFF" and `tests/unit/view3d/export/command_test.cpp` "`export3d -all` in mask-layer mode: temporary layers are exported with magic-compat ON only"
- Options: replicate | compat (ON drawn; OFF left out)
- Recommendation: compat (standing rule).
- Resolution: compat (X-2 tier 2) — with `magic-compat` ON the 3D view draws temporary mask layers, as Magic draws every CIF layer (W3Dmain.c:1767); with it OFF they are left out of the view and of `export3d`, also with `-all`. Already implemented (round 5) and tested per mode (view-3d "Rendered geometry", scenarios "Temporary mask layer drawn with magic-compat ON" and "Temporary mask layer left out with magic-compat OFF"; hefesto-3d-visualization "3D export", scenarios "Temporary layers with -all, magic-compat ON" and "... OFF"). Approved 2026-11-26.

### Q-MISC-89: mask generation errors during a 3D mask-layer build
- Kind: quirk
- Status: approved 2026-11-26 (item B20; found by MASKSRC in M10 round 5; CORE implements the magic-compat ON report in round 6)
- Magic behavior: W3DCIFredisplay sets `CIFErrorDef` to the rendered cell before CIFGen, so every generation error found while the 3D window redraws is added as feedback to that cell (CIFError, DBWFeedbackAdd), again on every redraw, so the same errors pile up as duplicate feedback areas.
- Evidence: graphics/W3Dmain.c:1749 (CIFErrorDef = cellDef), cif/CIFmain.c:409-432 (CIFError adds feedback when CIFErrorDef is set); `src/core/src/view3d/mesh/planes.cpp` (`report_errors = false` as built in round 5)
- Options: replicate (feedback on every redraw) | compat (ON report the errors once per build to the console; OFF not reported)
- Recommendation: compat. Copying feedback on every redraw duplicates feedback entries without bound, which no user wants, and feedback from a view would mix with `cif` feedback; the errors themselves are Magic's output, so ON reports them once per build.
- Resolution: compat (X-2 tier 2) — with `magic-compat` ON the generation errors of a 3D mask-layer build (a full build or an incremental rebuild) are reported once per build to the console, with the generator's messages, and are not added as feedback; with it OFF they are not reported (as built in round 5). Magic adds them as feedback on every redraw. CORE implements the ON report in round 6 (view-3d "Rendered geometry", scenarios "Mask generation errors reported once per build with magic-compat ON" and "Mask generation errors not reported with magic-compat OFF"). Approved 2026-11-26.
- As built (round 6 review fix of PR #133): an incremental rebuild reports the errors of the areas it regenerates (scenario "Mask generation errors of an incremental rebuild reported with magic-compat ON"); the same message from the pieces of one regenerated area counts once (items C2, C3 below, proposed); hefesto-cli's 3D face log (HEFESTO_GL_CAPTURE) reports through the dispatcher, as the desktop tab does.

### Round-2 decisions without a Q row (approved 2026-11-21)

Native or Hefesto-side questions from M10 round 1 that no Magic quirk entry covers; recorded here with the entries they touch. Each is listed in `docs/decisions/m10-3d-decision-table.md` "Round 2 decisions".

- **J2: bad `render` style reported when the 3D stack is built.**
  - Status: approved 2026-11-21 (proposed in m10-3d-view tasks.md 3.4, integration note (a)).
  - Resolution: Hefesto resolves the `render` statement's `<style>` when the 3D view builds its layer stack, because the display styles load after the cifoutput section, and reports "Error:  Bad render style for CIF layer." then (`VIEW3D-STACK-002`); the statement is ignored, as in Magic. Two differences from Magic are accepted in both modes: a line whose style is bad still gets the "Syntax: ..." and "Unknown layer name." checks at load (Magic checks the style first and reports only the bad style), and the bad-style error is reported on every stack build instead of once per technology load. No stored value differs (view-3d "Render statement in the cifoutput section").
- **J4: `noplaneordering` skips the extract defaults and the micron conversion.**
  - Status: approved 2026-11-21 (m10-3d-view tasks.md 3.4, integration note (c)).
  - Resolution: replicate, in both modes, no Q row — with `noplaneordering` Magic's extTechFinalStyle returns before the plane-order check, the defaults and the `units microns` conversion (extract/ExtTech.c:3801-3802), so a type without a `height` line keeps h = t = 0 (drawn flat) and `units microns` heights stay unconverted. Hefesto copies this with magic-compat ON and OFF (view-3d "Layer height and thickness for the layout-layer view").
- **J6: empty extents, abutting cells and clip-edge faces.**
  - Status: approved 2026-11-21 (round-1 as-landed behaviour of CAMERA and MESH).
  - Resolution: in both modes — the default view fits the non-empty dimension of the cell's bounding box when the other is empty, and an empty cell as 1 × 1 (Magic divides by zero; view-3d "Default view"); pieces of one layer that abut across cells get no inner side faces (Magic draws each cell's tile outlines, which no command returns; the D13 geometry comparison reduces both to the same boundary); faces cut by the clip area (the cut box, or the window area grown by 1) are closed, as Magic does for any clip area (W3Dmain.c:360-378) (view-3d "Rendered geometry").

### Round-3 decisions without a Q row (approved 2026-11-22)

Items left open by M10 round 2 (PR #81) that no Magic quirk entry covers, or that refine one; approved by the user on 2026-11-22 with the recommended option. Each is listed in `docs/decisions/m10-3d-decision-table.md` "Round 3 decisions". The rows with a Q entry are Q-MISC-80 (J5, revised), Q-MISC-81 (R1), Q-MISC-82 (N1) and Q-MISC-83 (N2) above.

- **R2: lighting per mode.**
  - Status: approved 2026-11-22 (RENDER3D round 2, as built).
  - Resolution: kept as built, no Q row — with magic-compat ON the lighting copies Magic's GL state (no normal is set, so every face uses (0, 0, 1); one white positional light at (50, 50, 50) in model coordinates; ambient 0.2 + 0.4; per vertex); with it OFF one white directional light from (1, 1, 1) on each face's own normal with ambient 0.4. Pixels are informative; view-3d "Viewing transform and shading" already states both.
  - Amended: by Q-MISC-84 (2026-11-24) for the desktop 3D tab, which uses the board's per-face shading in both modes; Magic's GL-state lighting was removed from the renderer (round 4, RENDER).
- **R3: translucent display styles drawn opaque.**
  - Status: approved 2026-11-22 (RENDER3D round 2, as built; follows from Q-MISC-77).
  - Resolution: in both modes, no Q row (a consequence of Q-MISC-77's depth test) — a display style that Magic draws translucent (grtoglSetWMandC blends the 2c − 0.8 supercolour with alpha mask/127 to show c, graphics/grTOGL1.c:90-133) is drawn opaque in its plain colour c; with depth-tested faces the blend has no Magic picture to match (view-3d "Viewing transform and shading"). The per-layer transparency of the layer panel (hefesto-3d-visualization) is a separate, Hefesto-side alpha.
- **M1: a spanned contact whose residues leave no gap keeps its own height and thickness.**
  - Status: approved 2026-11-22 (MESH round 2, task 6.5, the J1 option).
  - Resolution: in both modes (a native view option) — with "Span connected layers" on, a contact whose lowest residue's top (h + t) is not below its highest residue's h keeps its own technology h and t instead of being spanned (hefesto-3d-visualization "Mesh generation from layout").
- **M2: memory gate.**
  - Status: approved 2026-11-22 (PERF round 2, perf run 44969020740).
  - Resolution: the `view3d-bench` memory gates stay as set from the round-2 run (about 150 to 170 bytes per meshed rectangle; `bench/view3d/gates.json`); design D6's target of ≤ 64 bytes per rectangle (CPU model) is deferred and revisited after P1. No behaviour changes.
- **W1: console noise after tagged 3D-window commands.**
  - Status: approved 2026-11-22 (WIND3D round 2, as built; MISC-3D-02..04 and 08).
  - Resolution: compat, no Q row — with magic-compat ON a 3D-window command whose wrapper tag runs `magic::scrollupdate` (`view`, `scroll`, `zoom`, `findbox`) or `magic::toolupdate` (`see`) prints the two "Unknown command:" failures of that procedure's `*bypass logcommands suspend|resume`, as Magic does in the OpenGL window; with it OFF nothing is printed (view-3d "3D view commands").
- **W2: `specialopen` result for a path below a top-level window.**
  - Status: approved 2026-11-22 (WIND3D round 2, as built; MISC-3D-01..04, 12).
  - Resolution: compat, no Q row — with magic-compat ON `specialopen wind3d <cell> <path>` on a path below a top-level window (such as `render3d`'s `.render.magic`) leaves Magic's `window "<path>" isn't a top-level window` (the failing `wm protocol`) as its result; with it OFF the result is empty. The window opens in both settings (view-3d "Opening the 3D window").
- **W3: crashes fixed in both modes.**
  - Status: approved 2026-11-22 (WIND3D round 2, as built).
  - Resolution: fix, in both modes (standing rule: crashes and undefined behaviour) — the 3D window never dereferences a missing output style (Q-MISC-73; when the current output style disappears the window returns to layout-layer mode); `level n` and `level up`/`down` saturate at the `int` range instead of overflowing `atoi` or the increment; a call through a closed window's retired toolkit-path command is "invalid command name" and never reaches the closed window (Q-MISC-80) (view-3d "3D view commands", "Opening the 3D window").
- **W4: `level` limits.**
  - Status: approved 2026-11-22 (WIND3D round 2, as built).
  - Resolution: replicate, in both modes, no Q row — `level` is clamped at 0 from below and has no upper limit, as in Magic (values beyond the `int` range saturate, W3) (view-3d "3D view commands").
- **N3: only solid and stippled styles drawn.**
  - Status: approved 2026-11-22 (RENDER3D round 2; MISC-3D-15).
  - Resolution: replicate, in both modes — only display styles whose fill is solid or stippled are drawn (W3Dmain.c:406, :439), by the renderer and in the `HEFESTO_GL_CAPTURE` face log alike, so an outline or cross style (sky130A's contact cross) draws nothing (view-3d "Rendered geometry"; MISC-3D-15).

### Round-5 decisions without a Q row (approved 2026-11-25)

Items left for the user by the round-3 review fix pass (PR #91) and by round 4 (PR #102), approved with the recommended option on 2026-11-25. Each is listed in `docs/decisions/m10-3d-decision-table.md` "Round 5 and 6 decisions". The items with a Q entry are A1 and A2 (Q-MISC-86); A4 to A9 are also recorded as the 2026-11-25 amendments of Q-MISC-84 and Q-MISC-85.

- **A3: ownership exceptions of PR #91.**
  - Status: approved 2026-11-25.
  - Resolution: signed off — the round-3 ownership exceptions listed in PR #91 ("Ownership exceptions for sign-off (complete list for round 3)") and in `openspec/changes/m10-3d-view/design.md` "As-landed: round 3 integration": CORE `cmd/dispatcher`, SCRIPT `script/binding/dispatch.cpp`, UI `ui/bindings`; SHELL's `main_window.*`, `screen_paint.*`, `menu_bar_model.*`, one line of `tests/unit/shell/m4_menu_bar_test.cpp` and the rewrite of `tests/unit/render/mesh3d_target_test.cpp`; GOLDEN's `tools/golden/container/Containerfile`; WIND3D's `src/cli/app.cpp`; the README image; INSPECT's rewrite of `render/mesh3d_pick.hpp`; SHELL's removal of `mesh3d_spike.*`, its shaders, `src/generated/mesh3d_spv.hpp`, the `compile_shaders.py` entry and the `src/render/vulkan/CMakeLists.txt` line; the integration edit to `src/render/vulkan/src/mesh3d_shading.cpp`; the review fix pass's UI `src/core/src/ui/tool_commands.cpp` and SHELL `src/shell/desktop/src/input_router.cpp`. No behaviour changes.
- **A4: the opening z scale of the desktop tab.**
  - Status: approved 2026-11-25 (round 4, SHELL's choice; amended for mask-layer mode with magic-compat ON by B1).
  - Resolution: platform, in both modes — the isometric opening sets s_z = `kBoardScaleZ` = 1 in layout-layer mode and s_z = f in mask-layer mode, so layer heights are in proportion to x and y instead of Magic's 25× exaggeration; `zoom` reports it, the presets keep the current s_z and `defaults` restores Magic's values (view-3d "Default view"; Q-MISC-84 amendment).
- **A5: the z mirror of the displayed scene.**
  - Status: approved 2026-11-25 (round 4, RENDER).
  - Resolution: platform, in both modes — the renderer applies CAMERA's matrix to the model mirrored in z (a height h displayed at z = +h·s_z), so the projection and the depth test put the viewer above the cell and tilted views show the higher layers up; tz moves along the displayed height. Magic's `view`, `scroll` and `zoom` outputs, the window's view state and the GL face log are unchanged (view-3d "Viewing transform and shading").
- **A6: the isometric opening applies to any window the desktop opens with `render3d`.**
  - Status: approved 2026-11-25 (round-4 FIX of PR #102).
  - Resolution: platform, in both modes — a window on `render3d`'s toolkit path `.render.magic` (the 3D tab, Options > 3D Display, or `render3d` typed in the desktop) opens on the framed isometric preset; `specialopen wind3d` (on any path, by a script or the user), scripts and hefesto-cli keep Magic's top-down default (view-3d "Default view").
- **A7: mask-layer ghosting of layout layers in phase 1.**
  - Status: approved 2026-11-25 (round 4; historic, superseded by B2 on 2026-11-26).
  - Resolution: platform, in both modes — while the phase-1 tab drew the layout-layer stack in mask-layer mode, it ghosted the same layout layers there. Since round 5 the tab draws the mask-layer stack in mask-layer mode and B2 applies.
- **A8: gf180mcuD's `poly` plane is ghosted by the class rule.**
  - Status: approved 2026-11-25 (round 4, SHELL).
  - Resolution: platform, in both modes — gf180mcuD's `poly` plane holds only `fillpoly`, which no contact touches, so the class rule ghosts it (gf180's `poly` type is on the active plane and stays opaque) (hefesto-3d-visualization "Interactive 3D inspection", Ghosted layers).
- **A9: a technology with no contact types ghosts nothing.**
  - Status: approved 2026-11-25 (round 4, SHELL).
  - Resolution: platform, in both modes — when the technology defines no contact type, no layer is ghosted by default (otherwise every layer of a contact-free technology would be ghosted) (hefesto-3d-visualization "Interactive 3D inspection", Ghosted layers).

### Round-6 decisions without a Q row (approved 2026-11-26)

Items reported to the user by M10 round 5 (PR #122) as standing-rule or SHELL/SECTION/EXPORT/MASKSRC choices, approved with the recommended option on 2026-11-26. Each is listed in `docs/decisions/m10-3d-decision-table.md` "Round 5 and 6 decisions". The items with a Q entry are B18 (Q-MISC-87), B19 (Q-MISC-88) and B20 (Q-MISC-89). The standing user rule of 2026-11-25 for crafted-input resource limits (both modes, the recommended value; see Q-MODEL-38) also applies to the 3D code.

- **B1: mask-layer board view in proportion with magic-compat ON.**
  - Status: approved 2026-11-26 (SHELL implements in round 6; amends A4 and Q-MISC-84).
  - Resolution: platform, desktop tab only — in mask-layer mode with magic-compat ON the board z scale the tab sets (the opening isometric `zoom`) is `kBoardScaleZ` × f × 1 / (µm per internal unit), because Q-MISC-72 keeps the `render` heights as stored (µm numbers) while x and y are in mask units, and f alone drew sky130A's stack almost flat; with magic-compat OFF it stays `kBoardScaleZ` × f (heights already in internal units). Magic's own s_z values (`defaults`, the window state Magic's commands set, the golden cases, scripts and hefesto-cli windows) are untouched; `zoom` reports the value the tab set (view-3d "Viewing transform and shading", scenarios "Mask-layer board view in proportion with magic-compat ON" and "Mask-layer board view with magic-compat OFF").
- **B2: mask-layer mode ghosts nothing.**
  - Status: approved 2026-11-26 (replaces A7).
  - Resolution: platform, in both modes — the mask-layer stack has no planes, so no mask layer is ghosted by default (hefesto-3d-visualization "Interactive 3D inspection", scenario "Wells ghosted in mask-layer mode").
- **B3: substrate slab on by default.**
  - Status: approved 2026-11-26 (the item Q-MISC-85 deferred; built in round 5).
  - Resolution: platform, in both modes — the desktop tab's "Substrate" toggle is on by default; windows opened by scripts or hefesto-cli draw no slab (hefesto-3d-visualization "Interactive 3D inspection", scenario "Substrate slab under the stack").
- **B4: the cross-section clips the side away from the isometric viewer.**
  - Status: approved 2026-11-26 (round 5 SHELL).
  - Resolution: in both modes — the clipping plane of a cross-section keeps y ≥ c for a horizontal cut line y = c and x ≥ c for a vertical one x = c, the side away from the isometric viewer, so the cut face is seen from the default view (hefesto-3d-visualization "Interactive 3D inspection", scenario "Clipping plane hides one side").
- **B5: toggle state in the screen-reader name.**
  - Status: approved 2026-11-26 (interim, until Raven exposes a checked state for painted widgets).
  - Resolution: platform, in both modes — the 3D tab's toggles append ", on" or ", off" to their screen-reader name; replaced by Raven's checked state when it exists (upstream item).
- **B6: the A–A′ section panel sits in the side panel under Inspect.**
  - Status: approved 2026-11-26 (round 5 SHELL; differs from the board's right aside).
  - Resolution: platform — the section panel is pinned in the side panel below Inspect, not in a right aside (`docs/design/m10-3d-ui-conformance.md`).
- **B7: substrate slab parameters.**
  - Status: approved 2026-11-26 (round 5 SECTION).
  - Resolution: platform, in both modes — thickness 0.2 of the drawn stack's height (`kSubstrateFraction`), colour #2A2F37 (`kSubstrateColor`), top at the lowest drawn bottom, not pickable, never exported, not in the face log or visibility sets, none for an empty mesh or a flat stack.
- **B8: a split-tile triangle in a cross-section takes its own layer's height.**
  - Status: approved 2026-11-26 (round 5 SECTION).
  - Resolution: in both modes — a cross-section reports a split-tile triangle at its own layer's µm height; Magic's window draws it with the left type's height with magic-compat ON (Q-MISC-79), but a section is µm output, as Q-MISC-72's extensions (hefesto-3d-visualization "Interactive 3D inspection", Cross-sections).
- **B9: a cut line on a tile edge.**
  - Status: approved 2026-11-26 (round 5 SECTION).
  - Resolution: replicate Magic's tile convention, in both modes — a cut line along a tile edge takes the material above it (horizontal line) or to its right (vertical line), as a tile owns its bottom and left edges.
- **B10: per-layer STL file names.**
  - Status: approved 2026-11-26 (round 5 EXPORT).
  - Resolution: native — `<stem>_<layer>.stl` in the given file's directory; characters other than letters, digits and `_-.+` become `_`; names equal ignoring case get `_2`, `_3`, … before `.stl` (hefesto-3d-visualization "3D export").
- **B11: a `.gltf` export writes `<stem>.bin` beside it when there is geometry.**
  - Status: approved 2026-11-26 (round 5 EXPORT, as amended by the PR #122 review).
  - Resolution: native — the buffer goes to `<stem>.bin` (URI percent-encoded) when there is geometry; an export with no layer writes the JSON only, with no buffer and no `.bin`.
- **B12: `export3d` without a 3D window.**
  - Status: approved 2026-11-26 (round 5 EXPORT).
  - Resolution: native — fails with `VIEW3D-EXPORT-001` and writes nothing; there is no fallback to the edit cell.
- **B13: glTF axes and units.**
  - Status: approved 2026-11-26 (round 5 EXPORT).
  - Resolution: native — +Y up (layout x, height, −layout y); values in µm; `asset.extras` holds `{"hefesto":{"units":"um","zscale":k}}`, k = 1 without `-zscale`.
- **B14: a type in two display styles, and styles that are not drawn.**
  - Status: approved 2026-11-26 (round 5 EXPORT).
  - Resolution: native, in both modes — a type in two display styles is exported once, as its last drawn entry (the style that wins on coplanar faces, Q-MISC-77); entries whose style is neither solid nor stippled are not exported (N3).
- **B15: per-layer STL writes only layers with geometry.**
  - Status: approved 2026-11-26 (round 5 EXPORT).
  - Resolution: native — one file per exported layer that has geometry; the command's result is the list of files written.
- **B16: the substrate slab is never exported.**
  - Status: approved 2026-11-26 (round 5 EXPORT and SECTION).
  - Resolution: native — the slab is not part of the model and is never in an exported file.
- **B17: "Span connected layers" in the core window state.**
  - Status: approved 2026-11-26 (CORE and SHELL implement in round 6).
  - Resolution: native, in both modes — J1's "Span connected layers" option moves from the desktop's view options into `view3d::WindowState`, so `export3d` (and any front end) builds a spanned view spanned; the desktop toggle edits the window's flag (hefesto-3d-visualization "3D export", scenario "Export of a spanned view keeps the span").
- **B21: the Release frame-rate bench.**
  - Status: approved 2026-11-26 (round 5 MASKSRC/RENDER).
  - Resolution: accepted — a Release frame-rate run may build only the bench target with the desktop ON configured; the numbers are recorded in `docs/design/m10-3d-ui-conformance.md` "Performance" with an idle-machine note.

Items of the round-6 review fix pass (PR #133). C1 falls under the standing user rule of 2026-11-25 (crafted-input resource limits in both modes, the recommended value); C2 and C3 are proposed and reported to the user.

- **C1: limits of the mask-layer net overlay.**
  - Status: approved 2026-11-25 (standing rule for crafted-input limits; round-6 review fix).
  - Resolution: in both modes — `view3d::net_overlay` returns at most 65,536 prisms (`kMaxNetOverlayPrisms`), the renderer reads at most 65,536 prisms and spends at most 2^24 interval visits and 2^20 faces on the overlay's faces; when a limit cuts the overlay the desktop's mesh status adds "net overlay cut at 65,536 boxes" (hefesto-3d-visualization "Interactive 3D inspection", scenario "Net overlay of a very large net").
- **C2: the desktop tab does not repeat identical errors of a repeated full build.**
  - Status: proposed (round-6 review fix; recommendation: approve).
  - Resolution (proposed): with magic-compat ON, a full build of the same inputs (`BuildKey`) as the last full build the tab reported, with identical errors, prints nothing; this removes the duplicate print of the tab's double build on opening. A full build of other inputs, and every incremental rebuild, report as usual.
- **C3: a message repeated by the pieces of one regenerated area counts once.**
  - Status: proposed (round-6 review fix; recommendation: approve).
  - Resolution (proposed): an incremental rebuild that regenerates an area in several pieces reports each distinct message as often as the one piece that has it most, so a region the pieces split is reported once, as a full build reports it.

## Scope

### Q-MISC-90: `search` and `cellsearch` documented but not implemented
- Kind: doc-mismatch
- Magic behavior: doc/html/search.html and cellsearch.html describe Tcl-callback search commands that no longer exist in the source; only the wizard timing commands `*psearch`/`*tsearch` remain.
- Evidence: doc/html/search.html, doc/html/cellsearch.html, dbwind/DBWcommands.c:213-225, commands/CmdWizard.c:975-1120
- Options: omit | implement as Hefesto scripting API
- Recommendation: omit from the compatibility contract (no capability `layout-measurement-and-search` was written); consider a scripting API later.
- Resolution: n/a — spec follows Magic's code (the commands do not exist and are not specified); documentation is wrong (X-2 tier 3, 2026-11-07).
