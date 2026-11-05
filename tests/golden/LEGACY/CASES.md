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
| LEG-CH-15 | router-channel | channel file (`route *file`) W=4 L=6: left pin 2 and right pin 2 net 1, no obstacles | ASCII result dump; painted wiring | Greedy router: column sequence; Greedy router: extension and end of channel |
| LEG-CH-16 | router-channel | channel file W=6: top pin net a and bottom pin net b in the same column with only track 3 free (others occupied); repeat with only track 2 free | result dump; error messages | Greedy router: feasible top and bottom connections |
| LEG-CH-17 | router-channel | channel file W=6: nets split on tracks (1,3) and (4,6) with no remaining pins | result dump | Greedy router: collapsing split nets |
| LEG-CH-18 | router-channel | channel file W=7 with split net on 2 and 6; `route jog 1` vs `route jog 3` | result dumps | Greedy router: reducing the range of split nets |
| LEG-CH-19 | router-channel | channel file with rising net (pin on top 3 columns ahead); `route steady 1` vs `route steady 5` | result dumps | Greedy router: rising and falling runs; Greedy router: vacating tracks |
| LEG-CH-20 | router-channel | channel file with 3 right pins on one net; `route end 0.5`, `1.0`, `5.0` | result dumps; error counts | Greedy router: nets, end distance and density; Greedy router: uncollapse near the end; Q-LEGACY-77 |
| LEG-CH-21 | router-channel | channel file with layer-1-only, layer-2-only and both-layer obstacle letters | result dump including contacts | Greedy router: column flags; Greedy router: extension and end of channel |
| LEG-CH-22 | router-channel | layout routed with `route` where the first attempt has errors: channel taller than wide | `.mag` paint; feedback list | Greedy router: orientation and retry |
| LEG-CH-23 | router-channel | 3 nets with equal bounding-box half-perimeter; `route` | order of routed nets (paint and segment ids via `*seeflags`); `.mag` | Priority heap ordering; Global routing |
| LEG-CH-24 | router-channel | two subcells forming a corridor whose column density saturates; `route` with 4 nets | `.mag`; feedback | Global router channel map; Global router path search |
| LEG-CH-25 | router-channel | crossing near an obstacle (hazard) with alternative clean crossing; `route` | chosen crossing (paint) | Global router crossing adjustment and cost |
| LEG-CH-26 | router-channel | L-shaped arrangement of three subcells with corner gaps of 8 vs 16; `channels` | saved feedback rectangles | Channel decomposition (xDist/yDist ties, Q-LEGACY-78) |
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
| LEG-MZ-13 | router-maze | scmos; empty cell; `iroute route` between two metal1 labels 200 apart horizontally, then `*mzroute dumpEstimates` over the area | estimate dump text; resulting `.mag` paint | Cost estimation; Path to paint conversion |
| LEG-MZ-14 | router-maze | scmos; opaque unexpanded subcell between start and destination (estimation on vs off via `iroute search`/`iroute estimate`) | routed paint; `iroute` statistics (blooms, points processed) | Cost estimation; Windowed bloom search |
| LEG-MZ-15 | router-maze | scmos; destination metal1 box inside a larger same-net metal1 area (expand endpoints on); `iroute route` | routed paint end point | Destination areas; Walk extension |
| LEG-MZ-16 | router-maze | scmos; start on m2contact terminal; route to metal2 label | routed paint (initial contact use) | Initial paths; Contact extension (Q-LEGACY-85) |
| LEG-MZ-17 | router-maze | scmos; route that must jog upward past a narrow gap; compare to mirrored downward case | routed paint (asymmetry) | Interesting points (Q-LEGACY-87) |
| LEG-MZ-18 | router-maze | scmos; two equal-cost alternative routes (symmetric obstacle) | routed paint (tie choice) | Heap ordering; Windowed bloom search |
| LEG-MZ-19 | router-maze | scmos; magnet hint paint parallel to the route and rotate paint over part of it | routed paint; final cost from `iroute verbosity 2` | Segment cost model; Hint, fence and rotate layers |
| LEG-MZ-20 | router-maze | scmos; `iroute` with window width 1000 and bloom limit 5 | stdout result code / partial route | Windowed bloom search |
| LEG-PL-01 | plow | metal1 (10,0)-(13,20), box (5,0)-(20,20); `plow right` | `.mag` | Box plow initial edges; Plow search rules |
| LEG-PL-02 | plow | two metal1 boxes separated by 2 (< rule 3); plow left box right by 7 | `.mag` | Plow search rules; Plowing limitations |
| LEG-PL-03 | plow | transistor (poly over diff) with metal contacts; plow through gate in all 4 directions | `.mag` ×4 | plowing technology section; Plow search rules (fixed, covered, drag, contacts) |
| LEG-PL-04 | plow | wire with jogs; `plow horizon 10` vs `plow nojogs` | `.mag` ×2 | plow command; Edge queue ordering and merging |
| LEG-PL-05 | plow | `plow boundary` small box then plow beyond it | `.mag`, stdout | Plow boundary |
| LEG-PL-06 | plow | selection of a subcell + wires; `plow selection right 10` | `.mag` | Selection plow |
| LEG-PL-07 | plow | jogged wire; `straighten left`; `plow straighten` then plow | `.mag` | Straightening |
| LEG-PL-08 | plow | layout with 45° (split-tile) paint; plow across it | `.mag`, any errors | Plowing limitations (Q-LEGACY-50) |
| LEG-PL-09 | plow | `plow no`, `plow bogus`, `plow horizon`, `plow jogs` | stdout/stderr | plow command |
| LEG-PL-10 | plow | DRC dump of plow rule tables (`*plow` debug "showrules" if available) for scmos | text | Plowing rules derived from DRC |
| LEG-PL-11 | plow | metal1 A (0,0)-(3,10), B (5,0)-(8,10) (gap 2) and C (14,0)-(17,10); box (-2,0)-(5,10); `plow right` | `.mag` | Plow search rules (impact formula); Umbra rules |
| LEG-PL-12 | plow | metal1 (10,0)-(18,20) (8 wide); box (5,0)-(20,20); `plow right` | `.mag` | Real width; Umbra rules |
| LEG-PL-13 | plow | metal1 A (0,0)-(3,10), C (5,11)-(8,20); box (-2,0)-(10,10); `plow right` with horizon 0 and with `plow nojogs` | `.mag` ×2 | Penumbra rules; Sliver rules; Jog horizon extension |
| LEG-PL-14 | plow | lone metal1 wire (10,-20)-(13,30); box (5,0)-(20,10); `plow horizon 50` vs `plow jogs` | `.mag` ×2 | Jog horizon extension |
| LEG-PL-15 | plow | scmos nfet with poly gate extension above and below and diff stub left of gate; plow right through the fet | `.mag` | Covered, fixed, drag and contact rules; Fixed-RHS penumbra rules |
| LEG-PL-16 | plow | m2contact with metal1/metal2 wires; plow right through its left side | `.mag` | Covered, fixed, drag and contact rules (contact coupling) |
| LEG-PL-17 | plow | metal1 label at (11,5) on metal1 (10,0)-(13,20); box (5,0)-(20,20); `plow right`; also `plow straighten` enabled | `.mag` labels | Result write-back; Q-LEGACY-104 |
| LEG-PL-18 | plow | boundary (0,0)-(28,20); metal1 (10,0)-(13,20) and (24,0)-(27,20); box (5,0)-(20,20); `plow right` | `.mag`, stdout, final box | Plow boundary |
| LEG-PL-19 | plow | boundary (10,0)-(40,20); material at x 2..5 left of it that gets dragged; plow right from box (0,0)-(20,20) | `.mag`, stdout, final box | Plow boundary (Q-LEGACY-99) |
| LEG-PL-20 | plow | subcell bbox (10,0)-(20,10); `plow right` with box (5,0)-(14,10) (move 4) and (5,0)-(15,10) (move 5); also metal1 wire left of an unmoved subcell pushed into it | `.mag` uses' transforms | Cell rules; Q-LEGACY-97 |
| LEG-PL-21 | plow | chain of 40 metal1 wires at minimum spacing spanning 2000 units; plow the first one right by 10 | `.mag` | Incremental loading of the working planes (Q-LEGACY-105) |
| LEG-PL-22 | plow | short (height 2) initial edge next to diffusion/poly with H > 2; plow right by 5 | `.mag` | Inside sliver rule |
| LEG-PL-23 | plow | ndiff next to poly with an illegal-adjacency edge rule; plow ndiff right under poly | `.mag` | Illegal edge rules |
| LEG-PL-24 | plow | Z-shaped and C-shaped metal1 jogs; `straighten left`, `straighten right`, `straighten up` | `.mag` ×3 | Straightening |
| LEG-PL-25 | plow | same layout plowed `up`, `down`, `left`, `right` (non-symmetric rules: edge rule, touching_illegal spacing) | `.mag` ×4 | Working representation and final geometry; Plowing rules derived from DRC (Q-LEGACY-101) |
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
