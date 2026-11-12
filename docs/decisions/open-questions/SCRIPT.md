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
- Resolution: decided by D-4 (2026-11-06): replicate by default. Magic's ok-status convention is the default, independent of `magic-compat`. Strict mode is opt-in through the project setting `script-strict` (default off) and the per-interpreter command `hefesto::strict on|off`, which overrides the setting for its interpreter (capability `hefesto-diagnostics`).

### Q-SCRIPT-3: Tag `%r` substitution is unescaped, and substitution rescans
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: `%r`/`%R` insert `"<result>"` without escaping, so a result containing `"`, `$`, `[` or `]` changes the meaning of the tag script. `%0`–`%5` and `%W` insert the argument or window path raw, so an argument with spaces, `$` or brackets is split or evaluated. After each substitution, scanning restarts at the beginning of the string, so `%` sequences inside substituted text (arguments, results, window paths) are substituted again. `%%` is reduced to `%` and then rescanned, so it never yields a literal percent: `%%1` becomes the first argument. A `%` followed by any other character, or at the end of the script, falls into the `default` case, which does not advance the scan pointer, so `TagCallback` loops forever and Magic hangs (for example a tag script containing `format %d` or ending in `50%%`).
- Evidence: tcltk/tclmagic.c:TagCallback (161-262; `default: break;` leaves `sptr` on the `%`) (corrected 2026-11-11: added the hang on unknown escapes and on `%%`, and the raw insertion of arguments)
- Options: replicate | compat (replicate the quoting under `magic-compat`) | fix (single pass; each substituted value inserted as one properly quoted list element; `%%` → `%`; an unknown `%x` copied literally)
- Recommendation: fix in both modes. The hang is crash-class (X-2 tier 1). For every value without `"`, `$`, `[`, `]`, `\` or `%` the single-pass, list-quoted form gives the same word Magic gives, so a tag script that works in Magic keeps its meaning (a result with spaces is still one word). The remaining differences are cases where Magic evaluates text taken from a command result or argument, which the X-3 sandbox should not allow either. Golden cases: a result with spaces, a result containing `[`, and a tag script containing `%%` and `%d` (Magic hangs: capture with a timeout and record a deviation).
- Resolution: fix in both modes — tag `%`-substitution is one pass over the script: each `%r`/`%R`, `%0`–`%5` and `%W` value is inserted as one list-quoted word, `%%` gives `%`, and an unknown `%x` is copied literally, so Magic's hang on `%%` and unknown escapes (X-2 tier 1) cannot occur and a tag script that works in Magic keeps its meaning. Golden cases: a result with spaces, a result containing `[`, and a tag script containing `%%` and `%d` (Magic hangs: captured with a timeout and recorded as a deviation). Approved 2026-11-11.

### Q-SCRIPT-4: Output escaping depends on existing variable names
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: before output is passed to `puts`, a `$` is escaped only if the text after it, up to the next space, is NOT an existing Tcl variable in the printing interpreter. Text such as `$VDD` therefore prints the variable's value, not the literal text, while `$VDD,` (punctuation before the space) prints literally. `Tcl_escape` applies the same rule. Command substitution cannot happen, because `[` and `]` are always escaped.
- Evidence: tcltk/tclmagic.c:1437-1480 (Tcl_printf), 1514-1548 (Tcl_escape)
- Options: replicate | compat (replicate under `magic-compat`) | fix (always print literally)
- Recommendation: fix in both modes. Only console text changes, and printing it literally is the evident intent; this follows the approved text-only fixes Q-SCRIPT-15/16/17 (X-2 tier 3). Golden cases compare stdout, so SCRIPT-API-13 and any case whose messages contain a defined `$name` get an entry in `tests/golden/deviations.toml`. If stdout parity with Magic matters more, the alternative is compat (replicate with `magic-compat` ON).
- Resolution: fix in both modes — console output is printed literally, and a `$name` is never replaced by the value of an existing variable (X-2 tier 3, as Q-SCRIPT-15/16/17). SCRIPT-API-13 and any golden case whose messages contain a defined `$name` get an entry in `tests/golden/deviations.toml`. Approved 2026-11-11.

### Q-SCRIPT-5: Path variable expansion is cached per session
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: `$NAME` in search paths and file names is resolved from the Tcl variable once. The value is cached in a hash table, and later changes to the variable are ignored. Names that do not resolve are not cached, so they are retried on the next use. Because `.mag` `use` paths are written as `$PDK_ROOT/…` or `$PDKPATH/…`, the cached value decides which file a later `use` resolves to.
- Evidence: utils/path.c:268-330; database/DBio.c:6042-6088 (`$PDK_*` substitution on write) (corrected 2026-11-11: unresolved names are not cached)
- Options: replicate | compat (replicate under `magic-compat`; resolve each time with it OFF) | fix (resolve each time)
- Recommendation: compat (corrected 2026-11-11, was fix). The cached value decides which cell file is read after a variable changes mid-session, so it changes layout results (X-2 tier 2). With `magic-compat` ON the first resolved value is kept. With it OFF each expansion reads the current value from the path-variable table of Q-CELL-17. In both modes Hefesto warns once when the current value differs from the cached one. The verified PDK files set `PDK_ROOT` and `PDKPATH` once, before any cell is read, so PDK flows behave the same in both modes.
- Resolution: compat — with `magic-compat` ON the first resolved value of a path variable is kept for the session (names that do not resolve are retried); with it OFF each expansion reads the current value from the path-variable table of Q-CELL-17 (X-2 tier 2). In both modes Hefesto warns once when the current value differs from the cached one. The verified PDK rc files set `PDK_ROOT` and `PDKPATH` once, before any cell is read, so PDK flows behave the same in both modes. Approved 2026-11-11.

### Q-SCRIPT-6: `port last` counts non-port labels
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: `port last` is the maximum `lab_port` over all labels of the edit cell, including non-port labels. A label that never was a port has index 0, so `port last` returns 0 instead of -1 when a cell has labels but no ports. `port remove` clears only the port flags and keeps `lab_port`, so a removed port's old index still counts until the cell is reloaded (the index of a non-port label is not saved). `port first` considers ports only.
- Evidence: commands/CmdLQ.c:1781-1793 (PORT_LAST), 2061-2068 (PORT_REMOVE) (corrected 2026-11-11: added the removed-port case)
- Options: replicate | compat (replicate under `magic-compat`) | fix (ports only)
- Recommendation: compat (corrected 2026-11-11, was replicate). The value seeds new port numbers (`port last + 1` in the text helper and in user scripts), so it changes the indices written to `.mag` (X-2 tier 2). With `magic-compat` OFF, `port last` is the highest index among current ports, or -1. The earlier reason was inaccurate: `readspice` uses `port last` only as a loop bound and as the floor for parking unmatched ports, so its result differs only in the removed-port case (tcltk/readspice.tcl:116-131,177-185).
- Resolution: compat — with `magic-compat` ON `port last` is the maximum port index over all labels of the edit cell, including non-port labels (index 0) and removed ports until the cell is reloaded; with it OFF it is the highest index among current ports, or -1 (X-2 tier 2: it seeds new port indices written to `.mag`). Approved 2026-11-11.

### Q-SCRIPT-7: `cif scale` returns a widened single-precision value
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: the scale is computed in `float` and returned as a Tcl double, for example 0.01 becomes 0.009999999776482582. Scripts that use `expr` on it see that value.
- Evidence: cif/CIFmain.c:139-146; commands/CmdCD.c:1936-1952
- Options: replicate | compat (replicate under `magic-compat`) | fix (exact decimal double)
- Recommendation: compat (corrected 2026-11-11, was replicate). The value is visible to scripts and feeds `expr` in `measure`, `drc_save_report` and user scripts, so it is a value-changing quirk (X-2 tier 2). With `magic-compat` OFF Hefesto returns the exact decimal quotient. The PDK helpers round it to four decimals before use (`round([magic::cif scale output] * 10000) / 10000.0` in `magic::i2u`/`u2i`; open_pdks common/pdk.tcl:35-44, IHP ihp-sg13g2-util.tcl:691-702), so generated PDK geometry and cell names are the same in both modes.
- Resolution: compat — with `magic-compat` ON `cif scale` returns the single-precision value widened to double (0.01 gives 0.009999999776482582); with it OFF it returns the exact decimal quotient (X-2 tier 2). The PDK helpers round it to four decimals, so PDK geometry and generated cell names are the same in both modes. Approved 2026-11-11.

### Q-SCRIPT-8: Order of `drc listall why`
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: rules are emitted in the iteration order of `DRCErrorTable`, a string-keyed Magic hash table (`HT_STRINGKEYS`) keyed by the why text after `drcSubstitute`, and filled in the order errors are found. The order is therefore deterministic: it depends only on the why strings and the insertion history (X-1 as amended by D-1). `drc_save_report` and the DRC manager inherit it. The coordinates of each rule are in insertion order. (corrected 2026-11-11: the order was described as undefined)
- Evidence: drc/DRCmain.c:444 (HashFind), 646-700 (DRCWhyAll)
- Options: replicate (Magic string hash order, using the table of `hefesto-output-determinism`) | sort (rule text, ascending, with a canonicalizing comparator)
- Recommendation: replicate (corrected 2026-11-11, was sort by rule text). X-1/D-1 reproduce name-keyed hash order exactly, and the table algorithm is already specified once in `hefesto-output-determinism` requirement "Magic string hash table". The insertion order follows the DRC search order, which follows tile order (also reproduced). `drc listall why` and `drc.out` are then byte-identical without a comparator exception. Fall back to sorting, listed under X-1's "never affects results" bullet, only if the cross-cell search order cannot be reproduced. Spec impact: `scripting-api` says "unspecified order" and would cite X-1 instead.
- Resolution: replicate — `drc listall why`, `drc_save_report` and the DRC manager list why texts in Magic's string-hash order (`hefesto-output-determinism` "Magic string hash table", keyed by the substituted why text, filled in discovery order; X-1 as amended by D-1), with each text's rectangles in insertion order, so `drc listall why` and `drc.out` are byte-identical without a comparator exception. Sorting under X-1's "never affects results" bullet is a fallback only if the cross-cell search order cannot be reproduced, and needs its own decision. Approved 2026-11-11.

### Q-SCRIPT-9: PDK script inventory not verified against PDK sources — partially resolved
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: the Magic tree does not contain the open_pdks `.magicrc`/`<pdk>.tcl` files. The PDK startup contract and device-script contract in `scripting-pdk-integration` were first reconstructed from `toolkit.tcl`, `wrapper.tcl` and `DBio.c`.
- Evidence: tcltk/toolkit.tcl:1-105,379-402; tcltk/wrapper.tcl:485-505. Verified sources: open_pdks @ c6df28c1 (`sky130/magic/*`, `gf180mcu/magic/*`, `common/pdk.tcl`, `sky130/Makefile.in`, `gf180mcu/Makefile.in`) and IHP-Open-PDK @ 5e6d592e (`ihp-sg13g2/libs.tech/magic/*.tcl`, `*.magicrc`). open_pdks has no sg13g2 support; IHP ships its own Magic files.
- Partial findings (corrections made in the specs):
  - The rc files do not wrap `tech load` in `suspendall`/`resumeall`, do not set `drc style`/`drc on`, do not use `MAGTYPE`, and do not call `query_mylib_ip`/`query_my_projects`. Those were assumptions. Library paths are `libs.ref/<lib>/mag`.
  - The rc files do use `tech lambda` + `scalegrid`, `random seed`, `ext2spice scale off`, `quit -noprompt` on a tech-name mismatch, and (IHP) `info script` to find their own directory.
  - `PDKPATH` is set by the device script, not the rc file.
  - `Opts(callback)` is prepended with `set … [subst …]`, not `lappend`ed.
  - Fixed devices use `deltax/deltay/xstep/ystep`, not `pitchx/pitchy`.
  - The PDKs define helpers in `magic::` (`spice2float`, `u2i`, …) that Hefesto must not predefine.
  - Generators use far fewer framework-level commands than assumed: no `pushstack`, `identify`, `what -list`, `goto`, `drc check` or `instance list`.
- Pin mismatch (added 2026-11-11): the PDK data the repository's tests use is older than the spec pins. `scripts/fetch_pdk.py` pins open_pdks `0fe599b2` (2024-08-17) for sky130A and gf180mcuD and IHP-Open-PDK `ddb601a4` (2025-01-08) (tests/unit/common/reference_data.hpp:25-27). The spec and tests/golden/SCRIPT/CASES.md pin open_pdks `c6df28c1` (2026-11-05) and IHP `5e6d592e` (2026-09-01). The startup files differ between the two generations. At the older pins:
  - sky130A and gf180mcuD read `MAGTYPE` (default `mag`) and choose between `libs.ref/${MAGTYPE}/<lib>` and `libs.ref/<lib>/${MAGTYPE}`.
  - gf180mcuD lists 5 libraries, not 9.
  - Both end with `catch {magic::query_mylib_ip}` and `catch {magic::query_my_projects}`, which `glob` under `/home/$USER/design`. The sandbox denies this, and `catch` absorbs the error.
  - The IHP rc file has the generator `source` commented out ("alpha pre-release"), sets `SUB` to `VSUB` (not `sub!`), and runs four `addpath …/libs.ref/sg13g2_*/${MAGTYPE}`.

  The device-script points checked here (`Opts(callback)` via `subst`, the fixed-device keys, `fixed_dialog` reading `pitchx`; see Q-SCRIPT-38) are the same at both pins. (Checked against .cache/pdks/ciel/*/libs.tech/magic/ and the pinned files fetched read-only.) The bullet "Fixed devices use `deltax/deltay/xstep/ystep`, not `pitchx/pitchy`" holds for `_draw`, but `fixed_dialog` does read `pitchx/pitchy`.
- Remaining: (a) run SCRIPT-PDK-09 (command trace) to confirm the static inventory dynamically, because static grep can miss commands built in strings; (b) the expected values in the scenarios marked "(verify against Magic)"; (c) other open_pdks-style PDKs that were not checked (for example gf180mcu variants that differ only in `#ifdef`s, and sky130B); (d) one PDK pin for both unit tests and golden capture.
- Options: run the golden cases SCRIPT-PDK-01..17 on Magic 8.3.694 with the pinned PDK commits
- Recommendation: other (X-2 n/a: this entry records facts, not a behavior choice). Accept the static inventory at the spec pins, and move (a) and (b) to the golden capture (X-6). Before capture, choose one PDK pin for unit tests and golden cases (decision table, judgement item 3). Hefesto runs both rc generations unchanged under the sandboxed Tcl, so supporting older installs needs only a smoke case. Re-verify whenever a PDK repository changes its Magic templates.
- Resolution: other (X-2 n/a) — the static inventory at the spec pins is accepted, and the dynamic trace (SCRIPT-PDK-09) and the scenarios marked "(verify against Magic)" move to the golden capture (X-6). User decision (a): for M4, `scripts/fetch_pdk.py` moves to the spec pins (open_pdks `c6df28c1`, IHP-Open-PDK `5e6d592e`), so unit tests and golden capture use one pin; the M1–M3 PDK-based tests are re-run on them and any changed result is fixed or recorded through an approved decision; one smoke case covers the older rc form (`MAGTYPE`, `query_mylib_ip`/`query_my_projects`, the IHP rc without its generator `source`). Re-verify whenever a PDK repository changes its Magic templates. Approved 2026-11-11.

### Q-SCRIPT-10: Implicit sourcing of `<tech>.magicrc` during cell reads
- Kind: platform
- Magic behavior: reading a `.mag` whose tech differs, in a paint-free session, searches PDK roots and evaluates `<dir>/<tech>.magicrc` as a script. This is code execution triggered by opening a layout file.
- Evidence: database/DBio.c:600-700
- Options: replicate | fix (ask the user) | platform alternative (apply a declarative profile)
- Recommendation: on desktop, ask once per PDK (trust prompt). On iPadOS, apply the declarative profile only.
- Resolution: decided by X-3 (2026-11-05). Ask once per PDK version on every platform, iPadOS included (capability `hefesto-security`, Requirement "Consent before implicit script execution").

### Q-SCRIPT-11: Toolbar sources `./<tech>_toolbars.tcl` from the working directory
- Kind: platform
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: `maketoolbar` sources any `./<techname>_toolbars.tcl` in the current directory, which executes arbitrary code. The file is sourced inside the procedure, so only `current_toolbar` (declared global) survives; other variables become locals. The reorder dialog rewrites only the `set current_toolbar {…}` lines of that file.
- Evidence: tcltk/toolbar.tcl:55-73; tcltk/reorderLayers.tcl:178-200
- Options: replicate (source it, inside the X-3 sandbox) | fix (parse only `set <name> {...}` lines as data)
- Recommendation: fix in both modes (security; consistent with X-3 and `hefesto-security`). Read the file as data: accept lines of the form `set <name> {<layer> …}` (and `set <name> "<layers>"`), ignore and report everything else, and never `source` it. Files written by the reorder dialog round-trip unchanged. The trade-off is a hand-written file with other Tcl, whose side effects are lost (decision table, judgement item 5).
- Resolution: fix in both modes (user decision, option a) — `./<tech>_toolbars.tcl` is read as data and never `source`d: only lines of the form `set <name> {<layer> …}` (and `set <name> "<layers>"`) are read; every other statement is reported with a diagnostic and ignored. Files written by the reorder dialog round-trip unchanged; side effects of other Tcl in hand-written files are lost. Approved 2026-11-11.

### Q-SCRIPT-12: Unauthenticated socket evaluation
- Kind: platform
- Magic behavior: `socketcmd.tcl` evaluates any line received on TCP 12946 on all interfaces, with no authentication. It is opt-in.
- Evidence: tcltk/socketcmd.tcl:10-27
- Options: omit | provide a loopback-only, token-authenticated equivalent
- Recommendation: omit by default. If provided, bind to loopback only and require a token.
- Resolution: decided (capability `hefesto-security`, Requirement "No remote command server by default"). The Tcl sandbox denies `socket`, so `socketcmd.tcl` cannot run in Hefesto.

### Q-SCRIPT-13: Wrapper callbacks: scope and per-window key bug
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: `Opts(callback)` is `eval`ed inside `openwrapper`, so the callback sees its local variables (`framename`, `layoutframe`, `winname`, `cell`). The per-window callback is read from `Winopts(${framename}, callback)`, whose key contains a space, so a normally named `Winopts(.layout1,callback)` is never run. (Checked with tclsh 8.6: `$W(${framename}, callback)` reads the element `<frame>, callback`.)
- Evidence: tcltk/wrapper.tcl:1495-1499
- Options: replicate | fix (also accept the no-space key)
- Recommendation: fix (GUI hook, X-2 tier 3) while keeping Magic's scope. Evaluate the callbacks with `framename`, `layoutframe`, `winname` and `cell` defined as locals, because PDK callbacks read `$framename`. Run `Winopts(<frame>, callback)` as Magic does, then `Winopts(<frame>,callback)` if that element exists. No output depends on this, and Magic never runs the no-space key, so a script that sets only that key gets what it asked for.
- Resolution: fix (X-2 tier 3) keeping Magic's scope — the wrapper callbacks are evaluated with `framename`, `layoutframe`, `winname` and `cell` defined as locals; `Winopts(<frame>, callback)` runs as in Magic, then `Winopts(<frame>,callback)` if that element exists. Approved 2026-11-11.

### Q-SCRIPT-14: Option tables whose keyword ends with a tab
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: `Lookup` ends a table keyword only at a space or at the end of the string. A keyword followed by a tab is never an exact match, only a prefix match, so it becomes ambiguous when another entry of the same table starts with the same text. A scan of the Magic tree (2026-11-11) found exactly two such collisions. Both are in the layout-window command table:
  - `get` (`"get\tOR"`, alias of `getcell`): typing it matches `get`, `getcell` and `getnode` as prefixes.
  - `id` (`"id\tOR"`, alias of `identify`): typing it matches `id` and `identify`.

  Both commands are therefore reported as ambiguous. In the Tcl build, `magic::get` and `magic::id` are registered from the first word, so they always fail with TCL_ERROR. The 190 other tab-terminated keywords, for example `cellname self`, have no prefix sibling and behave as exact matches. (corrected 2026-11-11: the example `next [number]` ends at a space and is not affected, and the affected entries were not identified)
- Evidence: utils/lookup.c:96-118; dbwind/DBWcommands.c:348-350,365-367; windows/windSend.c:186-195; tcltk/tclmagic.c:763-767; commands/CmdCD.c:1139-1166
- Options: replicate | compat (replicate under `magic-compat`) | fix (treat any whitespace as the terminator)
- Recommendation: compat. With `magic-compat` ON, `get` and `id` stay ambiguous as in Magic. With it OFF, any whitespace ends a keyword, so `get` runs `getcell` and `id` runs `identify`. It is a parse-argument quirk that changes results: a command that fails versus one that edits the layout (X-2 tier 2). Per the scan, the OFF behavior changes nothing else.
- Resolution: compat — with `magic-compat` ON a table keyword ends only at a space, so `get` and `id` stay ambiguous as in Magic; with it OFF any whitespace ends a keyword, so `get` runs `getcell` and `id` runs `identify` (X-2 tier 2). No other keyword changes. Approved 2026-11-11.

### Q-SCRIPT-15: Unbalanced `resumeall` error text
- Kind: quirk
- Magic behavior: after the counter reaches 0 it is unset, so a further `resumeall` fails with Tcl's `can't read "Winopts(...,suspend)": no such element in array` instead of the intended `resume called without suspend`.
- Evidence: tcltk/tools.tcl:30-50
- Options: replicate | fix (raise `resume called without suspend`)
- Recommendation: fix. The message is informative.
- Resolution: fix — `resume called without suspend` is raised for any unmatched `resumeall`, which also covers a `resumeall` with no `suspendall` at all (the counter key never existed), a case the entry did not mention; the `<= 0` branch in Magic is unreachable because the key is unset at 0, and only error text changes (verified against tcltk/tools.tcl:magic::resumeall on 2026-11-09; approved 2026-11-09).

### Q-SCRIPT-16: `ruler` reads the box in the current units but draws in internal units
- Kind: bug
- Magic behavior: `ruler` captures `box values` before switching units to internal, then uses those numbers as internal coordinates. With units microns/lambda it draws at the wrong scale.
- Evidence: tcltk/tools.tcl:340-373
- Options: replicate | fix (capture after `units internal`)
- Recommendation: fix. Golden cases use `units internal` so that they pass either way.
- Resolution: fix — `box values` prints in the current units (`dbwPrintValue0`) but `ruler` draws after `units internal`, so with lambda, micron or user units the ruler is drawn at the wrong scale; rulers are non-persistent display elements and golden cases use internal units, so no output or script depends on the bug (verified against tcltk/tools.tcl:magic::ruler and dbwind/DBWprocs.c:dbwPrintValue0 on 2026-11-09; approved 2026-11-09).

### Q-SCRIPT-17: `measure` multiplies current-unit values by the CIF scale
- Kind: bug
- Magic behavior: the width/height comes from `box values` in the current units and is multiplied by µm per internal unit. The result is correct only for default/internal units.
- Evidence: tcltk/tools.tcl:431-458
- Options: replicate | fix
- Recommendation: fix (measure in internal units).
- Resolution: fix — `cif scale out` is microns per internal unit but the width comes from `box values` in the current units, so the label is wrong for every non-internal unit setting; measuring in internal units gives the same text as Magic in Magic's only correct case (default/internal units) (verified against tcltk/tools.tcl:magic::measure on 2026-11-09; approved 2026-11-09).

### Q-SCRIPT-18: Crash-backup interval documentation mismatch
- Kind: doc-mismatch
- Magic behavior: the docs and `crashbackups start` default to 600000 ms (10 min), but the tool setup sets 60000 ms (1 min) before starting.
- Evidence: tcltk/tools.tcl:92-103,584-585; doc/html/crashbackups.html
- Options: replicate 60 s | use 10 min
- Recommendation: replicate the effective 60 s default, and document it.
- Resolution: n/a — spec follows Magic's code (effective 60 s interval set by the tool setup); documentation is wrong (X-2 tier 3, 2026-11-07).

### Q-SCRIPT-19: `readspice` uses the Tcl `case` command
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: `.PININFO` handling uses `case`, which was removed in Tcl 9, so it fails under a Tcl 9 build.
- Evidence: tcltk/readspice.tcl:237-241
- Options: replicate (Tcl-8 only) | fix (`switch`)
- Recommendation: fix in both modes. `switch` with the same three literal patterns (`B`, `I`, `O`, case-sensitive) gives exactly the result of Tcl 8 `case`, so behavior equals Magic's Tcl 8 build and does not depend on the embedded Tcl version (X-2 tier 1 for the Tcl 9 failure; no observable difference otherwise).
- Resolution: fix in both modes — `readspice` uses `switch` with the same three case-sensitive literal patterns (`B`, `I`, `O`), which gives exactly Tcl 8 `case`'s result and runs on any embedded Tcl version. Approved 2026-11-11.

### Q-SCRIPT-20: gencell requires `_convert` although documented as optional
- Kind: doc-mismatch
- Magic behavior: a device is accepted only if `<lib>::<type>_convert` exists, while comments say `_convert` is optional in SPICE mode.
- Evidence: tcltk/toolkit.tcl:720-731
- Options: replicate | fix (test `_defaults`)
- Recommendation: replicate (PDKs always define `_convert`), and warn.
- Resolution: n/a — spec follows Magic's code (`_convert` is required); documentation is wrong (X-2 tier 3, 2026-11-07).

### Q-SCRIPT-21: Normalization of Inf/NaN parameter values
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: `string is double -strict` accepts `inf`, `-inf` and `nan`. `round()` then raises a Tcl error (`integer value too large to represent` for Inf; `floating point value is Not a Number` for NaN; checked with tclsh 8.6). This happens in `magic::get_gencell_hash` before any cell exists, so cell creation is aborted.
- Evidence: tcltk/toolkit.tcl:1848-1869 (normalize_value), 1167-1180 (get_gencell_hash)
- Options: replicate | fix (treat as non-numeric text)
- Recommendation: fix in both modes (verbatim text). Magic produces no cell, so there is no Magic output to stay compatible with (X-2 tier 3). The PDK `_check` still sees the original value.
- Resolution: fix in both modes — `inf`, `-inf` and `nan` parameter values are hashed and normalized as verbatim text, so cell creation is not aborted (X-2 tier 3: Magic makes no cell). The PDK `_check` still sees the original value. Approved 2026-11-11.

### Q-SCRIPT-22: Murmur hash over non-ASCII characters
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: the Tcl implementation slices by character and uses `binary scan` on characters, so each Unicode character contributes only its low 8 bits, and the length is the character count, not the UTF-8 byte count. (Checked with tclsh 8.6: `binary scan "ŁA" c*` gives `65 65`.) Under Tcl 9, `binary scan` rejects characters above U+00FF (TIP 568, not run here), so Magic's Tcl 9 build fails for such values instead.
- Evidence: tcltk/toolkit.tcl:1778-1832
- Options: replicate | fix (UTF-8 bytes)
- Recommendation: replicate in both modes. The hash decides generated cell names, and names must match cells that Magic already generated and saved. Changing it would break existing data, so it is not a candidate for compat (X-2 tier 3). Hefesto's native hash must implement the Tcl 8.6 semantics directly, not through the embedded interpreter's `binary`.
- Resolution: replicate in both modes — the generated-cell hash uses Tcl 8.6 semantics: each character contributes its low 8 bits and the length is the character count, so names match cells Magic already generated (X-2 tier 3). Hefesto's native hash implements these semantics directly, not through the embedded interpreter's `binary`. Approved 2026-11-11.

### Q-SCRIPT-23: Change of an existing `nocell` device reads undefined variables
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: in `magic::gencell`, the existing-instance path for `nocell` devices sets `pitchx $delx`/`pitchy $dely`, which are undefined, so it raises a Tcl error. It also uses `[lindex $arcount 1]` (xhi) as nx instead of xhi−xlo+1. The branch runs before the argument check, so both `magic::gencell lib::type inst` (which would open the dialog) and `magic::gencell lib::type inst k v` (non-interactive) fail for fixed devices. (corrected 2026-11-11: not only the non-interactive call)
- Evidence: tcltk/toolkit.tcl:736-752
- Options: replicate | fix (use the array pitch and counts as in `gencell_dialog`)
- Recommendation: fix in both modes. Magic raises an error and changes nothing, so there is no Magic output to match (X-2 tier 3). Use the dialog's computation (tcltk/toolkit.tcl:1642-1653): nx = xhi−xlo+1, ny = yhi−ylo+1, and `pitchx`/`pitchy` from `array -list pitch`. In the non-interactive path, `deltax`/`deltay` come from the arguments or the PDK defaults, because only the PDK `fixed_dialog` derives them from `pitchx` (Q-SCRIPT-38). After this fix the call reaches `gencell_change`, which needs Q-SCRIPT-29.
- Resolution: fix in both modes (X-2 tier 3: Magic raises an error and changes nothing) — changing an existing `nocell` device computes nx = xhi−xlo+1, ny = yhi−ylo+1 and `pitchx`/`pitchy` from `array -list pitch`, as the dialog does; in the non-interactive path `deltax`/`deltay` come from the arguments or the PDK defaults. The call then reaches `gencell_change` (Q-SCRIPT-29). Approved 2026-11-11.

### Q-SCRIPT-24: Create redraws into an already-existing generated cell
- Kind: bug
- Magic behavior: `gencell_create` and `gencell_makecell` call `cellname create` (a no-op if the cell exists) and draw again, so a second create with the same parameters re-draws into the existing cell. Paint is idempotent, but labels/ports and properties may duplicate. A same-named cell that exists on disk but is not loaded is shadowed by a new empty cell.
- Evidence: tcltk/toolkit.tcl:1260-1273,795-808; commands/CmdCD.c:1652-1659
- Options: replicate | fix (reuse the existing cell without drawing, as `gencell_change` does; load from disk first)
- Recommendation: fix, with a golden case comparing label counts.
- Resolution: fix — a create with existing parameters reuses the generated cell without drawing into it again, and never shadows a same-named cell on disk with a new empty one, so labels, ports and properties are not duplicated and on-disk content is not lost (X-2 tier 1, 2026-11-07).

### Q-SCRIPT-25: `add_dependency` without parameter names
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: it sets `parameters` to the procedure NAME string instead of calling it, so `dict keys` raises an error.
- Evidence: tcltk/toolkit.tcl:1351-1356
- Options: replicate | fix (call `_defaults`)
- Recommendation: fix in both modes. The no-name form always raises an error in Magic. No verified PDK script and no Magic script uses it: all 12 PDK calls and Magic's own call (tcltk/toolkit.tcl:1320) pass parameter names. Nothing can depend on the error (X-2 tier 3).
- Resolution: fix in both modes — `add_dependency` without parameter names calls `<type>_defaults` and uses its keys (X-2 tier 3: the form always fails in Magic and no PDK or Magic script uses it). Approved 2026-11-11.

### Q-SCRIPT-26: `drc_save_report` count line and units
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: the header writes `<cell> <drc list count>`, where the count is the raw `{cell n}` list for every cell with errors (for example `inv {inv 1}`). The coordinates come from `drc listall why`, which prints them with `DBWPrintValue` in the current units, and are then multiplied by `cif scale out`. The result is therefore correct only with default or internal units. With lambda, micron or user units the values are wrong. With the units print flag the values carry a suffix (`i`, `l`, `um`), `expr` fails, and the report is left partly written. (corrected 2026-11-11: the units come from `drc listall why`, and the suffix case)
- Evidence: tcltk/drcmgr.tcl:205-260; drc/DRCmain.c:446-455; dbwind/DBWprocs.c:dbwPrintValue0 (760-860)
- Options: replicate | fix (total count; internal units)
- Recommendation: fix the units, keep the file format. Replicate the header line (with the `{cell n}` list, whose cell order follows X-1), the separators, the `%.3f` coordinates and the blank line. Collect coordinates in internal units and restore the user's units afterwards. In Magic's only correct case (default or internal units) the file is byte-identical, as in the approved Q-SCRIPT-16/17 (X-2 tier 3).
- Resolution: fix the units, keep the file format — `drc_save_report` writes Magic's header line (with the `{cell n}` list, cell order per X-1), separators, `%.3f` coordinates and blank line, collecting coordinates in internal units and restoring the user's units afterwards; in Magic's only correct case (default or internal units) the file is byte-identical (X-2 tier 3, as Q-SCRIPT-16/17). Approved 2026-11-11.

### Q-SCRIPT-27: Library manager path handling
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: the tree ids carry a trailing `/` but are compared against paths without one, so every refresh deletes and rebuilds all nodes. Files are left open when a header check fails. Cell-path entries are passed through `subst`, which evaluates `$` and `[…]` in a path.
- Evidence: tcltk/libmgr.tcl:186-247,160-180
- Options: replicate | fix
- Recommendation: fix (GUI only, X-2 tier 3). Hefesto's library manager is native (Q-SCRIPT-34). It compares normalized paths, closes every file it opens, and expands path variables from the Q-CELL-17 table instead of `subst`. The refresh triggers (`addpath`, `path`, `save`, `writeall`) and the listed files are kept.
- Resolution: fix (GUI only, X-2 tier 3) — the native library manager compares normalized paths, closes every file it opens and expands path variables from the Q-CELL-17 table instead of `subst`; the refresh triggers (`addpath`, `path`, `save`, `writeall`) and the listed files are kept. Approved 2026-11-11.

### Q-SCRIPT-28: Layer reorder dialog inconsistencies
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: the Load action sources `$CAD_ROOT/magic/tcl/toolbar.tcl` instead of the presets file. That file defines no preset variables, and `set $name` inside the `apply` lambda reads a local, so Load always fails after the preset check passes. The error text names `<tech>_toolbar.tcl` but the file is `<tech>_toolbars.tcl`. The file is read with `exec cat`, which the X-3 sandbox denies.
- Evidence: tcltk/reorderLayers.tcl:146-200
- Options: replicate | fix
- Recommendation: fix (GUI only, X-2 tier 3). Load reads the presets from `<tech>_toolbars.tcl`, parsed as data (Q-SCRIPT-11). The error text names the real file, and the file is read with the host file API. The file format and the `set current_toolbar {…}` rewrite stay as in Magic. No working Magic behavior is lost.
- Resolution: fix (GUI only, X-2 tier 3) — the reorder dialog's Load reads the presets from `<tech>_toolbars.tcl` parsed as data (Q-SCRIPT-11) through the host file API, and its error text names the real file; the file format and the `set current_toolbar {…}` rewrite stay as in Magic. Approved 2026-11-11.

### Q-SCRIPT-29: gencell change requires an open dialog
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: `gencell_change` always reads `.params.title.ient`. When no dialog exists (batch mode, or a script call before any dialog was opened), the non-interactive `magic::gencell <lib>::<type> <existing-inst> k v` raises a Tcl error after `suspendall`, leaving redraw suspended.
- Evidence: tcltk/toolkit.tcl:882-900
- Options: replicate | fix (skip the dialog name when it is absent)
- Recommendation: fix in both modes. Magic raises an error and changes nothing, so there is no Magic output to match (X-2 tier 3). Without a dialog, keep the instance's current name, as if the entry were `(default)`. This is required for headless PDK flows.
- Resolution: fix in both modes (X-2 tier 3) — without an open dialog, `gencell_change` keeps the instance's current name, as if the name entry were `(default)`, so headless `magic::gencell <lib>::<type> <inst> k v` works. Approved 2026-11-11.

### Q-SCRIPT-30: mazeroute file fallback and topology
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: the fallback opens `<netfile>.net` (computing the root name but not using it). Multi-terminal nets are routed as a chain in file order, not as a tree.
- Evidence: tcltk/mazeroute.tcl:11-33
- Options: replicate | fix
- Recommendation: replicate in both modes, if the tool is provided. The chain order is the script's algorithm, not a defect, and it decides the routed geometry. The fallback works for its intended use (`mazeroute foo` opens `foo.net`), and the unused root name is dead code (X-2 tier 3). The script drives `iroute` (capability `router-maze-iroute`, M10), so this can be decided with M10.
- Resolution: replicate in both modes, if the tool is provided — `mazeroute` opens `<netfile>.net` as its fallback and routes multi-terminal nets as a chain in file order. The script drives `iroute` (`router-maze-iroute`, M10), so it is implemented and tested with M10, not M4. Approved 2026-11-11.

### Q-SCRIPT-31: Text helper with default font passes "default" as position
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: with font `default`, the helper issues `label <text> <just>`, and `<just>` may be the word `default`. `label` does not reject it. `GeoNameToPos` finds no position named `default` and prints nothing (non-verbose call). `default` is not an integer, and `DBNameToFont("default")` returns -1, the default font. The label is therefore created with the default font and the default justification, which is the helper's intent. The justification button always holds a non-empty value, so the failing case (an empty word) does not occur. (corrected 2026-11-11: the earlier text said the word is rejected)
- Evidence: tcltk/texthelper.tcl:281-330 (make_new_label), 65,108-130; commands/CmdLQ.c:224-252; database/DBlabel.c:1549-1559 (DBNameToFont); utils/geometry.c:353-417
- Options: replicate | fix (omit the position when default)
- Recommendation: replicate. There is no defect, and both forms produce the same label (X-2 n/a). Hefesto's `label` must take the font name `default` as the default font (-1), as `DBNameToFont` does. `cmd-labels-properties` says this for `setlabel font`, but its `label` text only says "unknown names fail", so that spec should name `default` explicitly.
- Resolution: replicate (n/a: no defect) — `label` takes the font word `default` as the default font (-1), as `DBNameToFont` does, so the text helper's `label <text> default` makes the intended label; `cmd-labels-properties` names the `default` font word for `label`. Approved 2026-11-11.

### Q-SCRIPT-32: SPICE continuation spacing and the always-modified message
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior:
  - Continuation spacing: the space-insertion test looks at the continuation line's last character, not at the previous line's. When the continuation line ends with a space, no separator is inserted. If the logical line also does not end with a space and the continuation has no space after `+` (as in `+c d `), the last token of the logical line and the first token of the continuation are joined: `…b` and `+c d ` give `…bc d `. Otherwise the extra or missing spaces only produce empty tokens, which are skipped.
  - Message: `readspice` always reports "port order was modified" for cells with existing ports. It first moves those ports to indices of at least the pin count, and then compares each pin's moved index with its new index.
- Evidence: tcltk/readspice.tcl:68-72,110-131,204-206; tcltk/toolkit.tcl:489-495 (corrected 2026-11-11: tokens are not always equivalent)
- Options: replicate | compat | fix
- Recommendation: two parts, judged separately.
  - Tokenization: compat. A joined token changes pin matching and so the resulting port order (X-2 tier 2). With `magic-compat` OFF the separator is decided from the logical line's last character, because in SPICE a `+` line always starts a new token.
  - Message: fix in both modes. Only text changes (X-2 tier 3, as in Q-SCRIPT-15): report a change only when some pin's final index differs from its index before `readspice`, and record the stdout deviation in `tests/golden/deviations.toml`.
- Resolution: compat for the tokenization, fix for the message. Tokenization: with `magic-compat` ON Magic's continuation spacing is kept (a `+` line ending in a space can join two tokens); with it OFF the separator is decided from the logical line's last character, so a `+` line always starts a new token (X-2 tier 2). Message: in both modes "port order was modified" is reported only when some pin's final index differs from its index before `readspice` (X-2 tier 3), with the stdout deviation recorded in `tests/golden/deviations.toml`. Approved 2026-11-11.

### Q-SCRIPT-33: `techmanager initall` is not a valid option
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: the startup script calls `magic::techmanager initall`, which matches no option. The window does not exist yet, so the call returns immediately, and menus are built only when the user opens the manager.
- Evidence: tcltk/magic.tcl.in:372; tcltk/wrapper.tcl:413-424
- Options: replicate (no-op) | fix
- Recommendation: replicate (no-op) in both modes. The call has no effect in Magic. Hefesto's technology manager is native (Q-SCRIPT-34) and builds its style menus from the current technology when it opens, so nothing is observable (X-2 tier 3).
- Resolution: replicate (no-op) in both modes — `techmanager initall` has no effect; the native technology manager builds its style menus from the current technology when it opens. Approved 2026-11-11.

### Q-SCRIPT-34: GUI managers on iPadOS and native desktop GUI
- Kind: platform
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: all managers are Tk widgets with fixed Tk path names (`.cellmgr`, `.params`, `.layoutN.pane.top`), and PDK scripts address `F.titlebar.mbuttons.*` directly.
- Evidence: tcltk/wrapper.tcl; tcltk/toolkit.tcl:61-105
- Options: emulate the Tk path names | provide the menu-hook API only
- Recommendation: platform. Provide the `magic::add_toolkit_*` hooks and the dialog builder API natively in both shells, and do not emulate Tk path names in general. Handle the PDK `addtechmenu` widget calls as decided for Q-SCRIPT-42/D-32. (corrected 2026-11-11: the earlier text said path names matter only when desktop Tcl runs PDK scripts, but under X-3 the PDK scripts also run on iPadOS; see docs/decisions/audit/DECISIONS.md D-32)
- Resolution: platform (user decision, option b, with Q-SCRIPT-42 and D-32) — both shells provide the `magic::add_toolkit_*` hooks and the dialog builder API natively and do not emulate Tk path names in general; the known PDK widget operations are intercepted and mapped to native menu actions as decided in Q-SCRIPT-42, and skipped statements are reported. Approved 2026-11-11.

### Q-SCRIPT-35: Crash backups on sandboxed platforms
- Kind: platform
- Magic behavior: `crash save` writes to the system temporary directory on a Tcl `after` timer.
- Evidence: tcltk/tools.tcl:52-120
- Options: app-container temporary directory and an app timer | OS state restoration
- Recommendation: use the app sandbox's caches directory, with the same interval semantics.
- Duplicate: tracked in Q-CELL-30, which is the canonical entry. Record the resolution there. (Consolidation audit, 2026-11-06.) The caches directory recommended here may be purged by the OS; see docs/decisions/audit/DECISIONS.md.
- Resolution: platform — decided by D-8 (2026-11-07). Duplicate of Q-CELL-30; see the resolution there.

### Q-SCRIPT-36: Wrapper "Rotate 90 degree" calls a non-existent command
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: the Edit menu runs `magic::clock`, but only `magic::clockwise` and `magic::rotate` are registered, so the menu item raises "invalid command name". (The global `clock` is renamed `orig_clock`, and the dispatcher's conflict table applies only to `clockwise`.)
- Evidence: tcltk/wrapper.tcl:1263; dbwind/DBWcommands.c:260,429-430; tcltk/tclmagic.c:407-415
- Options: replicate | fix (`magic::rotate 90`)
- Recommendation: fix (GUI only, X-2 tier 3). The item runs `magic::clockwise 90`, which is identical to `magic::rotate 90`: both call `CmdClockwise` with 90 (commands/CmdCD.c:2305-2350). Magic's item always fails, so nothing depends on it.
- Resolution: fix (GUI only, X-2 tier 3) — the Edit menu item "Rotate 90 degree" runs `magic::clockwise 90`. Approved 2026-11-11.

### Q-SCRIPT-37: Command-line pattern ordering
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: `^-r(e)?(cover)?$` also accepts `-rcover`. `^-n*` matches any argument starting with `-`, so a mistyped option is passed to the engine instead of being taken as a file. `-dnull` disables the wrapper only through its own row, and that row has no `$`, so it also matches `-dnullx`.
- Evidence: tcltk/magic.tcl.in:172-224
- Options: replicate | fix (strict option table)
- Recommendation: replicate in both modes. The table only decides which arguments are options and which are files, and no output value changes (X-2 tier 3). PDK flows and launchers depend on these spellings (for example `magic -dnull -noconsole -rcfile …`).
- Resolution: replicate in both modes — the command-line pattern table keeps Magic's spellings (`-rcover`, `^-n*`, `-dnullx`); only option-versus-file classification depends on it (X-2 tier 3). Approved 2026-11-11.

### Q-SCRIPT-38: Editing a fixed device adds `pitchx`/`pitchy`, but PDKs use `deltax`/`xstep`
- Kind: doc-mismatch
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior (corrected 2026-11-11): when the dialog opens for an existing `nocell` instance, the framework sets `nx`, `ny`, `pitchx` and `pitchy` from the instance array. Every verified PDK's `<ns>::fixed_dialog` reads `pitchx`/`pitchy` when present, converts them with `magic::i2u`, and sets `deltax = 3digitpastdecimal(pitch − xstep)` (clamped at 0) before building the `deltax`/`deltay` entries. The dialog therefore shows the instance's real spacing, and Apply re-places the array with it. The earlier text said the PDKs never read `pitchx`/`pitchy` and that custom spacing is lost; both are wrong. Remaining quirks:
  - `array -list pitch` prints in the current display units, while `fixed_dialog` treats the value as internal units, so the derived spacing is wrong only when the units are not default or internal.
  - `pitchx`/`pitchy` stay in the parameter dictionary passed to `_check`/`_draw`, which ignore them.
- Evidence: tcltk/toolkit.tcl:1642-1653; commands/CmdAB.c:415-430 (`array -list pitch` via DBWPrintValue); open_pdks sky130/magic/sky130-fix.tcl:166-203 @ c6df28c1; IHP ihp-sg13g2-fix.tcl:158-200 @ 5e6d592e; gf180mcuD.tcl:4531-4565 (installed, open_pdks 0fe599b2)
- Options: replicate (keys as Magic sets them; the PDK converts them) | fix in Hefesto by deriving `deltax` itself (now moot, because the PDKs already do it)
- Recommendation: replicate in both modes. Framework and PDKs agree, so there is nothing to fix and no upstream report is needed. The units quirk is PDK code, which runs unchanged under Tcl and which native generators reproduce (X-3; X-2 n/a). Spec impact: the `pcell-toolkit` "nocell" bullet ("They never read `pitchx`/`pitchy` … dropped on Apply") and the fixed-device note in `scripting-pdk-integration` need correcting when this is approved.
- Resolution: replicate in both modes (n/a: framework and PDKs agree) — the framework sets `nx`, `ny`, `pitchx` and `pitchy` for an existing `nocell` instance, and each PDK's `fixed_dialog` converts `pitchx`/`pitchy` to `deltax`/`deltay`, so the instance's spacing is kept; the display-units quirk is PDK code, reproduced by native generators. No upstream report. `pcell-toolkit` and `scripting-pdk-integration` are corrected. Approved 2026-11-11.

### Q-SCRIPT-39: sg13g2 uses `label … +<layer>`, which Magic rejects
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: the `label` command treats only a leading `-` as the sticky marker, and `DBTechNameType` has no `+` form. `label E c +m2` and `label npn13G2 c +comment` (sg13g2 bipolar generator) fail with `Unknown layer: +m2` and create no label. The following `port make` then acts on whatever label is under the box. The PDK comment says the intent was a sticky label on metal2. (Re-checked at IHP @ 5e6d592e, lines 254, 615 and 862.)
- Evidence: commands/CmdLQ.c:324-338; database/DBtechname.c:64-110; IHP ihp-sg13g2-bjt.tcl:254,615,862
- Options: replicate (error, no label); accept `+` as "attach, not sticky" (differs from Magic, and generated cells would differ)
- Recommendation: replicate in both modes. The PDK code runs unchanged under Tcl, and a native sg13g2 generator must produce the same cell, so names and geometry stay identical (X-3 validation; X-2 n/a, PDK defect). File an upstream issue with IHP to change `+` to `-`.
- Resolution: replicate in both modes — `label … +<layer>` fails with `Unknown layer: +<layer>` and makes no label, as in Magic, so sg13g2 cells from Tcl and from native generators are identical (X-2 n/a, PDK defect). An upstream issue asks IHP to change `+` to `-`. Approved 2026-11-11.

### Q-SCRIPT-40: `magic::spice2float` suffix-mapping defects
- Kind: bug
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior (PDK code, open_pdks `common/pdk.tcl` and its IHP copy): the value is lowercased before the first `string map`, so its `uF/nF/pF/aF` keys never match. In the second map, `n` maps to `"* 1.0 e-9"` (with an embedded space), which is a syntax error in `expr`. Any value with an `n` suffix (`10n`, and `1nf` after mapping) therefore returns the fault value `0.0` and prints `Value is not numeric!`. `a` maps to `1.0e-15`, not `1e-18`. Values such as `10pf` become `10* 1.0e-12* 1.0e-15`. Running the procedure under tclsh 8.6 (2026-11-11) gives: `10n`, `1nf` and `10nm` → `0.0` with the message; `10pf` → `1e-26`; `1a` → `1e-15`; `0.42u` → `4.1999999999999995e-7`. The `expr` is unbraced, so `[…]` in a dialog value is evaluated, inside the X-3 sandbox.
- Evidence: open_pdks common/pdk.tcl:87-98; IHP ihp-sg13g2-util.tcl:766-780
- Options: replicate when running PDK scripts (automatic, since the PDK code runs as is); in native generators, replicate bit-for-bit; report upstream
- Recommendation: replicate. The Tcl path does this automatically, and native generators must match it bit for bit, because X-3 validates them against the Tcl output (X-2 n/a, PDK defect). File upstream issues with open_pdks and IHP. Golden case SCRIPT-PDK-12 pins the behavior.
- Resolution: replicate — PDK `magic::spice2float` runs unchanged under Tcl, and native generators reproduce its results bit for bit (X-2 n/a, PDK defect); golden case SCRIPT-PDK-12 pins the behavior. Upstream issues go to open_pdks and IHP. Approved 2026-11-11.

### Q-SCRIPT-41: PDK helpers share the `magic::` namespace and differ between PDKs
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: sky130/gf180mcu (`common/pdk.tcl`) and sg13g2 (`ihp-sg13g2-util.tcl`) define the same procedure names in `magic::` with different results. sg13g2's `i2u` returns `%.3f` text and its `u2i` rounds to an integer. Sourcing a second PDK's script in one session silently replaces the first PDK's helpers, which changes the first PDK's generated geometry and names. The helper file also defines a global procedure `magic:cellnameunique` (single colon), which looks like a typo. No verified script calls it (grep, 2026-11-11), so it is harmless.
- Evidence: open_pdks common/pdk.tcl:35-44,116; IHP ihp-sg13g2-util.tcl:691-704,795
- Options: replicate (single namespace); isolate each PDK's helpers per library namespace (deviates from Tcl semantics); one interpreter per open project (Tcl semantics kept inside each interpreter)
- Recommendation: replicate Tcl semantics within an interpreter (X-2 n/a, PDK code), and warn when a PDK script redefines a `magic::` procedure that another PDK defined. If Hefesto can hold several projects in one process (desktop multi-window, iPadOS multi-scene), also give each project its own interpreter, so two projects on different PDKs never share helpers. Magic itself rarely meets the conflict, because the implicit `<tech>.magicrc` load (Q-SCRIPT-10) runs only in a paint-free session. Whether to have one interpreter per project or per process is a product choice (decision table, judgement item 4).
- Resolution: replicate Tcl semantics inside each interpreter, with one Tcl interpreter per open project (user decision, option a) — two projects on different PDKs never share `magic::` helpers, and Hefesto warns when a PDK script redefines a `magic::` procedure that another PDK's script defined in the same interpreter (X-2 n/a, PDK code). Per-PDK helper namespaces (c) are rejected because they break unmodified PDK scripts. Approved 2026-11-11.

### Q-SCRIPT-42: PDK scripts manipulate wrapper Tk widgets directly [platform]
- Kind: platform
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: `<ns>::addtechmenu` runs `${frame}.titlebar cget -height` to detect the frame layout, runs `add command` on `${frame}.titlebar.mbuttons.drc.toolmenu`, runs `insert 4 command`/`insert 4 separator` on `${frame}.titlebar.mbuttons.file.toolmenu`, sets `Winopts(<frame>,cmdentry)`, and calls `addcommandentry`. `importspice` calls `tk_getOpenFile`. `suspendall` uses `winfo`. The device script also renames `closewrapper`. The wrapper evaluates the callback inside `catch`, so if a widget path is missing, the rest of the PDK menu setup is skipped silently. No Hefesto spec defines these widget paths or the File-menu item order (index 4).
- Evidence: tcltk/wrapper.tcl:1219-1221,1495-1500,1541-1565; tcltk/tools.tcl:14-45; open_pdks sky130/magic/sky130.tcl:36-47,83-243; IHP ihp-sg13g2.tcl:115-126,162-282
- Options: (a) provide a Tk-compatible widget shim with these paths in the desktop GUI; (b) intercept the PDK callback and map the three known widget operations to native menu actions; (c) ignore and report
- Recommendation: platform, (b) on every platform, with a report of the statements that were skipped. This is consistent with D-32 and X-3. (corrected 2026-11-11: the earlier recommendation was "(b) on desktop", which predates X-3 running PDK scripts on iPadOS.) The mapping:
  - `${f}.titlebar cget -height` raises an error, so the PDK selects `${f}.pane.top`.
  - `…drc.toolmenu add command` appends to the native DRC menu.
  - `…file.toolmenu insert 4 …` inserts at the position that Magic's index 4 has in its File menu.
  - `tk_getOpenFile` opens the native file picker.

  `suspendall`/`resumeall` are provided natively, so their `winfo` use does not arise. Add a `ui-gui-managers` requirement that names these paths only if (a) is chosen.
- Resolution: platform, option b (user decision; settles D-32) — on every platform (desktop and iPadOS shells) the PDK `addtechmenu` callbacks run with the known widget operations intercepted and mapped to native menu actions: `${f}.titlebar cget -height` raises an error, so the PDK selects `${f}.pane.top`; `…drc.toolmenu add command` appends to the native DRC menu; `…file.toolmenu insert 4 …` inserts at the position Magic's index 4 has in its File menu; `tk_getOpenFile` opens the native file picker. Every other widget statement is skipped and reported (one diagnostic per statement, naming the script and line). No Tk widget shim. Approved 2026-11-11.

### Q-SCRIPT-43: PDK extraction sections use `$SUB` in device lines and `$[…]` model names
- Kind: ambiguity
- Magic behavior: all three PDK extract sections declare `substrate … $SUB …`, and this is specified. sg13g2 additionally uses `$SUB` as a device substrate terminal and in `s=$SUB`, and writes resistor model names as `$[rmod_rsil]`, `$[res_rppd]` and `$[rmod_rhigh]`. A grep of `openspec/specs` finds no specification of `$`-substitution in `device` lines or of the `$[…]` model-name form.
- Evidence: IHP ihp-sg13g2-extract.tech:60-62,1252-1254; open_pdks sky130/magic/sky130-extract.tech:149; extract/ExtBasic.c:900-916,2650-2665
- Options: hand over to the EXT area owner to specify, or confirm that Magic treats them literally
- Recommendation: open an EXT question and add golden extraction cases on an sg13g2 resistor. SCRIPT cannot verify it.

### Q-SCRIPT-44: PDK startup seeds the random generator from the clock
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: every verified rc file runs `catch {random seed}`, which seeds from the current time. GDS output draws library prefixes from that generator (see capability `gds-write`), so two runs of the same flow can produce different GDS files. The rc files recommend replacing it with a fixed seed for repeatable output. The other users of `random()` are the `random` command itself and the plow random test (plow/PlowRandom.c).
- Evidence: open_pdks sky130/magic/sky130.magicrc:17-19; commands/CmdRS.c:71-122; calma/CalmaWrite.c:747-770; Q-MASK-211
- Options: replicate; let a Hefesto profile pin the seed
- Recommendation: replicate (X-2 tier 3: the seed is session state, not a quirk). The golden-test harness MUST run `random seed 1` after startup, as tests/golden/SCRIPT/CASES.md already says. Whether Hefesto adds a project setting that pins the seed belongs with Q-MASK-211 (M5): its recommended hash-based GDS prefix would make the seed irrelevant to output. This entry does not block M4.
- Resolution: replicate (X-2 tier 3: the seed is session state) — PDK rc files seed `random` from the clock; the golden harness runs `random seed 1` after startup. A setting that pins the seed is decided with Q-MASK-211 (M5), so this entry does not constrain M4. Approved 2026-11-11.

### Q-SCRIPT-45: sg13g2 generators change the window root cell with `load` during a gencell draw
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11; resolution changed by the user 2026-11-12)
- Magic behavior: sg13g2 fixed and bipolar devices build their base cell on demand inside `_draw`, while the framework has the edit stack pushed into the generated cell. They save `cellname list window` or `cellname list self`, suspend the `load` tag, run `load <base> -silent`, paint, and then run `load <saved>`. The state of the framework's edit stack and of `popstack` after this sequence was not derived. `cellname list self` with no selection may return the generated cell, not the window root.
- Static derivation (added 2026-11-11, to confirm with SCRIPT-PDK-14):
  - The edit stack is the Tcl list `editstack` (tcltk/tools.tcl:126-183). `load` does not touch it, so `popstack` after the draw still returns to the window and view saved by `pushstack`.
  - Bipolar devices (generated cell, `pushstack`ed): with no cell selected, `cellname list self` returns the edit root (database/DBcellname.c:886-920). After `pushstack`'s `load`, that is the generated cell. `load $curcell` therefore returns to it, and the framework's `property` calls land on the right cell.
  - Fixed devices (`nocell`, no `pushstack`): `cellname list window` is the user's window root. Each `load` makes the root the edit cell (dbwind/DBWprocs.c:557-578), so a fixed device placed while a subcell is being edited is placed in the root cell. The view is also reset by the reload.
- Evidence: IHP ihp-sg13g2-fix.tcl:238-260; IHP ihp-sg13g2-bjt.tcl:195-262; tcltk/toolkit.tcl:1253-1283
- Options: replicate whatever Magic does (golden case SCRIPT-PDK-14)
- Recommendation: replicate (PDK code under Tcl; X-2 n/a). The native sg13g2 fixed-device generator must reproduce the edit-cell and view reset. Confirm the derivation with SCRIPT-PDK-14 before implementing native sg13g2 generators. The Tcl path runs the PDK code as is, so this does not block the M4 gencell framework.
- Capture (2026-11-12, SCRIPT-PDK-14, golden-capture run 43151033297): the static derivation is wrong for Magic's real sequence. After `magic::gencell sg13g2::npn13g2 Q1` and `magic::gencell sg13g2::diodevdd_2kv D1` in a new cell `top` (base cells not in memory) both gencells return 0, `cellname list window` is `top`, `cellname list self` is `diodevdd_2kv`, `cellname list children top` is empty (neither device is placed in `top`), and stderr has `Unknown layer: +comment` (Q-SCRIPT-39) and `Cell Topmost cell in the window (top) isn't a child of the edit cell.`
- Resolution: compat (user decision 2026-11-12; replaces the earlier "replicate" of 2026-11-11) — the Tcl path runs the sg13g2 generators unchanged. The native sg13g2 fixed-device generators reproduce the recorded Magic result with `magic-compat` ON (the base cell is generated with the PDK's sequence, the edit cell is left on the base cell and the window's cell gets no child, SCRIPT-PDK-14) and give the fixed result with it OFF: after the base cell is generated, the edit cell, window cell and box are restored and the device is placed in the edit cell (X-2 tier 2). Approved 2026-11-12.

### Q-SCRIPT-46: Is the declarative PDK profile still needed under X-3?
- Kind: platform
- Status: approved (recommendation drafted 2026-11-11, approved 2026-11-11)
- Magic behavior: none. Magic always evaluates PDK `.magicrc` files as Tcl.
- Evidence: tcltk/magic.tcl.in:339-470
- Options: (a) keep the declarative profile for PDKs that are not in the iPadOS curated packs when App Review rejects imported scripts; (b) drop it, and make curated-pack mode refuse PDKs without bundled scripts; (c) also use it when the user declines script consent, instead of the `minimum`-tech read-only fallback of `hefesto-security`
- Recommendation: platform, (b), unless App Review rejects imported scripts and users need uncurated PDKs on iPadOS (same as D-30). The requirement "Sessions without PDK script execution" in `scripting-pdk-integration` was written under the pre-X-3 assumption that iPadOS has no Tcl, and it is now limited to curated-pack mode until this is decided. Option (c) would contradict `hefesto-security` "Consent before implicit script execution", which opens a declined file read-only with the `minimum` tech.
- Resolution: platform, option b (user decision; settles D-30) — the declarative (non-script) PDK profile is dropped. iPadOS curated-pack mode refuses PDKs without bundled scripts, and a declined consent keeps the read-only `minimum`-tech fallback of `hefesto-security`. The requirement "Sessions without PDK script execution" in `scripting-pdk-integration` is replaced accordingly. Approved 2026-11-11.

### Q-SCRIPT-47: `readspice` maps `]` to `>]`
- Kind: bug
- Status: approved (recorded 2026-11-12 from the SCRIPT-RSP-03 capture; approved 2026-11-12)
- Magic behavior: the bracket mapping of the pin lookup is `string map {\[ < \] >]} $pin`; the second pair maps `]` to `>]`, so a netlist pin `D[0]` is looked up as `D<0>]` and never matches the layout port `D<0>`. The port keeps the index the renumbering's step 2 moved it to. The reverse mapping (`<`/`>` to `[`/`]`) is correct.
- Evidence: tcltk/readspice.tcl:165-167; SCRIPT-RSP-03 (golden-capture run 43173319299)
- Options: replicate | compat (replicate with `magic-compat` ON, the intended `]` → `>` with it OFF) | fix
- Recommendation: compat. A script relying on Magic's port order keeps it; the intended mapping is evident from the code comment and the reverse map.
- Resolution: compat (user decision 2026-11-12) — with `magic-compat` ON `D[0]` never matches `D<0>`, as in Magic; with it OFF the mapping is `[`/`]` to `<`/`>` and `D<0>` takes the netlist position of `D[0]` (X-2 tier 2). `readspice` "Port renumbering algorithm" and its scenario "Bus-bracket mapping" updated. Report upstream to Magic. Approved 2026-11-12.
