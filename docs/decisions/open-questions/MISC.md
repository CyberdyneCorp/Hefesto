# Open Questions — MISC area

Capabilities: `antenna-check`, `mask-hints`, `font-labels`, `view-3d`, `plot-output`.
Magic reference: `RTimothyEdwards/magic` @ `fc5daa9e`.

## antenna-check

### Q-MISC-1: Cumulative antenna model never reports violations
- Kind: bug
- Magic behavior: The per-layer sums S_metal and S_via are only accumulated when the PARTIAL model flag is set. With `model cumulative` both sums stay 0, so no gate ever fails, although the areas are accumulated over all lower planes.
- Evidence: extflat/EFantenna.c:574-590, extflat/EFantenna.c:647-668
- Options: replicate | fix (accumulate a[t]/ratio for all non-contact types in the cumulative model) 
- Recommendation: fix behind a compatibility flag; default to Magic's behaviour for golden tests, document loudly.

### Q-MISC-2: Antenna model flags are uninitialised
- Kind: bug
- Magic behavior: The style's model flag byte is never initialised (the style is allocated without zeroing) and `model` lines only OR flags in. Without a `model` line the model is undefined; reloading a style can keep stale flags.
- Evidence: extract/ExtTech.c:816-823, extract/ExtTech.c:942-951, extract/ExtTech.c:3086-3105
- Options: replicate (impossible deterministically) | fix (initialise to 0 on style init)
- Recommendation: fix — start empty; equals Magic's behaviour whenever malloc returns zeroed memory.
- Resolution: fix — the antenna model flags start empty for every style and on reload (X-2 tier 1, 2026-11-07).

### Q-MISC-3: Antenna line without calc type and no default
- Kind: quirk
- Magic behavior: The error is reported but the (uninitialised) calc type is still stored and the ratios are stored, so the layer may or may not count.
- Evidence: extract/ExtTech.c:3015-3028
- Options: replicate | fix (store "no calculation type")
- Recommendation: fix — treat the layer as not contributing.

### Q-MISC-4: Contact cut count mixes internal and output units
- Kind: bug
- Magic behavior: nx = (w − 2·border)/(size + sep) uses tile width in internal units but cut parameters in cifoutput units (scaled by scalefactor), so cut counts are wrong unless scalefactor = 1; additionally the cut area is truncated to an integer before multiplying. If the contact has no squares/slots rule the parameters are uninitialised.
- Evidence: extflat/EFantenna.c:853-863, extflat/EFantenna.c:974-985, cif/CIFtech.c:2339-2343
- Options: replicate | fix (convert w, h to output units first; use 0 area when no rule)
- Recommendation: replicate by default (affects sky130/gf180 antenna results), offer fixed mode.

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
- Magic behavior: Only the first contact type whose home plane is the check plane is restricted, and the restriction keeps types whose plane *number* (not plane order) is ≤ the check plane's number. Other via types on that plane still connect upward.
- Evidence: extflat/EFantenna.c:504-517
- Options: replicate | fix (restrict all contacts on the plane, compare plane order)
- Recommendation: replicate by default; techs with several via types per plane need a fixed mode.

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
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: The aspect correction and viewport use the initial 500×500 size even after the window is resized.
- Evidence: graphics/W3Dmain.c:1496-1497, graphics/W3Dmain.c:532-533, graphics/W3Dmain.c:955-958 (corrected 2026-11-13: Confirmed: initial size :1496-1497, the aspect glScalef and glViewport :532-533, the refresh area :955-958. Also: Tk_GeometryRequest 500×500 (:1566), TOGLEventProc handles StructureNotify (:1579), and graphics/grTOGL1.c:851-876 (ConfigureNotify calls WindReframe on the MagWindow but never updates `crec->width`/`height`). `render3d` packs the window with `-expand true -fill both` (tcltk/wrapper.tcl:1613), so a resize is routine; after one, drawing and refresh stay in the lower-left 500×500 area and the aspect stays 1.)
- Options: fix (use actual size)
- Recommendation: fix, in both modes (X-2 tier 3; a fixed viewport is meaningless in Raven or Metal hosts and on iPadOS). Hefesto uses the drawable's current size for the viewport and the refresh area, with A = H_w / W_w from that size. The initial size request stays 500 × 500. (corrected 2026-11-13, was fix; scope made explicit)
- Resolution: fix, in both modes (X-2 tier 3) — the viewport, the refresh area and the aspect A = H_w / W_w use the drawable's current size; the initial size request stays 500 × 500. Approved 2026-11-13.

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

## Scope

### Q-MISC-90: `search` and `cellsearch` documented but not implemented
- Kind: doc-mismatch
- Magic behavior: doc/html/search.html and cellsearch.html describe Tcl-callback search commands that no longer exist in the source; only the wizard timing commands `*psearch`/`*tsearch` remain.
- Evidence: doc/html/search.html, doc/html/cellsearch.html, dbwind/DBWcommands.c:213-225, commands/CmdWizard.c:975-1120
- Options: omit | implement as Hefesto scripting API
- Recommendation: omit from the compatibility contract (no capability `layout-measurement-and-search` was written); consider a scripting API later.
- Resolution: n/a — spec follows Magic's code (the commands do not exist and are not specified); documentation is wrong (X-2 tier 3, 2026-11-07).
