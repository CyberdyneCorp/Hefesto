# Open Questions: SCHEM area (xschem-compatible schematic editor)

These entries cover the capabilities `schematic-file-format`, `schematic-symbols-and-libraries`, `schematic-editing`, `schematic-netlisting`, `schematic-simulation-integration` and `schematic-command`. Product decision: `docs/decisions/product.md#P-12`.

The reference is xschem, not Magic: `StefanSchippers/xschem` @ `4142e6d6f1eb5cd0e271bb183f14202af374ce27` (version `3.4.8RC`, file format `1.3`). "Evidence" paths are relative to the xschem `src/` directory. PDK reference data: `xschem_sky130` @ `2bfd7d65`, `IHP-Open-PDK` @ `5e6d592e` (`ihp-sg13g2/libs.tech/xschem`), `gf180mcu_fd_pr` @ `aacc04cc` (`cells/xschem`), `open_pdks` @ `c6df28c1`. These trees were cloned read-only; nothing was built or run, so every expected netlist text in the specs is derived from the code and must be confirmed by the golden cases in `tests/golden/SCHEM/CASES.md`.

xschem is GPL-2.0-or-later, including its `xschem_library/devices` symbols (they carry the GPL header). The PDK symbol libraries are Apache-2.0. Hefesto is implemented clean-room from the specs.

**Clean-room process (J-1, approved 2026-11-13).** The `Evidence:` lines below and the `(xschem ref: ...)` traces in the specs were written by the spec side of a two-team process and exist for traceability only. Implementers SHALL NOT follow them into xschem's source, and no xschem source tree is reachable from implementation sessions (capability `hefesto-licensing`, Requirement "Clean-room rule for GPL-sourced behavior"). All 29 entries were decided on 2026-11-13 in `docs/decisions/m8-decision-table.md`.

**Compatibility setting (D-21, approved 2026-11-13).** Entries in this file that record `Resolution: compat` are governed by the `xschem-compat` setting (default ON) of capability `hefesto-magic-compat-mode`, not by `magic-compat`.

### Q-SCHEM-1: Version string written by Hefesto
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: always writes `v {xschem version=<its own version> file_version=1.3}`, so every save by a different xschem version changes the first line.
- Evidence: save.c:3223-3240, xschem.h:26-27
- Options: replicate (write a pinned xschem compatibility version, `3.4.8RC`) | write the version read from the file (byte-identical round trips of unedited files, but a 1.2 file would claim `file_version=1.3` content with an old version string) | write a Hefesto version (`xschem version=hefesto-1.0`), which xschem ignores.
- Recommendation: pinned compatibility version (spec `schematic-file-format`, Requirement "Writer version string"); bump it only with a re-validation against a newer xschem.
- Resolution: replicate — Hefesto writes `v {xschem version=3.4.8RC file_version=1.3}`, a pinned compatibility version, and changes it only together with a re-validation against a newer xschem (`schematic-file-format` "Writer version string"). Approved 2026-11-13.

### Q-SCHEM-2: Embedded symbol block quirks
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: the `[ ... ]` block omits the `F` record, writes polygons after texts (the main writer puts them before), and writes the wires of a schematic-used-as-symbol as `L 1` lines. The version line in the block has no header text.
- Evidence: save.c:3010-3078
- Options: replicate | fix (write the block like a normal `.sym`)
- Recommendation: replicate; the block is read back identically either way, and byte-identity with xschem keeps diffs clean.
- Resolution: replicate — embedded `[ ... ]` blocks are written as xschem writes them in both modes: no `F` record, polygons after texts, wires of a schematic-used-as-symbol as `L 1`, no header text in the block's version line. The block reads back the same either way, and byte identity keeps diffs clean. Golden case SCHEM-RT-05 uses a Hefesto-authored input (J-1). Approved 2026-11-13.

