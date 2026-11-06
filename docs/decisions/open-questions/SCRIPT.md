# Open Questions — SCRIPT area

Capabilities: `scripting-api`, `scripting-pdk-integration`, `pcell-toolkit`, `ui-box-stack-and-measure`, `ui-gui-managers`, `readspice`, `scripting-auxiliary-tools`.
Magic reference: fc5daa9e (8.3.694).

### Q-SCRIPT-1: Scripting language and host on each platform [platform]
- Kind: platform
- Magic behavior: Magic is a Tcl extension, and every automation path goes through Tcl 8.5+/9: startup files, PDK `.magicrc`, PDK device generators, GUI, tag callbacks. PDKs ship Tcl that depends on the Tcl language itself (namespaces, `dict`, `regexp`, `binary scan`, `info procs`, `uplevel`, `$env()`) and on Tk for dialogs.
- Evidence: tcltk/tclmagic.c:1603-1673; tcltk/magic.tcl.in; tcltk/toolkit.tcl
- Options: (a) embed a real Tcl 8.6/9 interpreter (C library) on Linux/macOS and run PDK scripts unchanged, using a non-Tk dialog layer that implements the `magic::add_entry/...` builder API natively; (b) on iPadOS, where Tcl execution is ruled out, provide a declarative PDK profile plus native C++ re-implementations of the generators for selected PDKs, validated against the desktop Tcl output by golden tests; (c) a Tcl-subset interpreter (for example Jim Tcl) on all platforms (rejected for iPadOS by the brief); (d) offline translation of PDK generator scripts.
- Recommendation: (a) on desktop and (b) on iPadOS. Both bind to the host-independent command API in `scripting-api`, so results are identical. The generated-cell naming (`pcell-toolkit`) and property preservation are data-level guarantees on every platform. Record the final choice as an ADR.
- Resolution: decided by X-3 (2026-11-05). Sandboxed embedded Tcl on every platform, including iPadOS, plus native C++ PCell generators as a fast path (capability `hefesto-scripting`). Option (b) survives only as the iPadOS App Store fallback.

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

### Q-SCRIPT-9: PDK script inventory not verified against PDK sources — partially resolved
- Kind: ambiguity
- Magic behavior: the Magic tree does not contain the open_pdks `.magicrc`/`<pdk>.tcl` files. The PDK startup contract and device-script contract in `scripting-pdk-integration` were first reconstructed from `toolkit.tcl`, `wrapper.tcl` and `DBio.c`.
- Evidence: tcltk/toolkit.tcl:1-105,379-402; tcltk/wrapper.tcl:485-505. Verified sources: open_pdks @ c6df28c1 (`sky130/magic/*`, `gf180mcu/magic/*`, `common/pdk.tcl`, `sky130/Makefile.in`, `gf180mcu/Makefile.in`) and IHP-Open-PDK @ 5e6d592e (`ihp-sg13g2/libs.tech/magic/*.tcl`, `*.magicrc`). open_pdks has no sg13g2 support; IHP ships its own Magic files.
- Status: partially resolved (source reading only). Corrections made in the specs:
  - The rc files do not wrap `tech load` in `suspendall`/`resumeall`, do not set `drc style`/`drc on`, do not use `MAGTYPE`, and do not call `query_mylib_ip`/`query_my_projects`. Those were assumptions. Library paths are `libs.ref/<lib>/mag`.
  - The rc files do use `tech lambda` + `scalegrid`, `random seed`, `ext2spice scale off`, `quit -noprompt` on a tech-name mismatch, and (IHP) `info script` to find their own directory.
  - `PDKPATH` is set by the device script, not the rc file.
  - `Opts(callback)` is prepended with `set … [subst …]`, not `lappend`ed.
  - Fixed devices use `deltax/deltay/xstep/ystep`, not `pitchx/pitchy`.
  - The PDKs define helpers in `magic::` (`spice2float`, `u2i`, …) that Hefesto must not predefine.
  - Generators use far fewer framework-level commands than assumed: no `pushstack`, `identify`, `what -list`, `goto`, `drc check` or `instance list`.
- Remaining: (a) run SCRIPT-PDK-09 (command trace) to confirm the static inventory dynamically, because static grep can miss commands built in strings; (b) the expected values in the scenarios marked "(verify against Magic)"; (c) other open_pdks-style PDKs that were not checked (for example gf180mcu variants that differ only in `#ifdef`s, and sky130B).
- Options: run the golden cases SCRIPT-PDK-01..17 on Magic 8.3.694 with the pinned PDK commits
- Recommendation: pin the PDK commits above in the golden-test harness, and re-verify whenever either PDK repository bumps its Magic templates.

