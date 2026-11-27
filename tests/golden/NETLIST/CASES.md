# NETLIST golden test-case inventory

These cases will be captured later by running Magic 8.3.694 (commit fc5daa9e) headless. Each case runs in a fresh session (see Q-NETLIST-16). Inputs are hand-written `.ext`/`.res.ext` files plus a minimal technology file whose extraction style defines the devices used. Captured netlists are compared byte for byte, except for coupling `C` lines, which are compared order-insensitively after renumbering (Q-NETLIST-1).

As authored (m7-extraction task 2.5): `tools/golden/authoring/netlist_cases.py` writes the 44 rows with hand-written inputs (NL-FLAT, NL-SPICE, NL-HIER, NL-SIM, NL-CHK). The technology is `authoring/nltest.tech` (style `nltest`; resistance classes diffusion, poly, metal1; devices `nfet` GND!, `pfet` VDD!, `rppd`, `mimcapm`, `dn`, `npn`), and every `.ext` input says `tech nltest` and `style nltest`. A case with several runs keeps each run's netlist as `<cell>.<tag>.spice` (`.cdl`, `.sim`, `.al`, `.nodes`). Every case collects `*.spice`, `*.cdl`, `*.sim`, `*.al` and `*.nodes` by name pattern. A case whose `.ext` inputs have a `cap` record compares SPICE and CDL netlists with `spice-cap` (`spice-cap:VSUB` and `spice-cap:VSUB,c2/VSUB` where the netlist names the substrate: NL-FLAT-14, NL-HIER-06) and `.sim` files with `sim-cap`; the other cases use `bytes` (design D21). NL-FLAT-04 has no `vsource` record: Magic's reader leaves its token count uninitialised for that class (EFread.c:313-340), and the V line is reached through `short voltage` (NL-FLAT-06, NL-HIER-06). NL-CHK-01 is an external-command case (design D15): `input/command.toml` runs the role `extcheck` four times (default thresholds, `-c 0 -r 0`, `-C -r 0`, and no arguments for the usage text and exit status 1). `authoring/m7_cases.py --not-authored` lists the rows not authored yet (NL-RES, NL-RESNET: task 17.3).

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| NL-FLAT-01 | ext-flattening-read | `.ext` with comments, backslash continuation, quoted names, a prefix keyword (`kill`), an ambiguous `s` line and an unknown keyword; `ext2spice` | netlist plus warnings | Line tokenization; Keyword recognition |
| NL-FLAT-02 | ext-flattening-read | two cells with `scale 1000 1000 5` vs `scale 1000 1000 10`; `ext2spice` | `.option scale` value and device w/l | Scale record |
| NL-FLAT-03 | ext-flattening-read | duplicate `node a` records with different C and area/perimeter; `ext2spice cthresh 0` | node C line and as/ad values | Node and substrate records |
| NL-FLAT-04 | ext-flattening-read | one device of each class (mosfet, fet, bjt, devres None/model, devcap None/model, devcaprev, diode, pdiode, ndiode, subckt, rsubckt, msubckt, csubckt, dsubckt, veriloga, vsource) | full netlist | Device records; Device instance names |
| NL-FLAT-05 | ext-flattening-read | `parameters nfet l=l w=w a1=as p1=ps x=xpos*0.5 l1=lt+2` plus devices with verbatim params; scale on and off | parameter text order and values | Parameters records; Device parameter output |
| NL-FLAT-06 | ext-flattening-read | ports `A` and `B` shorted by `equiv`; run with `short none`, `short resistor` and `short voltage` | netlists plus warning | Equivalence, merge and kill records |
| NL-FLAT-07 | ext-flattening-read | `equiv net net_uq1` between ports; case-only port difference `Vdd`/`VDD` | netlist, no warning | Equivalence, merge and kill records |
| NL-FLAT-08 | ext-flattening-read | `use` 2-D array `u[0:1:10][0:2:20]`, `merge` with `[0:1]` ranges; `ext2spice` | instance-derived node names | Use, port, cap, resist, attr and connect records; Instance and array element names |
| NL-FLAT-09 | ext-flattening-read | `resist a b 1234` with `scale 1 1 1`; `ext2spice` | `R` line value (expect 1.2345) | Use, port, cap, resist, attr and connect records |
| NL-FLAT-10 | ext-flattening-hierarchy | cell with 40 devices at distinct positions (no table growth), and a 120-device variant (crosses the 96-entry growth point of a 32-bucket table) | device line order | Container iteration order |
| NL-FLAT-11 | ext-flattening-hierarchy | parent with uses `u3`, `u1`, `u2`, `ua`, `b10` in file order | flat device order and hierarchical section order | Container iteration order; Definition traversal order |
| NL-FLAT-12 | ext-flattening-hierarchy | two instances each with label `vdd!`, not connected; `global on` vs `global off` | node names plus "not fully connected" text | Global name merging |
| NL-FLAT-13 | ext-flattening-hierarchy | `.ext` with `killnode` on a device terminal and on a coupling node | netlist without the killed items | Kills |
| NL-FLAT-14 | ext-flattening-hierarchy | coupling `cap` to the substrate node and between nodes; cthresh 0 | ground C sum and coupling lines | Coupling capacitance flattening |
| NL-FLAT-15 | ext-flattening-hierarchy | node with area/perimeter per class and `resistclasses`; `ext2sim rthresh 0` | `R` lumped value | Lumped node resistance |
| NL-FLAT-16 | ext-flattening-hierarchy | legacy `fet` records with 2 and 3 terminals and `ext:l=`/`ext:w=` gate attributes with `-s` symbols | w/l in `.sim` | Device length and width |
| NL-FLAT-17 | ext-flattening-names | nodes merged via `equiv` with names `a`, `b#`, `x/y`, `0`, `VSS`, `bb` | canonical names in SPICE | Name preference order |
| NL-FLAT-18 | ext-flattening-names | names containing `,` `=` `[` `]` `.` `#` `!` in leaf and prefix; formats spice3, hspice, cdl; `-t !#,=[` | node text | SPICE output name conversion; Sim output name conversion |
| NL-FLAT-19 | ext-flattening-read | top with `style nosuch` and `version 7.9` using a child whose `resistclasses` differ; `ext2spice cthresh 0` | warnings, zero area/perimeter, netlist | Technology, style, version and resistance classes (added by the audit, task 17.2) |
| NL-FLAT-20 | ext-flattening-hierarchy | `ext2spice -c 0.5`, `-C`; `ext2sim -R -s L=4 -S syms` (a line without `=`); `ext2spice -T`; `ext2spice -p input:input/sub` (last) | netlists, messages | Common netlister arguments (added by the audit, task 17.2) |
| NL-SPICE-01 | netlist-spice-options | `ext2spice` option queries and sets, including prefixes, `lvs`, `default` | reported values (Tcl results) | Command options |
| NL-SPICE-02 | netlist-spice-options | inverter; formats spice2, spice3, hspice, ngspice, cdl | full netlists and file names | File header and scale card; Node names per dialect; Run arguments and output file |
| NL-SPICE-03 | netlist-spice-options | values 0, 1e-19, 0.9999e-13, 1e-13, 1.0001e-10, 1e-7, 0.01, 999.9, 1000, 1e9 as capacitors and lengths | SI text | SI value formatting |
| NL-SPICE-04 | netlist-spice-options | hierarchical HSPICE with deep names (>15 chars) | `x<n>/` names, `z@` names, dictionary | Node names per dialect |
| NL-SPICE-05 | netlist-spice-options | flat with `subcircuits top on`, `auto` and `off`; cell with ports | presence of the `.subckt` wrapper | Flat netlist structure; Subcircuit header line |
| NL-SPICE-06 | netlist-spice-options | cell with 30 long port names, cell name starting with a digit, and `parameters :cell p=1` | wrapped `.subckt` line | Subcircuit header line |
| NL-SPICE-07 | netlist-spice-options | floating node with C, node attributes, ngspice and spice3, `-F`, `-B` | node lines | Parasitic and node lines |
| NL-SPICE-08 | netlist-spice-options | hierarchical with extraction-style substrates `VSUBS!`, `VDD!`, `errsub`; spice3 vs cdl | `.global` line | Global card in hierarchical mode |
| NL-SPICE-09 | netlist-spice-devices | MOSFETs sharing diffusion; scale on and off; terminal attributes `12,14`; `ext:aps` gate | `+  ad=` lines, asub/psub, `**devattr` | MOSFET lines; Terminal area and perimeter |
| NL-SPICE-10 | netlist-spice-devices | devres None and modeled, with tee on and off; rsubckt with and without substrate | R/X lines and node C changes | Resistor lines; Subcircuit-class device lines |
| NL-SPICE-11 | netlist-spice-devices | CDL with modeled R and C, unnamed `r`/`c` params, `s=` substrate param | value placement and ` /` | Resistor lines; Capacitor, diode, BJT and voltage source lines |
| NL-SPICE-12 | netlist-spice-devices | parallel nfets (equal W, unequal W, swapped S/D), parallel caps; merge none, conservative, aggressive | `M=` values, dropped devices, divided AP | Parallel device merging |
| NL-SPICE-13 | netlist-spice-devices | device with gate attribute name, inside instance `u1`; device with source attribute `D` | device name and swapped terminals | Device instance names; Device eligibility and annotations |
| NL-SPICE-14 | netlist-spice-devices | default substrate `VSUBS!` vs a non-default substrate net | substrate text | Substrate terminal text |
| NL-HIER-01 | netlist-spice-hierarchy | 3-level hierarchy; `ext2spice hierarchy on` | section order and contents | Definition traversal order; Section layout |
| NL-HIER-02 | netlist-spice-hierarchy | child net connected only via a parent `merge`; cthresh finite vs infinite with coupling into child | promoted ports | Port promotion |
| NL-HIER-03 | netlist-spice-hierarchy | abstract cell with blackbox on and off; primitive cell; wire-only cell | stub text, omissions | Omitted, stubbed and absorbed definitions |
| NL-HIER-04 | netlist-spice-hierarchy | top with no ports (auto) vs with ports; `lvs` preset | `* Top level circuit` vs `.subckt`, `.end` | Section layout |
| NL-HIER-05 | netlist-spice-hierarchy | 1-D and 2-D arrays of a cell with ports; renumber on and off | X lines | Subcircuit calls in hierarchical mode |
| NL-HIER-06 | netlist-spice-hierarchy | tee rsubckt, modeled resistor in scale mode, couplings with negative C, `short voltage` in two cells | hierarchical differences, V numbering | Hierarchical device and parasitic differences |
| NL-SIM-01 | netlist-sim | inverter whose node `out` is also named `y` (`equiv`); formats MIT, LBL, SU with alias off (the `=` line in the `.sim`); alias on (the `=` line in the `.al`); labels on | `.sim`, `.al`, `.nodes` | File structure and order; Device lines; Parasitic and node records |
| NL-SIM-02 | netlist-sim | bjt, diode, devres, devcap, subckt devices | device lines | Device lines |
| NL-SIM-03 | netlist-sim | SU with `ext:aps` gate attribute and shared diffusion | A_/P_/S_ fields | Transistor attributes |
| NL-SIM-04 | netlist-sim | `ext2sim` after `ext2spice` in the same session vs a fresh session, node `vdd!` | name trimming difference | Name conversion state |
| NL-SIM-05 | netlist-sim | `ext2sim extresist on` with a `.res.ext` | output file names and contents | Command options and defaults |
| NL-CHK-01 | netlist-extcheck | inverter plus a disconnected global net; `-c 0`, `-r 0`, `-C` | stdout | Counts and report format; Invocation |
| NL-RES-01 | extresist-command | long metal1 wire between an inverter output and two loads; default options | `.res.ext`, stdout totals | Run flow; Net selection and first cutoff; Overlay file grammar and order |
| NL-RES-02 | extresist-command | same; `threshold 0`, `mindelay 0`, `all`, `force`, `ignore`, `include` combinations | which nets appear | Command options; Net selection and first cutoff; Second cutoff after extraction |
| NL-RES-03 | extresist-command | `lumped on` with mindelay 0 and > 0 | `.res.lump` | Lumped resistance file |
| NL-RES-04 | extresist-command | `attr` records `res:skip`, `res:force`, `res:drive` | processed nets | Input model |
| NL-RES-05 | extresist-output | net `vdd!` and net with a missing device terminal | node names, killnode, orphan patch | Naming of network nodes |
| NL-RES-06 | extresist-output | `ext2spice extresist on` on NL-RES-01 | final netlist (R values with the .0005 offset) | Consumption by netlisters |
| NL-RES-07 | extresist-output | `extresist fasthenry 1e9` on a subcircuit with 3 ports and a multi-via contact | `.fh` file | FastHenry geometry file |
| NL-RES-08 | extresist-output | `extresist geometry` | element list (`element` command output) | Geometry centerlines |
| NL-RESNET-01 | extresist-network | straight wire 100×4 in metal1, two ends | resistor value and rnode caps | Tile resistors; Path resistance and capacitance distribution |
| NL-RESNET-02 | extresist-network | L-shaped and T-shaped wires (concave corners) | network topology | Net geometry collection; Network nodes and breakpoints |
| NL-RESNET-03 | extresist-network | via arrays of 1×1, 2×1, 3×3 cuts and undersized contact | contact resistor values | Contact resistors |
| NL-RESNET-04 | extresist-network | chain of small segments with minres 1000 vs 0; port-to-port short segment | simplified network | Network simplification |
| NL-RESNET-05 | extresist-network | branching net with gate loads; mindelay 1, 10 ps | pruned branches and Tdi debug output | Elmore delay and pruning |
| NL-RESNET-06 | extresist-network | `extresist skip poly` on a metal-poly net | network extent | Net geometry collection |
| NL-RESNET-07 | extresist-network | metal1 L-shapes `[0,100]×[0,10] ∪ [0,30]×[10,100]` and `∪ [0,10]×[10,100]`; `extresist` with `debug` tile dump | fractured tiles; network | Tile fracturing |
| NL-RESNET-08 | extresist-network | T-, U- and staircase-shaped wires with a driver and ports at the ends | `.res.ext` network | Tile fracturing; Breakpoints; Tile processing order |
| NL-RESNET-09 | extresist-network | 7×3 tile with breakpoints at (0,1) and (7,2); `[0,4]×[0,100]` north-south tile | resistor values and node capacitances | Tile resistors |
| NL-RESNET-10 | extresist-network | diffusion wire next to a transistor on one edge, two edges, and on top+bottom; single-breakpoint device tile | network, node capacitances | Near-device cutting (Q-NETLIST-43) |
| NL-RESNET-11 | extresist-network | L-wire with origin and port (scenario values); parallel straps; a 3-node loop forming a triangle | `.res.ext` values | Construction-time reductions (Q-NETLIST-44) |
| NL-RESNET-12 | extresist-network | same net extracted first and second in a session; net with equal-weight paths | node order and path resistances | Path resistance and capacitance distribution (Q-NETLIST-46) |
| NL-RESNET-13 | extresist-network | fanout net with gate loads, mindelay 1 ps and 100 ps | pruned network | Elmore delay and pruning |
| NL-RESNET-14 | extresist-network | networks with <10 and ≥10 sub-tolerance resistors, equal values, port-to-port shorts | `.res.ext` after final pass | Network simplification; Final scrunch pass (Q-NETLIST-26, Q-NETLIST-47) |
| NL-RESNET-15 | extresist-network | high-fanout node (≥10 resistors) inside a resistor triangle, run twice | network stability | Construction-time reductions (Q-NETLIST-45) |

