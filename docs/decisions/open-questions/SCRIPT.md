# Open Questions — SCRIPT area

Capabilities: `scripting-api`, `scripting-pdk-integration`, `pcell-toolkit`, `ui-box-stack-and-measure`, `ui-gui-managers`, `readspice`, `scripting-auxiliary-tools`.
Magic reference: fc5daa9e (8.3.694).

### Q-SCRIPT-1: Scripting language and host on each platform [platform]
- Kind: platform
- Magic behavior: Magic is a Tcl extension, and every automation path goes through Tcl 8.5+/9: startup files, PDK `.magicrc`, PDK device generators, GUI, tag callbacks. PDKs ship Tcl that depends on the Tcl language itself (namespaces, `dict`, `regexp`, `binary scan`, `info procs`, `uplevel`, `$env()`) and on Tk for dialogs.
- Evidence: tcltk/tclmagic.c:1603-1673; tcltk/magic.tcl.in; tcltk/toolkit.tcl
- Options: (a) embed a real Tcl 8.6/9 interpreter (C library) on Linux/macOS and run PDK scripts unchanged, using a non-Tk dialog layer that implements the `magic::add_entry/...` builder API natively; (b) on iPadOS, where Tcl execution is ruled out, provide a declarative PDK profile plus native C++ re-implementations of the generators for selected PDKs, validated against the desktop Tcl output by golden tests; (c) a Tcl-subset interpreter (for example Jim Tcl) on all platforms (rejected for iPadOS by the brief); (d) offline translation of PDK generator scripts.
- Recommendation: (a) on desktop and (b) on iPadOS. Both bind to the host-independent command API in `scripting-api`, so results are identical. The generated-cell naming (`pcell-toolkit`) and property preservation are data-level guarantees on every platform. Record the final choice as an ADR.

### Q-SCRIPT-2: Command failures return Tcl OK
- Kind: quirk
- Magic behavior: commands that print usage or internal errors still return TCL_OK. Only dispatch failures (unknown client command, ambiguity, >200 args, ≥2048 chars) return TCL_ERROR, and those have an empty message because dispatch is quiet. `tech load` failures set a result string but still return OK.
- Evidence: tcltk/tclmagic.c:496-528; textio/txCommands.c:1491-1575; windows/windSend.c:86-287; commands/CmdTZ.c:430-450
- Options: replicate | fix (raise script errors on command failure) | dual mode
- Recommendation: replicate in compatibility mode, because PDK scripts and `toolkit.tcl` rely on `catch` returning 0 (for example `load -fail`). Offer an opt-in strict mode for new scripts.

### Q-SCRIPT-3: Tag `%r` substitution is unescaped, and substitution rescans
- Kind: bug
- Magic behavior: `%r`/`%R` insert `"<result>"` without escaping, so a result containing `"`, `$`, `[` or `]` changes the meaning of the tag script. After each substitution, scanning restarts at the beginning of the string, so `%` sequences inside substituted text (arguments, results, window paths) are substituted again. The `%%` handling can then also re-expand.
- Evidence: tcltk/tclmagic.c:161-262
- Options: replicate | fix (substitute as properly quoted list elements, single pass)
- Recommendation: fix (single pass, list-quoted values) but keep `%r` braces-free for simple results. Add a golden case for results containing spaces, which must still be passed as one quoted word.

### Q-SCRIPT-4: Output escaping depends on existing variable names
- Kind: quirk
- Magic behavior: before output is passed to `puts`, a `$` is escaped only if the following word is NOT an existing Tcl variable. Text such as `$VDD` therefore prints the variable's value, not the literal text.
- Evidence: tcltk/tclmagic.c:1437-1480,1514-1548
- Options: replicate | fix (always print literally)
- Recommendation: fix. Output text is informative and printing it literally is the evident intent.

