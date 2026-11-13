# SCHEM golden test-case inventory

These cases will be captured later by running xschem `3.4.8RC` (`StefanSchippers/xschem` @ `4142e6d6f1eb5cd0e271bb183f14202af374ce27`) headless on Linux, with **gawk** installed as `awk` (see Q-SCHEM-18):
- netlists: `xschem -n -s -q --no_x -o <outdir> <file.sch>` (add `--rcfile <PDK>/libs.tech/xschem/xschemrc`; LVS mode through a `--command` or `--preinit` script setting `lvs_netlist 1`, `lvs_ignore 1` or `top_is_subckt 1`)
- round trips: `xschem --no_x -q --command "xschem load <in>; xschem saveas <out>; exit"`
- property and connectivity queries: `xschem --no_x --script <case.tcl>` printing results with `puts`

**Inputs.** Unmodified PDK example schematics at the pinned commits (`xschem_sky130` @ `2bfd7d65`, installed by open_pdks @ `c6df28c1` as `sky130A/libs.tech/xschem`; IHP `ihp-sg13g2/libs.tech/xschem` @ `5e6d592e`; gf180 `gf180mcu_fd_pr/cells/xschem` @ `aacc04cc`; all Apache-2.0), and small Hefesto-authored files described inline. No file from xschem's own distribution (`xschem_library`, `tests`) is an input or is committed: xschem is GPL, and the clean-room process requires Hefesto-authored golden inputs (capability `hefesto-licensing`, Requirement "Two-team clean-room process for specs written from GPL source"; J-1 of `docs/decisions/m8-decision-table.md`). Running the xschem binary in the capture runner is allowed; its outputs are black-box behaviour. `xschem_execute_scripts` SHALL be `yes` for captures so that `tcleval` values are evaluated.

**Normalisation.** Before comparing with Hefesto output:
- replace the absolute paths in `** sch_path:` and `** sym_path:` lines by paths relative to the PDK root or test folder (Q-SCHEM-13)
- for round-trip cases, compare bytes exactly, except the `xschem version=` value when the input was written by another xschem version (Q-SCHEM-1)
- Hefesto runs with `xschem-compat` ON (the default) and "portable netlist paths" off; the OFF behaviour of the compat entries is covered by unit tests, one per mode
- flat netlists (SCHEM-NL-21) are compared by their canonical parsed form, never by bytes (Q-SCHEM-15)

**What is captured.** The output file (`.spice` or `.sch`/`.sym`), xschem's stdout and stderr (ERC messages), and the exit code.

