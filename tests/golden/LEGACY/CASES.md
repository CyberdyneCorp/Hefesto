# Golden Test Cases — LEGACY area

Cases to be captured later by running Magic 8.3.694 (commit fc5daa9e) headless (`magic -dnull -noconsole`). Unless stated, the technology is `scmos` (`scmos/scmos.tech.in`, which has `router`, `mzrouter`, `plowing` and `wiring` sections) and coordinates are internal units. Capture files go next to this document under `<ID>/`.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| LEG-CH-01 | router-channel | scmos; empty cell; `route tech`, `route settings` | stdout text | Derived router technology values; route command dispatch and parameters |
| LEG-CH-02 | router-channel | tech with `router` section having odd trailing field, negative separation, bad gridspacing | tech-load error messages; `route tech` | Router technology section grammar |
| LEG-CH-03 | router-channel | empty cell, box (0,0)-(40,24), origin 0; `channels` | feedback list (`feedback save`) | Routing area rounding; channels command |
| LEG-CH-04 | router-channel | one subcell bbox (16,16)-(32,32) in box (-4,-4)-(76,76); `channels` | saved feedback rectangles | Channel decomposition |
| LEG-CH-05 | router-channel | 3×2 array of identical subcells with gaps; `channels` | saved feedback rectangles | Channel decomposition (array elements, corner choice) |
| LEG-CH-06 | router-channel | two subcells with labels `a`,`b` on metal1, netlist {u0/a,u1/a}; `route` | resulting `.mag` paint of edit cell; stdout | Routing flow; Global routing; Greedy channel router; Channel paint-back geometry; Stem paint geometry |
| LEG-CH-07 | router-channel | same as 06 with `route metal` toggled off | `.mag` | Metal maximization |
| LEG-CH-08 | router-channel | same as 06 with `route vias 0` and `route vias 10` | `.mag` (two runs) | Metal maximization |
| LEG-CH-09 | router-channel | 6 nets crossing one channel with density > tracks | feedback list, stdout error count | Greedy channel router (density error, end connection errors) |
| LEG-CH-10 | router-channel | metal1 obstacle (10,10)-(13,13) inside channel origin (-8,-8); `route` with net around it | `.mag`, feedback | Channel obstacle map; Hazards |
| LEG-CH-11 | router-channel | terminal on ndiff with mazestems off, then on | feedback text; `.mag` | Terminal stems outside channels |
| LEG-CH-12 | router-channel | routed cell from LEG-CH-06; `route viamin` | stdout count; `.mag` | Via minimization |
| LEG-CH-13 | router-channel | `route end 2.5`, `route jog 3`, `route obstacle 0.5`, `route steady 2`, then route LEG-CH-06 layout | `.mag` | Greedy channel router (parameters) |
| LEG-CH-14 | router-channel | `route s`, `route bogus`, `route vias x` | stderr text | route command dispatch and parameters |
| LEG-GA-01 | router-gate-array | `garoute channel 0 0 40 24`, `garoute channel 16 16 80 80` | stdout/stderr | Channel definition and alignment |
| LEG-GA-02 | router-gate-array | gate-array layout with cell rows; `garoute generate h gen.cmd`, `garoute generate v gen2.cmd` | generated files | Channel definition generation |
| LEG-GA-03 | router-gate-array | `garoute route` with no channels; with no `garouter` style (modified tech) | stderr | Gate-array routing flow |
| LEG-GA-04 | router-gate-array | gate-array layout + generated channels + netlist; `garoute route` | `.mag`, feedback, stdout | Gate-array routing flow; Channel initialization; Internal stem grid line selection; Internal stem pin checks; Simple stems |
| LEG-GA-05 | router-gate-array | as 04 with `garoute warn` vs `nowarn` and an unreachable terminal | feedback lists | Terminal classification for stems |
| LEG-GA-06 | router-gate-array | terminals A,B on same grid line in an h river channel | `.mag` | Internal stem pin checks |
| LEG-GA-07 | router-gate-array | `route mazestems` on, LEG-CH-06 layout with pin buried under obstacles | `.mag`, feedback | Maze-routed stems |
| LEG-N2I-01 | router-net2ir | feedback file from `find` + `feedback save`, netlist with 2-, 3- and 1-terminal nets and a comment line | stdout and stderr of `net2ir` | Feedback file parsing; Netlist conversion output |
| LEG-N2I-02 | router-net2ir | wrong arg count; missing file | stderr, exit status | net2ir invocation |
| LEG-MZ-01 | router-maze | scmos; `iroute saveParameters p.ir` | file content | saveParameters file format; Width and spacing derivation from DRC |
| LEG-MZ-02 | router-maze | tech variants: statement before `style`, duplicate plane layer, bad contact line | tech-load messages; `iroute layers`, `iroute contacts` | mzrouter section grammar |
| LEG-MZ-03 | router-maze-iroute | two metal1 labels A,B 100 apart, no obstacles; `iroute route -slabel A -dlabel B` | `.mag`, stdout | Route results; Segment and contact cost model; Path to paint conversion |
| LEG-MZ-04 | router-maze | as 03 with a metal1 wall between A and B (forces m2 via m2contact) | `.mag` | Blockage model; Interesting points; Path to paint conversion |
| LEG-MZ-05 | router-maze | as 04 with `rotate` paint over the region | `.mag` | Hint, fence and rotate layers |
| LEG-MZ-06 | router-maze | as 03 with `magnet` paint offset from the straight line | `.mag` | Hint, fence and rotate layers |
| LEG-MZ-07 | router-maze | start inside `fence` paint, dest outside | stdout result | Blockage model (fence parity) |
| LEG-MZ-08 | router-maze | `iroute wizard window 0`, `wizard penalty 1.3`, `wizard bloomLimit 5`, then route 04 | stdout, `.mag` | Windowed bloom search; Search, verbosity, version and wizard parameters |
| LEG-MZ-09 | router-maze-iroute | `iroute layers`, `iroute layers * hCost`, `iroute contacts m2contact cost 99`, `iroute spacings`, `iroute spacings CLEAR` | stdout | Layer and contact parameter tables; Spacings subcommand |
| LEG-MZ-10 | router-maze-iroute | `iroute route -foo`; `-slabel nosuch`; start already connected to dest | stderr; Tcl result | iroute route options; Route results |
| LEG-MZ-11 | router-maze-iroute | modify params, `saveParameters`, reload tech, `source` file, `iroute layers` | stdout | saveParameters file format |
| LEG-MZ-12 | router-maze | `route` estimation off (`iroute wizard estimate no`) vs on for 04 | `.mag` | Cost estimation |
| LEG-PL-01 | plow | metal1 (10,0)-(13,20), box (5,0)-(20,20); `plow right` | `.mag` | Box plow; Plow search rules |
| LEG-PL-02 | plow | two metal1 boxes separated by 2 (< rule 3); plow left box right by 7 | `.mag` | Plow search rules; Plowing limitations |
| LEG-PL-03 | plow | transistor (poly over diff) with metal contacts; plow through gate in all 4 directions | `.mag` ×4 | plowing technology section; Plow search rules (fixed, covered, drag, contacts) |
| LEG-PL-04 | plow | wire with jogs; `plow horizon 10` vs `plow nojogs` | `.mag` ×2 | plow command; Edge queue processing |
| LEG-PL-05 | plow | `plow boundary` small box then plow beyond it | `.mag`, stdout | Plow boundary |
| LEG-PL-06 | plow | selection of a subcell + wires; `plow selection right 10` | `.mag` | Selection plow |
| LEG-PL-07 | plow | jogged wire; `straighten left`; `plow straighten` then plow | `.mag` | Straightening |
| LEG-PL-08 | plow | layout with 45° (split-tile) paint; plow across it | `.mag`, any errors | Plowing limitations (Q-LEGACY-50) |
| LEG-PL-09 | plow | `plow no`, `plow bogus`, `plow horizon`, `plow jogs` | stdout/stderr | plow command |
| LEG-PL-10 | plow | DRC dump of plow rule tables (`*plow` debug "showrules" if available) for scmos | text | Plowing rules derived from DRC |
| LEG-WR-01 | wiring-tool | metal1 (0,0)-(100,4); `wire type at 50 2` | stdout, box | Wire type selection |
| LEG-WR-02 | wiring-tool | after 01: `wire horizontal to 80 2`, `wire vertical to 82 40`, `wire leg to 10 40` | `.mag`, box after each | Wire legs |
| LEG-WR-03 | wiring-tool | after 02: `wire switch metal2 4`, `wire leg to 10 80` | `.mag` | Contact placement (wire switch, increment, decrement) |
| LEG-WR-04 | wiring-tool | tech with 9-field contact (extends) and `scalefactor 2`; `wire switch` | `.mag` | wiring technology section; Contact placement |
| LEG-WR-05 | wiring-tool | `wire increment layer`, `wire decrement width 2`, `wire values`, `wire width` | stdout | Contact placement; wire show, values and width |
| LEG-WR-06 | wiring-tool | `wire segment metal1 4 0 0 20 0`, with `-noendcap`, 45° path `0 0 20 20 40 20`, file form | `.mag` | wire segment |
| LEG-WR-07 | wiring-tool | `wire bogus`; `wire segment metal1 x 0 0 1 1`; `wire leg` before any `wire type` | stderr, `.mag` | wire command dispatch; Wire legs (Q-LEGACY-51) |
| LEG-NL-01 | netlist-window | read example `.net` (header, comments, separators, duplicate terminal); `savenetlist out` | `out.net`, stderr | Netlist file grammar; Netlist file writing; Netlist enumeration order |
| LEG-NL-02 | netlist-window | 200 terminal names in 40 nets (forces hash table growth); `savenetlist` | `.net` file | Netlist enumeration order |
| LEG-NL-03 | netlist-window | bad header file; missing file | stderr | Netlist file grammar; Netlist selection and file lookup |
| LEG-NL-04 | netlist-window | `add`, `dterm`, `dnet`, `joinnets`, `print`, undo/redo sequence | stdout, saved file | Netlist editing operations |
| LEG-NL-05 | netlist-window | partially wired layout with an open and a short; `verify` | stdout/stderr, feedback list | Verify |
| LEG-NL-06 | netlist-window | same layout; `cull` | stdout, saved netlist | Cull |
| LEG-NL-07 | netlist-window | `extract` over a wire touching 3 subcell labels, one already in a net | stdout, netlist | Extract, ripup, trace and show |
| LEG-NL-08 | netlist-window | `find in` under box; `feedback save` | feedback file | Extract, ripup, trace and show (find) |
| LEG-NL-09 | netlist-window | `ripup`, `ripup netlist` | `.mag` | Extract, ripup, trace and show |
| LEG-NL-10 | netlist-window | routing netlist build for LEG-CH-06 with a single-terminal net and missing terminal | stderr, feedback | Netlist resolution for routing |
| LEG-SIM-01 | irsim-interface | hierarchical cell: top with label `out`, subcell node `u1/n1` connected; select wire; `getnode` | stdout | getnode command forms; Hierarchical node names; Node name preference order |
| LEG-SIM-02 | irsim-interface | as 01 with `getnode alias on` | stdout (alias lines) | Hierarchical node names |
| LEG-SIM-03 | irsim-interface | net labelled `vdd!` in two subcells; `getnode` with globals on and off | stdout | Node name preference order; Hierarchical node names |
| LEG-SIM-04 | irsim-interface | select transistor gate poly and source diff; `getnode fast` | stdout (`@=g`, `@=s` names) | Fast node names |
| LEG-SIM-05 | irsim-interface | `getnode abort vdd!` then getnode on vdd net | stdout | Abort names |
| LEG-SIM-06 | irsim-interface | unlabelled node in subcell (generated `#` names) vs labelled alias | stdout | Node name preference order |
| LEG-SIM-07 | irsim-interface | `getnode bogus`, `getnode alias`, `getnode globals` | stdout/stderr | getnode command forms |
