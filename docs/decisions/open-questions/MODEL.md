# Open Questions — MODEL (layout database)

Capabilities: `layout-geometry-model`, `layout-paint-semantics`, `layout-hierarchy`, `layout-labels-ports`, `layout-properties`, `layout-connectivity`, `layout-undo`. Magic reference: `fc5daa9e`.

### Q-MODEL-1: Label rotation sense under transforms
- Kind: ambiguity
- Magic behavior: transforming a label rotation through the clockwise-90 transform `(0,1,-1,0)` adds +90 degrees, while label text boxes are rotated counter-clockwise by `lab_rotate`. A clockwise placement therefore appears to rotate text counter-clockwise.
- Evidence: utils/geometry.c:154-189, database/DBlabel.c:1440-1460, commands/CmdCD.c:2279-2345
- Options: replicate | fix (subtract for clockwise) | verify rendering first
- Recommendation: capture golden output (MODEL-G-012) before deciding; replicate for file data.

### Q-MODEL-2: Transformed angle can be 360
- Kind: bug
- Magic behavior: the angle sum is reduced only when `> 360`, so 270 rotated by +90 yields 360 instead of 0 (non-flipped transforms).
- Evidence: utils/geometry.c:179
- Options: replicate | fix (reduce `>= 360`)
- Recommendation: fix internally but normalize 360 -> 0 only where it is not written to files; verify whether `flabel` output can contain 360.