### Q-SCRIPT-10: Implicit sourcing of `<tech>.magicrc` during cell reads
- Kind: platform
- Magic behavior: reading a `.mag` whose tech differs, in a paint-free session, searches PDK roots and evaluates `<dir>/<tech>.magicrc` as a script. This is code execution triggered by opening a layout file.
- Evidence: database/DBio.c:600-700
- Options: replicate | fix (ask the user) | platform alternative (apply a declarative profile)
- Recommendation: on desktop, ask once per PDK (trust prompt). On iPadOS, apply the declarative profile only.
- Resolution: decided by X-3 (2026-11-05). Ask once per PDK version on every platform, iPadOS included (capability `hefesto-security`, Requirement "Consent before implicit script execution").

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
- Resolution: decided (capability `hefesto-security`, Requirement "No remote command server by default"). The Tcl sandbox denies `socket`, so `socketcmd.tcl` cannot run in Hefesto.

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
- Duplicate: tracked in Q-CELL-30, which is the canonical entry. Record the resolution there. (Consolidation audit, 2026-11-06.) The caches directory recommended here may be purged by the OS; see docs/decisions/audit/DECISIONS.md.

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

### Q-SCRIPT-38: Editing a fixed device adds `pitchx`/`pitchy`, but PDKs use `deltax`/`xstep`
- Kind: doc-mismatch
- Magic behavior: when the dialog opens for an existing `nocell` instance, the framework sets `nx`, `ny`, `pitchx` and `pitchy` from the instance array. The verified PDKs define and read `nx ny deltax deltay xstep ystep` (pitch = step + delta) and never read `pitchx`/`pitchy`. The dialog therefore shows the instance's `nx`/`ny` but the default `deltax`/`deltay` (0). Applying the dialog re-places the array at pitch `xstep`, losing any custom spacing.
- Evidence: tcltk/toolkit.tcl:1642-1653; open_pdks sky130/magic/sky130-fix.tcl:10-30,254-282; IHP ihp-sg13g2-fix.tcl:446-476
- Options: replicate (keys and loss of spacing); fix in Hefesto by deriving `deltax = pitchx − xstep` when both keys exist (changes the parameters seen by `_check`, so cell placement can differ from Magic)
- Recommendation: replicate. Report upstream to open_pdks/Magic so that one side adopts the other's keys.

### Q-SCRIPT-39: sg13g2 uses `label … +<layer>`, which Magic rejects
- Kind: bug
- Magic behavior: the `label` command treats only a leading `-` as the sticky marker. `label E c +m2` and `label npn13G2 c +comment` (sg13g2 bipolar generator) fail with `Unknown layer: +m2` and create no label. The following `port make` then acts on whatever label is under the box. The PDK comment says the intent was a sticky label on metal2.
- Evidence: commands/CmdLQ.c:324-338; IHP ihp-sg13g2-bjt.tcl:254,615,862
- Options: replicate (error, no label); accept `+` as "attach, not sticky" (differs from Magic, and generated cells would differ)
- Recommendation: replicate, so that generated cells match Magic. File an upstream issue with IHP to change `+` to `-`.

### Q-SCRIPT-40: `magic::spice2float` suffix-mapping defects
- Kind: bug
- Magic behavior (PDK code, open_pdks `common/pdk.tcl` and its IHP copy): the value is lowercased before the first `string map`, so its `uF/nF/pF/aF` keys never match. In the second map, `n` maps to `"* 1.0 e-9"` (with an embedded space), which is a syntax error in `expr`: any value with an `n` suffix (`10n`, and `1nf` after mapping) returns the fault value `0.0` and prints `Value is not numeric!`. `a` maps to `1.0e-15`, not `1e-18`. Values such as `10pf` become `10* 1.0e-12* 1.0e-15`.
- Evidence: open_pdks common/pdk.tcl:87-98; IHP ihp-sg13g2-util.tcl:766-780
- Options: replicate when running PDK scripts (automatic, since the PDK code runs as is); in native generators, replicate bit-for-bit; report upstream
- Recommendation: replicate in native generators, and file upstream issues. Golden case SCRIPT-PDK-12 pins the behavior.

### Q-SCRIPT-41: PDK helpers share the `magic::` namespace and differ between PDKs
- Kind: quirk
- Magic behavior: sky130/gf180mcu (`common/pdk.tcl`) and sg13g2 (`ihp-sg13g2-util.tcl`) define the same procedure names in `magic::` with different results. sg13g2's `i2u` returns `%.3f` text and its `u2i` rounds to an integer. Sourcing a second PDK's script in one session silently replaces the first PDK's helpers, which changes the first PDK's generated geometry and names. The helper file also defines a global procedure `magic:cellnameunique` (single colon), which looks like a typo.
- Evidence: open_pdks common/pdk.tcl:35-44,116; IHP ihp-sg13g2-util.tcl:691-704,795
- Options: replicate (single namespace); isolate each PDK's helpers per library namespace (deviates from Tcl semantics)
- Recommendation: replicate, and document that only one PDK may be loaded per session. Hefesto may warn when a second PDK script redefines `magic::` procedures.