### Q-SCHEM-3: File version unknown during the first record
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: the file version defaults to `1.0` only after the first record has been processed. A file whose first record is a `C` record (no `v`) therefore does not get `.sym` appended to that first reference, but does for later ones.
- Evidence: save.c:3585-3734 (default set at the end of the loop body), save.c:3346-3350
- Options: replicate | fix (decide the version before reading any record)
- Recommendation: compat. With `xschem-compat` ON, the file version stays unset until the end of the first record, as in xschem, so a leading `C` record gets no `.sym`. With it OFF, a file with no `v` record is version `1.0` from its first record, so every reference gets `.sym`. Which symbol a reference loads is file content, so the standing rule makes this compat. Only hand-written or very old files are affected. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `xschem-compat` ON the file version is unset until the end of the first record, so a leading `C` record gets no `.sym`; with it OFF a file with no `v` record is version `1.0` from its first record, so every reference gets `.sym` (`schematic-file-format` "Version record and header text"). Approved 2026-11-13.

### Q-SCHEM-4: Verilog, VHDL, Spectre and tEDAx netlisters
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: netlists to five formats; `G`, `V`, `F`, `E` records and `*_format` attributes feed the non-SPICE ones.
- Evidence: verilog_netlist.c, vhdl_netlist.c, spectre_netlist.c, tedax_netlist.c
- Options: SPICE only (records preserved) | add Verilog (digital flows, P-17) | add Spectre
- Recommendation: SPICE only for P-12; keep all records and attributes round-trip. Revisit Verilog with P-17.
- Resolution: other — SPICE is the only netlister in M8 (judgement J-4, option a). Every `G`, `V`, `F` and `E` record and every `*_format` attribute is kept through round trips. Verilog is revisited with P-17 (digital flow). Approved 2026-11-13.

### Q-SCHEM-5: Pin sorting by sim_pinnumber is unstable
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: pins (and top-level ports) are sorted with C `qsort` when every pin has `sim_pinnumber`; equal numbers have an unspecified relative order that depends on the C library.
- Evidence: save.c:3825-3876, save.c:3878-3922
- Options: replicate glibc's order (not portable) | stable sort (file order on ties)
- Recommendation: stable sort; duplicate `sim_pinnumber` values are a user error, so warn about them.
- Resolution: fix — in both modes pins, and top-level ports, are sorted by `sim_pinnumber` with a stable sort (file order, or instance order, on ties), and duplicate `sim_pinnumber` values are reported as a warning. xschem's order on ties depends on the C library, so there is nothing portable to match (X-1). Approved 2026-11-13.

### Q-SCHEM-6: Symbol `drc` scripts
- Kind: platform
- Status: approved (resolved 2026-11-07 by X-3; confirmed in the M8 decision table 2026-11-13)
- xschem behavior: a symbol's `drc` attribute (for example sky130 `sky130_fet_drc {@name} {@symname} ...`, IHP `fet_drc ...`) names a Tcl procedure that the PDK's `xschemrc` sources (IHP `xschem-drc`) and that the "symbol check" menu runs.
- Evidence: actions.c:825-878; IHP xschemrc:446-450; xschem_sky130 sky130_fd_pr/nfet_01v8.sym
- Options: run in the sandboxed Tcl like `tcleval` | ignore | re-implement the checks natively
- Recommendation: run in the sandbox; the procedures only compute and print.
- Resolution: platform — symbol `drc` procedures run in the sandboxed embedded Tcl interpreter on every platform (X-3, 2026-11-07).

### Q-SCHEM-7: [platform] Tcl dependence of PDK symbols
- Kind: platform
- Status: approved (resolved 2026-11-07 by X-3; confirmed in the M8 decision table 2026-11-13)
- xschem behavior: PDK symbols evaluate attribute values with Tcl: sky130 `corner.sym` (`tcleval(.lib $::SKYWATER_MODELS/...)`), IHP resistor, capacitor and bipolar formats (`tcleval(... [ev7 {...}] ...)`, `cap_cmomi_C`), IHP testbench code blocks (`format="tcleval( @value )"`, 138 uses in the IHP tree), annotation symbols (`ngspice::get_node`). Without Tcl these netlist wrongly or not at all.
- Evidence: token.c:59-81, xschem.tcl:7224-7298, xschem.tcl:677-698
- Options: embedded sandboxed Tcl on every platform (decision X-3, capability `hefesto-scripting`) | re-implement the used Tcl subset natively | pre-expand PDK symbols at install time (breaks user edits)
- Recommendation: embedded Tcl, as already decided for Magic PDK scripts. If the iPadOS App Store fallback of `hefesto-scripting` applies, iPad needs a native evaluator for the exact constructs found in the three PDKs (`subst` with `[ev ...]`, `[ev7 ...]`, `$::VAR`, `[to_eng ...]`), validated by golden cases SCHEM-NL-07/08.
- Resolution: platform — PDK symbol attributes are evaluated by the sandboxed embedded Tcl interpreter on every platform, iPadOS included; the iPadOS App Store fallback of `hefesto-scripting` covers the rejected-scripts case (X-3, 2026-11-07).