### Q-SCRIPT-5: Path variable expansion is cached per session
- Kind: quirk
- Magic behavior: `$NAME` in search paths is resolved from the Tcl variable once. The value is cached in a hash table and later changes to the variable are ignored.
- Evidence: utils/path.c:268-330
- Options: replicate | fix (resolve each time)
- Recommendation: fix (resolve each time), with a warning when a cached value would have differed. Verify whether any PDK flow depends on the stale value.

### Q-SCRIPT-6: `port last` counts non-port labels
- Kind: bug
- Magic behavior: `port last` is the maximum `lab_port` over all labels, including non-port labels (index 0), so it returns 0 instead of -1 when a cell has labels but no ports. `port first` considers ports only.
- Evidence: commands/CmdLQ.c:1781-1793
- Options: replicate | fix (ports only)
- Recommendation: replicate. `readspice` and the text helper (`port last + 1`) are tuned to it.

### Q-SCRIPT-7: `cif scale` returns a widened single-precision value
- Kind: quirk
- Magic behavior: the scale is computed in `float` and returned as a Tcl double, for example 0.01 becomes 0.009999999776482582. Scripts that use `expr` on it see that value.
- Evidence: cif/CIFmain.c:139-146; commands/CmdCD.c:1936-1952
- Options: replicate | fix (exact decimal double)
- Recommendation: replicate. `round(3 / [cif scale out])`, `%g` formatting and `measure` text are designed around it, and golden comparisons need it.

### Q-SCRIPT-8: Order of `drc listall why`
- Kind: ambiguity
- Magic behavior: rules are emitted in hash-table iteration order, which is not defined. `drc_save_report` and the DRC manager inherit that order.
- Evidence: drc/DRCmain.c:646-700
- Options: replicate (unspecified) | fix (order by first occurrence, or by rule text)
- Recommendation: define a deterministic order (rule text, ascending). Golden comparisons of `drc.out` must sort the rule blocks.

### Q-SCRIPT-9: PDK script inventory not verified against PDK sources
- Kind: ambiguity
- Magic behavior: the Magic tree does not contain the open_pdks `.magicrc`/`<pdk>.tcl` files. The PDK startup contract and device-script contract in `scripting-pdk-integration` were reconstructed from `toolkit.tcl`, `wrapper.tcl`, `DBio.c` and the known open_pdks templates.
- Evidence: tcltk/toolkit.tcl:1-105,379-402; tcltk/wrapper.tcl:485-505
- Options: obtain sky130A, gf180mcuD and IHP sg13g2 files from open_pdks, and grep the full command and option inventory
- Recommendation: do this before implementation. Add golden cases SCRIPT-PDK-* that run each PDK's generators for every device with default parameters.

### Q-SCRIPT-10: Implicit sourcing of `<tech>.magicrc` during cell reads
- Kind: platform
- Magic behavior: reading a `.mag` whose tech differs, in a paint-free session, searches PDK roots and evaluates `<dir>/<tech>.magicrc` as a script. This is code execution triggered by opening a layout file.
- Evidence: database/DBio.c:600-700
- Options: replicate | fix (ask the user) | platform alternative (apply a declarative profile)
- Recommendation: on desktop, ask once per PDK (trust prompt). On iPadOS, apply the declarative profile only.

### Q-SCRIPT-11: Toolbar sources `./<tech>_toolbars.tcl` from the working directory
- Kind: platform
- Magic behavior: `maketoolbar` sources any `./<techname>_toolbars.tcl` in the current directory, which executes arbitrary code. The reorder dialog rewrites that file.
- Evidence: tcltk/toolbar.tcl:55-73; tcltk/reorderLayers.tcl:178-200
- Options: replicate | fix (parse only `set <name> {...}` lines as data)
- Recommendation: fix. Parse the file as data, accepting the same syntax.

### Q-SCRIPT-12: Unauthenticated socket evaluation
- Kind: platform
- Magic behavior: `socketcmd.tcl` evaluates any line received on TCP 12946 on all interfaces, with no authentication. It is opt-in.
- Evidence: tcltk/socketcmd.tcl:10-27
- Options: omit | provide a loopback-only, token-authenticated equivalent
- Recommendation: omit by default. If provided, bind to loopback only and require a token.