### Q-MODEL-3: Asymmetric rounding of non-Manhattan subdivision
- Kind: quirk
- Magic behavior: for `/` diagonals crossings are rounded half-up; for `\` the code computes the rounding term on a negative value (C `%` and left shift of a negative number, the latter undefined behaviour) which always yields 0, so crossings are truncated.
- Evidence: database/DBpaint.c:1657-1745
- Options: replicate | fix (symmetric round-half-up)
- Recommendation: replicate (geometry is written to files); golden cases MODEL-G-020/021.

### Q-MODEL-4: Opposite-diagonal overlap on 1-unit areas fills the rectangle
- Kind: quirk
- Magic behavior: painting a triangle over a triangle with the opposite diagonal quarters the area recursively; at width or height 1 the whole rectangle is painted, over-filling by half a unit square.
- Evidence: database/DBpaint.c:1572-1590
- Options: replicate | fix
- Recommendation: replicate.

### Q-MODEL-5: Non-unique representation of non-Manhattan geometry
- Kind: ambiguity
- Magic behavior: split tiles are merged only under specific collinearity conditions; the same region can end up as one large triangle or several smaller ones plus rectangles depending on painting history, which changes `.mag` `tri` lines.
- Evidence: database/DBpaint.c:1018-1320, 788-1012
- Options: replicate history-dependent behaviour | canonicalize triangles on output
- Recommendation: replicate operations exactly; accept history dependence; golden tests compare after identical command sequences.

### Q-MODEL-6: Font-label bounding box rounding mask
- Kind: bug
- Magic behavior: when converting 1/8 units to database units the code tests `(v & 0x7f)` instead of `(v & 0x7)`, so exact multiples of 8 that are not multiples of 128 are still expanded by one unit.
- Evidence: database/DBlabel.c:1484-1499
- Options: replicate | fix (`& 0x7`)
- Recommendation: replicate (it affects extended bounding boxes, which appear in `.mag` and LEF outputs); verify.

### Q-MODEL-7: Inconsistent array subscript order
- Kind: quirk
- Magic behavior: element names print as `[y,x]` and parse `[i1][i2]` as `[y][x]`, but in-place flattening prefixes labels with `[x][y]`.
- Evidence: database/DBcellbox.c:99-104, database/DBlabel2.c:410-440, database/DBcellcopy.c:650-662
- Options: replicate | fix (use `[y][x]`)
- Recommendation: replicate; document.

### Q-MODEL-8: Default identifier base differs between link and read
- Kind: quirk
- Magic behavior: linking uses the part of the child name after the last `/`; generating unique ids on read uses the full child name, producing ids containing `/`.
- Evidence: database/DBcellname.c:2425-2440, 2758-2770
- Options: replicate | fix (always use basename)
- Recommendation: fix (ids with `/` break hierarchical names); keep reading such ids.

### Q-MODEL-9: Natural-sort comparator not antisymmetric
- Kind: bug
- Magic behavior: `strcmpbynum` returns 1 both for (prefix, longer) and (longer, prefix); qsort order of such pairs is undefined.
- Evidence: database/DBcellname.c:676-713
- Options: replicate (impossible deterministically) | fix (shorter first)
- Recommendation: fix with shorter-first; affects port renumbering, `.mag` use order and property order (cell-* owners should align).

### Q-MODEL-10: Duplicate detection in port renumbering is case-sensitive
- Kind: quirk
- Magic behavior: the sort is case-insensitive but index sharing uses `strcmp`, so `vdd` and `VDD` adjacent get different indices while equal texts separated by a case variant do not share.
- Evidence: commands/CmdLQ.c:2100-2112
- Options: replicate | fix
- Recommendation: replicate.

### Q-MODEL-11: Label size adjustment threshold
- Kind: doc-mismatch
- Magic behavior: the comment says a label is shrunk only if material covers more than half of it; the code shrinks whenever any material remains, clipping against other tiles in enumeration order (not choosing the largest remaining rectangle).
- Evidence: database/DBlabel.c:672-718
- Options: replicate code | implement comment
- Recommendation: replicate code.

### Q-MODEL-12: Undo of paint does not mark the cell modified
- Kind: bug
- Magic behavior: after saving a cell and undoing an earlier paint step, the cell geometry differs from disk but the modified flag is not set by paint replay.
- Evidence: database/DBundo.c:306-420
- Options: replicate | fix (mark modified on any replay)
- Recommendation: fix; verify with MODEL-G-040.

### Q-MODEL-13: Lock/unlock does not mark the parent modified
- Kind: bug
- Magic behavior: the lock flag is persisted in `.mag` but changing it does not set the modified flag, so the change can be lost on exit without a prompt.
- Evidence: database/DBcellname.c:1260-1300, database/DBio.c:6247
- Options: replicate | fix
- Recommendation: fix.

### Q-MODEL-14: Property changes are not undoable
- Kind: quirk
- Magic behavior: no undo events for properties.
- Evidence: database/DBprop.c:58-125
- Options: replicate | add undo
- Recommendation: add undo in Hefesto (behavioral improvement, no file impact).

### Q-MODEL-15: Renamed cell itself not flagged modified
- Kind: ambiguity
- Magic behavior: `DBCellRenameDef` marks parents modified but not the renamed definition (the command layer may handle saving under the new name).
- Evidence: database/DBcellname.c:2122-2155
- Options: replicate | fix
- Recommendation: verify the `cellname rename` command behaviour (cmd owner) before deciding.

### Q-MODEL-16: Mask-hint plane lookup uses the wrong key on CIF/GDS read
- Kind: bug
- Magic behavior: when adding hints from a cifinput templayer, the existing plane is looked up under the bare layer name instead of `MASKHINTS_<layer>`, so a second batch replaces rather than extends the first.
- Evidence: cif/CIFrdcl.c:740-755
- Options: replicate | fix
- Recommendation: fix; coordinate with `mask-hints` owner.

### Q-MODEL-17: Flat connectivity cross-plane step picks a single tile
- Kind: bug
- Magic behavior: from a contact tile the flat search looks in a 1-expanded area on each other plane and continues from only the first matching tile, possibly a diagonal neighbour, possibly skipping the contact's own image.
- Evidence: database/DBconnect.c:640-668
- Options: replicate | fix (continue from the contact image and its edge neighbours)
- Recommendation: fix; affects only DEF/netlist-menu flat searches.

### Q-MODEL-18: Port uses reset/scan/tieoff are not persisted
- Kind: bug
- Magic behavior: the data model has `reset`, `scan`, `tieoff` uses but the `.mag` writer emits nothing for them.
- Evidence: database/database.h.in:299-301, database/DBio.c:4845-4862
- Options: replicate (lose on save) | fix (write names; reader must accept them)
- Recommendation: decide with cell-* owner; default replicate for byte-identical output.

### Q-MODEL-19: Bounding box includes DRC error and hint planes
- Kind: quirk
- Magic behavior: bounding-box computation scans every plane >= 1 except the DRC check plane, so DRC error tiles and router hint paint extend the box.
- Evidence: database/DBcellbox.c:575-585
- Options: replicate | fix (paint planes only)
- Recommendation: replicate (box appears in `.mag` and drives array/overlap computations).

### Q-MODEL-20: Flattened labels keep untransformed font attributes
- Kind: bug
- Magic behavior: flatten copies outline-font rotation and offset without transforming them (marked "to-do" in the source).
- Evidence: commands/CmdFI.c:1990-2005
- Options: replicate | fix (transform like hierarchical copies do)
- Recommendation: fix; record as intentional difference.

### Q-MODEL-21: Undo history limit
- Kind: ambiguity
- Magic behavior: fixed 1000 steps.
- Evidence: utils/undo.c:58-59,811-836
- Options: replicate | configurable larger limit
- Recommendation: configurable, default 1000.

### Q-MODEL-22: Erase-valid ignores stacked types in the mask
- Kind: quirk
- Magic behavior: paint-valid decomposes stacked types into residues; erase-valid does not, so erasing a stacked type name through the user path does nothing.
- Evidence: database/DBpaint2.c:272-277,380-417
- Options: replicate | fix
- Recommendation: replicate until a command-level test shows the effect.

### Q-MODEL-23: Paint/erase always mark modified
- Kind: quirk
- Magic behavior: the modified flag is set before checking whether anything changes.
- Evidence: database/DBpaint2.c:65,177
- Options: replicate | fix
- Recommendation: replicate (users rely on the prompt-on-exit behaviour).

### Q-MODEL-24: Geometry beyond the plane limits
- Kind: ambiguity
- Magic behavior: no clipping to `±(2^30-6)`; behaviour is undefined (tile plane corruption, overflow in transforms).
- Evidence: tiles/tile.h:149-165, tiles/tile.c:67
- Options: clip | reject with error
- Recommendation: reject with an error message.

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

### Q-MODEL-28: Flatten in place is not undoable
- Kind: quirk
- Magic behavior: undo recording is disabled for the whole operation, including deleting the flattened use, so neither the added paint nor the deletion can be undone.
- Evidence: commands/CmdFI.c:2116-2140
- Options: replicate | make undoable
- Recommendation: make undoable (behavioral improvement).

### Q-MODEL-29: Erasing "space" is a no-op
- Kind: bug
- Magic behavior: erase with operand space uses the paint table of space, all of whose entries are no-ops, contrary to the comment "erasing space is the same as erasing everything".
- Evidence: database/DBpaint2.c:179-192, database/DBtpaint.c:199-218
- Options: replicate | fix (clear all planes)
- Recommendation: replicate at the data-model level; commands use explicit masks.

### Q-MODEL-30: `undo 0` enables and `redo 0` disables undo recording
- Kind: quirk
- Magic behavior: a count of 0 is not a no-op. `undo 0` resumes recording by one nesting level and `redo 0` suspends it by one level, with no message. This is undocumented in the help text (`undo [count]`, `redo [count]`).
- Evidence: windows/windCmdSZ.c:686-691, windows/windCmdNR.c:382-385
- Options: replicate | fix (treat 0 as a no-op; keep `undo enable|disable` only)
- Recommendation: replicate behind `magic-compat` (scripts may rely on `redo 0`/`undo 0` as a disable/enable idiom); with the setting off, treat 0 as a no-op.

