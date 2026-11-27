# Open Questions: SCHEM area (xschem-compatible schematic editor)

These entries cover the capabilities `schematic-file-format`, `schematic-symbols-and-libraries`, `schematic-editing`, `schematic-netlisting`, `schematic-simulation-integration` and `schematic-command`. Product decision: `docs/decisions/product.md#P-12`.

The reference is xschem, not Magic: `StefanSchippers/xschem` @ `4142e6d6f1eb5cd0e271bb183f14202af374ce27` (version `3.4.8RC`, file format `1.3`). "Evidence" paths are relative to the xschem `src/` directory. PDK reference data: `xschem_sky130` @ `2bfd7d65`, `IHP-Open-PDK` @ `5e6d592e` (`ihp-sg13g2/libs.tech/xschem`), `gf180mcu_fd_pr` @ `aacc04cc` (`cells/xschem`), `open_pdks` @ `c6df28c1`. The gf180 data was re-pinned on 2026-11-17 to `gf180mcu_fd_pr` @ `5e4be872`, the tree the ciel release of M5 installs as `gf180mcuD/libs.tech/xschem` (design D17 of `openspec/changes/m8-schematic`); the gf180 conventions cited in the specs hold in both trees. These trees were cloned read-only; nothing was built or run, so every expected netlist text in the specs is derived from the code and must be confirmed by the golden cases in `tests/golden/SCHEM/CASES.md`.

xschem is GPL-2.0-or-later, including its `xschem_library/devices` symbols (they carry the GPL header). The PDK symbol libraries are Apache-2.0. Hefesto is implemented clean-room from the specs.

