# NETLIST golden test-case inventory

These cases will be captured later by running Magic 8.3.694 (commit fc5daa9e) headless. Each case runs in a fresh session (see Q-NETLIST-16). Inputs are hand-written `.ext`/`.res.ext` files plus a minimal technology file whose extraction style defines the devices used. Captured netlists are compared byte for byte, except for coupling `C` lines, which are compared order-insensitively after renumbering (Q-NETLIST-1).

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
| NL-SIM-01 | netlist-sim | inverter; formats MIT, LBL, SU; alias on and off; labels on | `.sim`, `.al`, `.nodes` | File structure and order; Device lines; Parasitic and node records |
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
