# Open Questions: SCHEM area (xschem-compatible schematic editor)

These entries cover the capabilities `schematic-file-format`, `schematic-symbols-and-libraries`, `schematic-editing`, `schematic-netlisting`, `schematic-simulation-integration` and `schematic-command`. Product decision: `docs/decisions/product.md#P-12`.

The reference is xschem, not Magic: `StefanSchippers/xschem` @ `4142e6d6f1eb5cd0e271bb183f14202af374ce27` (version `3.4.8RC`, file format `1.3`). "Evidence" paths are relative to the xschem `src/` directory. PDK reference data: `xschem_sky130` @ `2bfd7d65`, `IHP-Open-PDK` @ `5e6d592e` (`ihp-sg13g2/libs.tech/xschem`), `gf180mcu_fd_pr` @ `aacc04cc` (`cells/xschem`), `open_pdks` @ `c6df28c1`. These trees were cloned read-only; nothing was built or run, so every expected netlist text in the specs is derived from the code and must be confirmed by the golden cases in `tests/golden/SCHEM/CASES.md`.

xschem is GPL-2.0-or-later, including its `xschem_library/devices` symbols (they carry the GPL header). The PDK symbol libraries are Apache-2.0. Hefesto is implemented clean-room from the specs.

### Q-SCHEM-1: Version string written by Hefesto
- Kind: ambiguity
- xschem behavior: always writes `v {xschem version=<its own version> file_version=1.3}`, so every save by a different xschem version changes the first line.
- Evidence: save.c:3223-3240, xschem.h:26-27
- Options: replicate (write a pinned xschem compatibility version, `3.4.8RC`) | write the version read from the file (byte-identical round trips of unedited files, but a 1.2 file would claim `file_version=1.3` content with an old version string) | write a Hefesto version (`xschem version=hefesto-1.0`), which xschem ignores.
- Recommendation: pinned compatibility version (spec `schematic-file-format`, Requirement "Writer version string"); bump it only with a re-validation against a newer xschem.

### Q-SCHEM-2: Embedded symbol block quirks
- Kind: quirk
- xschem behavior: the `[ ... ]` block omits the `F` record, writes polygons after texts (the main writer puts them before), and writes the wires of a schematic-used-as-symbol as `L 1` lines. The version line in the block has no header text.
- Evidence: save.c:3010-3078
- Options: replicate | fix (write the block like a normal `.sym`)
- Recommendation: replicate; the block is read back identically either way, and byte-identity with xschem keeps diffs clean.

### Q-SCHEM-3: File version unknown during the first record
- Kind: quirk
- xschem behavior: the file version defaults to `1.0` only after the first record has been processed. A file whose first record is a `C` record (no `v`) therefore does not get `.sym` appended to that first reference, but does for later ones.
- Evidence: save.c:3585-3734 (default set at the end of the loop body), save.c:3346-3350
- Options: replicate | fix (decide the version before reading any record)
- Recommendation: replicate; only hand-written or very old files are affected.

### Q-SCHEM-4: Verilog, VHDL, Spectre and tEDAx netlisters
- Kind: ambiguity
- xschem behavior: netlists to five formats; `G`, `V`, `F`, `E` records and `*_format` attributes feed the non-SPICE ones.
- Evidence: verilog_netlist.c, vhdl_netlist.c, spectre_netlist.c, tedax_netlist.c
- Options: SPICE only (records preserved) | add Verilog (digital flows, P-17) | add Spectre
- Recommendation: SPICE only for P-12; keep all records and attributes round-trip. Revisit Verilog with P-17.

### Q-SCHEM-5: Pin sorting by sim_pinnumber is unstable
- Kind: quirk
- xschem behavior: pins (and top-level ports) are sorted with C `qsort` when every pin has `sim_pinnumber`; equal numbers have an unspecified relative order that depends on the C library.
- Evidence: save.c:3825-3876, save.c:3878-3922
- Options: replicate glibc's order (not portable) | stable sort (file order on ties)
- Recommendation: stable sort; duplicate `sim_pinnumber` values are a user error, so warn about them.

### Q-SCHEM-6: Symbol `drc` scripts
- Kind: platform
- xschem behavior: a symbol's `drc` attribute (for example sky130 `sky130_fet_drc {@name} {@symname} ...`, IHP `fet_drc ...`) names a Tcl procedure that the PDK's `xschemrc` sources (IHP `xschem-drc`) and that the "symbol check" menu runs.
- Evidence: actions.c:825-878; IHP xschemrc:446-450; xschem_sky130 sky130_fd_pr/nfet_01v8.sym
- Options: run in the sandboxed Tcl like `tcleval` | ignore | re-implement the checks natively
- Recommendation: run in the sandbox; the procedures only compute and print.
- Resolution: platform — symbol `drc` procedures run in the sandboxed embedded Tcl interpreter on every platform (X-3, 2026-11-07).