### Q-SCRIPT-42: PDK scripts manipulate wrapper Tk widgets directly [platform]
- Kind: platform
- Magic behavior: `<ns>::addtechmenu` runs `${frame}.titlebar cget -height` to detect the frame layout, runs `add command` on `${frame}.titlebar.mbuttons.drc.toolmenu`, runs `insert 4 command`/`insert 4 separator` on `${frame}.titlebar.mbuttons.file.toolmenu`, sets `Winopts(<frame>,cmdentry)`, and calls `addcommandentry`. `importspice` calls `tk_getOpenFile`. `suspendall` uses `winfo`. The device script also renames `closewrapper`. The wrapper evaluates the callback inside `catch`, so if a widget path is missing, the rest of the PDK menu setup is skipped silently. No Hefesto spec defines these widget paths or the File-menu item order (index 4).
- Evidence: tcltk/wrapper.tcl:1219-1221,1495-1500,1541-1565; tcltk/tools.tcl:14-45; open_pdks sky130/magic/sky130.tcl:36-47,83-243; IHP ihp-sg13g2.tcl:115-126,162-282
- Options: (a) provide a Tk-compatible widget shim with these paths in the desktop GUI; (b) intercept the PDK callback and map the three known widget operations to native menu actions; (c) ignore and report
- Recommendation: (b) on desktop, plus a report of skipped statements, consistent with "Sessions without PDK script execution" (capability `scripting-pdk-integration`). Add a `ui-gui-managers` requirement that names these paths if (a) is chosen.

### Q-SCRIPT-43: PDK extraction sections use `$SUB` in device lines and `$[…]` model names
- Kind: ambiguity
- Magic behavior: all three PDK extract sections declare `substrate … $SUB …`, and this is specified. sg13g2 additionally uses `$SUB` as a device substrate terminal and in `s=$SUB`, and writes resistor model names as `$[rmod_rsil]`, `$[res_rppd]` and `$[rmod_rhigh]`. A grep of `openspec/specs` finds no specification of `$`-substitution in `device` lines or of the `$[…]` model-name form.
- Evidence: IHP ihp-sg13g2-extract.tech:60-62,1252-1254; open_pdks sky130/magic/sky130-extract.tech:149; extract/ExtBasic.c:900-916,2650-2665
- Options: hand over to the EXT area owner to specify, or confirm that Magic treats them literally
- Recommendation: open an EXT question and add golden extraction cases on an sg13g2 resistor. SCRIPT cannot verify it.

### Q-SCRIPT-44: PDK startup seeds the random generator from the clock
- Kind: quirk
- Magic behavior: every verified rc file runs `catch {random seed}`, which seeds from the current time. GDS output draws library prefixes from that generator (see capability `gds-write`), so two runs of the same flow can produce different GDS files. The rc files recommend replacing it with a fixed seed for repeatable output.
- Evidence: open_pdks sky130/magic/sky130.magicrc:17-19; commands/CmdRS.c:71-122; Q-MASK-211
- Options: replicate; let a Hefesto profile pin the seed
- Recommendation: replicate. The golden-test harness MUST run `random seed 1` after startup.

### Q-SCRIPT-45: sg13g2 generators change the window root cell with `load` during a gencell draw
- Kind: ambiguity
- Magic behavior: sg13g2 fixed and bipolar devices build their base cell on demand inside `_draw`, while the framework has the edit stack pushed into the generated cell. They save `cellname list window` or `cellname list self`, suspend the `load` tag, run `load <base> -silent`, paint, and then run `load <saved>`. The state of the framework's edit stack and of `popstack` after this sequence was not derived. `cellname list self` with no selection may return the generated cell, not the window root.
- Evidence: IHP ihp-sg13g2-fix.tcl:238-260; IHP ihp-sg13g2-bjt.tcl:195-262; tcltk/toolkit.tcl:1253-1283
- Options: replicate whatever Magic does (golden case SCRIPT-PDK-14)
- Recommendation: capture SCRIPT-PDK-14 before implementing gencell `load` interaction.

### Q-SCRIPT-46: Is the declarative PDK profile still needed under X-3?
- Kind: platform
- Magic behavior: none. Magic always evaluates PDK `.magicrc` files as Tcl.
- Evidence: tcltk/magic.tcl.in:339-470
- Options: (a) keep the declarative profile for PDKs that are not in the iPadOS curated packs when App Review rejects imported scripts; (b) drop it, and make curated-pack mode refuse PDKs without bundled scripts; (c) also use it when the user declines script consent, instead of the `minimum`-tech read-only fallback of `hefesto-security`
- Recommendation: (b), unless App Review rejects imported scripts and users need uncurated PDKs on iPadOS. The requirement "Sessions without PDK script execution" in `scripting-pdk-integration` was written under the pre-X-3 assumption that iPadOS has no Tcl. It is now limited to curated-pack mode until this is decided.