### Q-SCHEM-8: Tcl procedures and variables required by PDK libraries
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: `ev`, `ev7`, `ev0`, `to_eng`, `from_eng` and `ngspice::*` are defined in xschem's Tcl code; PDK `xschemrc` files add variables and `source` PDK Tcl files.
- Evidence: xschem.tcl:612-720; IHP xschemrc:390-460
- Options: clean-room re-implementation of exactly the procedures used by the pinned PDKs | broader xschem Tcl API
- Recommendation: inventory script over the three PDK trees (every `tcleval(`, `[`-command and `$::` variable), then implement exactly that list (`ev`, `ev7`, `ev0`, `to_eng`, `from_eng`, `ngspice::*`, the `xschem` subset). Each procedure's semantics are specified from the public xschem manual and from expected values computed for the PDK symbols, never from xschem's Tcl. Keep the inventory as a test that fails when a PDK bump adds a procedure. (corrected 2026-11-13, adds the clean-room source rule)
- Resolution: other — an inventory script over the three pinned PDK trees lists every `tcleval(`, `[`-command and `$::` variable; Hefesto implements exactly that list (`ev`, `ev7`, `ev0`, `to_eng`, `from_eng`, `ngspice::*`, the `xschem` subset), with semantics specified from the public xschem manual and from expected values computed for the PDK symbols, never from xschem's Tcl. The inventory is kept as a test that fails when a PDK bump adds a procedure. Approved 2026-11-13.

### Q-SCHEM-9: Web (http) symbol references
- Kind: platform
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: symbol and schematic references starting with `http://` or `https://` are downloaded into a temporary folder.
- Evidence: save.c:4078-4090, save.c:4930-4945, actions.c:2564-2580, xschem.tcl:8804-8806
- Options: support through the platform network service | not support (show as missing symbol)
- Recommendation: not support in v1; no PDK uses it.
- Resolution: platform — `http://` and `https://` references are not supported in v1: the reference is kept as written, a missing-symbol placeholder is shown and a warning is reported. Nothing is fetched at load time (X-4). Approved 2026-11-13.

### Q-SCHEM-10: Clean-room system library and xschem command inventory
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: PDK schematics reference xschem's GPL `devices/` symbols (`res.sym`, `gnd.sym`, `code_shown.sym`, `launcher.sym`, ...) and launchers call `xschem` subcommands.
- Evidence: xschem_library/devices (127 files), scheduler.c (subcommand list)
- Options: re-draw every referenced symbol clean-room with identical netlisting properties | ship xschem's library under GPL (license conflict with P-9) | require users to install xschem's library
- Recommendation: clean-room re-draw of the symbols actually referenced by the three PDKs and their examples, specified by name, pins and properties only; generate the list and the `xschem` subcommand list by an inventory script.
- Resolution: other — judgement J-3, option a: Hefesto ships a clean-room re-draw of only the `devices/` symbols that the three PDKs and their examples reference, specified by file name, pin names, pin order and pin positions, and netlisting properties; the graphics are drawn independently. The interface table is extracted from the GPL `devices/*.sym` files by the spec side of the two-team process (J-1) and needs counsel sign-off. The symbol list and the `xschem` subcommand list are generated by inventory scripts over the Apache-2.0 PDK trees; unsupported subcommands raise the error `schematic-command` defines. Approved 2026-11-13.