### Q-SCHEM-7: [platform] Tcl dependence of PDK symbols
- Kind: platform
- xschem behavior: PDK symbols evaluate attribute values with Tcl: sky130 `corner.sym` (`tcleval(.lib $::SKYWATER_MODELS/...)`), IHP resistor, capacitor and bipolar formats (`tcleval(... [ev7 {...}] ...)`, `cap_cmomi_C`), IHP testbench code blocks (`format="tcleval( @value )"`, 138 uses in the IHP tree), annotation symbols (`ngspice::get_node`). Without Tcl these netlist wrongly or not at all.
- Evidence: token.c:59-81, xschem.tcl:7224-7298, xschem.tcl:677-698
- Options: embedded sandboxed Tcl on every platform (decision X-3, capability `hefesto-scripting`) | re-implement the used Tcl subset natively | pre-expand PDK symbols at install time (breaks user edits)
- Recommendation: embedded Tcl, as already decided for Magic PDK scripts. If the iPadOS App Store fallback of `hefesto-scripting` applies, iPad needs a native evaluator for the exact constructs found in the three PDKs (`subst` with `[ev ...]`, `[ev7 ...]`, `$::VAR`, `[to_eng ...]`), validated by golden cases SCHEM-NL-07/08.
- Resolution: platform — PDK symbol attributes are evaluated by the sandboxed embedded Tcl interpreter on every platform, iPadOS included; the iPadOS App Store fallback of `hefesto-scripting` covers the rejected-scripts case (X-3, 2026-11-07).

### Q-SCHEM-8: Tcl procedures and variables required by PDK libraries
- Kind: ambiguity
- xschem behavior: `ev`, `ev7`, `ev0`, `to_eng`, `from_eng` and `ngspice::*` are defined in xschem's Tcl code; PDK `xschemrc` files add variables and `source` PDK Tcl files.
- Evidence: xschem.tcl:612-720; IHP xschemrc:390-460
- Options: clean-room re-implementation of exactly the procedures used by the pinned PDKs | broader xschem Tcl API
- Recommendation: inventory script over the three PDK trees (all `tcleval(`, `[`-commands and `$::` variables), then implement that list; keep the inventory as a test.

### Q-SCHEM-9: Web (http) symbol references
- Kind: platform
- xschem behavior: symbol and schematic references starting with `http://` or `https://` are downloaded into a temporary folder.
- Evidence: save.c:4078-4090, save.c:4930-4945, actions.c:2564-2580, xschem.tcl:8804-8806
- Options: support through the platform network service | not support (show as missing symbol)
- Recommendation: not support in v1; no PDK uses it.

### Q-SCHEM-10: Clean-room system library and xschem command inventory
- Kind: ambiguity
- xschem behavior: PDK schematics reference xschem's GPL `devices/` symbols (`res.sym`, `gnd.sym`, `code_shown.sym`, `launcher.sym`, ...) and launchers call `xschem` subcommands.
- Evidence: xschem_library/devices (127 files), scheduler.c (subcommand list)
- Options: re-draw every referenced symbol clean-room with identical netlisting properties | ship xschem's library under GPL (license conflict with P-9) | require users to install xschem's library
- Recommendation: clean-room re-draw of the symbols actually referenced by the three PDKs and their examples, specified by name, pins and properties only; generate the list and the `xschem` subcommand list by an inventory script.

### Q-SCHEM-11: [platform] Symbol generators
- Kind: platform
- xschem behavior: a reference `name(args)` runs the program `name` from the search path and reads its output as a symbol (fork/exec, or `tclsh` on Windows).
- Evidence: token.c:83-182, save.c:4918-4930
- Options: Tcl generators only, in the sandbox | allow external programs on desktop | none
- Recommendation: Tcl only on all platforms (spec); allow external programs on desktop later behind a per-folder trust setting if users need them. iPadOS cannot spawn processes.
- Resolution: platform — symbol generators run only as Tcl in the sandbox on every platform; no external program is executed, since the sandbox forbids process execution (X-3, X-4, 2026-11-07).

### Q-SCHEM-12: `expr()` number lexing swallows minus signs
- Kind: bug
- xschem behavior: inside `expr(...)` a number token is the longest run of `.0-9a-zA-Z_-`, so `expr(2-1)` evaluates to 2 and `expr(1e-3-1)` to 0.001. With spaces (`2 - 1`) it is correct.
- Evidence: eval_expr.y:300-315, editprop.c:536-568
- Options: replicate | fix (stop a number at `-` unless it follows `e`/`E`)
- Recommendation: replicate for netlist identity, and warn when a number token contains an inner `-` not after `e`. PDK templates use spaces.

