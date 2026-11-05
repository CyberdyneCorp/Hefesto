# Golden Test Cases — SCRIPT area

Run all cases with Magic fc5daa9e built with Tcl. Use `magic -dnull -noconsole -norc -T <tech> case.tcl` unless the case states otherwise. GUI cases (SCRIPT-GUI-*) need Tk under a virtual X server (Xvfb). Capture stdout and stderr separately, together with the Tcl result and status of each command (`catch {cmd} r; puts "[list $code $r]"`).

Techs: `scmos` (bundled), `sky130A` (open_pdks). PDK cases require the PDK files listed in Q-SCRIPT-9.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| SCRIPT-API-01 | scripting-api | scmos; `namespace eval magic {info commands ::magic::*}`; `info commands paint` | full list of registered commands; global import of `paint` | Command registration in the magic namespace; Global import of magic commands |
| SCRIPT-API-02 | scripting-api | `catch {magic::bo values} e` | code=1 and the error text | Command registration in the magic namespace |
| SCRIPT-API-03 | scripting-api | `array set a {x 1}; array get a`; select an instance arrayed 1×4, then `array -list count` | built-in result; `0 0 0 3` | Name collisions with Tcl built-ins |
| SCRIPT-API-04 | scripting-api | `catch {load /nonexistent/libfoo.so} e`; `load ../lib.v2/inv`; `load cells/inv.mag.gz` | which handler ran; error text; loaded cell | Name collisions with Tcl built-ins |
| SCRIPT-API-05 | scripting-api | `cellname list top`; `units MICRONS`; `tech layer *`; `tech lock poly; tech locked`; `cif scale out` | each result | Option keyword matching; Result formats |
| SCRIPT-API-06 | scripting-api | `catch {box frobnicate}`; `catch {load nosuch -fail -silent}`; `catch {magic::shownet}` | status codes, results, stderr | Command status and error signalling |
| SCRIPT-API-07 | scripting-api | `tag select`; `magic::tag add select {puts A}`; `magic::tag add select {puts B}`; `tag select`; `magic::tag select {}` | tag strings after each step | Command tag callbacks |
| SCRIPT-API-08 | scripting-api | `magic::tag box {puts "W=%W 1=%1 2=%2 r=%r"}`; `box`; `box values`; `box 0 0 10 10` | printed substitution for each call | Tag callback execution and substitution |
| SCRIPT-API-09 | scripting-api | `magic::tag paint {error boom}`; `catch {paint m1} e`; `what -list` | code=1, e=boom, paint present | Tag callback execution and substitution |
| SCRIPT-API-10 | scripting-api | `magic::tag cellname {set x 1}` vs `{%R}` form; `cellname list top` | the result seen by the caller in each case | Tag callback execution and substitution |
| SCRIPT-API-11 | scripting-api | `magic::tag paint {puts "%r"}` with a result containing spaces, quotes and brackets (use `cellname list top` with names `a b`, `x[0]`) | evaluated text or error (Q-SCRIPT-3) | Tag callback execution and substitution |
| SCRIPT-API-12 | scripting-api | GUI: `.layout1.pane.top.magic box values`; `.layout1.pane.top.magic bo values` | results; abbreviation accepted | Window-directed commands |
| SCRIPT-API-13 | scripting-api | print text with `$abc` (undefined) and `$VDD` (defined) via `echo`/engine messages, e.g. `cellname create a\$abc; cellname list allcells` | stdout bytes | Output, flushing and escaping |
| SCRIPT-API-14 | scripting-api | pipe `box 0 0 1 1\nbox values\nquit -noprompt\n` into `magic -dnull -noconsole` | both outputs present | Terminal input queue without console |
| SCRIPT-API-15 | scripting-api | load `lib/inv.mag` from /w, `cd /tmp`, `save inv` | path of written file | Directory change rebases cell paths |
| SCRIPT-API-16 | scripting-api | tech whose extract substrate is `$SUB`; extract with `SUB` set to `VSUBS` and then unset | substrate node name in .ext/.spice | Script variables read by the engine |
| SCRIPT-API-17 | scripting-api | `set PDKPATH /pdk/x`; `addpath $PDKPATH/lib`; `path search`; then `set PDKPATH /other`; `addpath $PDKPATH/lib2` | path strings (shows caching, Q-SCRIPT-5) | Script variables read by the engine |
| SCRIPT-API-18 | scripting-api | box (0,0)-(30,100), scale 0.005; `box values` under units default, internal, lambda, microns, grid (grid 10 20), each with print and noprint | exact strings | Numeric value formatting in results |
| SCRIPT-API-19 | scripting-api | `units`; `units lambda print`; `set u [units]; units internal; units {*}$u; units` | list results | Numeric value formatting in results |
| SCRIPT-API-20 | scripting-api | layout with instance `x1` (inv), arrayed `x2`, labels and ports; run every form in the result-format table | result string of each | Result formats of commands used by scripts |
| SCRIPT-API-21 | scripting-api | cell with one non-port label: `port first`, `port last`; add ports 3 and 7: `port 3 next`, `port 7 next` | -1, 0; 7, -1 | Result formats of commands used by scripts |
| SCRIPT-API-22 | scripting-api | `tech version` with a version containing spaces; `tech lambda`; `drc list count`; `drc list count total`; `drc listall why` | exact list structures | Result formats of commands used by scripts |
| SCRIPT-API-23 | scripting-api | `*bypass box values` then `select` cycling check; read the command counter through a log file | the command number is not incremented | Post-command bookkeeping |
| SCRIPT-API-24 | scripting-api | start without console; `info commands unknown`; `foo` with `tcl_foo` defined | delegation result | Unknown-command fallback for renamed built-ins |
| SCRIPT-PDK-01 | scripting-pdk-integration | `magic -dnull -noconsole -rcfile sky130A.magicrc t.tcl`, where t.tcl prints `$VDD $GND $SUB $MAGTYPE [tech name] [path search] [snap list]` | values | Typical PDK startup file contract; Startup files are scripts |
| SCRIPT-PDK-02 | scripting-pdk-integration | an rc file whose line 3 is `frobnicate` | the error message text; that later lines did not run | Startup files are scripts |
| SCRIPT-PDK-03 | scripting-pdk-integration | command line `a.gds b.lef c.def d.cif e.tcl f.mag g g.mag.gz h.xyz -foo` in wrapper mode and NULL mode | the handling messages and actions in order | Command-line file arguments; Command-line options in the script host |
| SCRIPT-PDK-04 | scripting-pdk-integration | `--version`, `--commit`, `--prefix` | stdout and exit code | Command-line options in the script host |
| SCRIPT-PDK-05 | scripting-pdk-integration | empty session, `PDK_ROOT=/pdk` (env) then as a Tcl var only; `load inv` from a sky130A file | tech loaded, magicrc sourced?, cell path message | Technology auto-discovery from layout files |
| SCRIPT-PDK-06 | scripting-pdk-integration | `set PDKPATH /pdk/sky130A`; place a library cell from `$PDKPATH/libs.ref/...`; save the parent | `use` path line in the .mag | PDK-relative paths in saved files |
| SCRIPT-PDK-07 | scripting-pdk-integration | sky130A: for each device in the menus, `magic::gencell sky130::<dev> X0` with defaults; `save` all | generated cell names, .mag files, properties | PDK device-script contract; Command surface used by device generators |
| SCRIPT-PDK-08 | scripting-pdk-integration | GUI sky130A: open two wrappers | menu button names and items per window | PDK device-script contract |
| SCRIPT-PDK-09 | scripting-pdk-integration | instrument a sky130A, gf180mcuD and sg13g2 generator run with a trace of every engine command invoked (via `trace add execution`) | command-form inventory | Command surface used by device generators |
| SCRIPT-PCELL-01 | pcell-toolkit | pure Tcl: `magic::normalize_value` over {2, 2.0, 2e0, 0.1500, 1.23456, -0.5, 0.0004, 10.5, 0x10, " 3 ", abc, inf} | outputs or errors | Parameter value normalization for naming |
| SCRIPT-PCELL-02 | pcell-toolkit | pure Tcl: `magic::murmur3_32` over {"", a, ab, abc, abcd, hello, 0.15;1;1;, "é;"} | hex values | Deterministic generated-cell naming |
| SCRIPT-PCELL-03 | pcell-toolkit | pure Tcl: `magic::get_gencell_hash` over 200 random dicts, including a dict with non-ASCII values | suffixes | Deterministic generated-cell naming |
| SCRIPT-PCELL-04 | pcell-toolkit | toy library `toy` with defaults `w 1 l 0.15 nf 1` that draws a w×l rectangle of poly plus a label; `magic::gencell toy::nfet M1 w 2`; `magic::gencell toy::nfet M2 -spice w 2.0` (convert is identity) | cell names, instance names, properties, box before/after, .mag text | gencell entry point dispatch; Create algorithm; Generated cell properties |
| SCRIPT-PCELL-05 | pcell-toolkit | toy: create M1, M2 with the same parameters; change M1 to `w 3`; then change M2 to `w 3` | cells present after each step, parents, orientation retained | Change algorithm |
| SCRIPT-PCELL-06 | pcell-toolkit | toy: create the same parameters twice in one session | label count in the generated cell (Q-SCRIPT-24) | Create algorithm |
| SCRIPT-PCELL-07 | pcell-toolkit | headless: change an existing instance non-interactively without a dialog | error text and suspend state (Q-SCRIPT-29) | Change algorithm |
| SCRIPT-PCELL-08 | pcell-toolkit | toy `_draw` raising `error bad layer` | stderr, cell and properties existence | Create algorithm |
| SCRIPT-PCELL-09 | pcell-toolkit | `magic::gencell toy::nosuch x`; `magic::gencell {}` with nothing selected; odd argument count | error texts | Library and device procedure contract; gencell entry point dispatch |
| SCRIPT-PCELL-10 | pcell-toolkit | `magic::gencell_makecell toy::nfet w 1 l 0.15 nf 1` | returned name, no instance | Non-placing cell generation |
| SCRIPT-PCELL-11 | pcell-toolkit | toy nocell device (`_draw` does getcell of `fixedcap`, arrayed 2×3, pitch 10 12); create, then open the dialog for edit | the instance, and nx ny pitchx pitchy in the dialog parameters | Parameter dictionary model |
| SCRIPT-PCELL-12 | pcell-toolkit | `magic::netlist_to_layout top.spice toy`, where top.spice has 2 subckts (one existing on disk), ideal C/R/L elements, bus pins `vout[0]`, continuation lines, `.command` block, `M=2` subcircuit call | generated cells, pin labels and port indices, box positions, saved files, stdout | SPICE netlist to layout seeding; Seeding a subcircuit cell |
| SCRIPT-PCELL-13 | pcell-toolkit | GUI: open the dialog for a new toy device; dump widget tree names, `magic::gencell_getparams`; change an entry with a dependency callback | parameter dict, callback order | Parameter dialog structure; Dialog builder API; Dialog dependencies and validation |
| SCRIPT-PCELL-14 | pcell-toolkit | GUI: dialog open, then `select cell M2` | the dialog title/instance after the select | Selection-driven dialog refresh and key binding |
| SCRIPT-BOX-01 | ui-box-stack-and-measure | units microns, scale 0.01, box (0,0)-(1,2)µm; `pushbox`; `set boxstack` | stack content, units preserved | Box stack push |
| SCRIPT-BOX-02 | ui-box-stack-and-measure | `pushbox {10 10 30 50}`; `popbox size`; `popbox`; `peekbox` on an empty stack; fresh session `popbox` | box after each step, results, error texts | Box stack pop and peek |
| SCRIPT-BOX-03 | ui-box-stack-and-measure | top with children a, b: `pushstack a; pushstack b; popstack; popstack; popstack` | window cell, view, editstack, error text | Edit stack push; Edit stack pop and clear |
| SCRIPT-BOX-04 | ui-box-stack-and-measure | GUI: `suspendall; suspendall; resumeall; resumeall; catch resumeall e` | updatedisplay calls, Winopts state, error text (Q-SCRIPT-15) | Nested redraw suspension |
| SCRIPT-BOX-05 | ui-box-stack-and-measure | units internal, box (0,0)-(100,20): `ruler hello`; box (0,0)-(20,100): `ruler`; `element inbox` and the configuration of each element | element names, coordinates, flags | Ruler annotation |
| SCRIPT-BOX-06 | ui-box-stack-and-measure | box (0,0)-(250,10): `measure`; `measure vertical`; `unmeasure`; repeat with units microns | text elements; leftovers after unmeasure | Measure and unmeasure |
| SCRIPT-BOX-07 | ui-box-stack-and-measure | `genkey poly`; `genkey m1 6` | paint areas, text elements | Layer key annotation |
| SCRIPT-BOX-08 | ui-box-stack-and-measure | GUI: run `label A12`, then press `+` twice | labels created; `macro list .` | Macro auto-increment |
| SCRIPT-GUI-01 | ui-gui-managers | GUI startup with no args; `winfo children .layout1.pane.top`; `windownames layout` | widget tree, window path | Wrapper window construction |
| SCRIPT-GUI-02 | ui-gui-managers | `set Opts(callback) {puts "cb $framename $layoutframe"}`; `openwrapper`; `set Winopts(.layout3,callback) {...}` | callback output (Q-SCRIPT-13) | Wrapper hook points for scripts |
| SCRIPT-GUI-03 | ui-gui-managers | GUI: invoke each menu item via `$m invoke $i` with a layout loaded | command executed and errors (Q-SCRIPT-36) | Wrapper menus |
| SCRIPT-GUI-04 | ui-gui-managers | GUI: `box 0 0 1.5um 2um`; `box remove`; pointer motion events | text of the position label and caption | Wrapper tag callbacks and caption formats |
| SCRIPT-GUI-05 | ui-gui-managers | GUI: `techmanager create` with `PDK_ROOT` set; dump menu entries | entries and commands | Technology manager |
| SCRIPT-GUI-06 | ui-gui-managers | GUI: build the toolbar with and without `./<tech>_toolbars.tcl`; event-generate the bindings on the `poly` row | issued commands, row order, file rewrite | Layer toolbar; Toolbar order presets file |
| SCRIPT-GUI-07 | ui-gui-managers | GUI: cell manager with a 3-level hierarchy; press each button | tree ids, commands issued | Cell manager |
| SCRIPT-GUI-08 | ui-gui-managers | GUI: library manager over a dir with mixed-tech and malformed .mag files, Filter on and off | listed cells | Library manager |
| SCRIPT-GUI-09 | ui-gui-managers | scmos layout with known DRC errors: `drc_save_report`; `drc_load_report`; `feedback find` | exact drc.out bytes (rule blocks sorted, Q-SCRIPT-8); feedback count | DRC manager and DRC report file |
| SCRIPT-GUI-10 | ui-gui-managers | GUI: text helper new label with and without a layer, sticky, port; edit an existing label | commands issued (trace) | Text helper |
| SCRIPT-GUI-11 | ui-gui-managers | stub `tk_getOpenFile`/`tk_getSaveFile` to return fixed paths; `promptload gds` with a single top and with multiple tops; `promptsave magic` with (UNNAMED) | loaded cell, stdout, files written | File prompts |
| SCRIPT-GUI-12 | ui-gui-managers | `set Opts(backupinterval) 1000; crashbackups start`; wait 2.5 s; `crashbackups stop`; `crashbackups disable; crashbackups resume`; `after info` | backup files, scheduled events | Crash backup timer |
| SCRIPT-GUI-13 | ui-gui-managers | after `magic::enable_tools`: `tool`, `tool type`, `macro list` per tool, `tool info` | tool names, bindings tables | Tool modes |
| SCRIPT-RSP-01 | readspice | inv with ports Y=1 A=2 VDD=3 VSS=4; `readspice inv.spice` with `.subckt inv A Y VDD VSS` | port indices, stdout | Port renumbering algorithm |
| SCRIPT-RSP-02 | readspice | inv already ordered; plus an extra BODY port | indices, message (Q-SCRIPT-32) | Port renumbering algorithm |
| SCRIPT-RSP-03 | readspice | reg with ports `D<0>`, `d<1>` and bare label `CLK`; netlist pins `D[0] D[1] CLK` | indices; CLK made a port | Port renumbering algorithm |
| SCRIPT-RSP-04 | readspice | `readspice cells` with only `cells.cdl` present, containing `*.PININFO A:I Y:O VDD:B VSS:B` and a pin already classed `output` | classes | Netlist file resolution; Port direction from PININFO |
| SCRIPT-RSP-05 | readspice | Spectre `inline subckt inv (A Y VDD VSS)`; continuation after a comment line; tabs and double spaces; an unloaded cell; a parameter `W=1` after the pins | indices, messages | Line preprocessing; Subcircuit recognition |
| SCRIPT-RSP-06 | readspice | `readspice nofile.sp` | result `1`, stderr | Netlist file resolution |
| SCRIPT-AUX-01 | scripting-auxiliary-tools | run `strip_reflibs.tcl` in a directory with 2 .mag files containing GDS_START, GDS_END and GDS_FILE lines | file contents after | Reference-library stripper |
| SCRIPT-AUX-02 | scripting-auxiliary-tools | `mazeroute f.net` with iroute stubbed to log its arguments | call sequence | Netlist maze-routing script |
| SCRIPT-AUX-03 | scripting-auxiliary-tools | default start; `info commands magic::add_menu`; check for a listener on 12946; `set batch_mode` under `-dnull -noconsole` | empty; not listening; `true` | Socket command server is opt-in; BSI menu helpers; Interpreter launchers |
| SCRIPT-AUX-04 | scripting-auxiliary-tools | source `toolkit_rev0.tcl` in a fresh interpreter with a toy rev-0 library; create two devices | cell names, per-parameter properties | Legacy gencell toolkit revision 0 |