### Q-SCHEM-11: [platform] Symbol generators
- Kind: platform
- Status: approved (resolved 2026-11-07 by X-3 and X-4; confirmed in the M8 decision table 2026-11-13)
- xschem behavior: a reference `name(args)` runs the program `name` from the search path and reads its output as a symbol (fork/exec, or `tclsh` on Windows).
- Evidence: token.c:83-182, save.c:4918-4930
- Options: Tcl generators only, in the sandbox | allow external programs on desktop | none
- Recommendation: Tcl only on all platforms (spec); allow external programs on desktop later behind a per-folder trust setting if users need them. iPadOS cannot spawn processes.
- Resolution: platform — symbol generators run only as Tcl in the sandbox on every platform; no external program is executed, since the sandbox forbids process execution (X-3, X-4, 2026-11-07).

### Q-SCHEM-12: `expr()` number lexing swallows minus signs
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: inside `expr(...)` a number token is the longest run of `.0-9a-zA-Z_-`, so `expr(2-1)` evaluates to 2 and `expr(1e-3-1)` to 0.001. With spaces (`2 - 1`) it is correct.
- Evidence: eval_expr.y:300-315, editprop.c:536-568
- Options: replicate | fix (stop a number at `-` unless it follows `e`/`E`)
- Recommendation: compat. With `xschem-compat` ON, `-` belongs to the number token as in xschem (`expr(2-1)` = 2). With it OFF, a number stops at `-` unless the `-` follows `e` or `E` (`expr(2-1)` = 1). Both modes warn wherever the two lexings differ, that is a digit-started run with an inner `-` that does not follow `e`/`E`. PDK templates use spaces, so PDK netlists are identical in both modes. (corrected 2026-11-13, was replicate and warn)
- Resolution: compat — with `xschem-compat` ON `-` belongs to a number token (`expr(2-1)` = 2); with it OFF a number stops at `-` unless the `-` follows `e`/`E`; both modes warn where the two lexings differ (`schematic-netlisting` "expr evaluation"). Approved 2026-11-13.

### Q-SCHEM-13: Absolute paths in netlist comments
- Kind: platform
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: netlists contain `** sch_path: <abs>` and `** sym_path: <abs>` lines with host paths.
- Evidence: spice_netlist.c:329, spice_netlist.c:690-709
- Options: replicate (paths differ per machine, and iPadOS sandbox paths are long and private) | write paths relative to the PDK root or document folder | omit on request
- Recommendation: replicate by default: write host absolute paths as xschem does. A per-project setting "portable netlist paths" (default off) writes `$PDK_ROOT/...`-relative or document-relative paths instead. Golden comparisons normalise these lines. (corrected 2026-11-13, the portable-paths option is a per-project setting)
- Resolution: platform — by default the `** sch_path:` and `** sym_path:` lines carry host absolute paths, as in xschem; the per-project setting "portable netlist paths" (default off) writes `$PDK_ROOT/...`-relative or document-relative paths instead. Golden comparisons normalise these lines. Approved 2026-11-13.

### Q-SCHEM-14: Two subcircuits with the same cell name
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: definitions are de-duplicated by cell name only (file name without folder and extension), so `libA/inv.sym` and `libB/inv.sym` produce one `.subckt inv`, from whichever is found first. The netlist is silently wrong.
- Evidence: spice_netlist.c:505-512
- Options: replicate | fix (rename the second, e.g. `inv_1`) | replicate and warn
- Recommendation: compat. With `xschem-compat` ON, write one `.subckt` per cell name, from the first symbol found, as xschem does. With it OFF, a later subcircuit whose cell name was already written gets a unique name (`inv_1`, `inv_2`, ...), and its instance lines use it. Both modes warn. (corrected 2026-11-13, was replicate and warn)
- Resolution: compat — with `xschem-compat` ON one `.subckt` is written per cell name, from the first symbol found; with it OFF a later subcircuit whose cell name was already written gets a unique name (`<cell>_1`, `<cell>_2`, ...) that its instance lines use; both modes warn. The exact ON text is fixed by golden case SCHEM-NL-16. Approved 2026-11-13.

