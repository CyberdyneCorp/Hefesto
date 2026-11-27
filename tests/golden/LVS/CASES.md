# LVS golden test-case inventory

These cases will be captured later by running netgen 1.5.327 (`RTimothyEdwards/netgen` @ `93dcda14d36349d0b0ca63168d3cbbab20fe2ece`) in batch mode, on Linux (see Q-LVS-8). A typical invocation is `netgen -batch lvs "<f1> <c1>" "<f2> <c2>" <setup> comp.out -json`. Some cases instead source a short Tcl script of netgen commands; their results are captured with `puts`.

**Inputs.** Inputs are small hand-written SPICE, CDL, Verilog or `.sim` netlists, plus either an inline setup file or an unmodified open_pdks setup file (sky130A, gf180mcuD, sg13g2) pinned to a recorded open_pdks commit.

**What is captured.** Each case captures:
- `comp.out`
- `comp.json`, where `-json` is used
- console stdout and stderr
- returned Tcl values

**How outputs are compared.** Captured outputs are compared according to capability `lvs-reports`, Requirement "Canonical golden comparison":
- the final-result block and classifications byte for byte
- reordered sections as sets

Hefesto runs each case through `hefesto-cli` with the same inputs.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| NG-READ-01 | lvs-netlist-input | `readnet` on `x.spice`, `x.cir`, `x.v`, `x.sim`; extensionless file starting `*`; extensionless file starting `\|`; extensionless file starting `#` | returned file numbers, error text | Format selection |
| NG-READ-02 | lvs-netlist-input | read `a.spice` twice; then `readnet a.spice 5` | return values, error `File is already loaded as a different file number.` | Format selection |
| NG-READ-03 | lvs-netlist-input | SPICE with `+` continuation, interleaved `*` comment, backslash-EOL, quoted expression with spaces, `.control/.endc`, an unknown card repeated 102 times | `cells`/`describe` output, warnings (100 cap) | SPICE lexical structure |
| NG-READ-04 | lvs-netlist-input | `.subckt` with `PARAMS:` and `k=v`, no-port subckt, nested subckt, missing `.ends`, stray `.ends` | `ports`, `property <cell>` lists, messages | SPICE subcircuit definitions |
| NG-READ-05 | lvs-netlist-input | CDL file defining `buf` three times | cell list (`buf[[0]]`, `buf[[1]]`, `buf`), messages | SPICE subcircuit definitions |
| NG-READ-06 | lvs-netlist-input | one card of each of M Q D C R L T V I E (with and without model, CDL `$[model]`, value tokens `1k`, `1meg`, `10pF`, `2kohms`, `3x`) | instance names, model cells, ports, properties of interest, `describe` | SPICE primitive device cards; SI value parsing |
| NG-READ-07 | lvs-netlist-input | X calls: CDL `/ cell`, prefixed name `Xinv_1 … inv`, undefined subckt, too many/few nodes, self-instance | placeholder ports `1..k`, instance names, messages | SPICE subcircuit calls and placeholders |
| NG-READ-08 | lvs-netlist-input | `.include` relative and absolute, `.global` before/after use, `*.GLOBAL`, `.param` used in `W='2*wp'` | resulting nets/globals, resolved property values | Globals, includes and parameters |
| NG-READ-09 | lvs-netlist-input | Verilog read first (instancing `inv` with named pins), then SPICE `.subckt inv` with different pin order | `Verilog placeholder inv replaced by SPICE definition`, final pin order of instances | SPICE subcircuit definitions; Structural Verilog input |
| NG-READ-10 | lvs-netlist-input | structural Verilog: ANSI/non-ANSI ports, buses, `assign`, concatenation, `` `define ``, `generate for`, `specify` | nets, bus bit names, instance connections | Structural Verilog input |
| NG-READ-11 | lvs-netlist-input | `.sim` with `\| units: 100`, n/p/c/r/e/b records with and without positions | devices, property values (`length`, `width`, `value`) | .sim input |
| NG-READ-12 | lvs-netlist-input | `readlib spice lib.spice`, then a netlist using lib cells; `model blackbox on` then an empty subckt | classes (`model <c>` query) | Library and black-box reads |
| NG-READ-13 | lvs-netlist-input | Magic 8.3 `.ext` with `device mosfet` records read by `readnet ext` | `describe` (no devices) | Legacy .ext input |
| NG-NAME-01 | lvs-netlist-input | SPICE vs SPICE with nets `Vdd`/`VDD`, `d[3]`/`D<3>` | match result | Name matching and case sensitivity |
| NG-NAME-02 | lvs-netlist-input | Verilog vs SPICE with `Vdd`/`VDD`; escaped `\a[1] ` vs `a[1]` | match result, warning text | Name matching and case sensitivity |
| NG-NAME-03 | lvs-netlist-input | net names containing bytes ≥ 0x80 (UTF-8) | match result (Q-LVS-3) | Name matching and case sensitivity |
| NG-SET-01 | lvs-setup-file | setup with syntax error on line 3, unknown cell on line 7, multi-line `foreach` | stderr `Error setup.tcl:N (ignoring), …`, warning line | Setup file evaluation |
| NG-SET-02 | lvs-setup-file | `lvs` with setup `""`, `nosetup`, missing `foo.tcl` | console text, whether comparison ran | Setup file evaluation |
| NG-SET-03 | lvs-setup-file | each cell-reference form (`-circuit1 x`, `{x 0}`, `{0 x}`, `{file x}`, `{* x}`, `-current`, bad forms) with `property` | effects, error texts | Cell reference syntax |
| NG-SET-04 | lvs-setup-file | `permute`, `permute default`, `permute capacitors`, `permute x 1 2`, `permute forget x 1 2`, missing pin | messages; match of swapped pins | permute command; Permutable pins in matching |
| NG-SET-05 | lvs-setup-file | `property default` then `property <fet>` / `<res>` / `<cap>` query | key/type/tolerance lists, combination flags | property default |
| NG-SET-06 | lvs-setup-file | `property c add w`, `{nf integer}`, `{w 0.02}`, `{m integer 1}`, `{model string 3}`, duplicate add | `property c` list | property add and type declarations |
| NG-SET-07 | lvs-setup-file | `property c tolerance {w inf} {l 0.05}`, `delete mult`, `remove` (all), bad pair | `property c` list; match effect | property remove, tolerance and associate |
| NG-SET-08 | lvs-setup-file | sky130-style `series enable`, `series {w critical}`, `series {l add}`, `parallel {value par}`, `property parallel none/all/connected/open`, `series none/all`, deprecated `merge` | combination results on test circuits | property series and parallel controls |
| NG-SET-09 | lvs-setup-file | `equate classes` plain, `-unique`, 6-word pin-list form (valid and each error) | messages, matching | equate classes |
| NG-SET-10 | lvs-setup-file | `ignore class` fill cells, `ignore shorted` on a decap tied to one net | device counts | ignore command |
| NG-SET-11 | lvs-setup-file | `model x resistor` on 2- and 3-port cells, wrong port count, `model x copy`, `model blackbox` query | `model x` result, errors | model command |
| NG-SET-12 | lvs-setup-file | `global top VDD*`, `convert inv` | return count, ports of `inv` | global and convert commands |
| NG-SET-13 | lvs-setup-file | `format 30`, `format 50 30`, `format 0`, `format` | returned widths, comp.out line widths | Query and output commands available to setup files; Two-column layout |
| NG-ALG-01 | lvs-comparison-algorithm | identical inverters (no setup) | comp.out, JSON | Initial partition; Match verification; Final result |
| NG-ALG-02 | lvs-comparison-algorithm | layout with an extra nmos | comp.out device/net fragments, final result | Partition legality; Device mismatch report |
| NG-ALG-03 | lvs-comparison-algorithm | NAND2 with gate inputs swapped between two NANDs | net mismatch fragments, `Netlists do not match.` | Iterative refinement; Net mismatch report |
| NG-ALG-04 | lvs-comparison-algorithm | four identical parallel inverters, `property parallel none` | `Resolving symmetries …` lines, final result | Symmetry resolution |
| NG-ALG-05 | lvs-comparison-algorithm | symmetric pins A/B of a NAND resolved by name; then with A/B renamed in one netlist | pairing, result | Symmetry resolution |
| NG-ALG-06 | lvs-comparison-algorithm | extra disconnected port `TEST` in layout top | `uniquely with port errors`, final result | Match verification |
| NG-ALG-07 | lvs-comparison-algorithm | manual: `compare`, `iterate` ×3, `verify`, `equate nodes`, `equate elements`, `restart`, `run converge`, `verify equivalent/unique/only` | return values, messages | verify command; Forced equivalences; Reset and stepping commands |
| NG-RED-01 | lvs-device-reduction | 4 parallel fingers vs 1 device with `M=4` (non-top cell) | `Merged 3 parallel devices.`, summary `(4->1)` | Parallel combination; Merged-device counts in the summary |
| NG-RED-02 | lvs-device-reduction | source/drain-swapped parallel devices | merge count | Parallel combination |
| NG-RED-03 | lvs-device-reduction | resistor chain of 3 vs single resistor (sum), port on middle node variant | `Merged … series devices.`, property result | Series combination |
| NG-RED-04 | lvs-device-reduction | series-then-parallel nested network (2×2 resistors) | merge messages per pass | Reduction schedule |
| NG-RED-05 | lvs-device-reduction | open (no-connect) drains on parallel fingers in non-top and top cells; `property parallel connected` | merge counts | Parallel combination (Q-LVS-26) |
| NG-RED-06 | lvs-device-reduction | schematic `R1 a b 0` vs layout with a=b merged | `Removing zero-valued device …` | Zero-valued device removal |
| NG-PROP-01 | lvs-property-matching | W differs by 0.4% and by 5%, tolerance 0.01 | property block lines | Tolerance rules |
| NG-PROP-02 | lvs-property-matching | integer `nf` mismatch; string `model` exact and `{model string 3}` | report lines | Tolerance rules |
| NG-PROP-03 | lvs-property-matching | `M=1` vs missing M; `M=2` vs missing M; property only in one netlist | match / report lines | Missing properties |
| NG-PROP-04 | lvs-property-matching | W given as `1u` vs `1e-6` vs `1` (type promotion) | match result | Value types and promotion |
| NG-PROP-05 | lvs-property-matching | fingers {W=1}×2 vs {W=2}, with `{w add}{l critical}` and with `property default` | match vs property error | Series/parallel network reconciliation |
| NG-PROP-06 | lvs-property-matching | `as`/`ad` associated to source/drain, flipped device | match result | Pin-associated properties |
| NG-PROP-07 | lvs-property-matching | values 0 vs 0, 1 vs −1, −1 vs −2 (Q-LVS-7) | match result | Tolerance rules |
| NG-PROP-08 | lvs-property-matching | unresolved expression property | `(unresolved expressions.)` | Tolerance rules |
| NG-PROP-09 | lvs-property-matching | `property topology strict` with S>1 and M>1 records | result | Strict and relaxed topology |
| NG-PROP-10 | lvs-property-matching | parallel records where one lacks a double property with default (Q-LVS-18) | result | Series/parallel network reconciliation |
| NG-PROP-11 | lvs-property-matching | `property x derive area w l` | `property x` list, match on area | property topology and derive |
| NG-HIER-01 | lvs-hierarchy | top→mid→inv in both netlists | queue order in log, per-pair blocks | Compare queue construction; lvs per-pair flow |
| NG-HIER-02 | lvs-hierarchy | layout-only wrapper cell `filler_x` | `Flattening unmatched subcell …` | Preparing a cell pair |
| NG-HIER-03 | lvs-hierarchy | layout `top` with 2 `pair` (each 2 inv) vs schematic `top` with 4 inv | `… makes a better match` | Pre-match flattening |
| NG-HIER-04 | lvs-hierarchy | mismatched `nand2` subcell; same with `-noflatten={nand2}` | flatten lines, final result lists | lvs per-pair flow; Black boxes and flattening prohibitions |
| NG-HIER-05 | lvs-hierarchy | `equate classes` for prefixed layout cells (`XY_inv`↔`inv`) | queue, result | Compare queue construction |
| NG-HIER-06 | lvs-hierarchy | `-blackbox` with empty `adc` subckt in both; one-sided empty | messages, pin table | Black boxes and flattening prohibitions |
| NG-HIER-07 | lvs-hierarchy | duplicate pins (same name) and shorted ports (`Y1`,`Y2`) | console messages, matching | Duplicate and shorted pins |
| NG-HIER-08 | lvs-hierarchy | flattened net names `/x1/n1` visible in mismatch report | fragment net names | Flattening semantics |
| NG-HIER-09 | lvs-hierarchy | global VDD in subcells, ports elsewhere | conversion, match | Preparing a cell pair |
| NG-PIN-01 | lvs-hierarchy | port order differs (`Y A VDD VSS` vs `A Y VSS VDD`) | pin table, `Cell pin lists are equivalent.` | Pin matching |
| NG-PIN-02 | lvs-hierarchy | schematic top has connected `EN` missing in layout | pin table, `Top level cell failed pin matching.` | Pin matching; Final result |
| NG-PIN-03 | lvs-hierarchy | subcell pin missing in layout (proxy pin) | `altered to match`, parent result | Pin matching |
| NG-PIN-04 | lvs-hierarchy | pins named `VDD!`, `\A ` vs `A` | pin table | Pin matching |
| NG-PIN-05 | lvs-hierarchy | port net class with unequal port counts | `Port connection errors found:` block | Pin matching |
| NG-PIN-06 | lvs-hierarchy | non-placeholder cell with array pin `a[0]` vs `a` (Q-LVS-24) | pin table | Pin matching |
| NG-REP-01 | lvs-reports | NG-ALG-01 inputs with `-json` and log `comp.out`; log `comp` | exact JSON bytes; JSON file name | JSON file; Structured list result |
| NG-REP-02 | lvs-reports | NG-ALG-02 inputs with `-json` | `badnets`/`badelements` JSON | JSON file |
| NG-REP-03 | lvs-reports | NG-PROP-01 inputs with `-json` | `properties` JSON | JSON file |
| NG-REP-04 | lvs-reports | net/cell names containing `%` and `"` (Q-LVS-12, Q-LVS-13) | comp.out, JSON validity | Two-column layout; JSON file |
| NG-REP-05 | lvs-reports | `log file x.log; log start; log put hello; log echo off; log end`; error cases | file contents, messages | Log and console routing |
| NG-PDK-01 | lvs-hierarchy | sky130A `sky130A_setup.tcl`, Magic-extracted `inv` (`ext2spice lvs`) vs xschem netlist | full comp.out, JSON | all lvs-* (integration) |
| NG-PDK-02 | lvs-hierarchy | gf180mcuD setup, standard-cell block with fill/decap cells | full comp.out | ignore command; Compare queue construction |
| NG-PDK-03 | lvs-hierarchy | `ihp-sg13g2_setup.tcl` (Q-LVS-31), resistor/capacitor PCells with series/parallel segments | full comp.out | Series/parallel network reconciliation |
| NG-CMD-01 | lvs-command | Hefesto only: `lvs -layout inv inv.spice sky130A` vs netgen on the `ext2spice lvs` file | equality of result and canonical comp.out | Layout netlist from extraction |