### Q-SCHEM-13: Absolute paths in netlist comments
- Kind: platform
- xschem behavior: netlists contain `** sch_path: <abs>` and `** sym_path: <abs>` lines with host paths.
- Evidence: spice_netlist.c:329, spice_netlist.c:690-709
- Options: replicate (paths differ per machine, and iPadOS sandbox paths are long and private) | write paths relative to the PDK root or document folder | omit on request
- Recommendation: replicate by default; golden comparisons normalise these lines; offer a "portable paths" option that writes `$PDK_ROOT/...`-relative paths.

### Q-SCHEM-14: Two subcircuits with the same cell name
- Kind: bug
- xschem behavior: definitions are de-duplicated by cell name only (file name without folder and extension), so `libA/inv.sym` and `libB/inv.sym` produce one `.subckt inv`, from whichever is found first. The netlist is silently wrong.
- Evidence: spice_netlist.c:505-512
- Options: replicate | fix (rename the second, e.g. `inv_1`) | replicate and warn
- Recommendation: replicate and warn.

### Q-SCHEM-15: Flat netlist text
- Kind: ambiguity
- xschem behavior: flat netlists are produced by piping the hierarchical netlist through `flatten.awk`, which uppercases most text, uses `_` as path separator, has a fixed table of primitive node counts and does not expand undefined subcircuits.
- Evidence: flatten.awk:36-312
- Options: exact reproduction | electrically equivalent flat netlist (spec) | drop flat mode
- Recommendation: equivalent (behavioral); flat mode is rarely used with PDK flows, which need `.subckt` models anyway.

### Q-SCHEM-16: Shorts at top level are not always reported
- Kind: quirk
- xschem behavior: the short report is suppressed while the netlist counter is 0, i.e. during the first connectivity pass of the top level; top-level shorts appear only from the second pass after the hierarchy has been netlisted, and not at all in `-nohier` runs.
- Evidence: netlist.c:908-927, spice_netlist.c:556-560
- Options: replicate | fix (always report)
- Recommendation: fix: always report shorts; the netlist text is unaffected.

### Q-SCHEM-17: Simulation preparation must accept schematic decks
- Kind: doc-mismatch
- xschem behavior: testbench schematics carry `.control` blocks with `write`, `quit`, `set appendwrite`; capability `simulation-netlist-preparation` (Requirement "Deck validation and sanitisation") rejects `write` and `quit` even with `allowControl`, and Requirement "Circuit sources" has no schematic source.
- Evidence: IHP sg13g2_tests/dc_lv_nmos.sch; tests/SC_Test.spice
- Options: amend `simulation-netlist-preparation` with a `schematic` source and the translation of `schematic-simulation-integration` Requirement "Control block translation" | require users to edit testbenches
- Recommendation: amend `simulation-netlist-preparation` and `simulation-command` (`sim dut schematic <file>`) in a follow-up change.
- Resolution: fix. `simulation-netlist-preparation` now has the `schematic` source, and schematic decks are validated after control-block translation (2026-11-06).

### Q-SCHEM-18: awk dialect differences in xschem's post-processing
- Kind: platform
- xschem behavior: `spice.awk` relies on gawk's `IGNORECASE` for `.param` detection and the `value=` renames. With mawk (Debian/Ubuntu default) or BWK awk (macOS) these matches are case-sensitive, so the same schematic can give different netlists on different hosts.
- Evidence: spice.awk:101-140, spice.awk:238-262
- Options: follow gawk (case-insensitive) | follow mawk (case-sensitive)
- Recommendation: gawk semantics (spec); golden references must be generated with gawk installed as `awk`.

### Q-SCHEM-19: `extra` exclusion uses substring match
- Kind: quirk
- xschem behavior: when copying the template onto the `.subckt` line, a key is dropped if it occurs anywhere inside the `extra` string, so `extra="VPWR"` also drops a parameter `VPW`.
- Evidence: token.c:620-650
- Options: replicate | fix (whole-word match)
- Recommendation: replicate and warn when a dropped key is not a whole word of `extra`.

### Q-SCHEM-20: Line breaker state
- Kind: quirk
- xschem behavior: the quote state of `break.awk` (toggled by `{`, `}` and `'`, with no nesting) persists across lines, so one unbalanced quote disables breaking for later lines; after a break the column counter is reset without counting the field just written.
- Evidence: break.awk:26-75
- Options: replicate | fix
- Recommendation: replicate (it only affects layout of long lines), covered by golden case SCHEM-NL-12.