### Q-SCHEM-15: Flat netlist text
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: flat netlists are produced by piping the hierarchical netlist through `flatten.awk`, which uppercases most text, uses `_` as path separator, has a fixed table of primitive node counts and does not expand undefined subcircuits.
- Evidence: flatten.awk:36-312
- Options: exact reproduction | electrically equivalent flat netlist (spec) | drop flat mode
- Recommendation: equivalent (behavioral); flat mode is rarely used with PDK flows, which need `.subckt` models anyway.
- Resolution: replicate — behavioural: the flat netlist is electrically equivalent to xschem's (same devices, nets and values) and is compared by a canonical parsed form, not by bytes. xschem's flattening text rules are not reproduced. Golden case SCHEM-NL-21 checks equivalence only. Approved 2026-11-13.

### Q-SCHEM-16: Shorts at top level are not always reported
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: the short report is suppressed while the netlist counter is 0, i.e. during the first connectivity pass of the top level; top-level shorts appear only from the second pass after the hierarchy has been netlisted, and not at all in `-nohier` runs.
- Evidence: netlist.c:908-927, spice_netlist.c:556-560
- Options: replicate | fix (always report)
- Recommendation: fix: always report shorts; the netlist text is unaffected.
- Resolution: fix — shorts are always reported, including top-level shorts on the first pass and in `-nohier` runs. The netlist text is unchanged; only diagnostics differ. Approved 2026-11-13.

### Q-SCHEM-17: Simulation preparation must accept schematic decks
- Kind: doc-mismatch
- Status: approved (resolved 2026-11-06; confirmed in the M8 decision table 2026-11-13)
- xschem behavior: testbench schematics carry `.control` blocks with `write`, `quit`, `set appendwrite`; capability `simulation-netlist-preparation` (Requirement "Deck validation and sanitisation") rejects `write` and `quit` even with `allowControl`, and Requirement "Circuit sources" has no schematic source.
- Evidence: IHP sg13g2_tests/dc_lv_nmos.sch; tests/SC_Test.spice
- Options: amend `simulation-netlist-preparation` with a `schematic` source and the translation of `schematic-simulation-integration` Requirement "Control block translation" | require users to edit testbenches
- Recommendation: amend `simulation-netlist-preparation` and `simulation-command` (`sim dut schematic <file>`) in a follow-up change.
- Resolution: fix. `simulation-netlist-preparation` now has the `schematic` source, and schematic decks are validated after control-block translation (2026-11-06).

### Q-SCHEM-18: awk dialect differences in xschem's post-processing
- Kind: platform
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: `spice.awk` relies on gawk's `IGNORECASE` for `.param` detection and the `value=` renames. With mawk (Debian/Ubuntu default) or BWK awk (macOS) these matches are case-sensitive, so the same schematic can give different netlists on different hosts.
- Evidence: spice.awk:101-140, spice.awk:238-262
- Options: follow gawk (case-insensitive) | follow mawk (case-sensitive)
- Recommendation: gawk semantics (spec); golden references must be generated with gawk installed as `awk`.
- Resolution: replicate — post-processing follows gawk semantics (case-insensitive `.param` and `value=` matching) on every platform (X-1). Golden captures run with gawk installed as `awk`. Approved 2026-11-13.

### Q-SCHEM-19: `extra` exclusion uses substring match
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: when copying the template onto the `.subckt` line, a key is dropped if it occurs anywhere inside the `extra` string, so `extra="VPWR"` also drops a parameter `VPW`.
- Evidence: token.c:620-650
- Options: replicate | fix (whole-word match)
- Recommendation: compat. With `xschem-compat` ON, drop a template key that occurs anywhere in `extra`, as xschem does. With it OFF, drop only keys that are whole words of `extra`. Both modes warn when a dropped key is not a whole word of `extra`. (corrected 2026-11-13, was replicate and warn)
- Resolution: compat — with `xschem-compat` ON a template key is dropped from the `.subckt` line when it occurs anywhere in `extra`; with it OFF only when it is a whole word of `extra`; both modes warn when a dropped key is not a whole word. Approved 2026-11-13.

