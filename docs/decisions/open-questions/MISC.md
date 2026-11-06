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

### Q-MISC-6: Missing plane-order index reuses previous check plane
- Kind: bug
- Magic behavior: If no plane has order k, the check plane variable keeps its previous value (uninitialised on the first iteration).
- Evidence: extflat/EFantenna.c:492-494
- Options: replicate | fix (skip k)
- Recommendation: fix — skip order indices with no plane.

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

### Q-MISC-11: cifinput templayer hints look up the wrong key
- Kind: bug
- Magic behavior: When adding a templayer hint, the existing property is looked up under the bare layer name instead of `MASKHINTS_<name>`; normally not found, so a new region replaces any existing hint of the same name instead of adding to it.
- Evidence: cif/CIFrdcl.c:736-753
- Options: replicate | fix (look up `MASKHINTS_<name>` and union)
- Recommendation: fix (union), the evident intent.
- Duplicate: tracked in Q-MASK-308, which is the canonical entry. Record the resolution there. (Consolidation audit, 2026-11-06.)

### Q-MISC-12: `plane` property type keyword not accepted in .mag
- Kind: quirk
- Magic behavior: The `.mag` reader recognises only `string|integer|dimension|double`; a line starting with `plane` ends the properties section.
- Evidence: database/DBio.c:3211-3240
- Options: replicate | fix (accept `plane`)
- Recommendation: replicate on read (never write `plane`).

### Q-MISC-13: DRC exception edge touching the hint boundary
- Kind: ambiguity
- Magic behavior: The test searches the hint plane with a zero-width rectangle at the edge; whether an edge lying exactly on a hint boundary counts as inside depends on the tile search's treatment of degenerate areas.
- Evidence: drc/DRCbasic.c:935-946
- Options: replicate (determine by golden test)
- Recommendation: capture golden cases MISC-MH-08 and specify.

## font-labels

### Q-MISC-20: Bounding-box rounding tests 7 bits instead of 3
- Kind: bug
- Magic behavior: Conversion from 1/8 units uses `(v & 0x7f) != 0` to decide whether to round outward, so exact multiples of 8 that are not multiples of 128 are still expanded by one unit.
- Evidence: database/DBlabel.c:1481-1492
- Options: replicate | fix (`& 0x7`)
- Recommendation: replicate — affects extended bboxes saved to `.mag` timestamps/bboxes and redraw areas only marginally.

### Q-MISC-21: `flatten` copies font label rotation/offset untransformed
- Kind: bug
- Magic behavior: The non-inplace flatten path passes the original rotation and offset (marked "To-do" in source) while other copy paths transform them.
- Evidence: commands/CmdFI.c:1996-2011
- Options: replicate | fix (transform like other copies)
- Recommendation: fix.

### Q-MISC-22: Default font label size equals lambda numerator in eighth units
- Kind: quirk
- Magic behavior: When a font is applied to a label of size 0, or `label` is used with a font and no size and no defaults, size = lambda numerator (often 1), i.e. 1/8 internal unit — practically invisible.
- Evidence: commands/CmdRS.c:2104, commands/CmdLQ.c:255-256
- Options: replicate | fix (8 × numerator)
- Recommendation: replicate for data compatibility; GUI may suggest a size.

### Q-MISC-23: `setlabel offset` query prints x twice; size/offset query scaling inconsistent
- Kind: bug
- Magic behavior: The per-label offset query prints offset.x/8 for both coordinates; the `-default` queries print unscaled values while per-label queries divide by 8.
- Evidence: commands/CmdRS.c:1992-1993, commands/CmdRS.c:2380-2384, commands/CmdRS.c:2420-2424
- Options: replicate | fix
- Recommendation: fix (informative output only).
- Canonical entry for Q-CMDEDIT-90.

### Q-MISC-24: Rotation transform direction mismatch
- Kind: bug
- Magic behavior: The angle transform adds 90° for a clockwise 90° instance transform, while corners are rotated counter-clockwise, so copied rotated font labels (and their bboxes) are mirrored in angle relative to the geometric image.
- Evidence: utils/geometry.c:154-189, database/DBlabel.c:1440-1458
- Options: replicate | fix
- Recommendation: replicate (stored angles in `.mag`/GDS must match Magic); flag in UI.

### Q-MISC-25: Unclosed glyph path overflows point buffer
- Kind: bug
- Magic behavior: When a glyph path's last point differs from its first, the point count is one larger than the allocated array.
- Evidence: database/DBlabel.c:1745-1757
- Options: fix (allocate count points)
- Recommendation: fix — semantics unchanged (all distinct points kept).