| ID | capability | input description (files, options, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| SCHEM-RT-01 | schematic-file-format | sky130 `sky130_tests/not.sch`: load, save as | saved file; then load+save again (fixpoint) | Round-trip guarantee; Canonical write order |
| SCHEM-RT-02 | schematic-file-format | every `.sch` and `.sym` in `xschem_sky130`, IHP `libs.tech/xschem` and gf180 `cells/xschem`, each loaded and saved | saved files; list of files whose second save differs from the first | Round-trip guarantee; Brace strings; Numeric fields |
| SCHEM-RT-03 | schematic-file-format | hand-written file with CR-LF line ends, `L 4 20 0 10 5 {}`, `B 5 2.5 2.5 -2.5 -2.5 {}`, `N 10.50 -20.0 30 -20 {}` | saved file | File kinds and character handling; Line, box, arc and polygon records; Numeric fields |
| SCHEM-RT-04 | schematic-file-format | pre-1.2 symbol with only a `G {type=subcircuit ...}` global record | saved file (G/K order) | Global property records |
| SCHEM-RT-05 | schematic-file-format | Hefesto-authored schematic with embedded symbols: one embedded `.sym` that has lines, boxes, arcs, polygons and texts, one schematic used as a symbol with wires, and a second instance of each that is not embedded (replaces xschem's GPL `pcb_test1_embed.sch`, J-1) | saved file; position and content of `[`...`]` blocks | Embedded symbols; Q-SCHEM-2 |
| SCHEM-RT-06 | schematic-file-format | hand-written file: unknown record `Z 1 2 {a` LF `b} 3`, a `#` comment, a truncated `N 0 0 10`, out-of-range layer `L 30 ...`, unterminated `{` at EOF | loaded object counts (`xschem get wires/instances`), messages, saved file | Record grammar; Malformed input; Layers |
| SCHEM-RT-07 | schematic-file-format | symbol with header text (license block in `v`), e.g. `sky130_fd_pr/nfet_01v8.sym` | saved file first record | Version record and header text |
| SCHEM-RT-08 | schematic-file-format | file with no `v` record: (a) first record `K {}`, then `C {res} 0 0 0 0 {name=R1}`; (b) first record is that `C` | saved files (references with/without `.sym`) | Version record and header text; Q-SCHEM-3 |
| SCHEM-PR-01 | schematic-file-format | Tcl script: `xschem get_tok`/`xschem subst_tok` on the strings of the "Property strings" scenarios plus `a=1;b=2`, `k="x y"`, `k=\"q\"`, `k` (no value), empty string | returned strings | Property strings; Boolean values |
| SCHEM-PR-02 | schematic-symbols-and-libraries | Tcl script: place `res.sym` twice, copy `R1`, place `x1[3:0]` and copy it, in an empty schematic | instance names | Instance names |
| SCHEM-PR-03 | schematic-symbols-and-libraries | symbol with pins `B` (sim_pinnumber=3), `A` (1), `C` (2); and one with a pin missing sim_pinnumber; netlisted inside a top schematic | `.subckt` port lists | Pin order |
| SCHEM-PR-04 | schematic-symbols-and-libraries | `xschem translate` of texts `@name=@value`, `a\@b @name`, `@W/@L`, `%undef`, `@symname`, `@#0:pinnumber` on chosen instances | returned strings | Text substitution tokens; Q-SCHEM-26 |
| SCHEM-PR-05 | schematic-symbols-and-libraries | `XSCHEM_LIBRARY_PATH` with a missing folder, an alias `path \| lib`, and `.`; resolve `lib/x.sym`, `../x.sym`, `devices/res.sym` | `abs_sym_path` and `rel_sym_path` results | Symbol references; Library search path |
| SCHEM-NL-01 | schematic-netlisting | Hefesto-authored `SC_Test.sch`: a flat testbench with no ports, a few passive and source devices, and one `code_shown.sym` instance (replaces xschem's GPL `tests/SC_Test.sch`, J-1) | netlist | Netlist file structure; Subcircuit body; Element line post-processing |
| SCHEM-NL-02 | schematic-netlisting | sky130 `sky130_tests/test_inv.sch` with sky130A `xschemrc` | netlist | Format string substitution; expr evaluation; Element line post-processing (spiceprefix); tcleval values (corner) |
| SCHEM-NL-03 | schematic-netlisting | sky130 `sky130_tests/passgate.sch` and a testbench using `passgate.sym` | netlists | Hierarchical parameters; Subcircuit definitions |
| SCHEM-NL-04 | schematic-netlisting | sky130 `sky130_tests/adder_16bit.sch` (buses and vector instances) | netlist | Bus and vector notation; Element line post-processing (vector expansion); Unnamed nets |
| SCHEM-NL-05 | schematic-netlisting | sky130 `sky130_tests/top.sch`, hierarchical | netlist; order of `* expanding symbol` blocks | Subcircuit definitions (ordering) |
| SCHEM-NL-06 | schematic-netlisting | sky130 `sky130_tests/test_lvs_ignore.sch`, `test_short_option.sch`, `test_format_override.sch`, with lvs_ignore 0 and 1 | netlists | Ignored and shorted objects; Format string substitution (instance override) |
| SCHEM-NL-07 | schematic-netlisting | IHP `sg13g2_tests/dc_lv_nmos.sch`, `dc_res_temp.sch`, `tran_cap_cmomi.sch` with IHP `xschemrc` | netlists | tcleval values (`ev`, `ev7`, `cap_cmomi_C`); PDK attribute conventions; Q-SCHEM-7 |
| SCHEM-NL-08 | schematic-netlisting | IHP `sg13g2_tests/dc_hbt_13g2.sch`, `ac_mim_cap.sch` | netlists | tcleval values; Device models |
| SCHEM-NL-09 | schematic-netlisting | gf180 `tests/test_nfet_03v3.sch`, `test_cap_mim_1f0fF.sch` | netlists | PDK attribute conventions (quoted SPICE expressions) |
| SCHEM-NL-10 | schematic-netlisting | LVS mode (`lvs_netlist 1`, `lvs_ignore 1`) of sky130 `sky130_tests/not.sch`, IHP `dc_logic_not.sch` subcircuit, gf180 `test_nfet_03v3.sch` | netlists | LVS netlist mode; `*.PININFO`; lvs_format |
| SCHEM-NL-11 | schematic-netlisting | hand-written: two labels `A`,`B` on one wire; T junction; crossing wires; pin on wire interior; floating resistor pins; label `net1` plus unlabeled wires | netlists; stderr messages; exit code | Connection geometry; Net naming order; Unnamed nets; Errors and warnings; Q-SCHEM-16 |
| SCHEM-NL-12 | schematic-netlisting | hand-written: one device line of 200 characters with no quotes; one with `{...}` expressions; an unbalanced `'` followed by long lines; a long `.include` line; a long `*` comment | netlist | Line breaking; Q-SCHEM-20 |
| SCHEM-NL-13 | schematic-netlisting | hand-written: `expr(2-1)`, `expr(2 - 1)`, `expr_eng(1.5e-7)`, `expr(W_N*2)`, `expr('int((1 + 1)/2) * 1 / 1 * 0.29')` in a format | netlist lines | expr evaluation; Q-SCHEM-12 |
| SCHEM-NL-14 | schematic-netlisting | subcircuit symbol whose schematic file is missing; symbol with `spice_sym_def=".include x.spice"`; symbol with `spice_stop=true`; `default_schematic=ignore` | netlist; messages | Subcircuit definitions; Schematic resolution; Q-SCHEM-27 |
| SCHEM-NL-15 | schematic-netlisting | two instances of one symbol, one with `schematic=<other>.sch` | netlist | Schematic resolution; Q-SCHEM-28 |
| SCHEM-NL-16 | schematic-netlisting | `libA/inv.sym` and `libB/inv.sym` both used | netlist | Subcircuit definitions; Q-SCHEM-14 |
| SCHEM-NL-17 | schematic-netlisting | `netlist_options.sym` with `bus_replacement_char=<>`, `top_is_subckt=true`; option `uppercase_subckt` | netlists | Netlist options; Bus and vector notation |
| SCHEM-NL-18 | schematic-netlisting | five instances with distinct `device_model` texts plus one duplicate key | netlist (order of model lines) | Device models |
| SCHEM-NL-19 | schematic-netlisting | Hefesto-authored bus schematic with `bus_tap` instances on a 4-bit and an 8-bit bus, taps of single bits and of a sub-range (replaces xschem's GPL `xschem_library/examples` bus example, J-1) | netlist | Bus taps |
| SCHEM-NL-20 | schematic-netlisting | `inv.sch` netlisted as top with and without `inv.sym` present | netlist top line | Top-level port order |
| SCHEM-NL-21 | schematic-netlisting | sky130 `top.sch` in flat mode (`-f`) | netlist; Hefesto compares electrically (LVS of flat vs hier) | Flat netlist; Q-SCHEM-15 |
| SCHEM-NL-22 | schematic-netlisting | after netlisting SCHEM-NL-11 inputs, save the schematic | saved file (`lab=` on wires) | Net naming order; Q-SCHEM-29 |
| SCHEM-NL-23 | schematic-symbols-and-libraries | sky130 `sky130_tests/test_generators.sch` | netlist (Hefesto: missing-symbol message for non-Tcl generators) | Symbol generators; Q-SCHEM-11 |
| SCHEM-SIM-01 | schematic-simulation-integration | IHP `dc_lv_nmos.sch` simulated in xschem (ngspice batch) and in Hefesto | raw-file vectors `i(vd)`, op values; Hefesto job plots | Schematic circuit source; Control block translation |
| SCHEM-SIM-02 | schematic-simulation-integration | sky130 `test_inv.sch` op point: `xschem annotate_op`, then `xschem translate` of `@spice_get_voltage` texts | displayed strings | Operating-point annotation; Simulator vector names |
| SCHEM-SIM-03 | schematic-simulation-integration | hierarchical testbench: probe an internal net of `x1` and a port net from inside `x1` (`xschem resolved_net`) | vector names | Simulator vector names |
| SCHEM-LVS-01 | schematic-simulation-integration | sky130 inverter: layout extracted by Magic, schematic `not.sch`; netgen with the xschem LVS netlist vs Hefesto LVS from `not.sch` | netgen result; Hefesto match/mismatch and cross-probe map | Schematic to layout cross-probing; Schematic-driven LVS command |
| SCHEM-CMD-01 | schematic-command | Hefesto-only: the divider script of the "Subcommand table" scenario | returned netlist | Subcommand table |