### Q-SCHEM-20: Line breaker state
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: the quote state of `break.awk` (toggled by `{`, `}` and `'`, with no nesting) persists across lines, so one unbalanced quote disables breaking for later lines; after a break the column counter is reset without counting the field just written.
- Evidence: break.awk:26-75
- Options: replicate | fix
- Recommendation: replicate (it only affects layout of long lines), covered by golden case SCHEM-NL-12.
- Resolution: replicate — the line breaker carries the quote state across lines and resets the column counter as xschem does, in both modes; only where long lines wrap changes. Golden case SCHEM-NL-12. Approved 2026-11-13.

### Q-SCHEM-21: Multi-line templates break `.subckt` lines
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: the template is copied onto the `.subckt` header with its separators, so a template written on several lines (`template="name=x1` LF `W=1"`) puts parameters on new lines without `+`, which SPICE reads as garbage.
- Evidence: token.c:576-668, spice_netlist.c:715-728
- Options: replicate | fix (replace LFs by spaces)
- Recommendation: compat. With `xschem-compat` ON, copy the template's LFs onto the `.subckt` header as xschem does. With it OFF, replace each LF by a space, so the header is valid SPICE. Both modes warn. The inventory checks whether any PDK subcircuit symbol has a multi-line template. (corrected 2026-11-13, was replicate and warn)
- Resolution: compat — with `xschem-compat` ON the template's LFs are copied onto the `.subckt` header; with it OFF each LF becomes a space; both modes warn. The PDK inventory reports any subcircuit symbol with a multi-line template. The exact ON text is fixed by the golden captures. Approved 2026-11-13.

### Q-SCHEM-22: Trailing `m=1` removed everywhere
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: every netlist line, including comments and user code, loses a trailing ` m=1` (any case, spaces around `=`).
- Evidence: spice.awk:94
- Options: replicate | fix (device lines only)
- Recommendation: compat. With `xschem-compat` ON, remove a trailing ` m=1` from every line, as xschem does. With it OFF, remove it only from device lines (outside user-code blocks, first field not starting with `*` or `.`), keeping comments, `.param` and `.control` text unchanged. On device lines `m=1` is the default, so removing it changes nothing. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `xschem-compat` ON a trailing ` m=1` is removed from every line; with it OFF only from device lines, so comments, `.param` and `.control` text keep it. The exact texts are fixed by the golden captures. Approved 2026-11-13.

### Q-SCHEM-23: [platform] xschemrc evaluation on iPadOS
- Kind: platform
- Status: approved (resolved 2026-11-07 by X-3 and X-4; confirmed in the M8 decision table 2026-11-13)
- xschem behavior: `xschemrc` reads environment variables (`PDK_ROOT`, `PDK`, `HOME`, `PWD`), probes folders with `file isdir`, and sources Tcl files.
- Evidence: xinit.c:3552-3610; IHP and sky130 xschemrc
- Options: provide synthetic `env()` values mapped to the PDK store and the document folder (spec) | parse only `XSCHEM_LIBRARY_PATH` lines
- Recommendation: synthetic environment in the sandbox; the iPad sandbox grants read access to the PDK store only.
- Resolution: platform — `xschemrc` runs in the sandboxed Tcl interpreter with synthetic `env()` values mapped to the PDK store and the document folder, settings replacing environment variables (X-3, X-4, 2026-11-07).

### Q-SCHEM-24: [platform] Touch editing details
- Kind: platform
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: mouse and keyboard only; wire placement is click-click with live preview, polygons end on a double click, and many operations exist only as keys.
- Evidence: keys.help, callback.c
- Options: gesture set in `schematic-editing` Requirement "Desktop and touch interaction" | a floating tool palette | both
- Recommendation: both; usability-test wire drawing with finger vs Pencil before freezing.
- Resolution: platform — iPad schematic editing offers both the gesture set of `schematic-editing` and a floating tool palette. The gestures stay provisional until a usability test of finger vs Pencil wire drawing, which gates the iPad scope freeze (M14), not M8. Desktop keeps xschem-style default bindings, rebindable (X-5). Approved 2026-11-13.