### Q-SCHEM-21: Multi-line templates break `.subckt` lines
- Kind: bug
- xschem behavior: the template is copied onto the `.subckt` header with its separators, so a template written on several lines (`template="name=x1` LF `W=1"`) puts parameters on new lines without `+`, which SPICE reads as garbage.
- Evidence: token.c:576-668, spice_netlist.c:715-728
- Options: replicate | fix (replace LFs by spaces)
- Recommendation: replicate and warn; check whether any PDK subcircuit symbol has a multi-line template (inventory).

### Q-SCHEM-22: Trailing `m=1` removed everywhere
- Kind: quirk
- xschem behavior: every netlist line, including comments and user code, loses a trailing ` m=1` (any case, spaces around `=`).
- Evidence: spice.awk:94
- Options: replicate | fix (device lines only)
- Recommendation: replicate.

### Q-SCHEM-23: [platform] xschemrc evaluation on iPadOS
- Kind: platform
- xschem behavior: `xschemrc` reads environment variables (`PDK_ROOT`, `PDK`, `HOME`, `PWD`), probes folders with `file isdir`, and sources Tcl files.
- Evidence: xinit.c:3552-3610; IHP and sky130 xschemrc
- Options: provide synthetic `env()` values mapped to the PDK store and the document folder (spec) | parse only `XSCHEM_LIBRARY_PATH` lines
- Recommendation: synthetic environment in the sandbox; the iPad sandbox grants read access to the PDK store only.
- Resolution: platform — `xschemrc` runs in the sandboxed Tcl interpreter with synthetic `env()` values mapped to the PDK store and the document folder, settings replacing environment variables (X-3, X-4, 2026-11-07).

### Q-SCHEM-24: [platform] Touch editing details
- Kind: platform
- xschem behavior: mouse and keyboard only; wire placement is click-click with live preview, polygons end on a double click, and many operations exist only as keys.
- Evidence: keys.help, callback.c
- Options: gesture set in `schematic-editing` Requirement "Desktop and touch interaction" | a floating tool palette | both
- Recommendation: both; usability-test wire drawing with finger vs Pencil before freezing.

### Q-SCHEM-25: Script permission model
- Kind: platform
- xschem behavior: `tcleval` and launcher scripts run only if `xschem_execute_scripts` allows it (`ask` by default, `yes` in the sky130 `xschemrc`), and then run with full Tcl rights.
- Evidence: xschem.tcl:7224-7250, xschem.tcl:12088
- Options: always run `tcleval` in the sandbox and confirm launchers (spec) | replicate the ask/yes/no setting
- Recommendation: as specified; needs a security review together with capability `hefesto-security`.

### Q-SCHEM-26: `@` tokens end only at whitespace
- Kind: quirk
- xschem behavior: a token runs until whitespace, `@`, `%` or an escaped character, so `@W/@L` looks up the key `W/` and gives nothing; users must write `@W\/@L` or `@W / @L`.
- Evidence: token.c:2478-2490, token.c:5370-5380
- Options: replicate | fix (stop at non-identifier characters)
- Recommendation: replicate; changing it would alter PDK netlists that rely on tokens such as `@spice_get_voltage(net)`.

### Q-SCHEM-27: Missing subcircuit schematic
- Kind: ambiguity
- xschem behavior: when the schematic of a `subcircuit` symbol does not exist, the header lines are still written and the body comes from an empty load; the exact text was not derived.
- Evidence: spice_netlist.c:700-735, actions.c:2544-2562
- Options: replicate after golden capture | always error
- Recommendation: capture with golden case SCHEM-NL-14, then replicate and warn.

### Q-SCHEM-28: Instance-based schematic variants
- Kind: ambiguity
- xschem behavior: instances with a `schematic=` attribute create additional symbol entries whose naming and `.subckt` names were not fully derived.
- Evidence: actions.c:2474-2530, spice_netlist.c:470-500 (additional symbol entries)
- Options: replicate after golden capture
- Recommendation: golden case SCHEM-NL-15 decides the exact names.

### Q-SCHEM-29: Netlisting rewrites wire `lab` attributes
- Kind: quirk
- xschem behavior: computing connectivity stores each wire's net name (`lab=...`, including `#netN` for unnamed nets) in its property string, and fills missing label `lab` values; the next save writes them, so a netlist run changes the file content without an edit.
- Evidence: netlist.c:1060-1110, netlist.c:929-969
- Options: replicate (spec) | keep computed names out of the file
- Recommendation: replicate for round-trip identity with xschem-saved files, but do not mark the document modified.
