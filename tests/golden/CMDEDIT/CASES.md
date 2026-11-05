# Golden Cases — CMDEDIT

These cases are captured later by running Magic 8.3.694 (commit `fc5daa9e`) headless with `magic -dnull -noconsole` and a Tcl script. Unless a case says otherwise, they use the reference technology `scmos` (`L0 = L1 = 1`; `U` taken from its default output style). Cases marked "tech: fine" use a technology with `U` = 10 nm and λ = 2 internal units. "Capture" lists what to record. Layout outputs are `.mag` files written with `save`, and scripting results are recorded with `puts [cmd]`.

## Command registration checklist (dbwind/DBWcommands.c)

Every command name registered for the layout client is either covered by a CMDEDIT capability or owned elsewhere.

| Command(s) | Owner |
|---|---|
| `box` | cmd-box |
| `grid`, `snap`, `units`, `scalegrid` | cmd-coordinates-and-units |
| `paint`, `erase`, `fill`, `corner`, `contact`, `polygon`, `splitpaint`, `spliterase`, `drop` | cmd-paint-erase |
| `select`, `what`, `identify`, `id`, `delete`, `copy`, `move`, `stretch`, `clockwise`, `rotate`, `sideways`, `upsidedown`, `orient`, `array` | cmd-selection |
| `load`, `xload`, `edit`, `down`, `getcell`, `get`, `dump`, `expand`, `unexpand`, `flatten`, `save`, `writeall`, `flush`, `cellname`, `instance`, `path`, `addpath`, `locking`, `archive`, `crash` | cmd-hierarchy |
| `label`, `setlabel`, `port`, `property`, `findlabel`, `goto`, `element` | cmd-labels-properties |
| `xor`, `random`, `shell`, `see`, `crosshair`, `findbox`, `feedback`, `straighten` (surface only), `*coord`, `*extract`, `*plow`, `*psearch`, `*showmem`, `*showtech`, `*tilestats`, `*tsearch`, `*watch`, `history`, `makesw`, `sgraph` | cmd-misc-edit |
| `drc` | elsewhere: drc-commands |
| `extract`, `ext` | elsewhere: extract-commands |
| `ext2spice`, `exttospice`, `ext2sim`, `exttosim`, `extresist-command` | elsewhere: netlist / extresist agents |
| `cif`, `calma`, `gds` | elsewhere: MASK area |
| `lef`, `def`, `antennacheck` | elsewhere: LEFDEF / antenna agent |
| `tech` | elsewhere: tech-loading-and-scaling |
| `plot`, `plow` (algorithm for `straighten`), `route`, `iroute`, `garoute`, `channels`, `wire`, `netlist`, `getnode`, `rsim`, `simcmd`, `startrsim`, `*garoute`, `*groute`, `*iroute`, `*mzroute`, `*seeflags` | elsewhere: plot/plow/routing/netlist/sim agents |
| `tool` (non-Tcl build only) | elsewhere: ui-* |

Window-client commands (`window`, `view`, `zoom`, `macro`, `undo`, `redo`, …) are not registered in DBWcommands.c and are owned by ui-*.

