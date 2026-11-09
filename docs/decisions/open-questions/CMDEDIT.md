# Open Questions — CMDEDIT (editing command surface)

Area key `CMDEDIT` covers capabilities `cmd-coordinates-and-units`, `cmd-box`, `cmd-paint-erase`, `cmd-selection`, `cmd-hierarchy`, `cmd-labels-properties` and `cmd-misc-edit`. IDs are grouped by sub-area and are not contiguous: 1–29 for coordinates, box and paint; 30–59 for hierarchy; 60–89 for selection; 90–129 for labels and misc.

### Q-CMDEDIT-1: Un-suffixed operand directly before an operator is read as internal units
- Kind: quirk
- Magic behavior: In `100+5`, `100` is not followed by end-of-string or whitespace, so it falls through every suffix test and is taken as internal units; `5` uses the default unit. `{100 + 5}` (with spaces) applies the default unit to both.
- Evidence: commands/CmdSubrs.c:168-256 (suffix tests; lines 251-252 exempt `+ - * /` from the unknown-suffix error) (corrected 2026-11-09; was 162-228)
- Options: replicate | fix (apply the effective default unit to every un-suffixed operand)
- Recommendation: fix in Hefesto's native API but replicate in the Magic-compatible script mode, since existing scripts may rely on `N+M` arithmetic in internal units.
- Resolution: replicate in magic-compat, fix in native — the suffix chain exempts `+ - * /` from the unknown-suffix error, so an operand written directly before an operator silently becomes internal units, a value-changing parse quirk that scripts writing `N+M` may depend on (X-2 tier 2); with `magic-compat` OFF every un-suffixed operand uses the effective input unit (verified against commands/CmdSubrs.c:cmdScaleCoord on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-2: A suffixed operand terminates a coordinate expression
- Kind: bug
- Magic behavior: After `1um`, the parser does not skip the suffix; the operator scan sees `u` and returns, so `1um+2um` = 1um and the rest is silently ignored.
- Evidence: commands/CmdSubrs.c:282-315 (operator scan; any other character, including a suffix letter, returns at 311-313) (corrected 2026-11-09; was 262-297)
- Options: replicate | fix (skip the recognised suffix and continue the expression)
- Recommendation: fix, but emit a warning in compatibility mode when trailing text is ignored.
- Resolution: replicate in magic-compat, fix in native — the operator scan returns on the suffix letter, so Magic evaluates `1um+2um` as 1um, and a script that Magic accepts must give the same box; with `magic-compat` ON Hefesto also warns that trailing text was ignored, and with it OFF the suffix is skipped and the expression continues (changed from fixing in both modes, which would change results of scripts Magic runs) (verified against commands/CmdSubrs.c:cmdScaleCoord on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-3: Metric units without a CIF output style
- Kind: quirk
- Magic behavior: With no output style loaded the metric divisor is 1.0 regardless of the prefix, so `1um` = 1 internal unit and `1nm` = 1 internal unit; reported micron values equal internal units.
- Evidence: cif/CIFmain.c:139-146
- Options: replicate | fix (reject metric units with an error when no output style exists)
- Recommendation: fix (error), since results are meaningless.
- Resolution: fix — with no output style `CIFGetOutputScale` returns 1.0, so `1nm`, `1um` and `1mm` all give 1 internal unit, a meaningless result no script can rely on; a metric operand with no output style is an error (verified against cif/CIFmain.c:CIFGetOutputScale on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-4: Grid-unit operand with no window returns the previous partial value
- Kind: bug
- Magic behavior: `5g` when no layout window exists breaks out of the parse loop before accumulating, returning 0 (or the partial result of earlier operands).
- Evidence: commands/CmdSubrs.c:185-193
- Options: replicate | fix (treat as internal units or error)
- Recommendation: fix: report an error.
- Resolution: fix — the `break` at line 193 leaves the parse loop before the operand is accumulated, so the result is 0 or a partial sum, which no script can usefully depend on; a grid-unit operand (or an un-suffixed one under `units user`) with no layout window reports an error and the argument is invalid, as for division by zero (Q-CMDEDIT-5) (verified against commands/CmdSubrs.c:cmdScaleCoord on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-5: Division by zero in coordinate expressions
- Kind: bug
- Magic behavior: `{4 / 0}` performs C integer division by zero (undefined behaviour / crash).
- Evidence: commands/CmdSubrs.c:258-260
- Options: fix (error)
- Recommendation: fix: error "division by zero" and treat argument as invalid.
- Resolution: fix — division by zero in a coordinate expression reports "division by zero" and the argument is invalid (X-2 tier 1, 2026-11-07).

### Q-CMDEDIT-6: `units print` while units are `default` switches to microns
- Kind: bug
- Magic behavior: DBWUnits = -1; masking with 3 yields 3 (microns), so `units print`/`units noprint` alone silently select microns.
- Evidence: commands/CmdTZ.c:891-949 (`saveflag` set at 914-919, type masked with 3 at 944-949); tcltk/wrapper.tcl:1333-1335 (Magic's own "Display units" menu item sends `units print` / `units noprint` alone) (corrected 2026-11-09; was CmdTZ.c:918-940)
- Options: replicate | fix (ignore, or error "set a unit type first")
- Recommendation: fix with an error; low compatibility impact.
- Resolution: replicate in magic-compat, fix in native — Magic's own "Display units" menu sends `units print` / `units noprint` alone, which under `units default` silently selects microns and changes how later un-suffixed values are read, so stock Magic sessions reach this state; with `magic-compat` OFF the command reports "set a unit type first" and changes nothing (changed: the entry called the compatibility impact low) (verified against commands/CmdTZ.c:CmdUnits on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-7: `grid multiple` stores out-of-range values modulo 256
- Kind: bug
- Magic behavior: An error is printed for values outside 1-255 but the value is still stored as an unsigned char (300 -> 44, 0 -> 0).
- Evidence: commands/CmdFI.c:1407-1413, graphics/grClip.c:52 (corrected 2026-11-09; was CmdFI.c:1395-1402)
- Options: replicate | fix (reject without storing)
- Recommendation: fix.
- Resolution: fix — `GrGridMultiple` is only read back by the query, never by drawing code, and approved Q-UI-21 already decided that out-of-range values are rejected without being stored (verified against commands/CmdFI.c:CmdGrid on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-8: Documented `box DIR DIST` form is not implemented
- Kind: doc-mismatch
- Magic behavior: The help and HTML page list `box [direction [distance]]`, but an unrecognised first word falls to the report form; the box is not moved.
- Evidence: commands/CmdAB.c:744-753, doc/html/box.html
- Options: replicate | fix (treat `box DIR DIST` as `box move DIR DIST`)
- Recommendation: fix (implement as move) — harmless superset.
- Resolution: n/a — spec follows Magic's code (an unrecognized first word prints the report); documentation is wrong (X-2 tier 3, 2026-11-07).

### Q-CMDEDIT-9: `box corner DIR X Y` parses the direction word as the X coordinate
- Kind: bug
- Magic behavior: `box corner DIR X Y` (five words) fails the argument-count check (`argc != 4`) and prints the box help. `box corner DIR D` (four words) parses D as a relative distance and returns if it is 0. It then parses the direction word as the corner's x coordinate ("Coordinate value cannot be parsed: assuming 0") and D as its absolute y, so the named corner moves to (0, D). The help text and box.html give the form as `corner <direction> <distance>`. (Corrected 2026-11-09: the earlier text said the X Y form parses the direction as X; Magic rejects that form.)
- Evidence: commands/CmdAB.c:849-920, commands/CmdAB.c:1165-1184, commands/CmdAB.c:684-699 (help text), doc/html/box.html (corrected 2026-11-09; was CmdAB.c:845-858, 1124-1145)
- Options: replicate | fix (move named corner to absolute X Y)
- Recommendation: fix (spec already mandates the documented intent).
- Resolution: fix — Magic rejects the five-word `box corner DIR X Y` form and turns the four-word `box corner DIR D` into a move to (0, D) after a parse error, so neither form gives a usable result; Hefesto accepts `box corner DIR X Y` as an absolute corner move (a superset of Magic) and prints the usage for a four-word non-cursor form (the `cmd-box` "Box corner command" text describes Magic's behavior wrongly and should be updated) (verified against commands/CmdAB.c:CmdBox on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-10: Manhattan direction in corner/cursor box forms leaves corner undefined
- Kind: bug
- Magic behavior: The corner variable is only set for the four diagonal directions; `box move n cursor` uses an uninitialised value.
- Evidence: commands/CmdAB.c:804-822, commands/CmdAB.c:1128-1142
- Options: fix (default to lower-left) | error
- Recommendation: default to lower-left (as the spec states), or reject with an error.
- Resolution: fix — a Manhattan or `center` direction uses the lower-left (`sw`) corner instead of an uninitialised value, as `cmd-box` states (X-2 tier 1, 2026-11-07).

### Q-CMDEDIT-11: `box grow|shrink DIR cursor` does nothing
- Kind: quirk
- Magic behavior: Only move and corner are handled in the cursor branch; grow/shrink return after logging.
- Evidence: commands/CmdAB.c:858-892 (corrected 2026-11-09; was 823-836)
- Options: replicate | fix (grow/shrink the box edge(s) to the cursor)
- Recommendation: replicate (no-op) unless UI design needs it.
- Resolution: replicate — the cursor branch handles only move and corner, so grow and shrink log `box values` for the unchanged box and return; a no-op harms nothing, and Magic's stock bindings in tools.tcl use only the move and corner cursor forms (verified against commands/CmdAB.c:CmdBox on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-12: Box overflow check tests only the lower-left corner
- Kind: bug
- Magic behavior: Out-of-bounds detection checks r_ll only; an upper-right beyond the plane limit is accepted.
- Evidence: commands/CmdAB.c:1298-1304 (corrected 2026-11-09; was 1258-1266)
- Options: replicate | fix (check both corners)
- Recommendation: fix.
- Resolution: fix — only the lower-left corner is tested, so `box values 0 0 2000000000i 1` puts the upper-right outside the tile plane, where later paint is undefined; both corners are checked (verified against commands/CmdAB.c:CmdBox on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-13: Box tool gestures on touch devices
- Kind: platform
- Magic behavior: Left/right/both mouse buttons move the box, resize the upper-right corner, or pick the nearest corner; middle button paints/erases by example.
- Evidence: dbwind/DBWbuttons.c:416-490
- Options: platform alternative (e.g. drag inside box = move, drag near a corner handle = resize that corner, long-press = paint-by-example)
- Recommendation: UI team to define; the resulting box coordinates must follow cmd-box rules.
- Resolution: platform — under X-5's mode-based mapping, a one-finger drag starting inside the box moves it, one starting on a corner handle moves that corner, one starting elsewhere sweeps a new box from touch-down to lift, and paint/erase-by-example (Magic's middle button) is in the long-press menu; each gesture issues the same `box`/`paint` command as Magic's button, so `cmd-box` rules apply (verified against dbwind/DBWbuttons.c:DBWBoxHandler on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-14: `erase $` documentation says "paint and labels"
- Kind: doc-mismatch
- Magic behavior: `$` is "types under the pointer"; `erase` doc claims `$` erases paint and labels.
- Evidence: commands/CmdSubrs.c:556-568, doc/html/erase.html
- Options: replicate code | change docs
- Recommendation: replicate code behaviour; document correctly.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (X-2 tier 3, 2026-11-07).

### Q-CMDEDIT-15: `corner ... bevel` paints into the root cell of the edit window
- Kind: bug
- Magic behavior: Bevel polygons are computed in edit coordinates but painted directly into the planes of the edit window's root definition, bypassing contact/label handling; wrong when the edit cell is not the root.
- Evidence: commands/CmdCD.c:2970-3016 (paints with DBPaintPlane into `EditRootDef`), commands/CmdCD.c:3709-3718 (points moved to edit coordinates) (corrected 2026-11-09; was 2985-3020)
- Options: replicate | fix (paint into the edit cell)
- Recommendation: fix.
- Resolution: fix — bevel paths are converted to edit coordinates but painted with `DBPaintPlane` directly into `EditRootDef`'s planes, so with a child edit cell a different cell is changed, at the wrong place, without contact handling or a modified flag (silent corruption); bevel paint goes into the edit cell through the normal paint path, and golden case CMDEDIT-PAINT-10 checks the edit-cell-is-root case against Magic (verified against commands/CmdCD.c:CmdCorner on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-16: `erase` ignores explicitly named stacked contact types
- Kind: quirk
- Magic behavior: The erase loop covers only user types below the stacked-contact range, so `erase v12` (a stacked type) erases nothing directly. (verify against Magic)
- Evidence: database/DBpaint2.c:366-417
- Options: replicate | fix (decompose like paint)
- Recommendation: verify with golden case CMDEDIT-PAINT-09 before deciding.
- Resolution: replicate in magic-compat, fix in native — `DBEraseValid` loops only over user types below `DBNumUserLayers`, so a stacked type named by its `stackable` alias is skipped and `erase v12` changes nothing, a value-changing quirk that golden case CMDEDIT-PAINT-09 compares in the `.mag`; with `magic-compat` OFF a named stacked type is erased as its residue contacts, mirroring `DBPaintValid` (confirmed by code reading; no Magic binary was available to run PAINT-09) (verified against database/DBpaint2.c:DBEraseValid on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-17: `drop` modifies every instance and may mis-map coordinates
- Kind: ambiguity
- Magic behavior: Painting into child definitions affects all instances; when the edit cell is not the selection root the search uses the edit cell with an identity transform although the selection is in root coordinates. Read-only (locked) child cells are not checked.
- Evidence: commands/CmdCD.c:4628-4693, commands/CmdCD.c:4769-4825 (corrected 2026-11-09; was 4726-4770, which is mostly the header comment)
- Options: replicate | fix (require edit cell == selection root; refuse non-editable children)
- Recommendation: fix the coordinate case; keep all-instances semantics (documented).
- Resolution: fix — when the edit cell's root is not the selection root, Magic searches the edit cell's hierarchy with an identity transform using another cell's root coordinates and paints unrelated cells, and it never checks that the target cells are editable; Hefesto refuses that case with "The selection does not match the edit cell.", refuses before painting anything when a target cell is not editable, and keeps the all-instances semantics that Magic documents (verified against commands/CmdCD.c:CmdDrop on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-18: Layer-list name buffer is 50 characters
- Kind: bug
- Magic behavior: Names are copied into a 50-byte buffer without bounds check; longer names overflow. Names containing `-`/`+` cannot be expressed.
- Evidence: commands/CmdSubrs.c:497, commands/CmdSubrs.c:527-531
- Options: fix (no length limit)
- Recommendation: fix; keep `-`/`+` as operators.
- Resolution: fix — layer-list names of any length are accepted without a fixed buffer; `-` and `+` stay operators (X-2 tier 1, 2026-11-07).

### Q-CMDEDIT-19: `polygon` with non-convex or self-intersecting vertices
- Kind: ambiguity
- Magic behavior: Documentation says only convex polygons are guaranteed; result for others depends on the polygon-to-rect decomposer.
- Evidence: doc/html/polygon.html, cif/CIFrdpt.c:604-636
- Options: replicate decomposer | fix (general polygon fill, even-odd)
- Recommendation: use the same decomposition as GDS input (MASK area) so that both agree.
- Resolution: replicate — `polygon` goes through `PaintPolygon` to `CIFPolyToRects`, the same nonzero-winding strip decomposer that CIF and GDS input use (the `isCalma` flag only changes an error message), so Hefesto shares the MASK capability's decomposer and inherits its result for non-convex or self-intersecting vertices (verified against cif/CIFrdpt.c:PaintPolygon on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-20: Label-area rule for zero-area boxes
- Kind: quirk
- Magic behavior: With a zero-area box, a label counts as inside if it touches the box unless the box lies strictly inside the label's interior.
- Evidence: utils/geometry.h:149-152
- Options: replicate | simplify (touch only)
- Recommendation: replicate.
- Resolution: replicate — `GEO_LABEL_IN_AREA` is a deliberate rule (a point box picks up the labels it touches), and changing it would change which labels `erase` and selection affect (verified against utils/geometry.h:GEO_LABEL_IN_AREA on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-21: Manhattan directions accepted by splitpaint/spliterase
- Kind: quirk
- Magic behavior: The corner is derived as (pos>>1)-1; north/center act as nw, east as ne, south as se, west as sw, without error.
- Evidence: commands/CmdRS.c:3087-3088, commands/CmdRS.c:3184-3185 (corrected 2026-11-09; was 3082-3085, 3180-3183)
- Options: replicate | fix (reject non-diagonal names)
- Recommendation: replicate for script compatibility, with a warning.
- Resolution: replicate in magic-compat, fix in native — `(pos >> 1) - 1` maps north and center to nw, east to ne, south to se and west to sw without error, and the mapping decides the `.mag` result; with `magic-compat` ON the mapping is kept with a warning, and with it OFF a non-diagonal direction is an error (changed: the entry replicated in both modes) (verified against commands/CmdRS.c:CmdSplit, CmdSplitErase on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-30: `edit INSTPATH` does not update the edit transform
- Kind: bug
- Magic behavior: When an instance path is given, the edit cell is set to that instance, but the edit-to-root and root-to-edit transforms and the edit root are left unchanged. Only selection-based `edit` computes them. Later edits map coordinates through a stale transform.
- Evidence: commands/CmdE.c:97-104, commands/CmdE.c:140-150
- Options: replicate | fix (compute the instance's full transform)
- Recommendation: fix (the spec mandates the full transform).
- Resolution: replicate in magic-compat, fix in native — with INSTPATH only `EditCellUse` changes, so the box keeps being mapped through the previous edit transform (identity right after `load`, so box coordinates land unchanged in the child), and scripts written against Magic may depend on that; with `magic-compat` OFF the instance's full transform and edit root are set, as `cmd-hierarchy` states (changed: the entry fixed in both modes) (verified against commands/CmdE.c:CmdEdit on 2026-11-09; approved 2026-11-09). Additionally, with `magic-compat` ON Hefesto prints a warning when the box is mapped through a transform that no longer matches the instance (user decision 2026-11-09).

### Q-CMDEDIT-31: `flatten -doinplace` is not undoable
- Kind: quirk
- Magic behavior: Undo is disabled while flattening in place, so the deleted instances and added paint cannot be undone. The undo log may also become inconsistent.
- Evidence: commands/CmdFI.c:2125-2150
- Options: replicate | fix (record as one undo unit)
- Recommendation: fix — make it undoable.
- Resolution: compat — decided by D-10 (2026-11-07). Same answer as Q-MODEL-28: one undo step with `magic-compat` OFF, not recorded with it ON.

### Q-CMDEDIT-32: Search paths on sandboxed platforms
- Kind: platform
- Magic behavior: `path`/`addpath` take arbitrary file-system directories, with `~` and environment-variable expansion.
- Evidence: commands/CmdLQ.c:1090-1195, utils/path.c
- Options: platform alternative (security-scoped bookmarks / project-relative paths on iPadOS)
- Recommendation: keep the command surface; resolve entries against user-granted locations.
- Resolution: platform — the `path`/`addpath` command surface is kept, and entries resolve within the folders the user has granted access to (hefesto-files-and-pdks, X-4, 2026-11-07).

### Q-CMDEDIT-33: Advisory file locking mechanism
- Kind: platform
- Magic behavior: Uses `fcntl` record locks on open `.mag` files to make cells read-only for other Magic processes.
- Evidence: utils/flock.c, commands/CmdLQ.c:599-672
- Options: replicate on POSIX | platform alternative (lock files or file coordination on Apple platforms)
- Recommendation: use OS file coordination where available. Interoperate with Magic's `fcntl` locks on Linux/macOS.
- Duplicate: tracked in Q-CELL-26, which is the canonical entry. Record the resolution there. (Consolidation audit, 2026-11-06.)
- Resolution: platform — `fcntl` advisory locks interoperate with Magic on Linux and macOS; on iPadOS, platform file coordination prevents conflicting edits (hefesto-files-and-pdks "Locking and recovery replacements", X-4, 2026-11-07).

### Q-CMDEDIT-34: `archive` continues after a usage error
- Kind: bug
- Magic behavior: With the wrong argument count, the usage is printed, but execution falls through to the switch. The option is initialized to -1, so the switch does nothing; the only stray action is reading `argv[2]` into the file name, which is never used. The impact is cosmetic: observable behavior is the same as stopping after the usage message.
- Evidence: commands/CmdAB.c:118-145
- Options: fix (stop after usage)
- Recommendation: fix.
- Resolution: fix — after the usage message the option is still -1, so the switch does nothing and the stray `argv[2]` read is unused; stopping after the usage is observably identical (verified against commands/CmdAB.c:CmdArchive on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-35: Crash backup location and periodic backups
- Kind: platform
- Magic behavior: Backups go to a temporary file in `/tmp` (or `TMPDIR`), are written periodically using a timer signal, and are found by scanning for files on recovery.
- Evidence: commands/CmdCD.c:3751-3790, database/DBio.c DBWriteBackup/DBFileRecovery
- Options: platform alternative (app-container autosave)
- Recommendation: autosave to the app container. Keep `crash save|recover FILE` for explicit files.
- Duplicate: tracked in Q-CELL-30, which is the canonical entry. Record the resolution there. (Consolidation audit, 2026-11-06.)
- Resolution: platform — decided by D-8 (2026-11-07). Duplicate of Q-CELL-30; see the resolution there.

### Q-CMDEDIT-60: `select no …` is parsed as `nocycle`
- Kind: quirk
- Magic behavior: Qualifiers are matched as prefixes in the order more, less, nocycle, same, do, no, simple. `no` matches `nocycle` first. The effect is similar (no labels), but cycling is also disabled and the type counter is reset. Similarly, `s` means `same`, not `simple`.
- Evidence: commands/CmdRS.c:903-957 (corrected 2026-11-09; was 860-915)
- Options: replicate | fix (exact-match qualifiers first)
- Recommendation: replicate for script compatibility.
- Resolution: replicate in magic-compat, fix in native — qualifiers are tested with `strncmp` in the order more, less, nocycle, same, do, no, simple, so `no` takes the nocycle branch (which also turns off cycling and resets the type counter) and `s` means `same`, which changes selection results; with `magic-compat` OFF an exact match wins before prefix matching (changed: the entry replicated in both modes) (verified against commands/CmdRS.c:CmdSelect on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-61: `rotate` is an alias of `clockwise`
- Kind: doc-mismatch
- Magic behavior: The help text says "rotate [+/-][deg] rotate selection and box (counter)clockwise", but `rotate` runs the same code as `clockwise`, so positive angles rotate clockwise.
- Evidence: dbwind/DBWcommands.c:466-468, commands/CmdCD.c:2306-2390
- Options: replicate | change (counter-clockwise for `rotate`)
- Recommendation: replicate (scripts depend on it). Fix the documentation.
- Resolution: n/a — spec follows Magic's code (`rotate` is an alias of `clockwise`); documentation is wrong (X-2 tier 3, 2026-11-07).

### Q-CMDEDIT-62: `orient` is relative, and `180v` is listed but rejected
- Kind: quirk
- Magic behavior: `orient` composes a transform with the current orientation rather than setting an absolute orientation. The keyword `180v` is in the table but has no case, so it prints usage.
- Evidence: commands/CmdLQ.c:3088-3167; commands/CmdCD.c:5344-5345 (`getcell`/`dump` map `180v` to sideways) (corrected 2026-11-09; was CmdLQ.c:3090-3180)
- Options: replicate | fix (`180v` = sideways, as in `getcell`)
- Recommendation: fix `180v` (sideways, consistent with getcell/dump). Keep relative semantics.
- Resolution: fix — `180v` is in the keyword table but has no case and prints the usage, while `getcell` and `dump` map it to sideways; mapping it to sideways only accepts a form Magic rejects, and the relative (composed) semantics are kept as Magic implements them (verified against commands/CmdLQ.c:CmdOrient on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-63: `array -list` returns only the last instance's values
- Kind: bug
- Magic behavior: In the query loop, each instance replaces the scripting result, so with several selected arrays only the last one is returned. The setters for width, height, pitch and position print "Unimplemented function.".
- Evidence: commands/CmdAB.c:270-470
- Options: replicate | fix (return a list per instance; implement setters)
- Recommendation: fix (return a list of lists). Implement the setters as an extension.
- Resolution: replicate in magic-compat, fix in native — each instance's query calls `Tcl_SetObjResult`, so `array -list` returns the flat list of the last instance, and always returning a list of lists would break `lindex [array -list count] 0` for a single selected array; with `magic-compat` ON Magic's shape and the "Unimplemented function." setters are kept, and with it OFF the result has one sublist per selected array and the width, height, pitch and position setters are implemented (changed: the entry fixed in both modes) (verified against commands/CmdAB.c:CmdArray on 2026-11-09; approved 2026-11-09). The size, spacing and position setters that Magic leaves unimplemented are deferred to a later milestone; M2 implements the native-mode list-of-lists query only (user decision 2026-11-09).

### Q-CMDEDIT-90: `setlabel offset` query reports the x offset twice
- Kind: bug
- Magic behavior: The query prints `lab_offset.p_x/8` for both x and y.
- Evidence: commands/CmdRS.c:1905-1910
- Options: replicate | fix
- Recommendation: fix.
- Duplicate: tracked in Q-MISC-23, which is the canonical entry. Record the resolution there. (Consolidation audit, 2026-11-06.) Q-MISC-23 also covers the size/offset query scaling.
- Resolution: fix — duplicate of Q-MISC-23; see the resolution there (D-5, approved 2026-11-07, 2026-11-07).

### Q-CMDEDIT-91: `setlabel box` coordinates parsed as relative distances
- Kind: quirk
- Magic behavior: All four coordinates are parsed with the relative flag, so grid (`g`) units do not add the grid origin.
- Evidence: commands/CmdRS.c:2458-2517 (corrected 2026-11-09; was 2420-2470)
- Options: replicate | fix (absolute coordinates)
- Recommendation: fix. Only `g` units are affected.
- Resolution: replicate in magic-compat, fix in native — all four coordinates are parsed with `is_relative` TRUE, so `g` operands (and un-suffixed ones under `units user`) skip the grid origin and the label rectangle moves by the origin, a value-changing parse quirk; with `magic-compat` OFF they are parsed as absolute coordinates (changed: the entry fixed in both modes) (verified against commands/CmdRS.c:CmdSetLabel on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-92: Port auto-index depends on label order
- Kind: quirk
- Magic behavior: When making a port without an index, the code scans the labels. The first other port with the same text sets idx to its index − 1 and ends the scan (`break`), so a same-text port always wins. Otherwise idx is the highest port index seen. The new index is idx + 1. Order matters only when several same-text ports have different indices: the first in the label list wins. (Corrected 2026-11-09: the earlier text said a later port with a higher index overrides a same-text match; the `break` prevents that.)
- Evidence: commands/CmdLQ.c:2195-2215 (corrected 2026-11-09; was 2105-2125)
- Options: replicate | fix (same-text match wins)
- Recommendation: fix (same-text match wins). Add a golden case to confirm Magic's ordering.
- Resolution: replicate — the `break` at line 2208 already makes a same-text port win, so the only order dependence is between several same-text ports with different indices, where the first in the label list wins, and Hefesto keeps the label order read from the `.mag` file (changed: the bug described in the entry is not in this Magic version) (verified against commands/CmdLQ.c:CmdPort on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-93: `property` targets the window's top cell, not the edit cell
- Kind: ambiguity
- Magic behavior: With a window, `property` reads and writes the window's root cell even when a child is being edited. Without a window, it uses the edit cell. The setter's comment says "current edit cell".
- Evidence: commands/CmdLQ.c:2805-2820
- Options: replicate | fix (use the edit cell)
- Recommendation: replicate; scripts use `cellname property` for explicit targets.
- Resolution: replicate — with a window the target is the window's root cell (otherwise the edit cell), Magic's PCell toolkit calls `property` right after `pushstack`, where the two coincide, and scripts use `cellname property` for explicit targets (verified against commands/CmdLQ.c:CmdProperty on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-94: `xor` result is not a true XOR across different types
- Kind: quirk
- Magic behavior: Painting type T over a different type H gives T, so overlapping areas with different types show the source type instead of both.
- Evidence: commands/CmdTZ.c:2505-2525
- Options: replicate | fix (per-type XOR)
- Recommendation: replicate for golden compatibility. Offer a per-plane true XOR as a separate option.
- Resolution: replicate — the XOR table paints a type T over a different type H as T because a tile holds one type per plane, so "both types" cannot be represented, and the `.mag` result is what golden case CMDEDIT-MISC-01 compares; a per-type XOR would be a separate new option outside this resolution (verified against commands/CmdTZ.c:CmdXor on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-95: `xor` options all test the first option's letter
- Kind: bug
- Magic behavior: The switch uses `argv[1][3]` instead of `argv[i][3]`, so `xor -nolabels -novendor dest` applies `-nolabels` twice.
- Evidence: commands/CmdTZ.c:2448-2468
- Options: fix
- Recommendation: fix.
- Resolution: replicate in magic-compat, fix in native — the switch reads `argv[1][3]` for every option, so `xor -nolabels -novendor b` applies `-nolabels` twice and still descends vendor cells, which changes the `.mag` result; with `magic-compat` OFF each option is read from its own argument (`argv[i][3]`), following the Q-LVS-9 precedent for value-changing bugs (changed: the entry fixed in both modes) (verified against commands/CmdTZ.c:CmdXor on 2026-11-09; approved 2026-11-09).

### Q-CMDEDIT-96: `shell` on platforms without process spawning
- Kind: platform
- Magic behavior: Runs the command through the system shell.
- Evidence: commands/CmdRS.c:2736-2780
- Options: platform alternative (unavailable on iPadOS; native on desktop)
- Recommendation: provide on Linux/macOS only.
- Resolution: n/a (platform). Answered by P-13 and capability `hefesto-ipad-feature-scope`: `shell` is desktop-only.