## As authored (M9 R3, task 3.5)

Written by the LVS implementation team (branch `m9-round3-lvsinput`) from the specs only, with `tools/golden/authoring/lvs_cases.py`; rerun it after editing a case. Each case is `cases/<ID>/` with `case.toml` (tool `netgen`, `provenance`, requirement tags of its capability) and `input/`:
- `input/run.tcl` is what the capture runs (`netgen -batch source input/run.tcl` in `/work`): each command is wrapped in `catch` and prints `[<n>] code <c>: <value>`, so returned values are on stdout and an error never stops the script; it ends with `exit 0`.
- Outputs: stdout, stderr and the exit status (`bytes`), plus `comp.out` and, with `-json`, `comp.json` (`lvs-canonical`) when the case runs `lvs`; NG-REP-01 and NG-REP-05 also keep the log files they write.
- Replay runs the same script through `hefesto-cli` once the `lvs` command exists (`requires_commands = ["lvs"]`).

Differences from the table above:
- NG-PROP-11 is tagged `lvs-setup-file` ("property topology and derive" is a heading of that capability).
- Requirement names of other capabilities were dropped from NG-ALG-01..03 (Final result, Device mismatch report, Net mismatch report), NG-SET-04 (Permutable pins in matching) and NG-SET-13 (Two-column layout): a case carries one capability.
- NG-PDK-01 compares a hand-written netlist in the shape of an `ext2spice lvs` netlist (not one extracted by Magic) with a schematic-style netlist; NG-PDK-02 and -03 use small hand-written blocks with the PDK device names. Their setup file is staged unmodified from the pinned data set (`sky130-xschem`, `gf180mcu-netgen`, `ihp-netgen`).
- Left out after the first capture (run 46050777576), because netgen 1.5.327 crashed (exit 139): `ignore <unknown keyword>` in NG-SET-10 (Q-LVS-22's uninitialised option index; with it, the wildcard form `ignore shorted {* decap}` also crashed in run 46054592794, so NG-SET-10 names each circuit), and a stray `.ends` before any definition in NG-READ-04 (now after the first definition). Hefesto's behaviour for both is covered by unit tests. NG-REP-05 compares only the console streams (netgen wrote no `x.log` in `/work`).
- NG-REP-04 uses `%%` in net names only: netgen writes report names through printf (Q-LVS-12), and the first `%x` in a cell name printed an address that changed between captures. Hefesto writes names literally, so its `comp.out` differs from netgen's there by design (Q-LVS-12 fix).
- Cases that run `lvs` more than once separate the runs with `reset`, which netgen 1.5.327 does not have: its line records `invalid command name "reset"` and the next `lvs` reads its netlists again, as the captures show. Replay must give the same error.
- The stdout comparison masks netgen's banner time (`regex:compiled on ...`).
- Capture-blocked: NG-SET-10, because netgen 1.5.327 still exits 139 on its `ignore` commands with the wildcard and the unknown keyword removed (run 46058187767); NG-PDK-02 and -03 until `tools/golden/goldenlib/external.py` accepts the `gf180mcu-netgen` and `ihp-netgen` sets; NG-CMD-01 until the capture can produce Magic's `ext2spice lvs` netlist of the inv layout before netgen runs.
