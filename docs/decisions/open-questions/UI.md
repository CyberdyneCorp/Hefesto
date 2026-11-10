# Open Questions — UI area

Capabilities covered: `ui-command-dispatch`, `ui-windows-and-views`, `ui-tools-and-input`, `display-style-files`, `ui-color-window`, `ui-feedback-and-highlights`, `app-startup`, `ui-wizard-and-debug`.
Magic reference: commit fc5daa9e (8.3.694).

### Q-UI-1: `.dstyle5` fallback never parses the file
- Kind: bug
- Magic behavior: If `T.D.dstyle` is not found, Magic opens `T.D.dstyle5`. The parsing code only runs in the branch where the first open succeeded. A `.dstyle5` file is therefore opened, closed unread, and reported as missing all sections, so the load fails. The "not found" message names the `.dstyle5` file.
- Evidence: graphics/grDStyle.c:434-447, graphics/grDStyle.c:588-600
- Options: replicate (no `.dstyle5` support) | fix (parse `.dstyle5` as version 5)
- Recommendation: fix. Parse `.dstyle5` as a version-5 file. No PDK is known to rely on the failure.
- Resolution: fix — a `.dstyle5` file found by the fallback is parsed as a version-5 file; display resources only, and no PDK relies on the failure (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-2: Dstyle section-count and order handling
- Kind: quirk
- Magic behavior: A `display_styles` count other than 52 is only reported, and loading continues, so every technology style index shifts. A `layout_styles` header after `pale_styles` calls the process exit with status 1, which kills the editor.
- Evidence: graphics/grDStyle.c:497-520
- Options: replicate | fix (treat both as load errors, keep running)
- Recommendation: fix both to be non-fatal load errors that reject the file. Never exit the process because of a style file.
- Resolution: compat — a wrong `display_styles` count warns and continues like Magic with magic-compat ON, and rejects the file with it OFF; `layout_styles` after `pale_styles` rejects the file on every setting and never exits the process (exit is a tier-1 data-loss bug) (D-5, approved with change 2026-11-07).

### Q-UI-3: Unknown dstyle section name
- Kind: quirk
- Magic behavior: An unknown section header sets the internal section to "ignore" (−1). ORing −1 into the processed mask makes the final completeness check fail, with an empty list of missing sections. The whole file is rejected.
- Evidence: graphics/grDStyle.c:538-553, graphics/grDStyle.c:588-600
- Options: replicate (reject) | fix (skip the unknown section with a warning)
- Recommendation: replicate the rejection, with a clearer message.
- Resolution: n/a — Magic's rejection of the file is kept; Hefesto's message names the unknown section, and message text is informative (D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-4: Dstyle content before the first blank line
- Kind: ambiguity
- Magic behavior: The "next line is a header" flag starts false. A non-comment line before any blank line is processed under an uninitialized section variable, which is undefined behavior. All shipped files start with comments and then a blank line.
- Evidence: graphics/grDStyle.c:451-480
- Options: replicate (undefined) | fix (treat the first non-comment line as a header)
- Recommendation: fix. Treat the first non-comment line of the file as a section header.
- Resolution: fix — the line is processed under an uninitialised section variable (undefined behaviour); the first non-comment line of the file is treated as a section header (X-2 tier 1, D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-5: Dstyle field edge cases
- Kind: quirk
- Magic behavior:
  - A non-integer `num` column always gets ordinal 1, because the counter is reset for every line.
  - An unknown color name gives color −1, which after masking is all ones.
  - A layout style line without a long name stores NULL. A later tech `styles` lookup by name then compares against NULL, which crashes.
- Evidence: graphics/grDStyle.c:228-253, dbwind/DBWdisplay.c:1693-1704
- Options: replicate | fix (sequential ordinals; error on unknown color; skip unnamed styles in name lookup)
- Recommendation: replicate the ordinal and color results for compatibility. Fix the crash by never matching unnamed styles.
- Resolution: compat — ordinal 1 for a non-integer `num` and the all-ones mask for an unknown color, as in Magic; the crash on a style without a long name is always fixed: such a style never matches a lookup by name; with `magic-compat` OFF, ordinals are sequential and an unknown color name is an error (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-6: Stipple bit orientation differs between drivers
- Kind: platform
- Magic behavior:
  - X11/Tk drivers: row 0 is the top of the 8×8 cell and bit 0 is the leftmost pixel.
  - OpenGL (`glPolygonStipple` with defaults): row 0 is the bottom row and the most significant bit is leftmost.
  - The same file therefore renders mirrored and flipped between backends.
- Evidence: graphics/grTk1.c:288-295, graphics/grX11su1.c:288-295, graphics/grTOGL1.c:204-210
- Options: follow X11 | follow OpenGL | per-variant (X11 orientation for 7bit/24bit files, OpenGL orientation for OpenGL files)
- Recommendation: per-variant. Render each file in the orientation of the Magic backend it was written for, so screenshots match the reference.
- Resolution: decided by X-5 (2026-11-05). Stipple bit orientation follows Magic's OpenGL/Cairo driver for every file (capability `display-style-files`). The per-variant recommendation above is superseded.

### Q-UI-7: Color-map strictness and name loss
- Kind: quirk
- Magic behavior:
  - Blank lines in a `.cmap` are syntax errors. Comments must have `#` in column 1.
  - Saving merges equal runs and keeps only the name of each run's first index.
  - Editing a color clears its name.
- Evidence: graphics/grCMap.c:153-200, graphics/grCMap.c:281-299, graphics/grCMap.c:384-405
- Options: replicate | fix (tolerate blank lines; keep names)
- Recommendation: tolerate blank lines on read, as a superset. Replicate the write format exactly.
- Resolution: fix — blank lines in a `.cmap` are skipped on read, a superset of what Magic accepts; the write format is replicated exactly (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-8: Which dstyle variant Hefesto renders with
- Kind: platform
- Magic behavior: The display decides the file:
  - `OpenGL`: alpha from the mask column
  - `7bit`: bit-plane write masks combined through the colormap
  - `24bit`: X11 TrueColor, where the mask is ANDed with a 24-bit plane mask and colors index a 256-entry map
  - `bw`: monochrome
- Evidence: graphics/grTk1.c:633-654, graphics/grTOGL1.c:89-136, graphics/grDStyle.c:228-236
- Options: always `OpenGL` | user-selectable | emulate 7-bit plane mixing on the GPU
- Recommendation: default to the `OpenGL` variant, with fallback to `24bit` if missing. Offer a setting to emulate `7bit` plane mixing for users who want the classic look.
- Resolution: decided by X-5 (2026-11-05). The `OpenGL` dstyle/cmap variant is the default; the other variants are selectable in settings (capability `display-style-files`).

### Q-UI-9: "Did you point to the correct window?" is never printed
- Kind: quirk
- Magic behavior: The unknown-command branch returns before that message whenever messages are enabled, so the message is dead code.
- Evidence: windows/windSend.c:200-230
- Options: replicate | fix (print it)
- Recommendation: replicate. Hefesto's own message wording is informative anyway.
- Resolution: replicate — the message is unreachable in both builds (with messages on, the unknown-command branch returns -3 before it, and the `window is grabbing all input` line is equally dead), so Hefesto prints exactly Magic's output and the spec's no-extra-line scenario stands (verified against windows/windSend.c:WindSendCommand on 2026-11-09; approved 2026-11-09).

### Q-UI-10: Command-length and word-count limits
- Kind: quirk
- Magic behavior:
  - Non-Tcl: each command is copied into a 2048-byte buffer, so the rest of a longer line is silently lost. The word limit fires at 200, so at most 199 words are accepted, and the remaining commands on the line are dropped.
  - Tcl: errors are raised above 200 words or 2048 bytes.
- Evidence: textio/txCommands.c:1095-1153, utils/parser.c:140-155, textio/txCommands.c:1491-1520
- Options: replicate | fix (no fixed limits)
- Recommendation: fix. Use no practical limit, but keep error messages for scripts that test the limits.
- Resolution: fix — the normative Tcl build (D-7) only raises errors above 200 words or at 2048 bytes and the byte truncation is non-Tcl-only, so lifting both limits in every mode only turns former errors into successes and no Magic script or PDK can depend on them; the two error messages are dropped rather than kept at some other limit (verified against textio/txCommands.c:TxTclDispatch and textio/txCommands.c:TxParseString_internal on 2026-11-09; approved 2026-11-09).

### Q-UI-11: Non-Tcl key macros ignore the focused window's client
- Kind: quirk
- Magic behavior: The terminal-driven build looks up every key in the layout client's current-tool table, even when the pointer is over a color or netlist window. The Tcl build uses the window's client.
- Evidence: textio/txCommands.c:1284, graphics/grTOGL1.c:778
- Options: replicate the non-Tcl behavior | follow the Tcl behavior
- Recommendation: follow the Tcl behavior. Macros are per window client.
- Resolution: n/a — the Tcl build is normative (D-7): key macros are looked up in the focused window's client; the non-Tcl behaviour is informative (D-7, 2026-11-07).

### Q-UI-12: Key naming tied to X11 keysyms
- Kind: platform
- Magic behavior: Key names are X11 keysym names with modifier prefixes. Encoded keys are keysym & 0xFFFF | modifiers << 16. Non-X builds accept only single characters and `^X`.
- Evidence: utils/macros.c:514-761
- Options: embed an X11 keysym name table on all platforms | native key names
- Recommendation: embed the keysym name table, so `.magicrc` files are portable. Map native key codes (macOS, iPadOS hardware keyboards, Wayland) to keysyms.
- Resolution: replicate — `.magicrc` and PDK macro files name keys as X11 keysyms with `Meta_`/`Control_`/`Capslock_`/`Shift_` prefixes and 16-bit keysym masking, so Hefesto embeds the keysym name table on every platform and maps native key codes to it (note: Magic falls back to single-character/`^X`/`0x` names when no X display is open, which matters only for headless golden captures of `macro` listings) (verified against utils/macros.c:MacroName and utils/macros.c:MacroKey on 2026-11-09; approved 2026-11-09).

### Q-UI-13: Script line oddities
- Kind: bug
- Magic behavior:
  - A line continued with `\` keeps the backslash: `a \` + `b` gives `a \b`.
  - The non-Tcl reader strips the first character when the SECOND character is `;` (`inputLine[1] == ';'`), which was probably meant to test the first character.
- Evidence: textio/txCommands.c:1364-1418 (txGetFileCommand), tcltk/tclmagic.c:1290-1308 (the Tcl build's TxDispatch, which has no caller in the Tcl build), utils/main.c:899-1127 (startup files: Tcl_EvalFile in the Tcl build)
- Options: replicate | fix (drop the backslash; strip only a leading `:` or `;`)
- Recommendation: fix both. Scripts relying on them are unlikely. Golden case UI-G-021 records Magic's behavior.
- Resolution: fix — evidence corrected: `txGetFileCommand` is reached only through the non-Tcl `source` command and non-Tcl startup files, while in the normative Tcl build (D-7) startup files and `source` go through Tcl, where `\`-newline is a standard continuation and there is no `;` test, so both quirks are informative non-Tcl behaviour and Hefesto follows Tcl (the spec's `\10` scenario must change accordingly) (verified against textio/txCommands.c:txGetFileCommand and tcltk/tclmagic.c:TxDispatch on 2026-11-09; approved 2026-11-09).

### Q-UI-14: Command log omits ':' on the box line
- Kind: bug
- Magic behavior: In the non-Tcl build every logged command starts with `:` except the initial `box values` line, so replaying the log would read that line as key macros.
- Evidence: textio/txCommands.c:703-772
- Options: replicate | fix
- Recommendation: fix (prefix it like the other lines).
- Resolution: other (n/a) — the `:` prefix is written only by the non-Tcl build; the normative Tcl build (D-7) writes the `#` header and the `view` and `box values` lines with no `:` at all, which the spec already requires, so there is nothing to fix (verified against textio/txCommands.c:TxLogStart on 2026-11-09; approved 2026-11-09).

### Q-UI-15: sleep and echo -n off-by-one effects
- Kind: bug
- Magic behavior: `sleep N` sleeps N−1 seconds. `echo -n a b` prints a leading space.
- Evidence: windows/windCmdSZ.c:319-362, windows/windCmdAM.c:646-688
- Options: replicate | fix
- Recommendation: fix `sleep` to sleep N seconds. Replicate `echo` (harmless).
- Resolution: fix — `sleep` exists in both builds and its N−1 pause only changes timing, never output, so `sleep N` pauses N seconds; `echo` is a non-Tcl-only command, so its leading space is informative under D-7 rather than replicated (verified against windows/windCmdSZ.c:windSleepCmd and windows/windCmdAM.c:windEchoCmd on 2026-11-09; approved 2026-11-09).

### Q-UI-16: View numbers depend on the drawing-area pixel size
- Kind: platform
- Magic behavior: `view get`, `cursor`, `setpoint` and scrolling results depend on the window's pixel size and chrome, which differ across hosts. On Retina/HiDPI screens the pixel unit is also ambiguous.
- Evidence: windows/windView.c:154-234, windows/windInt.h:60-92
- Options: logical points | physical pixels
- Recommendation: use logical points for all window-pixel quantities. Compatibility tests compare views only for fixed headless window sizes.
- Resolution: platform — window-pixel quantities are logical points on every platform; compatibility tests compare views only for fixed headless window sizes (P-11, D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-17: windowpositions output bugs
- Kind: bug
- Magic behavior: With a file argument, lines are written through a pointer to the wrong object, so output is garbage or a crash. In Tcl without a file, each window replaces the result, so only the last window is returned.
- Evidence: windows/windCmdSZ.c:1177-1238
- Options: replicate | fix (write all lines; return a list of lists)
- Recommendation: fix.
- Resolution: fix — writing through the wrong object gives garbage or a crash; all lines are written to the file, and without a file one list per window is returned (X-2 tier 1, 2026-11-07).

### Q-UI-18: `view` single-argument form parses y as x
- Kind: quirk
- Magic behavior: When `view "llx lly urx ury"` is given as one argument, all four values are parsed as x coordinates. This only matters for non-square user-grid units.
- Evidence: windows/windCmdSZ.c:870-905
- Options: replicate | fix
- Recommendation: fix.
- Resolution: fix — the single-argument form parses its values as x, y, x, y; only the view changes, never layout data (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-19: `expand` toggle detection
- Kind: quirk
- Magic behavior: The last argument counts as `toggle` if it is any prefix of `toggle`, including `t`.
- Evidence: commands/CmdE.c:840-850
- Options: replicate | fix
- Recommendation: replicate.
- Resolution: n/a — prefix matching of `toggle` follows Magic's usual keyword rule; the spec keeps it (D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-20: Label text visibility depends on backend font sizes
- Kind: platform
- Magic behavior: Bitmap label size is chosen from four backend font sizes compared with pixels-per-unit. Text disappears when 3× pixels-per-unit is no more than the small font height.
- Evidence: dbwind/DBWdisplay.c:205-232
- Options: replicate with Hefesto's font metrics | a fixed threshold in points
- Recommendation: replicate the rule with Hefesto's four UI text sizes.
- Resolution: platform — the visibility rule is replicated with Hefesto's four UI text sizes in place of the backend font sizes (X-5, D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-21: `grid multiple` has no effect
- Kind: quirk
- Magic behavior: The value is stored, even when out of range (with an error message), but no drawing code reads it.
- Evidence: commands/CmdFI.c:1398-1418, graphics/grClip.c:52
- Options: replicate | implement (coarser grid at large scales)
- Recommendation: implement as documented. Draw every m-th line when lines are too dense, and reject out-of-range values without storing them.
- Resolution: fix — `grid multiple` is implemented as documented: when grid lines would be too dense to draw, every m-th line is drawn, and out-of-range values are rejected without being stored; display only (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-22: Touch gesture vocabulary
- Kind: platform
- Magic behavior: Three mouse buttons with Shift/Control modifiers, a wheel, and a keyboard.
- Evidence: magic/proto.magicrc.in:247-298, tcltk/tools.tcl:519-584
- Options: tap/long-press/modifier palette | on-screen button bar | Apple Pencil hover and squeeze
- Recommendation:
  - tap = Button1, long-press = Button3, two-finger tap = Button2
  - a floating modifier palette for Shift and Control
  - two-finger pan = scroll, pinch = zoom
  - all bindings rebindable through the macro tables
- Resolution: decided by X-5 (2026-11-05). Mode-based gesture table of X-5 (capability `hefesto-input-and-touch`); the mapping recommended above is superseded (long press opens a context menu, not Button3).

### Q-UI-23: `feedback add` style argument parsing
- Kind: quirk
- Magic behavior: A numeric third argument starts the point list and is not a style. Long style names are accepted only for indices 1–51, and the error text calls them "numbered styles". The invalid-option message lists `save` twice.
- Evidence: commands/CmdFI.c:198-235
- Options: replicate | fix (also accept technology style names)
- Recommendation: replicate the parsing, and accept technology style long names as an extension.
- Resolution: replicate — the numeric-third-argument rule, the five short names and the error texts are kept as in Magic, and technology style long names (index ≥ 52, which `GrGetStyleFromName` finds but Magic rejects) are accepted as a superset, following the Q-UI-7 precedent that accepting what Magic rejects breaks no script (verified against commands/CmdFI.c:CmdFeedback and graphics/grDStyle.c:GrGetStyleFromName on 2026-11-09; approved 2026-11-09).

### Q-UI-24: `feedback save` loses information
- Kind: quirk
- Magic behavior: It writes rounded internal-unit box coordinates with no unit suffix, so a replay under lambda snapping misplaces them. Non-named styles, diagonal flags and sub-unit precision are lost.
- Evidence: commands/CmdFI.c:415-445
- Options: replicate | fix (write `i`-suffixed coordinates and the full style)
- Recommendation: replicate the format, but suffix the coordinates with `i`. Magic accepts that suffix, so the files stay Magic-readable.
- Resolution: fix — suffixing the saved `box` coordinates with `i` keeps the file Magic-readable (Magic's coordinate parser accepts the `i` suffix) and makes replay correct under any `units` setting, and no PDK or tool reads these files back other than as scripts (verified against commands/CmdFI.c:CmdFeedback and dbwind/DBWfdback.c:DBWFeedbackNth on 2026-11-09; approved 2026-11-09).

### Q-UI-25: Command-line file argument handling
- Kind: quirk
- Magic behavior:
  - Files are loaded in reverse order, and the window ends showing the first one.
  - File arguments are dropped when no initial window is made (`-nowindow`).
  - `.cif` and `.gds` arguments are announced but never read.
  - Any extension starting with `.gd` counts as GDS.
- Evidence: utils/main.c:388-452, utils/main.c:1183-1226
- Options: replicate | fix (load in order, read CIF/GDS, check exact extensions)
- Recommendation: fix. Load in command-line order, ending on the first file, and read GDS and CIF.
- Resolution: fix — files are loaded in command-line order and the window ends on the first one, file arguments are kept with `-nowindow`, `.cif` and `.gds` arguments are read, and only exact extensions select a type (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-26: Installation root and search paths on each platform
- Kind: platform
- Magic behavior: `CAD_ROOT` comes from a Tcl variable, then an environment variable, then a compile-time prefix. Paths expand `~`, `$VAR` and `/tmp`-style host paths.
- Evidence: utils/path.c:193-388, tcltk/tclmagic.c:1663-1669
- Options: environment plus settings | settings only on iPadOS
- Recommendation: Linux and macOS honor `CAD_ROOT` and `~`. iPadOS uses an app-sandbox root chosen in settings, where `$CAD_ROOT` maps to that folder and `~` maps to the app's Documents folder.
- Duplicate: tracked in Q-CELL-17, which is the canonical entry. Record the resolution there. (Consolidation audit, 2026-11-06.) Platform owner: capability `hefesto-files-and-pdks`.
- Resolution: platform — duplicate of Q-CELL-17; see the resolution there (X-4, 2026-11-07).

### Q-UI-27: Home startup file read twice (non-Tcl)
- Kind: quirk
- Magic behavior: When started in the home directory, the non-Tcl build reads `~/.magicrc` both as the home file and as the local file. The Tcl build skips the second read.
- Evidence: utils/main.c:1066-1125, utils/main.c:991-993
- Options: replicate | fix
- Recommendation: fix, following the Tcl behavior.
- Resolution: n/a — the Tcl build is normative (D-7) and reads `~/.magicrc` once; the spec already follows it and records the non-Tcl behaviour as informative (D-7, 2026-11-07).

### Q-UI-28: Manual page says the default technology is scmos
- Kind: doc-mismatch
- Magic behavior: The code falls back to the built-in `minimum` technology. magic(1) says `scmos`.
- Evidence: utils/main.c:1130-1170, doc/man/magic.1
- Options: follow code | follow doc
- Recommendation: follow the code.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-UI-29: Display-type prefix rule and Hefesto's display names
- Kind: doc-mismatch
- Magic behavior: A `-d` value matches a known name when the VALUE STARTS WITH the name (so `NULLX` → NULL), not when it is a prefix of the name (`X` matches nothing).
- Evidence: graphics/grMain.c:275-282
- Options: replicate | fix
- Recommendation: replicate. Map `NULL` to headless and every other known name to the default GPU backend.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-UI-30: Headless mode on iPadOS
- Kind: platform
- Magic behavior: Batch runs read commands from standard input with no display.
- Evidence: tcltk/magicdnull.c:22-61
- Options: none on iPadOS | Shortcuts or App Intents running a script
- Recommendation: no stdin batch on iPadOS. Expose script execution through App Intents later.
- Resolution: platform — no standard-input batch mode on iPadOS; headless runs are desktop-only, and script execution through App Intents may come later (P-13, X-4, D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-31: Crash backup location and liveness check
- Kind: platform
- Magic behavior: Backups are `$TMPDIR/MAG<pid>.*`. The process-id liveness check uses signals, which are unavailable to sandboxed iPadOS apps.
- Evidence: database/DBio.c:1114-1200
- Options: replicate on desktop | app container directory with a lock file
- Recommendation: use an app-private backup directory on all platforms, with a lock file per session for the liveness check. Keep the backup file format Magic-readable.
- Duplicate: tracked in Q-CELL-30, which is the canonical entry. Record the resolution there. (Consolidation audit, 2026-11-06.) The recommendations of Q-UI-31, Q-CELL-30 and Q-SCRIPT-35 disagree on the desktop location; see docs/decisions/audit/DECISIONS.md.
- Resolution: platform — decided by D-8 (2026-11-07). Duplicate of Q-CELL-30; see the resolution there.

### Q-UI-32: `quit` exit status not range-checked
- Kind: bug
- Magic behavior: The range check tests the default value instead of the parsed one, so `quit 300` exits with status 300 mod 256 = 44, and negative values wrap.
- Evidence: windows/windCmdNR.c:280-289
- Options: replicate | fix (reject values outside 0–255)
- Recommendation: fix. Reject with `Invalid exit_status`.
- Resolution: fix — `quit` rejects a status outside 0–255 with `Invalid exit_status` and does not exit (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-33: `macro copy` from a non-layout client
- Kind: bug
- Magic behavior: When the source client is not the layout client, the table name variable is read uninitialized, which is undefined behavior.
- Evidence: utils/macros.c:462-512
- Options: replicate | fix (use the client name)
- Recommendation: fix.
- Resolution: fix — uninitialised read; the source client's name is used as the table name (X-2 tier 1, 2026-11-07).

### Q-UI-34: Glyph character mapping with shared short names
- Kind: quirk
- Magic behavior: The reverse short-name table keeps the earliest style with each character. Characters no style uses map to style 0 (transparent). Many styles use `-`.
- Evidence: graphics/grDStyle.c:621-636, graphics/grGlyphs.c:170-175
- Options: replicate
- Recommendation: replicate.
- Resolution: n/a — intended behaviour: the earliest style wins a shared short name and unused characters map to style 0; the spec keeps it (D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-35: OpenGL translucency formula
- Kind: platform
- Magic behavior: For mask < 127, the OpenGL driver draws color c' = 2c − 0.8 per channel (clamped by GL) with alpha = mask/127, using standard alpha blending, to get more saturated overlaps. Cairo has its own variant.
- Evidence: graphics/grTOGL1.c:90-136
- Options: replicate the formula | plain alpha
- Recommendation: replicate it in the shader, so colors match Magic's OpenGL screenshots.
- Resolution: platform — the OpenGL translucency formula (c' = 2c − 0.8, alpha = mask/127) is reproduced in Hefesto's shaders for the default OpenGL variant, so colors match Magic's OpenGL screenshots (X-5, D-5, approved 2026-11-07, 2026-11-07).

### Q-UI-36: `undo 0` / `redo 0` toggle undo recording
- Kind: quirk
- Magic behavior: `redo 0` disables undo recording and `undo 0` re-enables it. These commands are registered in the global window client, but their semantics belong to the undo capability.
- Evidence: windows/windCmdNR.c:335-408, windows/windCmdSZ.c:634-715
- Options: replicate
- Recommendation: replicate. Cross-reference with the undo capability owner.
- Resolution: compat — same as Q-MODEL-30: with `magic-compat` ON `undo 0` resumes and `redo 0` suspends recording; with it OFF both are no-ops (D-11, 2026-11-07).

### Q-UI-37: Color window button simulation and color picking
- Kind: quirk
- Magic behavior: The color window's `pushbutton` sends only a press. Picking a color from the screen is compiled out in the Tcl build, because it can only read the color window itself.
- Evidence: cmwind/CMWcmmnds.c:190-210, cmwind/CMWcmmnds.c:300-358
- Options: replicate | fix (a real eyedropper over any window)
- Recommendation: fix. Provide a cross-window eyedropper as an extension.
- Resolution: other — replicate `pushbutton` as press-only, since every color-window action fires on press and the release handler is reached only after a press over the current-color box; drop the screen eyedropper rather than build it, because the Tcl build compiles picking out entirely and under the X-5 translucent OpenGL default a screen pixel is a blend of several styles, not one colormap entry, so any "pick a color from the layout" feature would have to pick a layer's display style instead and belongs to the design-system/UX scope, not to Magic compatibility (verified against cmwind/CMWcmmnds.c:cmwButtonDown and cmwind/CMWcmmnds.c:cmwPushbutton on 2026-11-09; approved 2026-11-09).

### Q-UI-38: Feedback "small areas become solid" claim
- Kind: doc-mismatch
- Magic behavior: A comment says stippled feedback styles become solid at small sizes, but no code does this.
- Evidence: dbwind/DBWfdback.c:205-225
- Options: follow code
- Recommendation: follow the code.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-UI-39: dstyle(5), cmap(5) and glyphs(5) manual pages are outdated
- Kind: doc-mismatch
- Magic behavior: The pages describe styles 1–64/65–128, octal-only fields, `.dstyle5` and `.cmap1` names, and two sections. The code uses file-order indices, version 6/7/8 formats and four sections.
- Evidence: doc/man/dstyle.5, doc/man/cmap.5, graphics/grDStyle.c
- Options: follow code
- Recommendation: follow the code.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-UI-40: Window chrome
- Kind: platform
- Magic behavior: Magic draws its own caption, borders, scroll bars and zoom icon in pixel-exact positions, with frame dragging for move and resize.
- Evidence: windows/windDisp.c:386-668, windows/windClient.c:315-640
- Options: replicate the chrome | native toolkit chrome
- Recommendation: native chrome with equivalent actions. Keep the caption text exact.
- Resolution: platform — native shell chrome with equivalent actions (Raven on the desktop, SwiftUI on iPadOS); the caption text stays exact (P-11, 2026-11-07).

### Q-UI-41: Command names in a Tcl script resolve without abbreviation or case folding
- Kind: behavior
- Status: approved (raised by the M2 golden captures, 2026-11-10)
- Magic behavior: In the normative Tcl build (D-7) every command is a Tcl command with its full name, so a script line is resolved by Tcl, not by `windSend`'s unique-prefix lookup: `e` and `extr` give `invalid command name "e"` / `invalid command name "extr"` (only registered aliases such as `ext` run), `getc` is not `getcell`, `ZoOm 2` is `invalid command name "ZoOm"`, and the unknown word's message is the Tcl error `invalid command name "foo"`, printed as the result line with nothing on stderr; the next script line still runs. Interactive Tcl consoles add tclsh's own abbreviation of unique prefixes (`tcl_interactive`), but no case folding. The spec's "Command name lookup" (prefixes, case-insensitive) and "Unknown command" (`Unknown command:`) describe `windSend`, which the Tcl build reaches only after Tcl found the command.
- Evidence: golden UI-G-001 (`getc`), UI-G-002 (`e`, `extr`, `ext`), UI-G-003 (`foo`), UI-G-004 (`ZoOm`); tcltk/tclmagic.c (command registration per client), windows/windSend.c:WindSendCommand, utils/lookup.c.
- Options: replicate (script lines resolve exactly; unknown → `invalid command name "W"` result, script continues; abbreviation only for typed command-bar lines) | keep the `windSend` lookup everywhere (record UI-G-001..004 as deviations)
- Recommendation: replicate for scripts and the headless CLI, keep unique-prefix completion for lines typed in the command bar (a superset no script can observe); the D7 sentence "Prefix lookup and case handling move into cmd::Registry (closes UI-G-001..004 gaps)" and the "Command name lookup" scenarios then need rewording.
- Resolution: replicate for scripts and the headless CLI (exact names, unknown → `invalid command name "W"` result line, script continues); keep unique-prefix completion for lines typed in the desktop command bar. Approved 2026-11-10.
- Implementation note (INTEGRATION, 2026-11-10; for the user to confirm, the Resolution above is unchanged): "script continues" holds where each line's error is caught, as in the golden harness and `hefesto-cli --diagnostics=magic`. A plain `hefesto-cli` script stops at the first error with exit status 1, which is what Magic does with a script file given on its command line: `magic -dnull script.tcl` with the lines `e` and `puts after` prints `Error parsing "script.tcl": invalid command name "e"` and never runs `puts after` (checked in the `hefesto-magic-only` container). If the user wants the plain CLI to continue too, amend the Resolution.

### Q-UI-42: Macro tables in headless golden captures lack the X keysym bindings
- Kind: platform
- Status: approved (raised by the M2 golden captures, 2026-11-10)
- Magic behavior: The golden captures run `magic -dnull`, where no X display is open, so `MacroKey` cannot parse `XK_*`/`Button*` names and keysym-modified names (`Control_r`, `Shift_a`): the system `.magicrc` bindings that use them are never defined, and `macro Button1`, `macro Control_r` and `tool info` show nothing. Q-UI-12 decided that Hefesto embeds the keysym table on every platform, so Hefesto's tables hold those bindings; the listings then differ in content and, through the different table population, in hash order. `macro layout list` (a client name) also lists the tool table names (`pick nettool wiring color box`) in Magic.
- Evidence: golden UI-G-009, 011, 012, 013, 046, 049; utils/macros.c:MacroKey, MacroName (no-display fallback noted in Q-UI-12).
- Options: record these cases as deviations under Q-UI-12 (after DISPATCH checks that every remaining difference is a keysym binding) | capture with a virtual X display (Xvfb in the capture container) so Magic defines the bindings | drop the listing commands from the cases
- Recommendation: capture with Xvfb (it also unblocks UI-G-015/047/082), else deviations under Q-UI-12.
- Resolution: record the affected cases as deviations under Q-UI-12 (no virtual X display in the capture container, per the M2 plan); DISPATCH first checks that every remaining difference is a keysym binding. Approved 2026-11-10.

### Q-UI-43: The command point of script commands on Magic's null display
- Kind: platform
- Status: approved (raised by the M2 golden captures, 2026-11-10)
- Magic behavior: Under `-dnull` every script command carries a point at screen (100, 100) in the topmost window (`*winddebug` echoes `Sending command: '*winddebug' at (100, 100)`). So `*coord` prints a `Point:` line and the root cell of that window, and after `specialopen color` the topmost window is the color window and `color` goes to the color client (which, with no color map on the null display, prints nothing). `cursor` reads the X pointer position, which the null display leaves uninitialised (`cursor window` gave `-531539648 21889`), so its values are not reproducible at all. Hefesto's headless dispatcher has no command point until `setpoint`.
- Evidence: golden UI-G-048 (`cursor`), UI-G-060 (`color` after `specialopen color`), UI-G-081 (`*coord`, `*winddebug`); windows/windSend.c, graphics/grNull.c.
- Options: replicate a default script point (screen (100,100) of the topmost window, layout or color) in the headless CLI | keep "no point" and record the cases as deviations, dropping `cursor` from UI-G-048 (its Magic values are uninitialised memory)
- Recommendation: replicate the default point in the headless CLI only (no effect on the desktop shell, whose pointer is real), and drop the `cursor` queries from UI-G-048.
- Resolution: replicate the default script point (screen (100,100) of the topmost window, layout or color) in the headless CLI only; drop the `cursor` queries from UI-G-048. Approved 2026-11-10.

### Q-UI-44: `id` and `get` are ambiguous in Magic's command lookup
- Kind: quirk
- Status: approved (raised by DISPATCH while re-capturing golden UI-G-002, 2026-11-10)
- Magic behavior: The layout client registers the aliases as their own table entries, `id<TAB>OR` and `get<TAB>OR`. `Lookup()` ends a table name at a space only, so the names it compares are `id\tOR...` and `get\tOR...`: the word `id` is a prefix of both `id\t...` and `identify`, and `get` of both `get\t...` and `getcell`/`getnode`, so both words are ambiguous. In the Tcl build the quiet dispatch turns that into a Tcl error with an empty result and nothing printed; `identify`, `getcell` and every longer prefix work.
- Evidence: golden UI-G-002 (`id` fails with an empty result, run 42803858356); utils/lookup.c:90-103 (Lookup), dbwind/DBWcommands.c:349 and :366 (the `get<TAB>OR` and `id<TAB>OR` entries), tcltk/tclmagic.c (quiet dispatch).
- Options: replicate with magic-compat on (aliases fail quietly; with magic-compat off they run `identify` and `getcell`) | fix everywhere (`id` and `get` run `identify` and `getcell`; record UI-G-002 as a deviation)
- Recommendation: replicate under magic-compat (X-2 tier 2: a cosmetic lookup quirk, observable in scripts), which is what the round-3 tree implements (`ui::magic_lookup_ambiguous`, tests "id is an alias of identify only with magic-compat off" and "get is an alias of getcell only with magic-compat off"); with magic-compat off the aliases work.