## Cases

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| CMDEDIT-COORD-01 | cmd-coordinates-and-units | tech: fine; `units internal`; `box values 0 0 {2 + 3 * 4} 1` | `box values` result | Coordinate expressions |
| CMDEDIT-COORD-02 | cmd-coordinates-and-units | tech: fine; units default, snap lambda; `box values 0 0 100+5 1` | `box values` (expect 110 for x) | Coordinate expressions; Q-CMDEDIT-1 |
| CMDEDIT-COORD-03 | cmd-coordinates-and-units | tech: fine; `box values 0 0 1um+2um 1um` | `box values` | Q-CMDEDIT-2 |
| CMDEDIT-COORD-04 | cmd-coordinates-and-units | tech: fine; `units internal`; widths `1.5um`, `0.005um`, `25nm`, `-0.005um`, `2.5`, `-2.5`, `1e2` via `box width X; box width` | each width result | Numeric literal parsing; Unit suffixes |
| CMDEDIT-COORD-05 | cmd-coordinates-and-units | `box width 5km`; `box width abc` | stderr text and resulting width | Unit suffixes; Numeric literal parsing |
| CMDEDIT-COORD-06 | cmd-coordinates-and-units | `grid 10 20 5 5`; `box values 2g 2g 3g 3g`; `grid what` | `box values`, `grid what` | Unit suffixes (grid); Grid command |
| CMDEDIT-COORD-07 | cmd-coordinates-and-units | fresh session: `units`; `units microns print`; `units`; `units internal`; `units`; `units default`; `units print`; `units` | each `units` result | Units command; Q-CMDEDIT-6 |
| CMDEDIT-COORD-08 | cmd-coordinates-and-units | fresh session: `snap internal` (stderr), `units`, `snap`; then `units lambda; snap user; snap; units` | results and warnings | Snap command |
| CMDEDIT-COORD-09 | cmd-coordinates-and-units | tech: fine; for each `units` mode × print flag: `box values 0 0 15 20000`, `box size`, `box values` | formatted strings | Reported value formatting |
| CMDEDIT-COORD-10 | cmd-coordinates-and-units | metal1 (−3,0)–(3,4), box (0,0)–(3,5); `scalegrid 1 2`; save; then `scalegrid 2 1`; save | stdout, both `.mag` files, box values | Scalegrid command |
| CMDEDIT-COORD-11 | cmd-coordinates-and-units | `grid multiple 300; grid multiple`; `grid 0 5`; `grid 0`; `grid state` | results and errors | Grid command; Q-CMDEDIT-7 |
| CMDEDIT-COORD-12 | cmd-coordinates-and-units | `fill t`, `fill no`, `fill ne` (directions) | stderr | Direction and position names |
| CMDEDIT-BOX-01 | cmd-box | `box values 10 10 20 30`; then `box width 5`, `box height -4`, `box position 0 -5`, `box size 7 8` (results after each via `box values`) | box values sequence | Box size and position setters; Box normalization |
| CMDEDIT-BOX-02 | cmd-box | box (10,10)–(20,20); `box grow center 2`; `box shrink n 30`; `box move ne 3`; `box move n 0` | box values after each | Box move, grow and shrink |
| CMDEDIT-BOX-03 | cmd-box | `box 0 0 100 50` with tech scmos and with tech fine | stdout report verbatim | Default box report; Setting the box directly |
| CMDEDIT-BOX-04 | cmd-box | box (0,0)–(10,10); `box north 5`; `box values` | stdout and box | Q-CMDEDIT-8 |
| CMDEDIT-BOX-05 | cmd-box | box (0,0)–(10,10); `box corner ne 20 30`; `box values` | box (Magic bug) | Box corner command; Q-CMDEDIT-9 |
| CMDEDIT-BOX-06 | cmd-box | `box remove; box exists; box width`; `box exists` before remove | results, stderr | Box queries; Box remove and help |
| CMDEDIT-BOX-07 | cmd-box | edit child placed rotated 90; root box (0,0)–(10,20); `box width -edit`; `box values -edit`; `box values 0 0 5 5 -edit; box values` | results | Edit-coordinate switch |
| CMDEDIT-BOX-08 | cmd-box | `box position 2000000000i 0`; `box values 0 0 2000000000i 1` | stderr, box | Box normalization and bounds; Q-CMDEDIT-12 |
| CMDEDIT-BOX-09 | cmd-box | `box values 0 0 10 10; box values 5 5 20 20; undo; box values` | box | Box state |
| CMDEDIT-PAINT-01 | cmd-paint-erase | empty cell, box (0,0)–(10,20); `paint poly,metal1`; save; `undo`; save | both `.mag` | Paint command; Common effects |
| CMDEDIT-PAINT-02 | cmd-paint-erase | `paint *,-metal2`, `paint -poly,metal1`, `paint metal1,connect`, `paint foo`, `paint labels`, `paint subcell` | `.mag`, stderr | Layer list syntax; Paint command |
| CMDEDIT-PAINT-03 | cmd-paint-erase | metal1+poly at (5,5)–(6,6); box (20,0)–(30,10); `paint pick 5 5`; then `paint pick 50 50` over painted box | `.mag` after each | Paint or erase by example |
| CMDEDIT-PAINT-04 | cmd-paint-erase | metal1/poly/label A on metal1 in box; `erase`; separately `erase poly`, `erase labels`, `erase space` | `.mag` after each | Erase command |
| CMDEDIT-PAINT-05 | cmd-paint-erase | three instances, two overlapping box; `erase subcell` | `.mag` | Erase command (subcells) |
| CMDEDIT-PAINT-06 | cmd-paint-erase | zero-area box on a point label, on a label border, and strictly inside a rect label; `erase labels` | which labels remain | Erase command; Q-CMDEDIT-20 |
| CMDEDIT-PAINT-07 | cmd-paint-erase | box (0,0)–(10,20), poly (2,−5)–(4,0), metal1 (6,5)–(8,8); `fill n`; also `fill n metal1`, `fill s`, `fill e`, `fill w` on variants | `.mag` | Fill command |
| CMDEDIT-PAINT-08 | cmd-paint-erase | box (0,0)–(20,20); wires entering each side; all 8 `corner D1 D2` combos; `corner n s`; a too-wide wire for the room error | `.mag`, stderr | Corner command |
| CMDEDIT-PAINT-09 | cmd-paint-erase | stacked contact in tech; `paint <stacked>`; `erase <stacked>`; `erase <user contact>` | `.mag` | Paint command; Erase command; Q-CMDEDIT-16 |
| CMDEDIT-PAINT-10 | cmd-paint-erase | `corner n e metal1 bevel` in root, and in child edit cell at offset | `.mag` of root and child | Corner command; Q-CMDEDIT-15 |
| CMDEDIT-PAINT-11 | cmd-paint-erase | metal1 (0,0)–(6,10), metal2 (3,0)–(10,10); `contact m2c`; then `contact erase m2c`; `contact metal1` | `.mag`, stderr | Contact command |
| CMDEDIT-PAINT-12 | cmd-paint-erase | `polygon metal1 0 0 10 0 0 10`; `polygon metal1 0 0 20 0 20 10 10 20 0 10`; odd coords; 7-word | `.mag`, stderr | Polygon command; Q-CMDEDIT-19 |
| CMDEDIT-PAINT-13 | cmd-paint-erase | box (0,0)–(10,10); `splitpaint D metal1 poly` for D in ne nw se sw n e s w center | `.mag` | Splitpaint command; Q-CMDEDIT-21 |
| CMDEDIT-PAINT-14 | cmd-paint-erase | metal1 over box; `spliterase sw metal1`; `spliterase ne` | `.mag` | Spliterase command |
| CMDEDIT-PAINT-15 | cmd-paint-erase | top with child containing dnwell; `select area dnwell; drop pwell`; also with edit cell a child | child `.mag`, stderr | Drop command; Q-CMDEDIT-17 |
| CMDEDIT-SEL-01 | cmd-selection | metal1 (0,0)–(20,20), box (5,5)–(10,10); `select area metal1; what -list`; `select less area metal1` variants | `what -list`, `select bbox` | Area and visible selection |
| CMDEDIT-SEL-02 | cmd-selection | labels VDD VSS IN; `select area labels V*; what -list` | result | Area and visible selection |
| CMDEDIT-SEL-03 | cmd-selection | metal1–via–metal2 net; `select at X Y` three times; `select bbox` and `what -list` after each | results | Point selection and cycling |
| CMDEDIT-SEL-04 | cmd-selection | L-shaped metal1 (0,0)–(100,10) ∪ (0,0)–(10,50); `select chunk` with box ll at (5,5) | `select bbox`, box values | Chunk selection; Box-anchored selection |
| CMDEDIT-SEL-05 | cmd-selection | nested cells T⊃a(A)⊃b(B), expanded; `select cell` ×3 at P; `select cell a/b`; `select top cell` | results, box | Cell selection |
| CMDEDIT-SEL-06 | cmd-selection | dnwell + nwell; `select area dnwell; select intersection ~nwell; what -list; select bbox` | results | Intersection selection |
| CMDEDIT-SEL-07 | cmd-selection | `select no area`, `select s area`, `select simple labels`, `select foo bar` | stdout/stderr, selection | Select qualifiers; Q-CMDEDIT-60 |
| CMDEDIT-SEL-08 | cmd-selection | selection of edit metal1 + child poly; `delete` | `.mag` of both cells, stderr | Delete command; Edit-cell restriction |
| CMDEDIT-SEL-09 | cmd-selection | metal1 selected, box = selection; `copy e 20`, `copy 5 7`, `copy ne 3 4`, `copy to 100 100`, `copy 10` | `.mag`, box, stderr | Copy and move displacement |
| CMDEDIT-SEL-10 | cmd-selection | `move 5 7`, `move w`, `move to 0 0`; and selection including child paint | `.mag`, box | Copy and move displacement |
| CMDEDIT-SEL-11 | cmd-selection | `move origin e 10` | `.mag`, box | Move origin |
| CMDEDIT-SEL-12 | cmd-selection | wire (0,0)–(30,4); select (20,0)–(30,4); `stretch e 10`; also `stretch n 5` with abutting poly | `.mag` | Stretch command |
| CMDEDIT-SEL-13 | cmd-selection | metal1 (10,10)–(30,20) + label + instance; `clockwise`, `clockwise 180`, `rotate -90`, `clockwise 90 -orig`, `clockwise 45`, `sideways`, `upsidedown` | `.mag`, box | Rotation and flip commands; Q-CMDEDIT-61 |
| CMDEDIT-SEL-14 | cmd-selection | instance selected; `orient X` for every name in table plus `180v`, `9` | instance transform in `.mag` | Orient command; Q-CMDEDIT-62 |
| CMDEDIT-SEL-15 | cmd-selection | instance inv bbox (0,0)–(10,20), box (0,0)–(12,25); `array 3 2`; `array -list count`; `array 0 2 3 1` | `.mag` array line, results | Array command; Q-CMDEDIT-63 |
| CMDEDIT-SEL-16 | cmd-selection | labels `D0` and `A[0][0]` selected; `array 4 1`, `array 1 3`, `array 2 2` | label texts/positions | Array command (label numbering) |
| CMDEDIT-SEL-17 | cmd-selection | various selections; `what`, `what -list`, `what -listall` | stdout and results | What command |
| CMDEDIT-SEL-18 | cmd-selection | `identify buf1` on child instance; duplicate; `identify a/b`; none selected | `.mag`, stderr | Identify command |
| CMDEDIT-SEL-19 | cmd-selection | `select pick; select move 20 0; select keep` | `.mag` | Floating selection |
| CMDEDIT-SEL-20 | cmd-selection | labeled net A–B; `select short A B`; `select bbox`; `select save sel1` | `sel1.mag`, results | Selection utility options |
| CMDEDIT-HIER-01 | cmd-hierarchy | edit cell with `inv_0`, `inv_2`; `getcell inv` | result id, `.mag` | Cell and instance naming rules; Getcell command |
| CMDEDIT-HIER-02 | cmd-hierarchy | `getcell pad child label P`; `getcell c 90`; `getcell c child ur parent ll`; each ORIENT keyword | `.mag` transforms, box | Placement reference syntax |
| CMDEDIT-HIER-03 | cmd-hierarchy | `dump blk` with box ll (50,50); `dump blk 90v` | `.mag`, `what -list` | Dump command |
| CMDEDIT-HIER-04 | cmd-hierarchy | `load newcell`; `load nosuch -fail`; `load a[1]`; `load x scaled 2` | stdout/stderr, `cellname list allcells` | Load command; naming rules |
| CMDEDIT-HIER-05 | cmd-hierarchy | `edit` on selected instance at translate(100,0); `paint metal1`; `edit x1/x2`; `down` | child `.mag`, window top | Edit command; Down command; Q-CMDEDIT-30 |
| CMDEDIT-HIER-06 | cmd-hierarchy | 3-level hierarchy; `flatten f1`; `flatten -nolabels f2`; `flatten -dotoplabels f3`; `flatten -dobox f4`; `flatten f1` again; `flatten -doinplace x1` | `.mag` of each | Flatten command; Q-CMDEDIT-31 |
| CMDEDIT-HIER-07 | cmd-hierarchy | `expand`, `expand surround`, `unexpand`, `expand toggle`, `xload top` | expansion state via `cellname`/display dump (verify method) | Expand and unexpand; Xload |
| CMDEDIT-HIER-08 | cmd-hierarchy | `cellname list children top`, `parents`, `allcells`, `topcells`, `exists nosuch`, `instance orientation -def`, `cellname rename a b`, `cellname writeable a no` | results | Cellname and instance commands |
| CMDEDIT-HIER-09 | cmd-hierarchy | `save adder` from (UNNAMED); `writeall force`; `flush a -noprompt` | files, stdout | Save; Writeall; Flush |
| CMDEDIT-HIER-10 | cmd-hierarchy | `path`; `path search +lib`; `addpath x`; `path cell`; `locking`; `locking disable; locking` | results | Search path commands; File locking command |
| CMDEDIT-HIER-11 | cmd-hierarchy | `archive writeall arch.mag`; new session `archive read arch.mag`; `archive write` (missing file) | archive file, cells | Archive command; Q-CMDEDIT-34 |
| CMDEDIT-LBL-01 | cmd-labels-properties | cell bbox (0,0)–(90,90); `label IN` with box at each of 9 regions | stored justification | Label command |
| CMDEDIT-LBL-02 | cmd-labels-properties | `label VDD n -metal2`; `label A 0 2 45 1 1 sw metal1`; `label "a b"`; `label x/y` | `.mag` rlabel/flabel lines, stderr | Label command; Label units |
| CMDEDIT-LBL-03 | cmd-labels-properties | two selected labels; each `setlabel` query and set; `setlabel -default size 2` then `label` | results, `.mag` | Setlabel command; Q-CMDEDIT-90, Q-CMDEDIT-91 |
| CMDEDIT-LBL-04 | cmd-labels-properties | ports 1 and 4; `port make` on C; `port make 4` on D; `port A class input`; `port connections`; `port renumber`; `port first/last/next` | `.mag` port lines, results | Port make, numbering and directions; Port attributes; Q-CMDEDIT-92 |
| CMDEDIT-LBL-05 | cmd-labels-properties | `property FIXED_BBOX "0 0 100 200"`, `property`, `property foo bar`, `property foo ""`, `property integer n 1 2`, `property compat` | results, `.mag` | Property command; Q-CMDEDIT-93 |
| CMDEDIT-LBL-06 | cmd-labels-properties | `findlabel OUT`; `findlabel -glob V*`; `findlabel x1/A 1` | box, results | Findlabel command |
| CMDEDIT-LBL-07 | cmd-labels-properties | `goto m1_n5_7#`; `goto x1/VDD`; `goto 1_11_14`; `goto nosuch -nocomplain` | box, results | Goto command |
| CMDEDIT-LBL-08 | cmd-labels-properties | `element add rectangle r1 1 0 0 10 10`; configure flags persistent; save/reload; `element names`; `element inbox` | results, `.mag` | Element command |
| CMDEDIT-MISC-01 | cmd-misc-edit | identical cells; `xor b`; overlapping metal1 offsets; different types overlapping; `xor -nolabels -novendor b` | `b.mag` | Xor command; Q-CMDEDIT-94, Q-CMDEDIT-95 |
| CMDEDIT-MISC-02 | cmd-misc-edit | `see no metal1; select visible`; `see allSame` | selection | See command |
| CMDEDIT-MISC-03 | cmd-misc-edit | `feedback add "x"`; `feedback add "p" solid 0 0 10 0 0 10`; `feedback count`; `feedback why`; `feedback save f.tcl`; `feedback clear x` | results, file | Feedback command |
| CMDEDIT-MISC-04 | cmd-misc-edit | box (0,0)–(200,100); `findbox zoom`; `view` bbox | view | Findbox command |
| CMDEDIT-MISC-05 | cmd-misc-edit | `random seed 42; random; random seed 42; random` | equality | Random command |
