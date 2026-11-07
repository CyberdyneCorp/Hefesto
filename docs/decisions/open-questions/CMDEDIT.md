# Open Questions — CMDEDIT (editing command surface)

Area key `CMDEDIT` covers capabilities `cmd-coordinates-and-units`, `cmd-box`, `cmd-paint-erase`, `cmd-selection`, `cmd-hierarchy`, `cmd-labels-properties` and `cmd-misc-edit`. IDs are grouped by sub-area and are not contiguous: 1–29 for coordinates, box and paint; 30–59 for hierarchy; 60–89 for selection; 90–129 for labels and misc.

### Q-CMDEDIT-1: Un-suffixed operand directly before an operator is read as internal units
- Kind: quirk
- Magic behavior: In `100+5`, `100` is not followed by end-of-string or whitespace, so it falls through every suffix test and is taken as internal units; `5` uses the default unit. `{100 + 5}` (with spaces) applies the default unit to both.
- Evidence: commands/CmdSubrs.c:162-228
- Options: replicate | fix (apply the effective default unit to every un-suffixed operand)
- Recommendation: fix in Hefesto's native API but replicate in the Magic-compatible script mode, since existing scripts may rely on `N+M` arithmetic in internal units.

### Q-CMDEDIT-2: A suffixed operand terminates a coordinate expression
- Kind: bug
- Magic behavior: After `1um`, the parser does not skip the suffix; the operator scan sees `u` and returns, so `1um+2um` = 1um and the rest is silently ignored.
- Evidence: commands/CmdSubrs.c:262-297
- Options: replicate | fix (skip the recognised suffix and continue the expression)
- Recommendation: fix, but emit a warning in compatibility mode when trailing text is ignored.

### Q-CMDEDIT-3: Metric units without a CIF output style
- Kind: quirk
- Magic behavior: With no output style loaded the metric divisor is 1.0 regardless of the prefix, so `1um` = 1 internal unit and `1nm` = 1 internal unit; reported micron values equal internal units.
- Evidence: cif/CIFmain.c:139-146
- Options: replicate | fix (reject metric units with an error when no output style exists)
- Recommendation: fix (error), since results are meaningless.

### Q-CMDEDIT-4: Grid-unit operand with no window returns the previous partial value
- Kind: bug
- Magic behavior: `5g` when no layout window exists breaks out of the parse loop before accumulating, returning 0 (or the partial result of earlier operands).
- Evidence: commands/CmdSubrs.c:185-193
- Options: replicate | fix (treat as internal units or error)
- Recommendation: fix: report an error.

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
- Evidence: commands/CmdTZ.c:918-940
- Options: replicate | fix (ignore, or error "set a unit type first")
- Recommendation: fix with an error; low compatibility impact.

### Q-CMDEDIT-7: `grid multiple` stores out-of-range values modulo 256
- Kind: bug
- Magic behavior: An error is printed for values outside 1-255 but the value is still stored as an unsigned char (300 -> 44, 0 -> 0).
- Evidence: commands/CmdFI.c:1395-1402
- Options: replicate | fix (reject without storing)
- Recommendation: fix.

### Q-CMDEDIT-8: Documented `box DIR DIST` form is not implemented
- Kind: doc-mismatch
- Magic behavior: The help and HTML page list `box [direction [distance]]`, but an unrecognised first word falls to the report form; the box is not moved.
- Evidence: commands/CmdAB.c:744-753, doc/html/box.html
- Options: replicate | fix (treat `box DIR DIST` as `box move DIR DIST`)
- Recommendation: fix (implement as move) — harmless superset.
- Resolution: n/a — spec follows Magic's code (an unrecognized first word prints the report); documentation is wrong (X-2 tier 3, 2026-11-07).

### Q-CMDEDIT-9: `box corner DIR X Y` parses the direction word as the X coordinate
- Kind: bug
- Magic behavior: The corner point is parsed from argv[2] (the direction) and argv[3]; the direction yields 0 with a parse error. The "distance" argument is also first parsed as a distance and a zero value aborts.
- Evidence: commands/CmdAB.c:845-858, commands/CmdAB.c:1124-1145
- Options: replicate | fix (move named corner to absolute X Y)
- Recommendation: fix (spec already mandates the documented intent).

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
- Evidence: commands/CmdAB.c:823-836
- Options: replicate | fix (grow/shrink the box edge(s) to the cursor)
- Recommendation: replicate (no-op) unless UI design needs it.

### Q-CMDEDIT-12: Box overflow check tests only the lower-left corner
- Kind: bug
- Magic behavior: Out-of-bounds detection checks r_ll only; an upper-right beyond the plane limit is accepted.
- Evidence: commands/CmdAB.c:1258-1266
- Options: replicate | fix (check both corners)
- Recommendation: fix.

### Q-CMDEDIT-13: Box tool gestures on touch devices
- Kind: platform
- Magic behavior: Left/right/both mouse buttons move the box, resize the upper-right corner, or pick the nearest corner; middle button paints/erases by example.
- Evidence: dbwind/DBWbuttons.c:416-490
- Options: platform alternative (e.g. drag inside box = move, drag near a corner handle = resize that corner, long-press = paint-by-example)
- Recommendation: UI team to define; the resulting box coordinates must follow cmd-box rules.

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
- Evidence: commands/CmdCD.c:2985-3020, commands/CmdCD.c:3709-3720
- Options: replicate | fix (paint into the edit cell)
- Recommendation: fix.