### Q-MISC-26: Loading a font by file name duplicates fonts
- Kind: quirk
- Magic behavior: `setlabel font FreeSans.pt3 0.58` looks up "FreeSans.pt3" (not found), loads the file, and the new font is named "FreeSans"; repeating the command loads a second copy with a new index.
- Evidence: commands/CmdRS.c:2287-2300, database/DBlabel.c:1680-1688
- Options: replicate | fix (skip loading if the file's font name already exists)
- Recommendation: fix — font indices are not persisted (`.mag` stores names; GDS stores index & 3, see Q-MISC-27).

### Q-MISC-27: GDS stores only two bits of font index
- Kind: quirk
- Magic behavior: Font index is written modulo 4 in PRESENTATION bits; on read the index refers to the reader's load order.
- Evidence: calma/CalmaWrite.c:2860-2867, calma/CalmaRdpt.c:831
- Options: replicate
- Recommendation: replicate; Hefesto must load FreeSans, FreeSerif, FreeMono in that order by default.

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

### Q-MISC-47: PNM downsampling message computes required memory oddly
- Kind: quirk
- Magic behavior: The reported "Required for non-downsampled image" divides by 4^(original downsample) after adding 1023 inside an int cast of a float expression.
- Evidence: plot/plotPNM.c:675-685
- Options: replicate (informative message)
- Recommendation: message text is informative; do not replicate exactly.

## view-3d

### Q-MISC-70: 3D view clips subcell tiles in the wrong coordinate system
- Kind: bug
- Magic behavior: Tiles are transformed to root coordinates but clipped against the search area expressed in the subcell's coordinates, so transformed subcells render clipped incorrectly (only visible with a cut box or partial area).
- Evidence: graphics/W3Dmain.c:253-278, graphics/W3Dmain.c:589
- Options: replicate | fix (clip in root coordinates)
- Recommendation: fix (display only).

### Q-MISC-71: One visibility mask for two layer namespaces
- Kind: bug
- Magic behavior: `see` sets bits by tile type in layout mode and by mask-layer index in mask mode, using the same bit set; toggling `cif` reinterprets the bits.
- Evidence: graphics/W3Dmain.c:788-803, graphics/W3Dmain.c:1690-1695, graphics/W3Dmain.c:1767-1769
- Options: replicate | fix (separate sets)
- Recommendation: fix.

### Q-MISC-72: Units of `render` heights versus extract heights
- Kind: ambiguity
- Magic behavior: `render` height/thickness are used unscaled while the view is rescaled by the output scalefactor in mask mode; extract heights are converted to internal units. PDK files give render values in micrometres, so the vertical exaggeration differs between the two modes.
- Evidence: cif/CIFtech.c:1016-1026, graphics/W3Dmain.c:428-433, graphics/W3Dmain.c:847-857
- Options: replicate | fix (interpret render values as µm and convert)
- Recommendation: replicate numerically for now; revisit with visual comparison.

### Q-MISC-73: `cif` toggle with no output style
- Kind: bug
- Magic behavior: The window starts in mask mode even if no cifoutput style exists; toggling back dereferences the missing style.
- Evidence: graphics/W3Dmain.c:879-890, graphics/W3Dmain.c:1502-1503
- Options: fix (start in layout mode when no style; ignore toggle)
- Recommendation: fix.

### Q-MISC-74: 3D window size fixed at 500 x 500
- Kind: bug
- Magic behavior: The aspect correction and viewport use the initial 500×500 size even after the window is resized.
- Evidence: graphics/W3Dmain.c:1496-1497, graphics/W3Dmain.c:532-533, graphics/W3Dmain.c:955-958
- Options: fix (use actual size)
- Recommendation: fix.

### Q-MISC-75: 3D view depends on OpenGL, Tk and GLX
- Kind: platform
- Magic behavior: Available only with the OGL display and Tk; refused otherwise.
- Evidence: graphics/W3Dmain.c:1474-1480, graphics/W3Dmain.c:1540-1586
- Options: platform alternative (Metal/Vulkan/OpenGL abstraction; touch gestures)
- Recommendation: implement natively on all platforms including iPadOS.

## Scope

### Q-MISC-90: `search` and `cellsearch` documented but not implemented
- Kind: doc-mismatch
- Magic behavior: doc/html/search.html and cellsearch.html describe Tcl-callback search commands that no longer exist in the source; only the wizard timing commands `*psearch`/`*tsearch` remain.
- Evidence: doc/html/search.html, doc/html/cellsearch.html, dbwind/DBWcommands.c:213-225, commands/CmdWizard.c:975-1120
- Options: omit | implement as Hefesto scripting API
- Recommendation: omit from the compatibility contract (no capability `layout-measurement-and-search` was written); consider a scripting API later.