### Q-SCHEM-25: Script permission model
- Kind: platform
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: `tcleval` and launcher scripts run only if `xschem_execute_scripts` allows it (`ask` by default, `yes` in the sky130 `xschemrc`), and then run with full Tcl rights.
- Evidence: xschem.tcl:7224-7250, xschem.tcl:12088
- Options: always run `tcleval` in the sandbox and confirm launchers (spec) | replicate the ask/yes/no setting
- Recommendation: as specified: `tcleval` always runs in the X-3 sandbox, launcher `tclcommand` scripts are confirmed once per document, and `xschem_execute_scripts` is ignored with a log line. Needs a `hefesto-security` review sign-off before the M8 exit. (corrected 2026-11-13, the sign-off is an M8 exit condition)
- Resolution: platform — judgement J-5, option a: `tcleval` attribute values always run in the X-3 sandbox, launcher `tclcommand` scripts are confirmed once per document and run in the same sandbox, and `xschem_execute_scripts` is read, ignored and logged. A `hefesto-security` review sign-off is an M8 exit condition. Approved 2026-11-13.

### Q-SCHEM-26: `@` tokens end only at whitespace
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: a token runs until whitespace, `@`, `%` or an escaped character, so `@W/@L` looks up the key `W/` and gives nothing; users must write `@W\/@L` or `@W / @L`.
- Evidence: token.c:2478-2490, token.c:5370-5380
- Options: replicate | fix (stop at non-identifier characters)
- Recommendation: replicate; changing it would alter PDK netlists that rely on tokens such as `@spice_get_voltage(net)`. Warn when a token lookup fails but a shorter identifier prefix of the token is defined (`@W/` → `W`). (corrected 2026-11-13, adds the lookup-prefix warning)
- Resolution: replicate — `@` tokens end as in xschem, in both modes; a warning names the shorter identifier prefix when a token lookup fails but that prefix is defined (`@W/` → `W`). Approved 2026-11-13.

### Q-SCHEM-27: Missing subcircuit schematic
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: when the schematic of a `subcircuit` symbol does not exist, the header lines are still written and the body comes from an empty load; the exact text was not derived.
- Evidence: spice_netlist.c:700-735, actions.c:2544-2562
- Options: replicate after golden capture | always error
- Recommendation: capture with golden case SCHEM-NL-14, then replicate and warn.
- Resolution: replicate — a `subcircuit` symbol whose schematic is missing gets its header lines and an empty body, and a warning, as `schematic-netlisting` says; the exact text is fixed by golden case SCHEM-NL-14. Approved 2026-11-13.

### Q-SCHEM-28: Instance-based schematic variants
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: instances with a `schematic=` attribute create additional symbol entries whose naming and `.subckt` names were not fully derived.
- Evidence: actions.c:2474-2530, spice_netlist.c:470-500 (additional symbol entries)
- Options: replicate after golden capture
- Recommendation: golden case SCHEM-NL-15 decides the exact names.
- Resolution: replicate — each distinct instance `schematic=` value produces one extra subcircuit, named after that schematic's cell name; the exact naming is fixed by golden case SCHEM-NL-15. Approved 2026-11-13.

### Q-SCHEM-29: Netlisting rewrites wire `lab` attributes
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: computing connectivity stores each wire's net name (`lab=...`, including `#netN` for unnamed nets) in its property string, and fills missing label `lab` values; the next save writes them, so a netlist run changes the file content without an edit.
- Evidence: netlist.c:1060-1110, netlist.c:929-969
- Options: replicate (spec) | keep computed names out of the file
- Recommendation: replicate for round-trip identity with xschem-saved files, but do not mark the document modified.
- Resolution: replicate — netlisting writes the computed `lab` values into wires and labels, which the next save writes, without marking the document modified, in both modes. The xschem manual documents the behaviour, and PDK files saved by xschem already carry these values. Approved 2026-11-13.