### Q-CMDEDIT-16: `erase` ignores explicitly named stacked contact types
- Kind: quirk
- Magic behavior: The erase loop covers only user types below the stacked-contact range, so `erase v12` (a stacked type) erases nothing directly. (verify against Magic)
- Evidence: database/DBpaint2.c:366-417
- Options: replicate | fix (decompose like paint)
- Recommendation: verify with golden case CMDEDIT-PAINT-09 before deciding.

### Q-CMDEDIT-17: `drop` modifies every instance and may mis-map coordinates
- Kind: ambiguity
- Magic behavior: Painting into child definitions affects all instances; when the edit cell is not the selection root the search uses the edit cell with an identity transform although the selection is in root coordinates. Read-only (locked) child cells are not checked.
- Evidence: commands/CmdCD.c:4726-4770
- Options: replicate | fix (require edit cell == selection root; refuse non-editable children)
- Recommendation: fix the coordinate case; keep all-instances semantics (documented).

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

### Q-CMDEDIT-20: Label-area rule for zero-area boxes
- Kind: quirk
- Magic behavior: With a zero-area box, a label counts as inside if it touches the box unless the box lies strictly inside the label's interior.
- Evidence: utils/geometry.h:149-152
- Options: replicate | simplify (touch only)
- Recommendation: replicate.

### Q-CMDEDIT-21: Manhattan directions accepted by splitpaint/spliterase
- Kind: quirk
- Magic behavior: The corner is derived as (pos>>1)-1; north/center act as nw, east as ne, south as se, west as sw, without error.
- Evidence: commands/CmdRS.c:3082-3085, commands/CmdRS.c:3180-3183
- Options: replicate | fix (reject non-diagonal names)
- Recommendation: replicate for script compatibility, with a warning.

### Q-CMDEDIT-30: `edit INSTPATH` does not update the edit transform
- Kind: bug
- Magic behavior: When an instance path is given, the edit cell is set to that instance, but the edit-to-root and root-to-edit transforms and the edit root are left unchanged. Only selection-based `edit` computes them. Later edits map coordinates through a stale transform.
- Evidence: commands/CmdE.c:97-104, commands/CmdE.c:140-150
- Options: replicate | fix (compute the instance's full transform)
- Recommendation: fix (the spec mandates the full transform).

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
- Evidence: commands/CmdRS.c:860-915
- Options: replicate | fix (exact-match qualifiers first)
- Recommendation: replicate for script compatibility.

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
- Evidence: commands/CmdLQ.c:3090-3180
- Options: replicate | fix (`180v` = sideways, as in `getcell`)
- Recommendation: fix `180v` (sideways, consistent with getcell/dump). Keep relative semantics.

### Q-CMDEDIT-63: `array -list` returns only the last instance's values
- Kind: bug
- Magic behavior: In the query loop, each instance replaces the scripting result, so with several selected arrays only the last one is returned. The setters for width, height, pitch and position print "Unimplemented function.".
- Evidence: commands/CmdAB.c:270-470
- Options: replicate | fix (return a list per instance; implement setters)
- Recommendation: fix (return a list of lists). Implement the setters as an extension.

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
- Evidence: commands/CmdRS.c:2420-2470
- Options: replicate | fix (absolute coordinates)
- Recommendation: fix. Only `g` units are affected.

### Q-CMDEDIT-92: Port auto-index depends on label order
- Kind: quirk
- Magic behavior: When making a port without an index, the code scans labels: a same-text port sets idx to its index − 1, but a later port with a higher index overrides it. The result depends on label list order.
- Evidence: commands/CmdLQ.c:2105-2125
- Options: replicate | fix (same-text match wins)
- Recommendation: fix (same-text match wins). Add a golden case to confirm Magic's ordering.

### Q-CMDEDIT-93: `property` targets the window's top cell, not the edit cell
- Kind: ambiguity
- Magic behavior: With a window, `property` reads and writes the window's root cell even when a child is being edited. Without a window, it uses the edit cell. The setter's comment says "current edit cell".
- Evidence: commands/CmdLQ.c:2805-2820
- Options: replicate | fix (use the edit cell)
- Recommendation: replicate; scripts use `cellname property` for explicit targets.

### Q-CMDEDIT-94: `xor` result is not a true XOR across different types
- Kind: quirk
- Magic behavior: Painting type T over a different type H gives T, so overlapping areas with different types show the source type instead of both.
- Evidence: commands/CmdTZ.c:2505-2525
- Options: replicate | fix (per-type XOR)
- Recommendation: replicate for golden compatibility. Offer a per-plane true XOR as a separate option.

### Q-CMDEDIT-95: `xor` options all test the first option's letter
- Kind: bug
- Magic behavior: The switch uses `argv[1][3]` instead of `argv[i][3]`, so `xor -nolabels -novendor dest` applies `-nolabels` twice.
- Evidence: commands/CmdTZ.c:2448-2468
- Options: fix
- Recommendation: fix.

### Q-CMDEDIT-96: `shell` on platforms without process spawning
- Kind: platform
- Magic behavior: Runs the command through the system shell.
- Evidence: commands/CmdRS.c:2736-2780
- Options: platform alternative (unavailable on iPadOS; native on desktop)
- Recommendation: provide on Linux/macOS only.
- Resolution: n/a (platform). Answered by P-13 and capability `hefesto-ipad-feature-scope`: `shell` is desktop-only.