<!-- BEGIN requirement audit (generated by tools/golden/authoring/m7_cases.py --audit) -->

## Requirement audit (m7-extraction task 17.2)

Every exact and behavioral requirement of the catalogue's capabilities, with the golden cases that show it and the unit-test files that carry its tag. A requirement without a case states why a unit test is enough. Generated by `tools/golden/authoring/m7_cases.py --audit` (authoring/m7_audit.py).

| Capability | Requirement | Class | Golden cases | Unit tests | Note |
|---|---|---|---|---|---|
| ext-flattening-hierarchy | Container iteration order | exact | NL-FLAT-10, NL-FLAT-11 | tests/unit/extflat/flatten_test.cpp |  |
| ext-flattening-hierarchy | Common netlister arguments | behavioral | NL-FLAT-16, NL-FLAT-18, NL-FLAT-20 | tests/unit/extflat/args_names_test.cpp |  |
| ext-flattening-hierarchy | Flat node construction | exact | NL-FLAT-02, NL-FLAT-08, NL-FLAT-11 | tests/unit/extflat/flatten_test.cpp, tests/unit/extflat/netlist_flatten_test.cpp, tests/unit/extflat/smoke_test.cpp |  |
| ext-flattening-hierarchy | Node merge semantics | exact | NL-FLAT-06, NL-FLAT-07, NL-FLAT-17 | tests/unit/extflat/flatten_test.cpp, tests/unit/extflat/smoke_test.cpp |  |
| ext-flattening-hierarchy | Kills | exact | NL-FLAT-13 | tests/unit/extflat/flatten_test.cpp, tests/unit/extflat/netlist_flatten_test.cpp |  |
| ext-flattening-hierarchy | Global name merging | exact | NL-FLAT-12 | tests/unit/extflat/flatten_test.cpp, tests/unit/extflat/netlist_flatten_test.cpp, tests/unit/extflat/round3_test.cpp |  |
| ext-flattening-hierarchy | Coupling capacitance flattening | behavioral | NL-FLAT-14 | tests/unit/extflat/netlist_flatten_test.cpp |  |
| ext-flattening-hierarchy | Visit orders for output | exact | NL-FLAT-09, NL-FLAT-10, NL-FLAT-11, NL-HIER-01 | tests/unit/def/write/visit_order_test.cpp, tests/unit/extflat/flatten_test.cpp, tests/unit/extflat/netlist_flatten_test.cpp, tests/unit/extflat/round3_test.cpp, tests/unit/extflat/smoke_test.cpp, tests/unit/netlist/sim/visit_order_test.cpp |  |
| ext-flattening-hierarchy | Lumped node resistance | exact | NL-FLAT-15 | tests/unit/extflat/netlist_flatten_test.cpp |  |
| ext-flattening-hierarchy | Device length and width | exact | NL-FLAT-16 | tests/unit/extflat/netlist_flatten_test.cpp |  |
| ext-flattening-hierarchy | One-level flattening for hierarchical output | exact | NL-HIER-01, NL-HIER-02, NL-HIER-03 | tests/unit/extflat/netlist_flatten_test.cpp |  |
| ext-flattening-names | Hierarchical name structure | exact | NL-FLAT-08, NL-FLAT-18 | tests/unit/extflat/names_test.cpp |  |
| ext-flattening-names | Component hash | exact | NL-FLAT-10 | tests/unit/extflat/names_test.cpp |  |
| ext-flattening-names | Instance and array element names | exact | NL-FLAT-08 | tests/unit/extflat/names_test.cpp |  |
| ext-flattening-names | Name preference order | exact | NL-FLAT-17 | tests/unit/extflat/names_test.cpp, tests/unit/extflat/netlist_flatten_test.cpp |  |
| ext-flattening-names | Global and ground detection | behavioral | NL-FLAT-12 | tests/unit/extflat/names_test.cpp |  |
| ext-flattening-names | SPICE output name conversion | exact | NL-FLAT-18 | tests/unit/extflat/args_names_test.cpp |  |
| ext-flattening-names | Sim output name conversion | exact | NL-FLAT-18 | tests/unit/extflat/args_names_test.cpp |  |
| ext-flattening-read | Locating and recursively reading .ext files | behavioral | NL-FLAT-02, NL-FLAT-20, NL-HIER-01 | tests/unit/extflat/hardening_test.cpp, tests/unit/extflat/read_test.cpp, tests/unit/extflat/smoke_test.cpp |  |
| ext-flattening-read | Line tokenization | exact | NL-FLAT-01 | tests/unit/extflat/names_test.cpp, tests/unit/extflat/read_test.cpp |  |
| ext-flattening-read | Keyword recognition | exact | NL-FLAT-01 | tests/unit/extflat/names_test.cpp, tests/unit/extflat/read_test.cpp |  |
| ext-flattening-read | Scale record | exact | NL-FLAT-02 | tests/unit/extflat/netlist_flatten_test.cpp, tests/unit/extflat/read_test.cpp |  |
| ext-flattening-read | Node and substrate records | exact | NL-FLAT-03 | tests/unit/extflat/devices_test.cpp, tests/unit/extflat/names_test.cpp, tests/unit/extflat/read_test.cpp |  |
| ext-flattening-read | Device records | exact | NL-FLAT-04 | tests/unit/extflat/devices_test.cpp, tests/unit/extflat/read_test.cpp |  |
| ext-flattening-read | Parameters records | exact | NL-FLAT-05 | tests/unit/extflat/devices_test.cpp, tests/unit/extflat/read_test.cpp |  |
| ext-flattening-read | Equivalence, merge and kill records | exact | NL-FLAT-06, NL-FLAT-07 | tests/unit/extflat/devices_test.cpp, tests/unit/extflat/read_test.cpp |  |
| ext-flattening-read | Use, port, cap, resist, attr and connect records | exact | NL-FLAT-08, NL-FLAT-09 | tests/unit/extflat/devices_test.cpp, tests/unit/extflat/hardening_test.cpp, tests/unit/extflat/read_test.cpp, tests/unit/extflat/round3_test.cpp |  |
| ext-flattening-read | Technology, style, version and resistance classes | behavioral | NL-FLAT-19 | tests/unit/extflat/read_test.cpp |  |
| ext-flattening-read | Resistance-extraction overlay | exact | NL-RES-06, NL-SIM-05 | tests/unit/extflat/devices_test.cpp |  |
| extresist-command | Command options | behavioral | NL-RES-02 | tests/unit/extresist/command/options_test.cpp, tests/unit/extresist/output/geometry_run_test.cpp |  |
| extresist-command | Run flow | behavioral | NL-RES-01 | tests/unit/extresist/command/run_test.cpp |  |
| extresist-command | Input model | exact | NL-RES-04 | tests/unit/extresist/command/run_test.cpp, tests/unit/extresist/output/input_test.cpp |  |
| extresist-command | Net selection and first cutoff | exact | NL-RES-01, NL-RES-02 | tests/unit/extresist/command/cutoff_test.cpp, tests/unit/extresist/command/run_test.cpp |  |
| extresist-command | Second cutoff after extraction | exact | NL-RES-02 | tests/unit/extresist/command/run_test.cpp, tests/unit/extresist/output/geometry_run_test.cpp |  |
| extresist-command | Lumped resistance file | exact | NL-RES-03 | tests/unit/extresist/command/run_test.cpp |  |
| extresist-network | Net geometry collection | behavioral | NL-RESNET-02, NL-RESNET-06 | tests/unit/extresist/network/fracture_test.cpp |  |
| extresist-network | Tile fracturing | exact | NL-RESNET-07, NL-RESNET-08 | tests/unit/db/plane_stitch_test.cpp, tests/unit/extresist/network/fracture_test.cpp, tests/unit/extresist/network/golden_pin_test.cpp |  |
| extresist-network | Device annotation | behavioral | NL-RESNET-10 | tests/unit/extresist/network/build_test.cpp |  |
| extresist-network | Start tile and origin | exact | NL-RESNET-10 | tests/unit/extresist/network/build_test.cpp |  |
| extresist-network | Breakpoints | exact | NL-RESNET-02, NL-RESNET-08 | tests/unit/extresist/network/build_test.cpp, tests/unit/extresist/network/golden_pin_test.cpp |  |
| extresist-network | Tile processing order | exact | NL-RESNET-08 | tests/unit/extresist/network/build_test.cpp, tests/unit/extresist/network/golden_pin_test.cpp |  |
| extresist-network | Tile resistors | exact | NL-RESNET-01, NL-RESNET-09 | tests/unit/extresist/network/build_test.cpp, tests/unit/extresist/network/golden_pin_test.cpp |  |
| extresist-network | Near-device cutting | exact | NL-RESNET-10 | tests/unit/extresist/network/build_test.cpp |  |
| extresist-network | Contact resistors | exact | NL-RESNET-03 | tests/unit/extresist/network/build_test.cpp |  |
| extresist-network | Node merging | exact | NL-RESNET-11 | tests/unit/extresist/network/reduce_test.cpp |  |
| extresist-network | Construction-time reductions | exact | NL-RESNET-11, NL-RESNET-15 | tests/unit/extresist/network/build_test.cpp, tests/unit/extresist/network/golden_pin_test.cpp, tests/unit/extresist/network/reduce_test.cpp |  |
| extresist-network | Path resistance and capacitance distribution | exact | NL-RESNET-01, NL-RESNET-12 | tests/unit/extresist/network/reduce_test.cpp |  |
| extresist-network | Elmore delay and pruning | exact | NL-RESNET-05, NL-RESNET-13 | tests/unit/extresist/network/build_test.cpp, tests/unit/extresist/network/reduce_test.cpp |  |
| extresist-network | Network simplification | exact | NL-RESNET-04, NL-RESNET-14 | tests/unit/extresist/network/reduce_test.cpp |  |
| extresist-network | Final scrunch pass | exact | NL-RESNET-14 | tests/unit/extresist/network/reduce_test.cpp |  |
| extresist-output | Overlay file grammar and order | exact | NL-RES-01 | tests/unit/extresist/command/run_test.cpp, tests/unit/extresist/output/writer_test.cpp |  |
| extresist-output | Naming of network nodes | exact | NL-RES-05 | tests/unit/extresist/output/writer_test.cpp |  |
| extresist-output | Consumption by netlisters | exact | NL-RES-06 | tests/unit/extresist/output/writer_test.cpp |  |
| extresist-output | FastHenry geometry file | exact | NL-RES-07 | tests/unit/extresist/output/fasthenry_test.cpp, tests/unit/extresist/output/geometry_run_test.cpp |  |
| extresist-output | Geometry centerlines | behavioral | NL-RES-08 | tests/unit/extresist/output/fasthenry_test.cpp, tests/unit/extresist/output/geometry_run_test.cpp |  |
| netlist-extcheck | Invocation | behavioral | NL-CHK-01 | tests/unit/netlist/extcheck/extcheck_test.cpp |  |
| netlist-extcheck | Counts and report format | exact | NL-CHK-01 | tests/unit/netlist/extcheck/extcheck_test.cpp |  |
| netlist-sim | Command options and defaults | behavioral | NL-SIM-05 | tests/unit/netlist/sim/short_ports_test.cpp, tests/unit/netlist/sim/sim_command_test.cpp, tests/unit/netlist/sim/sim_writer_test.cpp |  |
| netlist-sim | File structure and order | exact | NL-SIM-01 | tests/unit/netlist/sim/sim_command_test.cpp, tests/unit/netlist/sim/sim_writer_test.cpp |  |
| netlist-sim | Device lines | exact | NL-SIM-01, NL-SIM-02 | tests/unit/netlist/sim/short_ports_test.cpp, tests/unit/netlist/sim/sim_writer_test.cpp |  |
| netlist-sim | Transistor attributes | exact | NL-SIM-03 | tests/unit/netlist/sim/sim_command_test.cpp, tests/unit/netlist/sim/sim_writer_test.cpp |  |
| netlist-sim | Parasitic and node records | exact | NL-SIM-01 | tests/unit/netlist/sim/sim_writer_test.cpp |  |
| netlist-sim | Name conversion state | behavioral | NL-SIM-04 | tests/unit/netlist/sim/sim_command_test.cpp |  |
| netlist-spice-devices | Device instance names | exact | NL-FLAT-04, NL-SPICE-13 | tests/unit/netlist/spice/device_lines_test.cpp |  |
| netlist-spice-devices | Device eligibility and annotations | exact | NL-SPICE-13 | tests/unit/netlist/spice/device_lines_test.cpp |  |
| netlist-spice-devices | MOSFET lines | exact | NL-SPICE-09 | tests/unit/netlist/spice/device_lines_test.cpp |  |
| netlist-spice-devices | Terminal area and perimeter | exact | NL-SPICE-09 | tests/unit/netlist/spice/device_lines_test.cpp |  |
| netlist-spice-devices | Device parameter output | exact | NL-FLAT-05 | tests/unit/netlist/spice/device_lines_test.cpp |  |
| netlist-spice-devices | Resistor lines | exact | NL-SPICE-10, NL-SPICE-11 | tests/unit/netlist/spice/device_lines_test.cpp, tests/unit/netlist/spice/short_ports_test.cpp |  |
| netlist-spice-devices | Capacitor, diode, BJT and voltage source lines | exact | NL-SPICE-11 | tests/unit/netlist/spice/device_lines_test.cpp |  |
| netlist-spice-devices | Subcircuit-class device lines | exact | NL-SPICE-10 | tests/unit/netlist/spice/device_lines_test.cpp |  |
| netlist-spice-devices | Substrate terminal text | exact | NL-SPICE-14 | tests/unit/netlist/spice/device_lines_test.cpp |  |
| netlist-spice-devices | Parallel device merging | exact | NL-SPICE-12 | tests/unit/netlist/spice/device_lines_test.cpp |  |
| netlist-spice-hierarchy | Definition traversal order | exact | NL-FLAT-11, NL-HIER-01 | tests/unit/netlist/spice/hier_test.cpp |  |
| netlist-spice-hierarchy | Port promotion | exact | NL-HIER-02 | tests/unit/netlist/spice/hier_test.cpp |  |
| netlist-spice-hierarchy | Omitted, stubbed and absorbed definitions | exact | NL-HIER-03 | tests/unit/netlist/spice/hier_test.cpp |  |
| netlist-spice-hierarchy | Section layout | exact | NL-HIER-01, NL-HIER-04 | tests/unit/netlist/spice/hier_test.cpp |  |
| netlist-spice-hierarchy | Subcircuit calls in hierarchical mode | exact | NL-HIER-05 | tests/unit/netlist/spice/hier_test.cpp |  |
| netlist-spice-hierarchy | Hierarchical device and parasitic differences | exact | NL-HIER-06 | tests/unit/netlist/spice/hier_test.cpp |  |
| netlist-spice-options | Command options | behavioral | NL-SPICE-01 | tests/unit/netlist/spice/hier_test.cpp, tests/unit/netlist/spice/options_test.cpp, tests/unit/netlist/spice/run_test.cpp |  |
| netlist-spice-options | Run arguments and output file | behavioral | NL-SPICE-02 | tests/unit/netlist/spice/hier_test.cpp, tests/unit/netlist/spice/run_test.cpp |  |
| netlist-spice-options | File header and scale card | exact | NL-SPICE-02 | tests/unit/netlist/spice/flat_netlist_test.cpp |  |
| netlist-spice-options | SI value formatting | exact | NL-SPICE-03 | tests/unit/netlist/spice/si_format_test.cpp |  |
| netlist-spice-options | Node names per dialect | exact | NL-SPICE-02, NL-SPICE-04 | tests/unit/netlist/spice/flat_netlist_test.cpp |  |
| netlist-spice-options | Flat netlist structure | exact | NL-SPICE-05 | tests/unit/netlist/spice/flat_netlist_test.cpp |  |
| netlist-spice-options | Subcircuit header line | exact | NL-SPICE-05, NL-SPICE-06 | tests/unit/netlist/spice/flat_netlist_test.cpp, tests/unit/netlist/spice/run_test.cpp |  |
| netlist-spice-options | Subcircuit instance lines | exact | NL-HIER-01, NL-HIER-05 | tests/unit/netlist/spice/flat_netlist_test.cpp |  |
| netlist-spice-options | Parasitic and node lines | exact | NL-SPICE-07 | tests/unit/netlist/spice/flat_netlist_test.cpp |  |
| netlist-spice-options | Global card in hierarchical mode | exact | NL-SPICE-08 | tests/unit/netlist/spice/flat_netlist_test.cpp |  |

<!-- END requirement audit -->
