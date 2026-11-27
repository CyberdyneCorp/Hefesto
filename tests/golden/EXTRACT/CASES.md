# Golden Cases — EXTRACT

Test cases to be produced later by running Magic 8.3.694 (`fc5daa9e`) headless (`magic -dnull -noconsole`) and capturing the `.ext` output. Unless stated, use a minimal test technology `exttest.tech` (planes `well, active, metal1, metal2, metal3, mimcap`; types `pwell, nwell, dnwell, ndiff, pdiff, psd, nfet, pfet, poly, rpoly, metal1, metal2, metal3, mimcap, m1c(poly/metal1 contact), via(metal1/metal2)`; `planeorder well 0 active 1 metal1 2 metal2 3 metal3 4 mimcap 5`; `cscale 1`, `rscale 1`, `lambda 1`, 1:1 grid) with the rule lines named in each case.

As authored (m7-extraction tasks 2.6 and 17.1): the technology is `tools/golden/authoring/exttest.tech`, and `authoring/extract_cases.py` appends each case's rule lines to its style `exttest` (the style and grammar rows give a whole extract section). It adds the types `ndc` (ndiff/metal1 contact) and `ndiffh` (EX-027) and the identifier type `thkox` on a seventh plane `oxide` (`planeorder oxide 6`, EX-038), links `poly` to the gate types in the connect section, and has a cifoutput style of 0.5 µm per lambda (EX-058). Planes have no short alias, so generated node names read `metal1_<x>_<y>#`. A type that a case's rules do not mention and that connects to no other type is non-electrical, so each case mentions the types it paints. Each layout is an `input/<cell>.mag`; the `.ext` file is written next to it (`input/<cell>.ext`), and a case with several runs keeps each run as `input/<cell>.<tag>.ext`. The `.ext` files are collected by name pattern (`*.ext`), not listed. `authoring/m7_cases.py --not-authored` lists the rows not authored yet. Compare `.ext` files per Q-EXTRACT-5/Q-EXTRACT-7: node/device/use records as sets within their class, `cap` records as unordered pairs with summed values, `merge` records as name sets with values.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| EX-001 | extract-file-format | empty style; one metal1 rect; `extract all` | full `.ext` header lines | Header records; Lexical conventions |
| EX-002 | extract-capacitance | `areacap metal1 30`, `perimc metal1 space 20`; metal1 (0,0)-(10,4) | node record C = 1760; repeat with `cscale 1000` → 1.76 | Area capacitance to substrate; Perimeter capacitance |
| EX-003 | extract-capacitance | same rules; metal1 triangle in 4×4 split tile | node C = 500 | Perimeter capacitance (diagonal) |
| EX-004 | extract-capacitance | `overlap metal2 metal1 20`, `areacap metal2 10`; metal2 over metal1 10×10, separate labels | node C of metal2, `cap` value 2000 | Overlap coupling with shielding |
| EX-005 | extract-capacitance | `overlap metal3 metal1 8 metal2`; metal3/metal1 10×10, metal2 10×4 between | `cap` metal3–metal1 = 480; metal3 node C | Overlap coupling with shielding |
| EX-006 | extract-capacitance | `sidewall metal1 space space metal1 40`, `sidehalo 4`, `extract no fringehalo`; two parallel wires length 10 sep 2 | `cap` = 200 | Sidewall coupling value; Sidewall halving |
| EX-007 | extract-capacitance | EX-006 with offset `0.5` | `cap` = 160 | Sidewall coupling value (offset, float) |
| EX-008 | extract-capacitance | three parallel wires spacing 2, halo 10 | no A–C cap | Nearest-neighbor shadowing |
| EX-009 | extract-capacitance | `sideoverlap metal2 space metal1 6`, `perimc metal2 space 9`, legacy model; metal2 10×10 over larger metal1 | `cap` 240; metal2 node C reduced by 360 | Legacy sideoverlap |
| EX-010 | extract-capacitance | EX-009 with `overlap metal2 metal1 20`, fringehalo on, `sidehalo 10` | `cap` ≈ 202.56 + overlap; node C | Fringe-halo sideoverlap |
| EX-011 | extract-capacitance | metal3 edge over a metal1 plate with `sideoverlap metal3 space metal1 6 metal2` and a metal2 shield covering half of the fringe region | cap and node C | Fringe-halo sideoverlap shielding |
| EX-012 | extract-capacitance | EX-010 with in-plane metal2 neighbor at distance 3 | node C reduction (blocked fringe) | Nearest-neighbor shadowing (substrate removal) |
| EX-013 | extract-capacitance | `extract do corners` / `extract do corners 0.5`; lone metal2 rect; L-shaped metal2 (one concave corner) over metal1 plate | node C and cap deltas vs. no corners | Corner fringe capacitance; Corner fringe coupling |
| EX-014 | extract-capacitance | `extract no capacitance`; EX-004 layout | node C = 0, cap present | Capacitance option gating |
| EX-015 | extract-capacitance | `extract no coupling`; EX-004 layout | no `cap` records | Capacitance option gating |
| EX-016 | extract-capacitance | `defaultareacap metal1 metal1 well 30` with `substrate *psd,space,pwell well`, metal1 over pwell tied to substrate | no cap to substrate node; node C | Coupling to the substrate node |
| EX-017 | extract-resistance-lumped | `resist metal1 80`, `rscale 1`; metal1 10×2 and 4×4 | R = 400 and 80; with `rscale 1000` R = 0 | Lumped resistance formula; Resistance output rounding |
| EX-018 | extract-resistance-lumped | two classes poly/metal1 with m1c contact node | per-class area/perim pairs | Per-class area and perimeter |
| EX-019 | extract-nodes-and-labels | unlabeled nodes at negative coordinates; split tile right side | generated names `metal1_n10_n6#`, `...r#` | Node reference point and generated name |
| EX-020 | extract-nodes-and-labels | node with labels in order int1, A(port 1), B(port 2) | name B, port records, equiv lines | Node name selection; Port records; Alias output |
| EX-021 | extract-nodes-and-labels | node with first label `x@` then `a`, `b` | name `b`, attr record | Name selection quirk (Q-EXTRACT-4); Node and attribute records |
| EX-022 | extract-nodes-and-labels | label on boundary of two nodes; floating label over space | attachment target; sticky node record | Label attachment |
| EX-023 | extract-nodes-and-labels | `extract no aliases` vs `do aliases`, node with labels a, b | equiv presence | Alias output |
| EX-024 | extract-nodes-and-labels | two unconnected `net1` and two unconnected `vdd!` labels; `extract do unique` on `ex`; then, on `exb` (the same layout, which Magic's permanent step-1 renames of Q-EXTRACT-6 do not reach), `extract unique #` (`vdd!` kept, `net1` warned), then `extract unique` | `.ext` names `_uq0`; feedback counts; layout labels after each step | Unique naming of duplicate labels; Duplicate label warning |
| EX-025 | extract-nodes-and-labels | pwell abutting psd with well–psd connect | single node | Node connectivity (cross-plane halo) |
| EX-026 | extract-devices | `device mosfet nfet nfet ndiff None GND!`; 2×4 nfet with labeled s/d/g | exact device line of the spec scenario | Terminal discovery; MOSFET-family length and width; Terminal output |
| EX-027 | extract-devices | asymmetric mosfet `... ndiff,ndc ndiffh pwell VSUB` with drain on left | terminal order not sorted | Device declaration parsing; Terminal output and ordering |
| EX-028 | extract-devices | annular (ring) nfet, inner 4×4, ring width 2 | L=2, W=24 | Effective length and width |
| EX-029 | extract-devices | bent (L-shaped) gate with different lengths | L, W and "multiple lengths" message | Effective length and width |
| EX-030 | extract-devices | MOS capacitor (source and drain tied) | L, W | Boundary lists (split) |
| EX-031 | extract-devices | `resist rpoly 2000`, `device resistor None rpoly poly`, 10×2 resistor | device line with value 10 | Resistor dimensions and value |
| EX-032 | extract-devices | serpentine rpoly resistor with model name; multi-tile | L, W | Resistor dimensions (multi-tile, side lists) |
| EX-033 | extract-devices | `device capacitor None mimcap metal2 10 2`; 10×10 mim over metal2 12×12 | value 0.6, terminal `144,48` | Capacitor dimensions and value; Terminals off the device plane |
| EX-034 | extract-devices | modeled capacitor non-rectangular (L-shape) | "W area/W" output | Capacitor dimensions and value |
| EX-035 | extract-devices | `device diode dn ndiff pwell a=area p=pj` | params a, p; substrate field (Q-EXTRACT-2) | Diode records; Device declaration parsing |
| EX-036 | extract-devices | msubcircuit with `l=l w=w*1e6 c=cg r=rr` and devresist area/perimeter | `parameters` record and device params | Parameters records; Device parameter values |
| EX-037 | extract-devices | two records with `l<5` / `l>=5` ranges; gates of length 2 and 8 | chosen model per gate | Model selection by parameter ranges |
| EX-038 | extract-devices | identifier layer variant `+thkox`; substrate-layer variants | chosen models; error when no match | Device record selection |
| EX-039 | extract-devices | nfet with diffusion on one side only; `extract warn no fets` | copied terminal, feedback count | Missing terminals and rejected devices |
| EX-040 | extract-devices | two fingers sharing diffusion | terminal area/perim halved | Terminal area, perimeter and sharing |
| EX-041 | extract-devices | `$` and `^` attribute labels, `mult=2^` | attribute output and param | Terminal output; Device parameter values |
| EX-042 | extract-devices | cell property `device` = `primitive m=1`; and `device mosfet nmod 0 0 2 4 4 2 extra` | primitive/property device lines | Device cell property |
| EX-043 | extract-devices | mimcap duplicated in child at same location | parent has no device | Subcell device priority |
| EX-044 | extract-devices | legacy `fet` line | `fet` record | Device record header and substrate field |
| EX-045 | extract-substrate | `substrate *psd,space,pwell well $SUB -dnwell`, nfet in dnwell island and outside | node list, device substrate fields | Isolated substrate regions; Device substrate terminals |
| EX-046 | extract-substrate | metal-only cell with `substrate space well VSUB` | `substrate "VSUB" 0 0 -1073741817 ...` | Global substrate node discovery; Substrate record |
| EX-047 | extract-substrate | top with two standard-cell instances (1-D array of 3) | `merge "x[1]/VSUB" ...` names | Subcell substrate connections |
| EX-048 | extract-substrate | `LEFview` cell | `abstract`, no `substrate` record | Substrate record; Cell properties |
| EX-049 | extract-hierarchy | parent metal1 over identical child metal1, labels b / c1/a | merge line values | Merge records and pass-2 adjustments; Name unification order |
| EX-050 | extract-hierarchy | parent metal2 crossing child metal1 (different nets) | hierarchical `cap` | Hierarchical coupling records |
| EX-051 | extract-hierarchy | parent poly overlapping child metal1 on shared plane, no connect | feedback error | Interaction processing and connections |
| EX-052 | extract-hierarchy | unlabeled child node touched by parent | `I1/metal1_10_4#` name | Hard-way node names |
| EX-053 | extract-hierarchy | 1×4 array abutting elements; 2-D 2×2 array with diagonal overlap | array merge names with ranges | Array interactions |
| EX-054 | extract-hierarchy | `extract do labelcheck` sticky parent label over child | merge VDD | Sticky labels across the hierarchy |
| EX-055 | extract-hierarchy | large cell spanning several step squares (`step 20`) with coupling across square borders | number and sum of `cap` records | Interaction halo and step squares |
| EX-056 | extract-hierarchy | cell tree with an empty (label-only) child | no `.ext` / `use` for empty child | Bottom-up extraction order |
| EX-057 | extract-tech-section | style variants `(),(si)`; `variants` conditional lines | values per style | Extraction styles and variants |
| EX-058 | extract-tech-section | `units microns` with known cifoutput scale; areacap, perimc, sidewall offset, sidehalo, step | `scale` line, extracted values | Micron conversion |
| EX-059 | extract-tech-section | `scalegrid 1 2` with lambda style | `scale` line and values | Scaling to internal units |
| EX-060 | extract-tech-section | tech `format 33` vs `format 35`; `extract do` listing after load and after style change | fringehalo setting | Fringe model default chosen from tech format |
| EX-061 | extract-tech-section | missing planeorder lines; `noplaneordering` | warnings, default orders | Plane ordering |
| EX-062 | extract-tech-section | malformed lines (too many args, `cscale abc`, `resist m1 1.5`) | error messages | Extract section line grammar; Resistance classes |
| EX-063 | extract-tech-section | `defaultperimeter`/`defaultsideoverlap` with substrate default type | resulting caps on a test layout | Default capacitance shorthands |
| EX-064 | extract-commands | `extract` twice without edits; edit child; run again | which files rewritten (mtime) | Incremental extraction |
| EX-065 | extract-commands | `extract cell myinv` on selected instance | `myinv.ext` exists, no child files | Extract all, cell, parents and showparents |
| EX-066 | extract-commands | `extract path ext/` with missing directory | file location | Output path and file location |
| EX-067 | extract-commands | `extract do` with no args at startup | option listing | Option defaults; Extraction options |
| EX-068 | extract-commands | `extract length driver d`, `receiver r`, straight wire | `distance` record | Path length extraction |
| EX-069 | extract-commands | `extract halo 8` then extract; `extract style` change then extract | halo used | Style, halo and step size |
| EX-070 | extract-file-format | rotated/mirrored instances and arrays | `use` records | Use records; Record grammar |
| EX-071 | extract-commands | before any cell: `extract all` (no window or box), `extract style`; after `load`: `extract nosuch`, `extract s`, `extract st` (ambiguous), `extract STY`, `extract list style`, `extract listall style`, `extract list path`, `extract path`; then `extract all` | error texts, Tcl results, `.ext` | Command syntax and option matching (added by the audit, task 17.2) |
| EX-072 | extract-tech-section | `overlap metal2 metal1 20`, `sidewall metal1 space space metal1 40`, `sidehalo 4`, no fringehalo; m1c contact and a metal1 wire under a metal2 plate | `cap` records of the contact node | Contact residues and coupling table completion (added by the audit, task 17.2) |
| EX-073 | extract-tech-section | `disconnect pwell psd`, `connect metal1 metal2`; EX-025's well and tap, metal1 under metal2 with no contact | node records | Connectivity overrides (added by the audit, task 17.2) |

<!-- BEGIN requirement audit (generated by tools/golden/authoring/m7_cases.py --audit) -->

## Requirement audit (m7-extraction task 17.2)

Every exact and behavioral requirement of the catalogue's capabilities, with the golden cases that show it and the unit-test files that carry its tag. A requirement without a case states why a unit test is enough. Generated by `tools/golden/authoring/m7_cases.py --audit` (authoring/m7_audit.py).

| Capability | Requirement | Class | Golden cases | Unit tests | Note |
|---|---|---|---|---|---|
| extract-capacitance | Capacitance option gating | exact | EX-014, EX-015 | tests/unit/extract/cap/substrate_caps_test.cpp |  |
| extract-capacitance | Area capacitance to substrate | exact | EX-002, EX-003 | tests/unit/extract/cap/substrate_caps_test.cpp |  |
| extract-capacitance | Perimeter capacitance to substrate | exact | EX-002, EX-003 | tests/unit/extract/cap/substrate_caps_test.cpp |  |
| extract-capacitance | Corner fringe capacitance to substrate | exact | EX-013 | tests/unit/extract/cap/compat_branches_test.cpp, tests/unit/extract/cap/corners_test.cpp |  |
| extract-capacitance | Overlap coupling with shielding | exact | EX-004, EX-005 | tests/unit/extract/cap/overlap_test.cpp |  |
| extract-capacitance | Sidewall edge enumeration | exact | EX-006 | tests/unit/extract/cap/sidewall_test.cpp |  |
| extract-capacitance | Nearest-neighbor shadowing along an edge | exact | EX-008, EX-012 | tests/unit/extract/cap/sideoverlap_test.cpp, tests/unit/extract/cap/sidewall_test.cpp |  |
| extract-capacitance | Sidewall coupling value | exact | EX-006, EX-007 | tests/unit/extract/cap/sidewall_test.cpp |  |
| extract-capacitance | Legacy sideoverlap (fringe directly below the edge) | exact | EX-009 | tests/unit/extract/cap/compat_branches_test.cpp, tests/unit/extract/cap/sideoverlap_test.cpp |  |
| extract-capacitance | Fringe-halo sideoverlap | exact | EX-010, EX-011, EX-012 | tests/unit/extract/cap/compat_branches_test.cpp, tests/unit/extract/cap/cr_atan_test.cpp, tests/unit/extract/cap/sideoverlap_test.cpp |  |
| extract-capacitance | Corner fringe coupling | exact | EX-013 | tests/unit/extract/cap/corners_test.cpp |  |
| extract-capacitance | Coupling to the substrate node | exact | EX-016 | tests/unit/extract/cap/coupling_output_test.cpp |  |
| extract-capacitance | Coupling record output | exact, behavioral | EX-004, EX-005, EX-006, EX-015 | tests/unit/extract/cap/coupling_output_test.cpp |  |
| extract-commands | Command syntax and option matching | behavioral, informative | EX-071 | tests/unit/core/m7_scaffold_test.cpp, tests/unit/extract/command/options_test.cpp |  |
| extract-commands | Incremental extraction | behavioral | EX-064 | tests/unit/extract/command/run_test.cpp, tests/unit/extract/file/atomic_output_test.cpp |  |
| extract-commands | Extract all, cell, parents and showparents | behavioral | EX-065 | tests/unit/extract/command/run_test.cpp |  |
| extract-commands | Cell properties affecting extraction | exact | EX-042, EX-048 | tests/unit/extract/command/run_test.cpp, tests/unit/extract/hier/extract_cell_flat_test.cpp |  |
| extract-commands | Extraction options | behavioral | EX-067 | tests/unit/extract/command/options_test.cpp |  |
| extract-commands | Option defaults | exact | EX-067 | tests/unit/extract/command/options_test.cpp |  |
| extract-commands | Warnings | behavioral | EX-024, EX-039 | tests/unit/extract/command/options_test.cpp |  |
| extract-commands | Style, halo and step size | behavioral | EX-069 | tests/unit/extract/command/options_test.cpp |  |
| extract-commands | Output path and file location | behavioral | EX-066 | tests/unit/extract/command/options_test.cpp, tests/unit/extract/command/run_test.cpp, tests/unit/extract/file/atomic_output_test.cpp |  |
| extract-commands | Unique command and temporary unique option | exact, behavioral | EX-024 | tests/unit/extract/command/run_test.cpp |  |
| extract-commands | Path length extraction | exact | EX-068 | tests/unit/extract/command/options_test.cpp, tests/unit/extract/length/length_test.cpp |  |
| extract-devices | Device regions | exact | EX-026, EX-028, EX-029, EX-030 | tests/unit/extract/device/devices_test.cpp |  |
| extract-devices | Subcell device priority | exact | EX-043 | tests/unit/extract/hier/subcell_test.cpp |  |
| extract-devices | Terminal discovery on the device plane | exact | EX-026, EX-027 | tests/unit/extract/device/devices_test.cpp |  |
| extract-devices | Device record selection | exact | EX-038 | tests/unit/extract/device/abut_probe_test.cpp, tests/unit/extract/device/devices_test.cpp |  |
| extract-devices | Terminals off the device plane | exact | EX-033 | tests/unit/extract/device/devices_test.cpp |  |
| extract-devices | Terminal area, perimeter and sharing | exact | EX-040 | tests/unit/extract/device/devices_test.cpp |  |
| extract-devices | Missing terminals and rejected devices | exact, informative | EX-039 | tests/unit/extract/device/devices_test.cpp |  |
| extract-devices | Device record header and substrate field | exact | EX-044 | tests/unit/extract/device/devices_test.cpp |  |
| extract-devices | MOSFET-family length and width | exact | EX-026 | tests/unit/extract/device/devices_test.cpp |  |
| extract-devices | Boundary lists for dimension computation | exact | EX-030 | tests/unit/extract/device/devices_test.cpp, tests/unit/extract/device/dimensions_test.cpp |  |
| extract-devices | Effective length and width of non-rectangular gates | exact | EX-028, EX-029 | tests/unit/extract/device/devices_test.cpp, tests/unit/extract/device/dimensions_test.cpp |  |
| extract-devices | Resistor dimensions and value | exact | EX-031, EX-032 | tests/unit/extract/device/devices_test.cpp |  |
| extract-devices | Capacitor dimensions and value | exact | EX-033, EX-034 | tests/unit/extract/device/devices_test.cpp, tests/unit/extract/device/dimensions_test.cpp |  |
| extract-devices | Diode records | exact | EX-035 | tests/unit/extract/device/devices_test.cpp, tests/unit/extract/device/dimensions_test.cpp |  |
| extract-devices | Device parameter values | exact | EX-036, EX-041 | tests/unit/extract/device/devices_test.cpp, tests/unit/extract/device/dimensions_test.cpp |  |
| extract-devices | Model selection by parameter ranges | exact | EX-037 | tests/unit/extract/device/dimensions_test.cpp |  |
| extract-devices | Terminal output and ordering | exact | EX-026, EX-027, EX-041 | tests/unit/extract/device/devices_test.cpp |  |
| extract-devices | Parameters records | exact | EX-036 | tests/unit/extract/device/dimensions_test.cpp |  |
| extract-devices | Device cell property | exact | EX-042 | tests/unit/extract/device/dimensions_test.cpp |  |
| extract-file-format | Lexical conventions | exact | EX-001 | tests/unit/extract/file/writer_test.cpp |  |
| extract-file-format | Record grammar | exact | EX-070 | tests/unit/extract/file/writer_test.cpp |  |
| extract-file-format | Header records | exact | EX-001 | tests/unit/extract/file/writer_test.cpp |  |
| extract-file-format | Use records | exact, behavioral | EX-070 | tests/unit/extract/file/writer_test.cpp |  |
| extract-file-format | Record order | exact, behavioral | EX-020, EX-043, EX-049, EX-068 | tests/unit/extract/file/writer_test.cpp, tests/unit/extract/hier/extract_cell_flat_test.cpp |  |
| extract-file-format | Reader constraints honored by the writer | exact | EX-001, EX-026 | tests/unit/extract/file/writer_test.cpp |  |
| extract-hierarchy | Bottom-up extraction order | exact, behavioral | EX-056 | tests/unit/extract/hier/extract_cell_flat_test.cpp |  |
| extract-hierarchy | Interaction halo and step squares | exact | EX-055 | tests/unit/extract/hier/subtree_test.cpp |  |
| extract-hierarchy | Interaction area | exact | EX-055 | tests/unit/extract/hier/subtree_test.cpp |  |
| extract-hierarchy | Hierarchical label names | exact | EX-049, EX-052 | tests/unit/extract/hier/subtree_test.cpp |  |
| extract-hierarchy | Interaction processing and connections (pass 1) | exact | EX-051 | tests/unit/extract/hier/captured_test.cpp, tests/unit/extract/hier/subtree_test.cpp |  |
| extract-hierarchy | Name unification order | exact | EX-049 | tests/unit/extract/hier/subtree_test.cpp |  |
| extract-hierarchy | Merge records and pass-2 adjustments | exact, behavioral | EX-049 | tests/unit/extract/hier/captured_test.cpp, tests/unit/extract/hier/subcell_test.cpp, tests/unit/extract/hier/subtree_test.cpp |  |
| extract-hierarchy | Hierarchical coupling records | exact, behavioral | EX-050 | tests/unit/extract/hier/subtree_test.cpp |  |
| extract-hierarchy | Hard-way node names | exact, informative | EX-052 | tests/unit/extract/hier/subtree_test.cpp |  |
| extract-hierarchy | Array interactions | exact | EX-053 | tests/unit/extract/hier/captured_test.cpp, tests/unit/extract/hier/subtree_test.cpp |  |
| extract-hierarchy | Sticky labels across the hierarchy | exact | EX-054 | tests/unit/extract/hier/subtree_test.cpp |  |
| extract-hierarchy | Hierarchical section order in the file | exact | EX-049, EX-050, EX-053 | tests/unit/extract/hier/extract_cell_flat_test.cpp, tests/unit/extract/hier/subtree_test.cpp |  |
| extract-nodes-and-labels | Node connectivity | exact | EX-025 | tests/unit/extract/node/pass_lookup_test.cpp, tests/unit/extract/node/regions_test.cpp |  |
| extract-nodes-and-labels | Node reference point and generated name | exact | EX-019 | tests/unit/extract/node/regions_test.cpp |  |
| extract-nodes-and-labels | Label attachment | exact | EX-022 | tests/unit/extract/node/labels_test.cpp |  |
| extract-nodes-and-labels | Label types by trailing character | exact | EX-021 | tests/unit/extract/node/labels_test.cpp |  |
| extract-nodes-and-labels | Node name selection | exact | EX-020, EX-021 | tests/unit/extract/node/labels_test.cpp |  |
| extract-nodes-and-labels | Port records | exact | EX-020 | tests/unit/extract/node/labels_test.cpp |  |
| extract-nodes-and-labels | Node and attribute records | exact | EX-021 | tests/unit/extract/node/labels_test.cpp |  |
| extract-nodes-and-labels | Alias output | exact | EX-020, EX-023 | tests/unit/extract/node/labels_test.cpp |  |
| extract-nodes-and-labels | Duplicate label warning | behavioral, informative | EX-024 | tests/unit/extract/node/labels_test.cpp |  |
| extract-nodes-and-labels | Unique naming of duplicate labels | exact | EX-024 | tests/unit/extract/node/unique_test.cpp |  |
| extract-resistance-lumped | Per-class area and perimeter of a node | exact | EX-018 | tests/unit/extract/cap/compat_branches_test.cpp, tests/unit/extract/cap/lumped_test.cpp |  |
| extract-resistance-lumped | Lumped resistance formula | exact | EX-017 | tests/unit/extract/cap/compat_branches_test.cpp, tests/unit/extract/cap/lumped_test.cpp |  |
| extract-resistance-lumped | Resistance output rounding | exact | EX-017 | tests/unit/extract/cap/lumped_test.cpp |  |
| extract-resistance-lumped | Area and perimeter values in records | exact | EX-018 | tests/unit/extract/cap/lumped_test.cpp |  |
| extract-resistance-lumped | Resistance class header | exact | EX-018 | tests/unit/extract/cap/lumped_test.cpp |  |
| extract-substrate | Backward-compatible mode without a substrate declaration | exact | EX-026, EX-027 | tests/unit/extract/device/substrate_test.cpp |  |
| extract-substrate | Isolated substrate regions | exact | EX-045 | tests/unit/extract/device/substrate_test.cpp |  |
| extract-substrate | Global substrate node discovery | exact | EX-046 | tests/unit/extract/device/substrate_test.cpp |  |
| extract-substrate | Substrate node naming | exact, behavioral | EX-045, EX-046 | tests/unit/extract/device/substrate_test.cpp |  |
| extract-substrate | Substrate record | exact | EX-046, EX-048 | tests/unit/extract/device/substrate_test.cpp |  |
| extract-substrate | Device substrate terminals | exact | EX-045 | tests/unit/extract/device/substrate_test.cpp |  |
| extract-substrate | Subcell substrate connections | exact | EX-047 | tests/unit/extract/hier/captured_test.cpp, tests/unit/extract/hier/subcell_test.cpp |  |
| extract-substrate | Substrate coupling relocation | exact | EX-016 | tests/unit/extract/device/substrate_test.cpp |  |
| extract-tech-section | Extract section line grammar and keyword matching | exact | EX-062 | tests/unit/extract/tech/ext_style_rules_test.cpp, tests/unit/extract/tech/height_view_test.cpp, tests/unit/extract/tech/style_view_test.cpp |  |
| extract-tech-section | Extraction styles and variants | exact | EX-057 | tests/unit/extract/tech/ext_style_scale_test.cpp, tests/unit/extract/tech/height_view_test.cpp, tests/unit/extract/tech/style_view_test.cpp |  |
| extract-tech-section | Style defaults | exact | EX-001, EX-067 | tests/unit/extract/tech/ext_style_rules_test.cpp, tests/unit/extract/tech/style_view_test.cpp |  |
| extract-tech-section | Units, lambda, cscale and rscale | exact | EX-002, EX-017, EX-058, EX-062 | tests/unit/extract/tech/ext_style_rules_test.cpp, tests/unit/extract/tech/height_view_test.cpp |  |
| extract-tech-section | Step size and side halo parsing | exact | EX-006, EX-055, EX-069 | tests/unit/extract/tech/ext_style_rules_test.cpp |  |
| extract-tech-section | Plane ordering | exact | EX-061 | tests/unit/extract/tech/ext_style_rules_test.cpp, tests/unit/extract/tech/height_view_test.cpp |  |
| extract-tech-section | Connectivity overrides | exact | EX-073 | tests/unit/extract/tech/ext_style_rules_test.cpp |  |
| extract-tech-section | Resistance classes and contact resistance | exact | EX-062 | tests/unit/extract/tech/ext_style_rules_test.cpp |  |
| extract-tech-section | Area and perimeter capacitance rules | exact | EX-002, EX-003 | tests/unit/extract/tech/ext_style_rules_test.cpp |  |
| extract-tech-section | Overlap capacitance rules | exact | EX-004, EX-005 | tests/unit/extract/tech/ext_style_rules_test.cpp |  |
| extract-tech-section | Sidewall coupling rules and halving | exact | EX-006, EX-007 | tests/unit/extract/tech/ext_style_rules_test.cpp |  |
| extract-tech-section | Sideoverlap rules | exact | EX-009, EX-010, EX-011 | tests/unit/extract/tech/ext_style_rules_test.cpp |  |
| extract-tech-section | Default capacitance shorthands | exact | EX-063 | tests/unit/extract/tech/ext_style_rules_test.cpp |  |
| extract-tech-section | Substrate declaration | exact | EX-016, EX-045, EX-046 | tests/unit/extract/tech/ext_style_rules_test.cpp, tests/unit/extract/tech/style_view_test.cpp |  |
| extract-tech-section | Device declaration parsing | exact | EX-027, EX-035 | tests/unit/extract/tech/ext_style_rules_test.cpp, tests/unit/extract/tech/ext_style_scale_test.cpp, tests/unit/extract/tech/style_view_test.cpp |  |
| extract-tech-section | Contact residues and coupling table completion | exact | EX-072 | tests/unit/extract/tech/ext_style_rules_test.cpp |  |
| extract-tech-section | Non-electrical types | exact, informative | EX-001 | tests/unit/extract/tech/ext_style_rules_test.cpp, tests/unit/extract/tech/style_view_test.cpp |  |
| extract-tech-section | Layer height and thickness defaults | behavioral | - | tests/unit/extract/tech/ext_style_scale_test.cpp, tests/unit/extract/tech/height_view_test.cpp | heights and thicknesses are not written to the .ext file and no extract command prints them (3-D view and other consumers read them); pinned in the HeightView tests |
| extract-tech-section | Antenna keywords parse-level reference | exact | MISC-ANT-01, MISC-ANT-02, MISC-ANT-03, MISC-ANT-04, MISC-ANT-05, MISC-ANT-06, MISC-ANT-07, MISC-ANT-08, MISC-ANT-09 | tests/unit/extract/tech/ext_style_rules_test.cpp, tests/unit/extract/tech/style_view_test.cpp |  |
| extract-tech-section | Micron conversion | exact | EX-058 | tests/unit/extract/tech/ext_style_scale_test.cpp, tests/unit/extract/tech/height_view_test.cpp |  |
| extract-tech-section | Scaling to internal units | exact | EX-059 | tests/unit/extract/tech/ext_style_scale_test.cpp, tests/unit/extract/tech/height_view_test.cpp |  |
| extract-tech-section | Fringe model default chosen from tech format | exact | EX-060 | tests/unit/extract/tech/ext_style_rules_test.cpp |  |

<!-- END requirement audit -->