### Q-SCRIPT-13: Wrapper callbacks: scope and per-window key bug
- Kind: bug
- Magic behavior: `Opts(callback)` is `eval`ed inside `openwrapper`, so the callback sees its local variables (`framename`, `layoutframe`, `winname`, `cell`). The per-window callback is read from `Winopts(${framename}, callback)`, whose key contains a space, so a normally named `Winopts(.layout1,callback)` is never run.
- Evidence: tcltk/wrapper.tcl:1495-1499
- Options: replicate | fix (also accept the no-space key)
- Recommendation: replicate the scope (PDKs depend on `$framename`). Fix by also honoring the key without the space.

### Q-SCRIPT-14: Option tables whose keyword ends with a tab
- Kind: quirk
- Magic behavior: the option-table matcher ends an entry's keyword only at a space. In entries like `"self\t\t..."` or `"next [number]"`, an exact match is treated as a prefix match and can become ambiguous if another entry starts with the same text.
- Evidence: utils/lookup.c:96-118; commands/CmdCD.c:1139-1166
- Options: replicate | fix (treat any whitespace as the terminator)
- Recommendation: fix (any whitespace), after checking that no current table changes meaning.

### Q-SCRIPT-15: Unbalanced `resumeall` error text
- Kind: quirk
- Magic behavior: after the counter reaches 0 it is unset, so a further `resumeall` fails with Tcl's `can't read "Winopts(...,suspend)": no such element in array` instead of the intended `resume called without suspend`.
- Evidence: tcltk/tools.tcl:30-50
- Options: replicate | fix (raise `resume called without suspend`)
- Recommendation: fix. The message is informative.

### Q-SCRIPT-16: `ruler` reads the box in the current units but draws in internal units
- Kind: bug
- Magic behavior: `ruler` captures `box values` before switching units to internal, then uses those numbers as internal coordinates. With units microns/lambda it draws at the wrong scale.
- Evidence: tcltk/tools.tcl:340-373
- Options: replicate | fix (capture after `units internal`)
- Recommendation: fix. Golden cases use `units internal` so that they pass either way.

### Q-SCRIPT-17: `measure` multiplies current-unit values by the CIF scale
- Kind: bug
- Magic behavior: the width/height comes from `box values` in the current units and is multiplied by µm per internal unit. The result is correct only for default/internal units.
- Evidence: tcltk/tools.tcl:431-458
- Options: replicate | fix
- Recommendation: fix (measure in internal units).

### Q-SCRIPT-18: Crash-backup interval documentation mismatch
- Kind: doc-mismatch
- Magic behavior: the docs and `crashbackups start` default to 600000 ms (10 min), but the tool setup sets 60000 ms (1 min) before starting.
- Evidence: tcltk/tools.tcl:92-103,584-585; doc/html/crashbackups.html
- Options: replicate 60 s | use 10 min
- Recommendation: replicate the effective 60 s default, and document it.

### Q-SCRIPT-19: `readspice` uses the Tcl `case` command
- Kind: bug
- Magic behavior: `.PININFO` handling uses `case`, which was removed in Tcl 9, so it fails under a Tcl 9 build.
- Evidence: tcltk/readspice.tcl:237-241
- Options: replicate (Tcl-8 only) | fix (`switch`)
- Recommendation: fix. The Tcl 8 behavior is the reference.

### Q-SCRIPT-20: gencell requires `_convert` although documented as optional
- Kind: doc-mismatch
- Magic behavior: a device is accepted only if `<lib>::<type>_convert` exists, while comments say `_convert` is optional in SPICE mode.
- Evidence: tcltk/toolkit.tcl:720-731
- Options: replicate | fix (test `_defaults`)
- Recommendation: replicate (PDKs always define `_convert`), and warn.

### Q-SCRIPT-21: Normalization of Inf/NaN parameter values
- Kind: bug
- Magic behavior: `string is double -strict` accepts `inf`/`nan`, and `round()` then raises a Tcl error, which aborts cell creation.
- Evidence: tcltk/toolkit.tcl:1848-1869
- Options: replicate | fix (treat as non-numeric text)
- Recommendation: fix (verbatim text).

### Q-SCRIPT-22: Murmur hash over non-ASCII characters
- Kind: quirk
- Magic behavior: the Tcl implementation slices by character and uses `binary scan` on characters, so each Unicode character contributes only its low 8 bits, and the length is the character count, not the UTF-8 byte count.
- Evidence: tcltk/toolkit.tcl:1778-1832
- Options: replicate | fix (UTF-8 bytes)
- Recommendation: replicate. Names must match cells already generated by Magic.

### Q-SCRIPT-23: Non-interactive change of a `nocell` device reads undefined variables
- Kind: bug
- Magic behavior: in `magic::gencell`, the existing-instance path for `nocell` devices sets `pitchx $delx`/`pitchy $dely`, which are undefined, so it raises a Tcl error. It also uses `[lindex $arcount 1]` (xhi) as nx instead of xhi−xlo+1.
- Evidence: tcltk/toolkit.tcl:744-752
- Options: replicate | fix (use the array pitch and counts as in `gencell_dialog`)
- Recommendation: fix.

### Q-SCRIPT-24: Create redraws into an already-existing generated cell
- Kind: bug
- Magic behavior: `gencell_create` and `gencell_makecell` call `cellname create` (a no-op if the cell exists) and draw again, so a second create with the same parameters re-draws into the existing cell. Paint is idempotent, but labels/ports and properties may duplicate. A same-named cell that exists on disk but is not loaded is shadowed by a new empty cell.
- Evidence: tcltk/toolkit.tcl:1260-1273,795-808; commands/CmdCD.c:1652-1659
- Options: replicate | fix (reuse the existing cell without drawing, as `gencell_change` does; load from disk first)
- Recommendation: fix, with a golden case comparing label counts.

### Q-SCRIPT-25: `add_dependency` without parameter names
- Kind: bug
- Magic behavior: it sets `parameters` to the procedure NAME string instead of calling it, so `dict keys` raises an error.
- Evidence: tcltk/toolkit.tcl:1351-1356
- Options: replicate | fix (call `_defaults`)
- Recommendation: fix.

### Q-SCRIPT-26: `drc_save_report` count line and units
- Kind: quirk
- Magic behavior: the header writes the raw `drc list count` list (for example `inv {inv 1}`), and the coordinates are multiplied by `cif scale out`, which assumes default/internal units.
- Evidence: tcltk/drcmgr.tcl:248-291
- Options: replicate | fix (total count; internal units)
- Recommendation: replicate the file format, since tools may parse it. Force internal units while writing.

### Q-SCRIPT-27: Library manager path handling
- Kind: bug
- Magic behavior: the tree ids carry a trailing `/` but are compared against paths without one, so every refresh deletes and rebuilds all nodes. Files are left open when a header check fails.
- Evidence: tcltk/libmgr.tcl:186-247,160-180
- Options: replicate | fix
- Recommendation: fix (GUI only).

### Q-SCRIPT-28: Layer reorder dialog inconsistencies
- Kind: bug
- Magic behavior: the Load action sources `$CAD_ROOT/magic/tcl/toolbar.tcl` instead of the presets file. The error text names `<tech>_toolbar.tcl` but the file is `<tech>_toolbars.tcl`. The file is read with `exec cat`.
- Evidence: tcltk/reorderLayers.tcl:146-200
- Options: replicate | fix
- Recommendation: fix (GUI only). Keep the file format.

### Q-SCRIPT-29: gencell change requires an open dialog
- Kind: bug
- Magic behavior: `gencell_change` always reads `.params.title.ient`. When no dialog exists (batch mode, or a script call before any dialog was opened), the non-interactive `magic::gencell <lib>::<type> <existing-inst> k v` raises a Tcl error after `suspendall`, leaving redraw suspended.
- Evidence: tcltk/toolkit.tcl:882-900
- Options: replicate | fix (skip the dialog name when it is absent)
- Recommendation: fix. This is required for headless PDK flows.

### Q-SCRIPT-30: mazeroute file fallback and topology
- Kind: quirk
- Magic behavior: the fallback opens `<netfile>.net` (computing the root name but not using it). Multi-terminal nets are routed as a chain in file order, not as a tree.
- Evidence: tcltk/mazeroute.tcl:11-33
- Options: replicate | fix
- Recommendation: replicate if the tool is provided at all (informative).

### Q-SCRIPT-31: Text helper with default font passes "default" as position
- Kind: bug
- Magic behavior: with font `default`, the helper issues `label <text> <just>`, and `<just>` may be the word `default`, which is not a valid position.
- Evidence: tcltk/texthelper.tcl:make_new_label
- Options: replicate | fix (omit the position when default)
- Recommendation: fix.

### Q-SCRIPT-32: SPICE continuation spacing and the always-modified message
- Kind: quirk
- Magic behavior: the space-insertion test looks at the continuation line's last character, not at the previous line's. `readspice` always reports "port order was modified" for cells with existing ports, because it first moves them.
- Evidence: tcltk/readspice.tcl:68-72,110-131,204-206; tcltk/toolkit.tcl:489-495
- Options: replicate | fix
- Recommendation: replicate the tokenization (tokens are equivalent after empty-token skipping). Fix the message to report real changes only.

### Q-SCRIPT-33: `techmanager initall` is not a valid option
- Kind: bug
- Magic behavior: the startup script calls `magic::techmanager initall`, which matches no option. The window does not exist yet, so the call returns immediately, and menus are built only when the user opens the manager.
- Evidence: tcltk/magic.tcl.in:372; tcltk/wrapper.tcl:413-424
- Options: replicate (no-op) | fix
- Recommendation: replicate (no-op).

### Q-SCRIPT-34: GUI managers on iPadOS and native desktop GUI
- Kind: platform
- Magic behavior: all managers are Tk widgets with fixed Tk path names (`.cellmgr`, `.params`, `.layoutN.pane.top`), and PDK scripts address `F.titlebar.mbuttons.*` directly.
- Evidence: tcltk/wrapper.tcl; tcltk/toolkit.tcl:61-105
- Options: emulate the Tk path names | provide the menu-hook API only
- Recommendation: provide the `magic::add_toolkit_*` hooks and the dialog builder API natively. Path-name compatibility is only needed when desktop Tcl runs PDK scripts.

### Q-SCRIPT-35: Crash backups on sandboxed platforms
- Kind: platform
- Magic behavior: `crash save` writes to the system temporary directory on a Tcl `after` timer.
- Evidence: tcltk/tools.tcl:52-120
- Options: app-container temporary directory and an app timer | OS state restoration
- Recommendation: use the app sandbox's caches directory, with the same interval semantics.

### Q-SCRIPT-36: Wrapper "Rotate 90 degree" calls a non-existent command
- Kind: bug
- Magic behavior: the Edit menu runs `magic::clock`, but only `magic::clockwise` and `magic::rotate` are registered, so the menu item raises "invalid command name".
- Evidence: tcltk/wrapper.tcl:1263; dbwind/DBWCommands.c:260,430
- Options: replicate | fix (`magic::rotate 90`)
- Recommendation: fix.

### Q-SCRIPT-37: Command-line pattern ordering
- Kind: quirk
- Magic behavior: `^-r(e)?(cover)?$` also accepts `-rcover`. `^-n*` matches any argument starting with `-`, so a mistyped option is passed to the engine instead of being taken as a file. `-dnull` disables the wrapper only through its own row.
- Evidence: tcltk/magic.tcl.in:172-224
- Options: replicate | fix (strict option table)
- Recommendation: replicate. Command-line compatibility matters for PDK flows.