**Clean-room process (J-1, approved 2026-11-13).** The `Evidence:` lines below and the `(xschem ref: ...)` traces in the specs were written by the spec side of a two-team process and exist for traceability only. Implementers SHALL NOT follow them into xschem's source, and no xschem source tree is reachable from implementation sessions (capability `hefesto-licensing`, Requirement "Clean-room rule for GPL-sourced behavior"). All 29 entries were decided on 2026-11-13 in `docs/decisions/m8-decision-table.md`. Q-SCHEM-30 was added on 2026-11-18 from a golden capture and approved by the user the same day, together with the amendment of Q-SCHEM-21 (decision table, "Decisions of 2026-11-18"). Q-SCHEM-31 (drafted 2026-11-20), Q-SCHEM-32 (input limits) and Q-SCHEM-33 (fuzzing) were approved by the user on 2026-11-21; Q-SCHEM-34 to -38 were drafted by the spec team on 2026-11-21 from the round-1 spec issues; Q-SCHEM-34 to -37 were approved by the user on 2026-11-25 with the recommended option, in both modes (decision table, "Decisions of 2026-11-25 (Q-SCHEM-34 to -37)"). Q-SCHEM-38 (option (a)) and Q-SCHEM-42, -40 and -41 (input limits M8-D1, M8-D2 and M8-D3, proposed by round 2) were approved by the user on 2026-11-22 (decision table, "Decisions of 2026-11-22"). On 2026-11-23 the user confirmed Q-SCHEM-30 (M8-D4), approved Q-SCHEM-43 (generator time and script size limits, M8-D5 and M8-D6), Q-SCHEM-44 (absolute-path generators, M8-D7) and Q-SCHEM-45 (default `CAD_ROOT`, M8-D10), and recorded clarifications of Q-SCHEM-42 (M8-D8) and Q-SCHEM-10 (M8-D9) (decision table, "Decisions of 2026-11-23"). Q-SCHEM-46 to -51 (proposed 2026-11-24 and 2026-11-25) were approved by the user on 2026-11-25 (decision table, "Proposed 2026-11-24 and 2026-11-25, approved 2026-11-25"). Q-SCHEM-52 to -54 were drafted by the spec team on 2026-11-26 (M8 round 6) and await the user (decision table, "Proposed 2026-11-26 (awaiting the user)").

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
- Verified 2026-11-21 (golden case SCHEM-RT-08, capture run 44239093096; round-1 spec issue F1): xschem saves the leading instance of `b.sch` as `C {res.sym}`, not `C {res}`, because a document whose file version is `1.0` also gets `.sym` added when it is written and when its symbols are looked up. The leading reference stays without `.sym` only in the model (what `xschem getprop instance R1 cell::name` returns). The spec now says so ("Version record and header text", scenarios "Leading instance record, xschem-compat ON/OFF"): the saved files are identical in both modes. The `Resolution:` is unchanged; it governs only the stored reference.

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
- Verified 2026-11-21 (golden case SCHEM-RT-02, IHP `nmoscl_2.sym` and `nmoscl_4.sym`; round-1 spec issue F2): xschem also sorts a document's layer-5 boxes by `sim_pinnumber` each time it saves, when every box has a non-empty one, and the new order stays in memory. Recorded in `schematic-file-format` "Canonical write order" (paragraph "Pin boxes on save") with the same stable sort; the `Resolution:` is unchanged.

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
- Location (M8-D9, recorded 2026-11-23): the system library lives at `assets/schematic/xschem_library/devices/` in the source tree and the install, under `${XSCHEM_SHAREDIR}` = `<CAD_ROOT>/assets/schematic`, the folder the PDK `xschemrc` files append (`openspec/changes/m8-schematic` design D3 and D8, `docs/contributing/cleanroom.md`). Q-SCHEM-45 makes `CAD_ROOT` default to the install root.

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
- Verified 2026-11-18 (golden case SCHEM-NL-16, capture run 44239093096, task 3.6 of `openspec/changes/m8-schematic`): with `libA/inv.sym` and `libB/inv.sym` both used, xschem writes `x1 A B inv` and `x2 A B inv` and one definition, `* expanding   symbol:  libA/inv.sym # of pins=2`, `** sym_path: .../libA/inv.sym`, `** sch_path: .../libA/inv.sch`, `.subckt inv A Y`, the body of `libA/inv.sch`, `.ends`; it prints no warning. The ON text is recorded in `schematic-netlisting` "Subcircuit definitions", scenario "Same cell name from two libraries". The `Resolution:` is unchanged.

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
- xschem behavior: a short at the top level is not reported when the top level's connectivity is first computed; it is reported only when the top level is processed again after the hierarchy below it has been netlisted, and not at all in `-nohier` runs. (Reworded as behaviour by the paraphrase review of 2026-11-17.)
- Evidence: netlist.c:908-927, spice_netlist.c:556-560
- Options: replicate | fix (always report)
- Recommendation: fix: always report shorts; the netlist text is unaffected.
- Resolution: fix — shorts are always reported, including top-level shorts on the first pass and in `-nohier` runs. The netlist text is unchanged; only diagnostics differ. Approved 2026-11-13.
- Answered 2026-11-21 (round-1 spec issue N5, from xschem's source by the spec team): the six short tags and the situation each one names are tabled in `schematic-netlisting` "Net naming order". The `Bus tap` tag is the subject of Q-SCHEM-37 (approved 2026-11-25). The `Resolution:` is unchanged.

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
- xschem behavior: xschem post-processes its netlist with the system `awk`. Under gawk the `.param` detection and the `value=` renames match case-insensitively; with mawk (Debian/Ubuntu default) or BWK awk (macOS) they are case-sensitive, so the same schematic can give different netlists on different hosts. (Reworded as behaviour by the paraphrase review of 2026-11-17.)
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
- xschem behavior: when wrapping long lines, a quote opened by `{`, `}` or `'` (no nesting) stays open across lines, so one unbalanced quote stops later lines from wrapping; after a wrap, the field written after the `+` does not count toward the new line's length. (Reworded as behaviour by the paraphrase review of 2026-11-17.)
- Evidence: break.awk:26-75
- Options: replicate | fix
- Recommendation: replicate (it only affects layout of long lines), covered by golden case SCHEM-NL-12.
- Resolution: replicate — when wrapping long lines, a quote stays open across lines and the field written after a `+` does not count toward the new line's length, as xschem does, in both modes; only where long lines wrap changes. Golden case SCHEM-NL-12. Approved 2026-11-13; reworded 2026-11-20 (meaning unchanged, approved by the maintainer; second paraphrase review).

### Q-SCHEM-21: Multi-line templates break `.subckt` lines
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13; OFF-mode amendment drafted 2026-11-17, approved by the user 2026-11-18)
- xschem behavior: the template is copied onto the `.subckt` header with its separators, so a template written on several lines (`template="name=x1` LF `W=1"`) puts parameters on new lines without `+`, which SPICE reads as garbage.
- Evidence: token.c:576-668, spice_netlist.c:715-728
- Options: replicate | fix (replace LFs by spaces)
- Recommendation: compat. With `xschem-compat` ON, copy the template's LFs onto the `.subckt` header as xschem does. With it OFF, replace each LF by a space, so the header is valid SPICE. Both modes warn. The inventory checks whether any PDK subcircuit symbol has a multi-line template. (corrected 2026-11-13, was replicate and warn)
- Resolution: compat — with `xschem-compat` ON the template's LFs are copied onto the `.subckt` header; with it OFF each LF becomes a space, except an LF followed by optional blanks and `+`, which is kept so a SPICE continuation line stays one; both modes warn. The PDK inventory reports any subcircuit symbol with a multi-line template. The exact ON text is fixed by the golden captures. Approved 2026-11-13; OFF rule amended (the `+` exception) and approved by the user 2026-11-18 (`schematic-netlisting` "Subcircuit definitions", scenario "Template continuation lines, xschem-compat OFF"). ON is unchanged.
- Verified 2026-11-17 (inventory, task 2.5 of `openspec/changes/m8-schematic`): three PDK subcircuit symbols have a multi-line `template`, all in sky130 (`tests/data/schem/inventory.json`, `multiline_templates`); gf180mcu and IHP have none. So the two modes differ on PDK data. `sky130_tests/passgate.sym` (used by `passgate.sch`'s testbenches, `bandgap.sch`, `gain_stage.sch`, `tb_bandgap_opamp.sch`) continues its template on a line without `+` (`... VSSBPIN=VSS` LF `modeln=nfet_01v8 modelp=pfet_01v8 m=1`), the case this entry describes; SCHEM-NL-03 captures its ON text, so no Hefesto-authored case is needed for it. `sky130_tests/not.sym` and `lvtnot.sym` (used by `test_inv.sch`, `logic_ngspice.sch`, `test_ff.sch` and others) continue theirs with SPICE `+` continuation lines (`name=x1 m=1` LF `+ W_N=1 ...` LF `+ VCCPIN=VCC VSSPIN=VSS`): with `xschem-compat` ON the header stays valid SPICE, while the OFF rule as written (each LF becomes a space) would put bare `+` words inside the header line. Raised to the user as a proposed amendment of the OFF rule (keep an LF that is followed by optional blanks and `+`); approved by the user 2026-11-18 and folded into the `Resolution:` above.
- Proposed amendment (status: proposed, drafted 2026-11-17, not approved): with `xschem-compat` OFF, an LF of the template that is followed by optional spaces or tabs and a `+` is kept, since it already starts a SPICE continuation line; every other LF becomes a space. ON is unchanged, and both modes still warn. Effect on PDK data: `not.sym` and `lvtnot.sym` headers are the same in both modes; `passgate.sym` differs as before.
- Verified 2026-11-18 (golden case SCHEM-NL-03, capture run 44239093096, task 3.6): the ON text of `passgate.sym` in `gain_stage.sch` is the header line `.subckt passgate Z A GP GN VCCBPIN VSSBPIN  W_N=1 L_N=0.2 W_P=1 L_P=0.2` followed by an empty line: the text after the template's LF is its own line, and the netlist post-processing empties it (the `extra` keys `modeln`/`modelp` removed, then the trailing ` m=1`, then the trailing spaces). xschem prints no warning. Recorded in `schematic-netlisting` "Subcircuit definitions", scenario "Multi-line template, xschem-compat ON". The ON text is unaffected by the amendment of 2026-11-18; the `+` form of `not.sym`/`lvtnot.sym` is not captured as a header (no case netlists them hierarchically yet).

### Q-SCHEM-22: Trailing `m=1` removed everywhere
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: every netlist line, including comments and user code, loses a trailing ` m=1` (any case, spaces around `=`).
- Evidence: spice.awk:94
- Options: replicate | fix (device lines only)
- Recommendation: compat. With `xschem-compat` ON, remove a trailing ` m=1` from every line, as xschem does. With it OFF, remove it only from device lines (outside user-code blocks, first field not starting with `*` or `.`), keeping comments, `.param` and `.control` text unchanged. On device lines `m=1` is the default, so removing it changes nothing. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `xschem-compat` ON a trailing ` m=1` is removed from every line; with it OFF only from device lines, so comments, `.param` and `.control` text keep it. The exact texts are fixed by the golden captures. Approved 2026-11-13.
- Verified 2026-11-18 (golden case SCHEM-NL-01, capture run 44239093096, task 3.6): with ON, the user-code line `.param m=1` of a `code_shown.sym` instance is written `.param`, and the device lines `R1 VIN VOUT 1k` and `C1 VOUT 0 1n` lose their `m=1`. Recorded in `schematic-netlisting` "Element line post-processing", scenario "Trailing m=1 in a testbench, xschem-compat ON". The `Resolution:` is unchanged.

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
- xschem behavior: when the schematic of a `subcircuit` symbol does not exist, the header lines are still written and the body is empty; the exact text was not derived. (Reworded as behaviour by the second paraphrase review of 2026-11-20.)
- Evidence: spice_netlist.c:700-735, actions.c:2544-2562
- Options: replicate after golden capture | always error
- Recommendation: capture with golden case SCHEM-NL-14, then replicate and warn.
- Resolution: replicate — a `subcircuit` symbol whose schematic is missing gets its header lines and an empty body, and a warning, as `schematic-netlisting` says; the exact text is fixed by golden case SCHEM-NL-14. Approved 2026-11-13.
- Verified 2026-11-18 (golden case SCHEM-NL-14, capture run 44239093096, task 3.6): for `missing.sym` with no `missing.sch`, xschem writes `* expanding   symbol:  missing.sym # of pins=2`, `** sym_path: .../missing.sym`, `** sch_path: .../missing.sch`, `.subckt missing A B`, `.ends` and an empty line, prints `load_schematic(): unable to open file: .../missing.sch, ...` on stderr and exits 0. The same case confirms `spice_sym_def` (its text and a LF, nothing else), `spice_stop` (header and `.ends`, no body) and `default_schematic=ignore` (no definition, instance line kept). Recorded in `schematic-netlisting` "Schematic resolution". The `Resolution:` is unchanged. Not captured: with `spice_sym_def=".include x.spice"` xschem also tries to open `x.spice` (stdout `has_included_subcircuit: error opening file ...`); what it does with that file when it exists is not observed.

### Q-SCHEM-28: Instance-based schematic variants
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: instances with a `schematic=` attribute create additional symbol entries whose naming and `.subckt` names were not fully derived.
- Evidence: actions.c:2474-2530, spice_netlist.c:470-500 (additional symbol entries)
- Options: replicate after golden capture
- Recommendation: golden case SCHEM-NL-15 decides the exact names.
- Resolution: replicate — each distinct instance `schematic=` value produces one extra subcircuit, named after that schematic's cell name; the exact naming is fixed by golden case SCHEM-NL-15. Approved 2026-11-13.
- Verified 2026-11-18 (golden cases SCHEM-NL-15 and SCHEM-NL-03, capture run 44239093096, task 3.6): for `x2` of `amp.sym` with `schematic=amp_fast.sch`, xschem writes the instance line `x2 A B amp_fast` and, after the `amp` definition, `* expanding   symbol:  amp_fast.sym # of pins=2`, `** sym_path: .../amp.sym` (the original symbol), `** sch_path: .../amp_fast.sch`, `.subckt amp_fast IN OUT` and the body of `amp_fast.sch`. In sky130 `gain_stage.sch`, `x6` of `sky130_tests/passgate.sym` has `schematic=passgate_1` and no such file exists: xschem writes `passgate_1` on the instance line and a definition `passgate_1.sym` whose `** sch_path:` is `passgate.sch`, whose body takes `x6`'s `modelp=pfet_01v8_lvt`, and it still writes the plain `passgate` definition. Recorded in `schematic-netlisting` "Schematic resolution". The `Resolution:` is unchanged. Not captured: whether the fallback to the symbol's own schematic comes from the missing file or from the value having no extension.

### Q-SCHEM-29: Netlisting rewrites wire `lab` attributes
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- xschem behavior: computing connectivity stores each wire's net name (`lab=...`, including `#netN` for unnamed nets) in its property string, and fills missing label `lab` values; the next save writes them, so a netlist run changes the file content without an edit.
- Evidence: netlist.c:1060-1110, netlist.c:929-969
- Options: replicate (spec) | keep computed names out of the file
- Recommendation: replicate for round-trip identity with xschem-saved files, but do not mark the document modified.
- Resolution: replicate — netlisting writes the computed `lab` values into wires and labels, which the next save writes, without marking the document modified, in both modes. The xschem manual documents the behaviour, and PDK files saved by xschem already carry these values. Approved 2026-11-13.

### Q-SCHEM-30: Relative references follow xschem's start folder
- Kind: platform
- Status: approved (recommendation drafted 2026-11-18 from golden case SCHEM-PR-05, approved by the user 2026-11-18)
- xschem behavior: symbol references starting with `./` or `../`, and the element `.` of `XSCHEM_LIBRARY_PATH`, are resolved against the folder xschem was started in, not against the folder of the schematic that contains the instance. In SCHEM-PR-05 (started in `/work`, loading `/work/input/sub/top.sch`) `./x.sym` resolves to `/work/x.sym`, `../x.sym` to `//x.sym`, and the instance `../x.sym` of `top.sch` is reported as `Symbol not found` although `/work/input/x.sym` exists. The same schematic therefore resolves differently depending on where xschem is started.
- Evidence: golden case SCHEM-PR-05 (`tests/golden/SCHEM/cases/SCHEM-PR-05/expected/stdout.txt`, `stderr.txt`), capture run 44239093096; black-box observation only.
- Options: replicate (resolve against the process working folder; on iPadOS there is none, so a substitute folder would be needed) | fix (resolve against the folder of the containing schematic, as `schematic-symbols-and-libraries` "Symbol references" step 5 says today) | compat (replicate with `xschem-compat` ON, using the folder Hefesto was started in on desktop and the document's folder on iPadOS; fix with OFF)
- Recommendation: fix in both modes: resolve `./`, `../` and `.` against the folder of the containing schematic, and warn when a `./` or `../` reference is resolved, since xschem may resolve it elsewhere. A document then opens the same way on every platform and from every start folder, and xschem started in the schematic's folder, the usual case, gives the same result. SCHEM-PR-05 gets a recorded deviation for its `./` and `../` lines.
- Spec effect: `schematic-symbols-and-libraries` "Symbol references" (step 5 and the "Working folder" paragraph, scenario "Relative reference resolved against the schematic folder") and "Library search path" (the `.` element, scenario "Dot element is the schematic folder"); golden deviation for SCHEM-PR-05 in `tests/golden/deviations.toml` (`stdout.txt`, `stderr.txt`).
- Resolution: fix — in both modes of `xschem-compat`, `./` and `../` references and the search-path element `.` are resolved against the folder of the schematic that contains the instance, never against the folder Hefesto was started in; each reference resolved that way reports, once, the warning `SCHEM-LIB-001` `Relative symbol reference <reference> resolved against the schematic folder <folder>; xschem resolves it against its start folder`. SCHEM-PR-05 has an approved deviation for its `./` and `../` lines. Approved by the user 2026-11-18.
- Confirmation (M8-D4, approved 2026-11-23): the user confirmed the resolution; the round-3 fixes follow it, so a document that has not been saved (no folder) resolves no `./` or `../` reference and no `.` element: such a reference is a missing symbol (`SCHEM-LIB-003`).

### Q-SCHEM-31: `netlist_options.sym` is outside the system-library inventory
- Kind: ambiguity
- Status: approved 2026-11-21 (drafted 2026-11-20 by the spec team, task 4.1 of `openspec/changes/m8-schematic`; user decision H1 of 2026-11-21)
- xschem behavior: `devices/netlist_options.sym` is a symbol of type `netlist_options` with no pins and no `format`; its instance's keys set netlister options (`schematic-netlisting` "Netlist options").
- Evidence: golden case SCHEM-NL-17 (`tests/golden/SCHEM/cases/SCHEM-NL-17/input/nl17.sch` places `netlist_options.sym`); `tests/data/schem/inventory.json` (no PDK of the three lists it); xschem_library/devices/netlist_options.sym.
- Options: ship it (the system library holds the inventoried symbols plus the symbols the golden suite places, today only `netlist_options.sym`) | inventory only (Q-SCHEM-10 as written; SCHEM-NL-17 then embeds or draws its own symbol, and its input changes)
- Recommendation: ship it. `netlist_options` is a type the netlister must support anyway (`schematic-netlisting` "Netlist options"), the symbol has no pins and no graphics beyond a line and two texts, and changing SCHEM-NL-17's input would need a recapture. The interface table of `schematic-symbols-and-libraries` "System library" already lists it, marked as outside the inventory, and `scripts/tests/test_devices_interface.py` allows exactly this one extra row.
- Spec effect: `schematic-symbols-and-libraries` "System library" (the sentence "The library SHALL contain only the symbols that the three PDK libraries and their example schematics reference" gains "and the symbols the golden suite places"); design D8 step 3 (the DEVLIB test's "exactly the inventoried symbols"); Q-SCHEM-10's Resolution is extended, not changed.
- Resolution: other — ship `netlist_options.sym` in the system library: the library holds the symbols the three PDKs reference plus the symbols the golden suite places, today only `netlist_options.sym` (`schematic-symbols-and-libraries` "System library"; Q-SCHEM-10 extended, not changed). Approved by the user 2026-11-21 (H1).

### Q-SCHEM-32: Input limits of the schematic readers
- Kind: platform
- Status: approved 2026-11-21 (proposed by round 1 of `openspec/changes/m8-schematic`, `docs/contributing/security.md` "Schematic input"; user decision H2 of 2026-11-21)
- xschem behavior: none of these inputs is bounded. Deeply nested embedded blocks, a schematic used as a symbol that places the next one twice per level (2^40 records), an `xschemrc` with `while 1 {}`, a 64 KB name with `[1048575:0]`, deep parentheses in bus names and in `expr()` make xschem run out of memory, hang or overflow its stack.
- Evidence: round-1 fuzzing and review (`docs/contributing/security.md` "Schematic input"); spec issues F4, Y5 of round 1.
- Options: cap each input, the same in both modes, each with a message and a regression test | leave unbounded (as xschem)
- Recommendation: cap, both modes.
- Resolution: platform — in both modes of `xschem-compat`, each with a message and a regression test: expanded bus text at most 64 MiB per name (`kMaxBusBytes`, 67,108,864 bytes) and at most 2^20 names (`kMaxBusBits`), bus parentheses at most 256 deep (`schematic-netlisting` "Bus and vector notation"); `expr` nesting at most 200 levels (`kMaxExprDepth`, "expr evaluation"); embedded-block nesting at most 64 levels (`schematic-file-format` "Embedded symbols"); a schematic used as a symbol at most 1,000,000 records when flattened (`schematic-symbols-and-libraries` "Schematic used as symbol"); `xschemrc` evaluation at most 10 s per document (`SandboxLimits`, "xschemrc as data"). Approved by the user 2026-11-21 (H2).

### Q-SCHEM-33: Fuzzing duration of the schematic fuzz targets
- Kind: other
- Status: approved 2026-11-21 (raised by task 6.4 of `openspec/changes/m8-schematic`, which asks for a 10-minute run per target; user decision H3 of 2026-11-21)
- xschem behavior: not applicable (Hefesto process).
- Evidence: tasks.md 6.4; the CI `fuzz` job runs each target for 60 s.
- Options: keep 60 s in pull-request CI and add a scheduled nightly run of 10 minutes per target | 10 minutes in pull-request CI | 60 s only
- Recommendation: keep 60 s per target in pull-request CI and add a nightly scheduled workflow.
- Resolution: other — pull-request CI keeps 60 s per fuzz target; a scheduled nightly workflow runs each fuzz target for 10 minutes, and that run satisfies the 10-minute run of task 6.4. Approved by the user 2026-11-21 (H3).

### Q-SCHEM-34: Out-of-range integers in schematic files
- Kind: quirk
- Status: approved 2026-11-25 (drafted 2026-11-21 by the spec team from round-1 spec issues F5 and Y1; user decision of 2026-11-25)
- xschem behavior: layer, polygon count, rotation and flip are read with C's integer scan (rotation and flip into 16-bit integers), and `sim_pinnumber` with `atoi`; a value that does not fit is undefined behaviour in C. A negative polygon count is passed to the allocator, which is undefined too. Values that fit are stored as read: flip 2 and rotation 5 are kept and written back.
- Evidence: save.c (instance, text and polygon readers; pin sorting on save); xschem.h (16-bit rotation and flip fields).
- Options: fix (a layer, count, rotation or flip that does not fit a signed 32-bit integer, a rotation or flip outside -32768..32767, and a negative polygon count make the record skipped as a missing field; a `sim_pinnumber` value outside the 32-bit range is clamped) | replicate glibc's wrap-around for 16-bit fields (not portable, platform-dependent)
- Recommendation: fix, in both modes. xschem has no defined result to match (the standing rule for undefined input), the records concerned are hostile or corrupt, and skipping them is the existing malformed-field rule.
- Spec effect: `schematic-file-format` "Record grammar" (rules "Out-of-range integers" and "Polygon count", scenarios "Polygon counts", "Integer that does not fit"); `schematic-symbols-and-libraries` "Pin order" (the integer value of `sim_pinnumber`).
- Resolution: fix, both modes: a layer, polygon count, rotation or flip that does not fit a signed 32-bit integer, a rotation or flip outside -32768..32767, and a negative polygon count make the record skipped as a missing field (the existing malformed-field rule); a `sim_pinnumber` value outside the 32-bit range is clamped to it. Approved by the user 2026-11-25 with the recommended option.

### Q-SCHEM-35: Bus notation counts that xschem does not handle
- Kind: quirk
- Status: approved 2026-11-25 (drafted 2026-11-21 by the spec team from round-1 spec issue N1; user decision of 2026-11-25)
- xschem behavior: a repetition count of 0 (`a*0`, `0*a`) gives an empty list of width 0; a negative count reads undefined memory; a range `step` of 0 or below, or a stepped range whose ends are equal, loops forever; a `rep` of 0 or below leaves an undefined index list.
- Evidence: expandlabel.y (multiplier and range actions); parselabel.l (integers may be negative). Round-1 CI fuzzing found that an empty expansion crashed Hefesto's naming stage.
- Options: fix (all of these are syntax errors, the string is used unchanged with unknown width, except that a stepped range with equal ends gives its one element) | replicate the defined case (`a*0` and `0*a` give an empty list of width 0) and fix the undefined ones
- Recommendation: fix, in both modes. An empty net or pin name has no SPICE meaning, no PDK uses a count of 0, and the undefined cases have no xschem result.
- Spec effect: `schematic-netlisting` "Bus and vector notation" (scenarios "Zero and negative counts are syntax errors", "Stepped range with equal ends").
- Resolution: fix, both modes: a repetition count of 0 or below, a range `step` of 0 or below and a `rep` of 0 or below are syntax errors (the string is used unchanged, with unknown width); a stepped range with equal ends is its one element. Approved by the user 2026-11-25 with the recommended option.

### Q-SCHEM-36: `%` with a zero divisor and the text of NaN in `expr`
- Kind: quirk
- Status: approved 2026-11-25 (drafted 2026-11-21 by the spec team from round-1 spec issue N2; user decision of 2026-11-25)
- xschem behavior: `%` converts both operands to C `int`; a right operand that truncates to 0 is an integer division by zero (a crash on most hosts), and operands outside the 32-bit range are undefined. A NaN result is printed by the C library: `nan`, or `-nan` on the GNU C library when the sign bit is set, and whether 0/0 sets it depends on the processor. `expr_eng` prints NaN as `nana` or `-nana` and infinity as `infT`.
- Evidence: eval_expr.y (`%`, result printing); editprop.c (engineering formatter).
- Options: (a) fix: `%` is fmod(trunc(a), trunc(b)) in binary64 (C's `%` wherever xschem has a result; NaN for a zero divisor), and NaN always prints `nan` (`nana` with `expr_eng`) | (b) as (a), but print `-nan` when the sign bit is set and define the sign of every NaN Hefesto makes, matching x86-64 Linux | (c) make a zero divisor an `expr` error (text copied without the marker)
- Recommendation: (a), in both modes. No PDK uses either case, and (a) gives the same netlist on every platform (D20).
- Spec effect: `schematic-netlisting` "expr evaluation" (the `%` and NaN bullets, scenario "Remainder by zero and NaN text").
- Resolution: fix (a), both modes: `%` is fmod(trunc(a), trunc(b)) in binary64 (C's `%` wherever xschem has a result; NaN for a zero divisor), and NaN always prints `nan` (`nana` with `expr_eng`). Approved by the user 2026-11-25 with the recommended option.

### Q-SCHEM-37: `Bus tap` shorts when the tap side is already named
- Kind: quirk
- Status: approved 2026-11-25 (drafted 2026-11-21 by the spec team from round-1 spec issue N4; user decision of 2026-11-25)
- xschem behavior: when the tap pin of a `bus_tap` already has a non-automatic name, it is kept, and xschem reports `Error: Bus tap shorted: <bus net> - <tap pin name>` whenever the two texts differ. A bus name and a bit name nearly always differ, so a tap whose bit wire carries the matching label (`DATA[3]` on a tap `[3]` of `DATA[3:0]`) is reported as a short.
- Evidence: netlist.c (`bus_tap` branch of the instance check).
- Options: fix (report the `Bus tap` short only when the tap name computed from `lab` differs from the name the tap pin already has, naming those two) | replicate (report whenever the bus net and the tap pin name differ)
- Recommendation: fix, in both modes. The netlist text is the same either way; Q-SCHEM-16 makes every short fail the run, so the replicated report would fail correct schematics.
- Spec effect: `schematic-netlisting` "Bus taps" (scenario "Tap side already named").
- Resolution: fix, both modes: the `Bus tap` short is reported only when the tap name computed from `lab` differs from the name the tap pin already has, and the message names those two. Approved by the user 2026-11-25 with the recommended option.

### Q-SCHEM-38: Environment, relative library elements and unknown commands in `xschemrc`
- Kind: platform
- Status: approved 2026-11-22 (drafted 2026-11-21 by the spec team from round-1 spec issues Y2 and Y6; user decision of 2026-11-22)
- xschem behavior: `env(HOME)` and `env(PWD)` are the user's home folder and xschem's start folder; relative search-path elements other than `.` (`lib`, `../lib`) are checked and used against the start folder; an unknown command in an `xschemrc` is a Tcl error that stops the rest of the file. The search path itself expands only a leading `~`; `$NAME` references are expanded by Tcl when the rc file is evaluated.
- Evidence: xschem.tcl (search-path construction and path cleanup); golden case SCHEM-PR-05 (start-folder resolution, Q-SCHEM-30).
- Options: (a) `env(PWD)` and `env(HOME)` are the document's folder; relative elements other than `.` resolve against the document's folder with warning `SCHEM-LIB-001` (as Q-SCHEM-30 does for `.`); unknown commands raise Tcl's error except the GUI-only commands the three PDK `xschemrc` files call, which are accepted and do nothing | (b) as (a), but `env(HOME)` is a Hefesto per-user folder (desktop) or the app container (iPadOS) | (c) as (a), but every unknown command is accepted and ignored (round 1's interim behaviour)
- Recommendation: (a), in both modes: the document opens the same way on every machine and start folder, only granted folders are visible, and a typo in an rc file is reported instead of silently ignored. User libraries belong in the Hefesto search-path setting, not in `~`.
- Spec effect: `schematic-symbols-and-libraries` "Library search path" (relative elements; scenario "Relative library element resolved against the document folder") and "xschemrc as data" (environment, unknown commands; scenarios "Synthetic HOME and PWD", "Unknown command in an xschemrc", "GUI-only command in a PDK xschemrc").
- Resolution: platform — option (a) in both modes of `xschem-compat`: `env(PWD)` and `env(HOME)` are the document's folder; relative search-path elements other than `.` resolve against the document's folder with warning `SCHEM-LIB-001` (extends Q-SCHEM-30); an unknown command in an `xschemrc` raises Tcl's error (reported as `SCHEM-LIB-006`, earlier settings kept) except the GUI-only commands the three PDK `xschemrc` files call (the `rc_commands` of `tests/data/schem/inventory.json` that are GUI-only), which are accepted and do nothing. Approved by the user 2026-11-22.

### Q-SCHEM-39: SCHEM-LVS-01 needs the LVS engine
- Kind: platform
- Status: approved 2026-11-22 (proposed 2026-11-22 by the M9 plan, `openspec/changes/m9-lvs-simulation` design D16; approved by the user 2026-11-22, `docs/decisions/m9-decision-table.md`)
- xschem behavior: n/a. This is a question about when a golden case is scheduled.
  - SCHEM-LVS-01 runs netgen with `sky130A_setup.tcl` to compare Magic's extracted `sky130_fd_sc_hd__inv_1` against `inv_1.sch`.
  - On the Hefesto side the case writes `lvs.out`. That needs the `lvs-*` capabilities, which design D13 of m8-schematic places in M9.
  - `tests/golden/deviations.toml` marks the case `deferred = "M8"`, so the M8 exit (m8 design D21) would have to wait for M9 code.
- Evidence: `tests/golden/deviations.toml` (`[SCHEM-LVS-01]`); `tests/golden/SCHEM/cases/SCHEM-LVS-01/`; `openspec/changes/m8-schematic/design.md` D13, D21
- Options: (a) move SCHEM-LVS-01 to `deferred = "M9"`, as SCHEM-SIM-01/02 were moved on 2026-11-18; M8 keeps checking the case's `inv_1.spice` netlist output, and M9 makes the whole case pass; (b) keep the case in M8 and let the M8 exit wait for M9's LVS; (c) split it into a netlist case for M8 and an LVS case for M9.
- Recommendation: (a). The reasoning is the same as for the SCHEM-SIM decision, and no captured file changes.
- Resolution: other — option (a): SCHEM-LVS-01 is `deferred = "M9"` in `tests/golden/deviations.toml`, as SCHEM-SIM-01/02 were; M8 keeps checking the case's `inv_1.spice` netlist output, and M9 makes the whole case pass. No captured file changes. Approved by the user 2026-11-22. As implemented: the entry carries `deferred_outputs = { "inv_1.spice" = "M8" }` (`schem_cases.LATER_OUTPUTS`), and `replay.py --milestone M8` fails the case at the M8 exit when `inv_1.spice` differs.

### Q-SCHEM-40: Byte size of a schematic used as a symbol
- Kind: platform
- Status: approved 2026-11-22 (proposed by round 2 of `openspec/changes/m8-schematic`; user decision M8-D2 of 2026-11-22)
- xschem behavior: not bounded (Q-SCHEM-32). The 1,000,000-record cap of Q-SCHEM-32 bounds the record count, but records can be large: polygons with many points and long property strings let 1,000,000 records hold many gigabytes.
- Evidence: round-2 review of the schematic-as-symbol flattener (`docs/contributing/security.md` "Schematic input", row "Schematic used as a symbol: flattened size").
- Options: add a byte cap beside the record cap, both modes, same warning | record cap only
- Recommendation: add the byte cap, both modes, reported with the record cap's code.
- Spec effect: `schematic-symbols-and-libraries` "Schematic used as symbol" (paragraph "Flattened size", scenario "Flattened symbol larger than 256 MiB"); `docs/contributing/security.md` "Schematic input".
- Resolution: platform — in both modes of `xschem-compat`, besides the 1,000,000-record cap of Q-SCHEM-32, the flattened symbol of a schematic used as a symbol is capped at about 256 MiB (268,435,456 bytes), estimated as 16 bytes per point plus the size of every string; past it no further record is added and `SCHEM-LIB-009` is reported (the same code as the record cap); regression test. Approved by the user 2026-11-22.

### Q-SCHEM-41: Total bus expansion per netlist run or editing session
- Kind: platform
- Status: approved 2026-11-22 (proposed by round 2 of `openspec/changes/m8-schematic`; user decision M8-D3 of 2026-11-22)
- xschem behavior: not bounded (Q-SCHEM-32). The per-name caps of Q-SCHEM-32 bound one expansion, but a schematic with many names each just under `kMaxBusBytes` still expands to an unbounded total.
- Evidence: round-2 review of bus notation (`docs/contributing/security.md` "Schematic input", row "Bus notation: expanded text size").
- Options: cap the total expanded text per netlist run and per editing session, both modes, with the per-name warning | per-name caps only
- Recommendation: cap the total at 64 × `kMaxBusBytes`, both modes.
- Spec effect: `schematic-netlisting` "Bus and vector notation" (paragraph "Input limits", scenario "Total bus expansion limit"); `docs/contributing/security.md` "Schematic input".
- Resolution: platform — in both modes of `xschem-compat`, the expanded bus text of one netlist run, or of one editing session, is capped in total at 64 × `kMaxBusBytes` = 4,294,967,296 bytes (4 GiB); a name whose expansion would pass the total is used unchanged with `SCHEM-NET-006` (as a single name over `kMaxBusBytes` is today); regression test. Approved by the user 2026-11-22.

### Q-SCHEM-42: Size of a flat netlist
- Kind: platform
- Status: approved 2026-11-22 (proposed by round 2 of `openspec/changes/m8-schematic`; user decision M8-D1 of 2026-11-22)
- xschem behavior: flattening is not bounded. A subcircuit body that places itself is substituted without end, and a deep or wide hierarchy gives a flat netlist of any size (as for the other unbounded inputs of Q-SCHEM-32).
- Evidence: round-2 review of `flat` mode (`docs/contributing/security.md` "Schematic input", row "Flat netlist: subcircuit nesting"; proposed `schem::kMaxFlatDepth` = 64 with a regression test).
- Options: cap the nesting depth and the output size, the same in both modes, with a warning and a regression test per limit | cap the depth only | leave unbounded (as xschem)
- Recommendation: cap both, both modes; the run is not failed, the warning names the limit.
- Spec effect: `schematic-netlisting` "Flat netlist" (scenarios "Flat netlist deeper than 64 levels", "Flat netlist larger than 256 MiB"); `docs/contributing/security.md` "Schematic input"; new warning `SCHEM-NET-017` in `docs/diagnostics/codes/schem.md`.
- Resolution: platform — in both modes of `xschem-compat`, flattening a hierarchy (`schematic-netlisting` "Flat netlist") stops at `kMaxFlatDepth` = 64 levels and when the flat output passes 256 MiB (268,435,456 bytes); flattening stops there with the warning `SCHEM-NET-017` (`flat netlist stopped: <depth limit of 64 levels | output passed 268435456 bytes>`); the run is not failed; a regression test per limit. Approved by the user 2026-11-22.
- Clarification (M8-D8, approved 2026-11-23): the flat output's byte count is, for each kept line, the bytes of its text plus 1 for its LF plus `kFlatLineOverhead` = 40 bytes for its in-memory record; a substituted instance, which is not kept, counts its instance line the same way. The limit therefore bounds memory as well as text (`schematic-netlisting` "Flat netlist", paragraph "Size").

### Q-SCHEM-43: Total time and script size of symbol generators
- Kind: platform
- Status: approved 2026-11-23 (proposed by the M8 round-3 review fix of `openspec/changes/m8-schematic`; user decisions M8-D5 and M8-D6 of 2026-11-23)
- xschem behavior: not bounded (as the other inputs of Q-SCHEM-32). Each generator reference runs a program; a schematic with thousands of distinct generator references, each running up to the 10 s per-run limit, would block an opening for hours, and a generator file of any size is read to check its first line.
- Evidence: M8 round-3 review of the symbol generators (`schematic-symbols-and-libraries` "Symbol generators", paragraph "Limits").
- Options: a total time per symbol cache and a script size cap, both modes, reported with `SCHEM-LIB-012` | per-run limit only
- Recommendation: both caps, both modes.
- Spec effect: `schematic-symbols-and-libraries` "Symbol generators" (paragraph "Limits"; scenarios "Endless generators share the total time limit", "Generator time budget spent", "Generator budget restored by reload", "Generator script larger than 256 MiB", "Oversized generator file") and "Symbol cache and reload".
- Resolution: platform — in both modes of `xschem-compat`:
  - Time (M8-D5): all generator runs of one symbol cache share a total of 60 s (`kGeneratorTotalTimeLimit`) until the cache is cleared or the symbols are reloaded; each run gets at most the 10 s of Q-SCHEM-32 or what is left, if less. Once the total is spent no generator runs: each further generator reference is a missing symbol and one `SCHEM-LIB-012` (`Symbol <reference>: symbol generators ran for their total of 60000 ms; no further generator runs until the symbols are reloaded`) is reported. Clearing or reloading restores the full 60 s.
  - Script size (M8-D6): a generator file larger than 268,435,456 bytes (256 MiB, `kMaxGeneratorScriptBytes`) is not read to check its first line and is not run, whatever its name: the reference is a missing symbol and one `SCHEM-LIB-012` (`Symbol <reference>: the generator <file> is larger than 268435456 bytes; it is neither read nor run`) is reported. (Integration note, round 4: as implemented by SECURITY; an earlier draft of this line gave `SCHEM-LIB-011` for a file without `.tcl`, which would have required reading its first line.)
  - One regression test per limit (SECURITY, M8 round 4). Approved by the user 2026-11-23.

### Q-SCHEM-44: Absolute-path generators outside the search path
- Kind: platform
- Status: approved 2026-11-23 (raised by the M8 round-3 review of symbol generators; user decision M8-D7 of 2026-11-23)
- xschem behavior: a generator reference whose program is an absolute path runs that program wherever it is, outside the library search path.
- Evidence: M8 round-3 review of the symbol generators (`schematic-symbols-and-libraries` "Symbol generators").
- Options: allow an absolute-path `.tcl` generator and run it in the sandbox | refuse generators outside the search path
- Recommendation: allow, sandboxed: the sandbox, not the location, bounds what the script can do.
- Spec effect: `schematic-symbols-and-libraries` "Symbol generators" (scenario "Absolute-path generator runs sandboxed").
- Resolution: platform — in both modes of `xschem-compat`, a Tcl generator named by an absolute path outside the library search path stays allowed and runs in the generator sandbox like any other: it may read only the script's folder and the document's folder, may write nothing and may run no process; the limits of Q-SCHEM-32 and Q-SCHEM-43 apply. Regression test by SECURITY (M8 round 4). Approved by the user 2026-11-23.

### Q-SCHEM-45: Default `CAD_ROOT` for the system library
- Kind: platform
- Status: approved 2026-11-23 (raised by M8 round 3, where the system library was found only with `CAD_ROOT` set; user decision M8-D10 of 2026-11-23)
- xschem behavior: n/a (xschem finds its own `XSCHEM_SHAREDIR` from its install prefix).
- Evidence: M8 round-3 integration of the system library (design D3 and D8 of `openspec/changes/m8-schematic`; `XSCHEM_SHAREDIR` = `<CAD_ROOT>/assets/schematic`).
- Options: `CAD_ROOT` defaults to the install root in `hefesto-cli` and the desktop app | require `CAD_ROOT` to be set
- Recommendation: default to the install root.
- Spec effect: `app-startup` (installation root) and `schematic-symbols-and-libraries` "System library" (SHELL, M8 round 4).
- Resolution: platform — when `CAD_ROOT` is not set, `hefesto-cli` and the desktop app use the install root as `CAD_ROOT`, so the system library (`<CAD_ROOT>/assets/schematic/xschem_library/devices/`, Q-SCHEM-10) is found without setting it; tests and docs by SHELL (M8 round 4). Approved by the user 2026-11-23.

### Q-SCHEM-46: xschem driver text in SCHEM stdout/stderr
- Kind: other
- Status: approved 2026-11-25 (proposed 2026-11-24, awaiting the user; drafted by the M8 round-5 GOLDEN workstream; user decision of 2026-11-25)
- xschem behavior: the SCHEM captures run xschem as a program (`xschem -n -s -q --no_x ...` or `--script`), so each case's `stdout.txt` and `stderr.txt` hold xschem's own driver text around the result: start-up and `sourcing ...` lines from the `xschemrc` and its Tcl files, and the per-netlist banner (`== div`, the cell netlisted). Hefesto replays the same case through `hefesto-cli` and the `schematic`/`xschem` command, which never print those lines, so `stdout.txt`/`stderr.txt` differ on every SCHEM case even when the netlist or saved file is byte-identical.
- Evidence: `tests/golden/SCHEM/cases/SCHEM-CMD-01/expected/stdout.txt` and `stderr.txt` (capture of golden-capture.yml); round-4 integration note of task 10.1 in `openspec/changes/m8-schematic/tasks.md` (SCHEM-CMD-01 replays `div.spice` in process, `tests/unit/schem/cmd/edit_scripts_test.cpp`); round 5: 30 SCHEM cases replay their file outputs (`tools/golden/authoring/schem_cases.py` `REPLAYED`), and each keeps `stdout.txt`/`stderr.txt` deferred to M8 with this question as the reason (`DRIVER_OUTPUTS`, `tests/golden/deviations.toml` SCHEM block); SCHEM-PR-01, PR-02 and PR-04 print their result on stdout, so they cannot replay before this is decided.
- Options: (a) an approved deviation for `stdout.txt` and `stderr.txt` of the SCHEM cases whose result is a file (the file outputs and `exitcode.txt` must still match; the Tcl query cases, whose result is printed with `puts`, keep stdout compared) | (b) recapture with the harness filtering xschem's driver text (a capture-side mask of the start-up, `sourcing` and `== <cell>` lines, so stdout/stderr keep only what the case itself prints) | (c) Hefesto prints the same lines in `xschem-compat` ON
- Recommendation: (b). The driver text is xschem's program banner, not schematic behaviour, and (b) keeps every other stdout/stderr line compared (warnings, `puts` results), which (a) gives up; (c) would make Hefesto print paths and file names of another program's installation. Until decided, the outputs stay deferred to M8 per case (not an exit blocker only once decided).
- Spec effect: (a) `tests/golden/deviations.toml` entries with the decision; (b) `tools/golden/harness/schem/` and a mask in `tests/golden/SCHEM/CASES.md` "Normalisation", then a recapture of the affected cases; (c) `schematic-command` "Headless use".
- Resolution: other: (b). The driver text is xschem's program banner, not schematic behaviour, and (b) keeps every other stdout/stderr line compared (warnings, `puts` results), which (a) gives up; (c) would make Hefesto print paths and file names of another program's installation. Until decided, the outputs stay deferred to M8 per case (not an exit blocker only once decided). Approved by the user 2026-11-25 with the recommended option.

### Q-SCHEM-47: Undo history bounded by memory as well as steps
- Kind: platform
- Status: approved 2026-11-25 (proposed 2026-11-24, awaiting the user; implemented by the M8 round-4 review fix pass, PR #101; user decision of 2026-11-25)
- xschem behavior: the undo history keeps a fixed number of steps; their size is not bounded, so a few edits of a very large schematic can hold many copies of it.
- Evidence: `src/core/include/hefesto/schem/edit/edit_state.hpp` (`kUndoMaxBytes` = 512 MiB, `EditSession::undo_max_bytes`); test "Undo history byte bound" (`tests/unit/schem/edit/history_test.cpp`); task 9.2 round-4 review note.
- Options: bound each document's history to 512 MiB (536,870,912 bytes) as well as 200 steps, dropping the oldest steps first and always keeping the newest, both modes (as implemented) | bound by steps only (as xschem)
- Recommendation: the 512 MiB and 200-step bound, both modes (as implemented): a step-only bound lets a large document exhaust memory, and the bound only drops old steps.
- Spec effect: `schematic-editing` "Undo and redo" (the byte bound).
- Resolution: platform: the 512 MiB and 200-step bound, both modes (as implemented): a step-only bound lets a large document exhaust memory, and the bound only drops old steps. Approved by the user 2026-11-25 with the recommended option.

### Q-SCHEM-48: Non-finite numbers in `schematic` options
- Kind: platform
- Status: approved 2026-11-25 (proposed 2026-11-24, awaiting the user; implemented by the M8 round-4 review fix pass, PR #101; user decision of 2026-11-25)
- xschem behavior: coordinates, angles and scales given to its commands are read with the C library; `inf`, `nan` or `1e999` are accepted and put non-finite coordinates into the document.
- Evidence: `src/core/src/schem/edit/context.cpp` (numbers read for coordinates, angles and scales must be finite), `src/core/src/schem/edit/geometry.cpp` (snap and scale); test "Non-finite coordinate refused" (`tests/unit/schem/edit/editing_test.cpp`).
- Options: refuse a non-finite number (`inf`, `nan`, an overflow such as `1e999`) as an invalid argument of the option, both modes (as implemented) | accept it (as xschem)
- Recommendation: refuse, both modes (as implemented): a non-finite coordinate cannot be written back as a valid file value and breaks every geometric operation after it.
- Spec effect: `schematic-editing` "Document model" (scenario "Non-finite coordinate refused").
- Resolution: platform: refuse, both modes (as implemented): a non-finite coordinate cannot be written back as a valid file value and breaks every geometric operation after it. Approved by the user 2026-11-25 with the recommended option.

### Q-SCHEM-49: Script sandbox read-only roots of the install root
- Kind: platform
- Status: approved 2026-11-25 (proposed 2026-11-24, awaiting the user; implemented by the M8 round-4 review fix pass, PR #101; user decision of 2026-11-25)
- xschem behavior: n/a (xschem scripts run without a sandbox).
- Evidence: `src/core/src/app/startup.cpp` (only `<root>/assets` and `<root>/magic` of the install root are read-only sandbox roots); tests "the install root and the PDK store are read-only roots of the script sandbox" and "Install prefix not readable by scripts" (`tests/unit/app/startup_run_test.cpp`); task 13.2 round-4 review note. Since Q-SCHEM-45 the install root defaults to the install prefix (`/usr/local`, `/opt/homebrew`), which must not become readable as a whole.
- Options: of the install root only `<root>/assets` (the schematic system library) and `<root>/magic` (Magic's system files) are read-only roots (as implemented) | the whole install root is a read-only root
- Recommendation: only `<root>/assets` and `<root>/magic` (as implemented).
- Spec effect: `app-startup` "Installation root and search paths" (the script sandbox roots).
- Resolution: platform: only `<root>/assets` and `<root>/magic` (as implemented). Approved by the user 2026-11-25 with the recommended option.

### Q-SCHEM-50: Time units in the `SCHEM-TCL-003` texts
- Kind: other
- Status: approved 2026-11-25 (proposed 2026-11-24, awaiting the user; implemented by the M8 round-4 review fix pass, PR #101; user decision of 2026-11-25)
- xschem behavior: n/a (xschem has no `tcleval` time limit).
- Evidence: `src/core/src/schem/tcl/tcl_bridge.cpp` (`tcleval value stopped after 10 s`, `tcleval values ran for their total of 60 s; no further value is evaluated in this run`); test "the tcleval limit texts print whole seconds in s" (`tests/unit/schem/security/tcleval_sandbox_test.cpp`); `docs/diagnostics/codes/schem.md` `SCHEM-TCL-003`. The generator limit `SCHEM-LIB-012` of Q-SCHEM-43 gives its total in ms (`60000 ms`).
- Options: whole seconds, `10 s` per value and `60 s` per run (a limit that is not whole seconds in ms), as implemented | milliseconds (`10000 ms`, `60000 ms`), like `SCHEM-LIB-012`
- Recommendation: whole seconds (as implemented); the texts are diagnostics only.
- Spec effect: `schematic-symbols-and-libraries` "tcleval values" (paragraph "Run time and denials", the `SCHEM-TCL-003` texts); `docs/diagnostics/codes/schem.md`.
- Resolution: other: whole seconds (as implemented); the texts are diagnostics only. Approved by the user 2026-11-25 with the recommended option.

### Q-SCHEM-51: Pasted or merged embedded symbol vs the document's symbol
- Kind: platform
- Status: approved 2026-11-25 (proposed 2026-11-25, awaiting the user; raised by the review of PR #120, implemented as recommended by its fix pass; user decision of 2026-11-25)
- xschem behavior: not established (clean room: not read from xschem's source; no capture covers it).
- Evidence: `src/core/src/schem/edit/clipboard.cpp` `insert_objects`; `SymbolCache::embedded` resolves an embedded block by its reference for the whole document, so pasting an embedded `res.sym` instance into a document whose other instances use the library `res.sym` changed the drawing and netlist of all of them; test "Pasted embedded symbol does not redefine the document's symbol" (`tests/unit/schem/edit/arrange_test.cpp`).
- Options: (a) keep the document's symbol: the inserted block is not added when an instance of the document already uses the reference, and the inserted instances resolve it as the existing ones do, both modes (as implemented) | (b) add the block whenever the document has none (the round-5 behaviour; redefines the existing instances) | (c) give the inserted instances a new reference (for example `res_1.sym`) with the block, so both drawings are kept
- Recommendation: (a): a paste never changes objects the user did not touch, and it needs no new reference names; (c) keeps the pasted drawing but invents references that xschem files would then carry.
- Spec effect: `schematic-editing` "Clipboard" (paragraph on embedded symbols; scenario "Pasted embedded symbol does not redefine the document's symbol").
- Resolution: platform: (a): a paste never changes objects the user did not touch, and it needs no new reference names; (c) keeps the pasted drawing but invents references that xschem files would then carry. Approved by the user 2026-11-25 with the recommended option.

### Q-SCHEM-52: Default library search path of a document without an `xschemrc`
- Kind: platform
- Status: proposed (drafted 2026-11-26 by the spec team, M8 round 6 DECIDE, from the round-5 CMD note of task 10.1; awaiting the user)
- xschem behavior: before any `xschemrc` is evaluated, `XSCHEM_LIBRARY_PATH` holds a built-in value: the user library `~/.xschem/xschem_library`, the system library `${XSCHEM_SHAREDIR}/xschem_library/devices`, then eleven example folders of the installed documentation (`examples`, `ngspice`, `ngspice_verilog_cosim`, `logic`, `xschem_simulator`, `generators`, `inst_sch_select`, `binto7seg`, `pcb`, `rom8k`, `analyses`). The system `xschemrc` leaves it alone (its own lines are commented out); an `xschemrc` that sets the variable replaces it, one that appends to it keeps it. So a schematic in a folder without an `xschemrc` finds the `devices` symbols. The spec said the path is then the document's folder only, so `res.sym` is a missing symbol in Hefesto (SCHEM-CMD-01 works around it with an input `xschemrc`).
- Evidence: the build configuration's default library path and the start-up code that sets it before the rc files are sourced; `xschemrc` (system rc, commented-out block); task 10.1 round-5 integration note.
- Options: (a) before the `xschemrc` files run, `XSCHEM_LIBRARY_PATH` is `${XSCHEM_SHAREDIR}/xschem_library/devices` only, both modes | (b) as xschem: also `~/.xschem/xschem_library` first (under the synthetic `env(HOME)` of Q-SCHEM-38 that is `<document folder>/.xschem/xschem_library`) and the documentation folders (Hefesto ships none, so they are dropped as missing) | (c) keep the current spec: the document's folder only when nothing sets the path
- Recommendation: (a), both modes. The system library is found as in xschem; (b) adds only folders that are always missing or hidden in the document's folder; (c) leaves every placed system symbol missing in a plain folder.
- Spec effect: `schematic-symbols-and-libraries` "Library search path" (paragraph "Default value", scenarios "Default search path without an xschemrc", "xschemrc replaces the default search path").

### Q-SCHEM-53: Net driver check errors and the simulation deck
- Kind: other
- Status: proposed (drafted 2026-11-26 by the spec team, M8 round 6 DECIDE, while specifying xschem's net driver check from SCHEM-NL-09/NL-10; awaiting the user)
- xschem behavior: the net driver check reports `Error: undriven node: <net>` for a net with no `out` or `inout` pin, which makes the netlist run fail (exit status 10), but the netlist file is still written, so a simulator can still be run on it. PDK testbenches whose gate nets are driven only by sources in a code block (gf180 `tests/test_nfet_03v3.sch`: `vg g 0 0`) fail this check.
- Evidence: golden cases SCHEM-NL-09, SCHEM-NL-10 (`exitcode.txt` 10, netlist written); the net driver check (traces in `schematic-netlisting` "Errors and warnings").
- Options: (a) a run whose only errors are driver-check findings still builds the simulation deck, with the findings as warnings of the deck build; `schematic netlist`, `schematic check` and `hefesto-cli --netlist` still fail (exit status 10), both modes | (b) every error stops the deck (`SCHEM-SIM-004`, the current rule), so such testbenches cannot be simulated | (c) the driver check never fails a run in Hefesto (warnings only), both modes, and SCHEM-NL-09/NL-10 get an approved deviation for `exitcode.txt`
- Recommendation: (a): the netlist status stays xschem's (golden exit status 10), and PDK testbenches whose netlist xschem writes can still be simulated.
- Spec effect: `schematic-simulation-integration` "Schematic circuit source" (scenario "Undriven node does not block the deck"); `schematic-netlisting` "Errors and warnings" (paragraph "Net driver check").

### Q-SCHEM-54: Shell processing of symbol generator arguments
- Kind: platform
- Status: proposed (drafted 2026-11-26 by the spec team, M8 round 6 DECIDE, from the spec issue of task 7.4 on `tier.tcl(@lab\)`; awaiting the user)
- xschem behavior: the text between the parentheses of a generator reference is not substituted when the symbol is loaded (`@lab` stays `@lab`); `(`, `)`, `,` and spaces become spaces, and the program path plus the rest of the text is run as a shell command line, so the shell splits the words and treats a backslash as an escape character. In the SCHEM golden captures the sky130 generators `tier.tcl` and `res.tcl` were not executable, so the shell refused them (`Permission denied`) and every such instance is a symbol with no `type`.
- Evidence: the generator reference handling of the symbol loader; golden cases SCHEM-NL-05, NL-21, NL-23 (`stderr.txt`: `Permission denied`, `Warning: Symbol: sky130_tests/tier.tcl(@lab\): no type attribute set`).
- Options: (a) no shell processing: `argv` is the argument text split at `(`, `)`, `,`, spaces and tabs, empty words dropped, every word kept as written (backslashes included), both modes | (b) apply POSIX shell word rules to the argument text (quotes, backslash escapes) before building `argv`
- Recommendation: (a). Hefesto runs only Tcl generators in its sandbox and never a shell; (b) would need a shell parser for no PDK benefit (no pinned PDK generator argument holds a quote or a meaningful backslash).
- Spec effect: `schematic-symbols-and-libraries` "Symbol generators" (paragraph "Arguments", scenario "Generator arguments are not substituted").

### Q-SCHEM-60: Diagnostics in the SCHEM golden `stderr.txt`
- Kind: other
- Status: proposed (drafted 2026-11-26 at the M8 round-6 integration from the GOLDEN round-6 replay; numbers 55 to 59 are not used; awaiting the user)
- xschem behavior (black box, golden captures): after the Q-SCHEM-46 filter, `stderr.txt` holds xschem's ERC and loader lines (`Warning: open net: <net>`, `Error: undriven node: <net>`, `-----------<schematic>`, `l_s_d(): Symbol not found: ...`, `load_schematic(): unable to open file ...`, `Error: Symbol ...: Unmatched subcircuit schematic pin direction`, `WARNING:  missing fields for WIRE object, ignoring`). Hefesto prints coded diagnostics with other wording, adds its own warnings (`SCHEM-NET-015`, `SCHEM-NET-009/010/011`, `SCHEM-LIB-006`) and does not print the open-net or undriven-node lines yet.
- Evidence: golden cases SCHEM-NL-02, 03, 05, 06, 09, 10, 11, 12, 13, 14, 16, 18, 20, 21, 22, RT-02, RT-06 (`replay.py --milestone M8`, round 6).
- Options: (a) approved deviation for `stderr.txt` of the SCHEM cases; files, exit status and `puts` results stay compared, and message texts are covered by the "Errors and warnings" and "Malformed input" unit tests | (b) a diagnostics comparator that strips `<severity>: <CODE>: `, ` at <file>:<line>` and xschem's `Warning:`/`Error:` prefixes, with per-case lists of Hefesto-only and xschem-only messages as approved deviations | (c) with `xschem-compat` ON, Hefesto prints xschem's message texts only
- Recommendation: (a). The diagnostics are Hefesto's coded contract; (b) is a large comparator for little gain and (c) duplicates every message path.
- Spec effect: `schematic-command` (golden comparison paragraph); `tests/golden/deviations.toml` SCHEM block.

### Q-SCHEM-61: Result of `xschem netlist` after a run that reports an error
- Kind: other
- Status: proposed (drafted 2026-11-26 at the M8 round-6 integration from the GOLDEN round-6 replay; awaiting the user)
- xschem behavior (black box): after a netlist run that reports an error, `xschem netlist` returns `1` and the script goes on (SCHEM-NL-22 prints `netlist 1` and exits 10; SCHEM-SIM-03 still writes `tb.spice`). Hefesto's `xschem netlist` raises, as `schematic netlist` does.
- Evidence: golden cases SCHEM-NL-22 (`stdout.txt`), SCHEM-SIM-03.
- Options: (a) with `xschem-compat` ON, `xschem netlist` returns xschem's value without raising and the exit status stays 10; `schematic netlist` keeps raising; GOLDEN adds a success-path capture to pin the value | (b) keep raising, and record an approved deviation for SCHEM-NL-22 `stdout.txt` and SCHEM-SIM-03
- Recommendation: (a): the `xschem` command set exists to run xschem scripts unchanged.
- Spec effect: `schematic-command` xschem table (`netlist` row).

### Q-SCHEM-62: Third argument of `xschem get_tok`
- Kind: other
- Status: proposed (drafted 2026-11-26 at the M8 round-6 integration from the GOLDEN round-6 replay; awaiting the user)
- xschem behavior (black box, SCHEM-PR-01): with a third argument `1` xschem keeps the quotes of a quoted value (`"tantalium capacitor"`); with `2` it strips them; for unquoted values both give the plain value. Hefesto raises `wrong number of arguments`.
- Evidence: golden case SCHEM-PR-01 (`stdout.txt`).
- Options: (a) accept the optional third argument with the captured behaviour, both modes | (b) leave it unsupported and record SCHEM-PR-01 as a deviation
- Recommendation: (a).
- Spec effect: `schematic-command` xschem table (`get_tok` row).

### Q-SCHEM-66: Copy and Paste keys of the desktop Schematic tab
- Kind: platform
- Status: proposed (drafted 2026-11-26 by the M8 round-6 SHELL workstream, recorded at the round-6 integration; numbers 63 to 65 are not used; awaiting the user)
- xschem behavior: not established (clean room: not read from xschem's source; no capture covers the desktop keys).
- Evidence: `src/shell/desktop/src/schematic_clipboard.hpp`, tests in `tests/unit/shell/schematic_clipboard_test.cpp`; task 13.2 round-6 note; `docs/design/m8-ui-conformance.md` row "Copy and Paste with the system clipboard".
- Options: (a) Command-C / Command-V on macOS; off macOS Ctrl-C / Ctrl-V, where Ctrl-C interrupts a running netlist and copies otherwise; both modes (as implemented) | (b) Ctrl-C / Ctrl-V on every platform, Ctrl-C always copies and the netlist is interrupted only from the Interrupt card | (c) a separate interrupt key (Escape) and Ctrl-C always copies
- Recommendation: (a): each platform's own clipboard keys, and the terminal habit of Ctrl-C as interrupt kept while a run is going.
- Spec effect: `schematic-editing` "Desktop and touch interaction" (clipboard keys).

### Q-SCHEM-67: `schematic` client of the core `macro` command
- Kind: platform
- Status: proposed (drafted 2026-11-26 by the M8 round-6 SHELL workstream, recorded at the round-6 integration; awaiting the user)
- xschem behavior: not applicable (Magic's `macro` has no `schematic` client; xschem binds keys in its Tcl GUI).
- Evidence: `src/core/src/ui/macro_commands.cpp` (`kSchematicClient`), tests in `tests/unit/shell/schematic_bindings_test.cpp`; task 13.2 round-6 note.
- Options: (a) the core `macro` command accepts the client `schematic`, both modes, so the key-binding sheet's `macro schematic KEY "TEXT"` lines and scripts edit the editor's key table; unbound keys keep the xschem-style defaults (as implemented) | (b) the shell applies `macro schematic` lines itself and the core rejects the client (the round-5 behaviour; scripts cannot rebind schematic keys)
- Recommendation: (a): one key-binding path for layout and schematic, scriptable and covered by the key-binding sheet test.
- Spec effect: `schematic-editing` "Desktop and touch interaction" (key bindings).
