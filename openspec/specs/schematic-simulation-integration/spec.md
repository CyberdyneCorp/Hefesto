# Schematic Simulation Integration

## Purpose
Defines how schematics drive simulation and LVS in Hefesto (decisions P-12, P-7 and P-5). This covers:
- testbench schematics and their code blocks (`.include`, `.lib`, `.param`, `.control` ... `.endc`)
- turning a schematic netlist into a deck for the embedded ngspice engine
- the mapping between schematic nets and simulator vector names
- operating-point back-annotation on the schematic
- graphs embedded in schematics
- waveform cross-probing from schematic nets
- schematic-to-layout cross-probing through LVS results

xschem runs ngspice as a separate program and reads its raw files. Hefesto runs ngspice embedded (capability `simulation-engine-embedding`) and keeps the results in memory (capability `simulation-analyses-and-results`). Where xschem behaviour is reproduced, it is reproduced on top of that model.

**Compatibility adaptation.** This capability is about xschem, not Magic. The compatibility classes in `docs/spec-conventions.md` therefore apply relative to **xschem** (`StefanSchippers/xschem` @ `4142e6d6f1eb5cd0e271bb183f14202af374ce27`, version `3.4.8RC`):
- `exact`: identical to xschem's results (for example the same vector names and the same displayed values)
- `behavioral`: same results; mechanism or interaction MAY differ
- `informative`: xschem detail that Hefesto MAY differ from
- `native`: Hefesto-only, and cites `(Hefesto decision: docs/decisions/product.md#P-12)` or another decision

Traces use `(xschem ref: path/file.c:line)`, relative to the xschem `src/` directory. They exist for traceability only. Implementers SHALL work from this spec and SHALL NOT consult the GPL sources (capability `hefesto-licensing`).

Other capabilities own related parts:
- The netlist text: capability `schematic-netlisting`.
- Deck preparation, validation and model resolution: capability `simulation-netlist-preparation`.
- Jobs, vectors and raw files: capability `simulation-analyses-and-results`. Waveform viewer: capability `simulation-ui-and-crossprobe`.
- LVS entry point, result browser and layout cross-probing: capability `lvs-command`.

Open questions are in `docs/decisions/open-questions/SCHEM.md` (Q-SCHEM-n).

## Requirements

### Requirement: Schematic circuit source
A simulation testbench SHALL accept the source `schematic <file>` in addition to the sources of capability `simulation-netlist-preparation` (Requirement "Circuit sources"). With this source, the schematic is the complete testbench, as in xschem: its netlist (capability `schematic-netlisting`, `hier` mode, not LVS mode) SHALL be the deck. Hefesto SHALL treat it like a raw deck, apply Requirement "Control block translation", resolve file references, and validate it. The deck SHALL be generated in memory (capability `schematic-netlisting`, Requirement "In-process netlisting"). The `sim dut` command SHALL accept `schematic <file>` (capability `simulation-command`); `schematic simulate` (capability `schematic-command`) SHALL be the shortcut. See Q-SCHEM-17 for the corresponding change to capability `simulation-netlist-preparation`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12)

#### Scenario: Simulate an IHP testbench
- **GIVEN** IHP `sg13g2_tests/dc_lv_nmos.sch` open with the sg13g2 PDK active
- **WHEN** the user runs `schematic simulate`
- **THEN** a job SHALL run in the embedded engine with the schematic's netlist as its deck, and its DC sweep vectors SHALL be available in the waveform viewer

### Requirement: Testbench code blocks
Testbench text SHALL come from `netlist_commands` instances (for example `code.sym` and `code_shown.sym`, whose `value` attribute is copied), placed in the netlist as defined by capability `schematic-netlisting` (`place=header`, middle, `place=end`, `only_toplevel`). Typical content that Hefesto SHALL support: `.include`, `.lib <file> <section>`, `.param`, `.option`, `.temp`, `.save`, analysis lines, and `.control` ... `.endc` blocks. `.include` and `.lib` paths SHALL be resolved:
1. absolute paths as given, through the platform file services
2. relative paths against the netlist folder, then against the schematic's folder
3. paths below the PDK root of the `xschemrc` variables (`PDK_ROOT`, `SKYWATER_MODELS`, `SG13G2_MODELS`, ...) mapped to the PDK store (capability `hefesto-files-and-pdks`)

Compatibility: behavioral

(xschem ref: spice_netlist.c:328-350, spice_netlist.c:410-432, spice_netlist.c:578-612)

#### Scenario: Corner library from the PDK store
- **GIVEN** sky130A installed in the PDK store and a testbench with `corner.sym` (`corner=ss`)
- **WHEN** the deck is prepared
- **THEN** it SHALL contain `.lib <PDK store>/sky130A/libs.tech/combined/sky130.lib.spice ss` and that file SHALL resolve

### Requirement: Control block translation
xschem testbenches drive ngspice from `.control` blocks that write raw files and quit. Before validation, Hefesto SHALL translate every `.control` ... `.endc` block of a schematic-source deck line by line:

| Control line | Translation |
|---|---|
| `write <file> [<vectors>]` | Capture the current plot (or the named vectors) into the job results under the name `<file>`; on desktop also write the raw file into the netlist folder if the setting "write raw files" is on (default off) |
| `set appendwrite` | Later `write` to the same name appends a plot, as in ngspice |
| `wrdata <file> ...` | Capture like `write`; export as text only if the setting is on |
| `quit`, `exit` | Removed |
| `shell ...`, `cd`, `load`, `source`, `codemodel`, `osdi`, `pre_osdi` | Rejected with `Control command '<cmd>' is not allowed in simulation decks` |
| `plot`, `print`, `meas`, `let`, `save`, analyses, `alter`, `alterparam`, `reset`, `remzerovec`, `foreach`, `if`, other commands | Passed through |

`.control` blocks SHALL then be allowed for schematic-source decks without setting `allowControl`. Messages that ngspice prints from the control block SHALL appear in the job log.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Raw file write captured
- **GIVEN** a code block `.control` / `save all` / `op` / `write dc_lv_nmos.raw` / `set appendwrite` / `dc Vds 0 1.2 0.01 Vgs 0.3 1.0 0.1` / `write dc_lv_nmos.raw` / `.endc`
- **WHEN** the testbench is simulated
- **THEN** the job SHALL contain two plots under the name `dc_lv_nmos.raw`, an `op` plot and a `dc` plot, and no file SHALL be written with default settings

#### Scenario: Shell refused
- **GIVEN** a code block containing `shell rm -f x.raw` inside `.control`
- **WHEN** the deck is prepared
- **THEN** preparation SHALL fail with `Control command 'shell' is not allowed in simulation decks`

### Requirement: Simulator vector names
A schematic net SHALL map to the simulator node name as follows, where the hierarchy path is that of capability `schematic-editing` (Requirement "Hierarchy path"), counted from the level at which the results were loaded (normally the top):
1. Global nets (capability `schematic-netlisting`, Requirement "Global nets") and `0` map to their own name.
2. Otherwise, a net that is a port of the current schematic is replaced by the parent net connected to that port, repeatedly, up to the loaded level; a net passed as an attribute value of the parent instance is replaced by that value first.
3. The resulting net is prefixed by the path components below the level where it was resolved, each followed by `.`, and each component that does not start with `x` or `X` is prefixed with `x` (the SPICE subcircuit-call prefix).
4. The name is lowercased.

The vector of a node voltage SHALL be that name (ngspice `v(<name>)` plot vector). Device currents SHALL use the device's full name: path, `spiceprefix`, instance name. Bus nets map bit by bit.

Compatibility: exact

(xschem ref: hilight.c:2155-2260, xschem.tcl:7252-7280)

#### Scenario: Internal node of a subcircuit
- **GIVEN** top instance `x1` of `inv`, and inside it an unlabeled net `net1` that is not a port
- **WHEN** the user probes `net1` while viewing `x1`
- **THEN** the vector name SHALL be `x1.net1`

#### Scenario: Port net resolved upwards
- **GIVEN** inside `x1` the port net `A`, connected at top level to net `IN`
- **WHEN** the user probes `A` while viewing `x1`
- **THEN** the vector name SHALL be `in`

### Requirement: Operating-point annotation
After an `op` analysis, or at cursor B of a DC or transient plot, Hefesto SHALL resolve the simulation tokens in symbol texts (capability `schematic-symbols-and-libraries`, Requirement "Text substitution tokens"):
- `@spice_get_voltage` (on a one-pin symbol): the voltage of its net
- `@spice_get_voltage(<net>)`: the voltage of `<net>` in the instance's scope (path + instance name + `.` + net), or of the global net
- `@spice_get_current`, `@spice_get_current(<dev>)`, `@spice_get_current_<param>(...)`: device current or the named device parameter
- `@spice_get_diff_voltage`: the voltage between the first two pins
- `@spice_get_node <expr> [<unit>]`: the value of the ngspice expression, for example `i(\@m.xm1.msky130_fd_pr__nfet_01v8[id])`, followed by the unit text
- `@#<n>:spice_get_voltage`: the voltage of the net on pin n

Values SHALL be formatted in engineering notation with the `ev_precision` setting (default 4) significant digits (capability `schematic-netlisting`, Requirement "expr evaluation", for the suffix rules). A net named `0` or `GND` (case-insensitive) SHALL show `0.0`. A vector that is not in the results SHALL show `-`. Before any results are loaded, the tokens SHALL show nothing. PDK annotation symbols that use `tcleval(... ngspice::get_node ...)` (for example `annotate_fet_params.sym`) SHALL work through the Tcl procedures of capability `schematic-symbols-and-libraries`. Annotations SHALL NOT be saved.

Compatibility: exact

(xschem ref: token.c:4797-5220, editprop.c:590-621)

#### Scenario: Node voltage label
- **GIVEN** an op result with `v(out)` = 1.2345678
- **WHEN** a `lab_show`-style symbol with text `@spice_get_voltage` on net `out` is drawn
- **THEN** it SHALL read `1.235`

#### Scenario: Small current
- **GIVEN** an op result with the drain current of `M1` = 2.5e-5 A
- **WHEN** a text `@spice_get_current` on `M1` is drawn
- **THEN** it SHALL read `25u`

### Requirement: Embedded graphs
A box on layer 2 whose property has `flags` containing `graph` SHALL be a waveform graph. Hefesto SHALL preserve all its attributes byte-for-byte on save, and SHALL draw it from the current job results using these keys:

| Key | Meaning |
|---|---|
| `node` | LF- or space-separated list of vectors or expressions, each optionally `"label;expr"` |
| `color` | Space-separated layer numbers, one per node |
| `x1`, `x2`, `y1`, `y2` | Axis ranges; `ypos1`, `ypos2` for digital lanes |
| `divx`, `divy`, `subdivx`, `subdivy` | Major and minor divisions |
| `unitx`, `unity` | Unit scaling (`1`, `m`, `u`, `n`, `p`, `k`, `M`, ...) |
| `logx`, `logy` | 1 for logarithmic axes |
| `dataset` | Sweep or step index to show, −1 for all |
| `sim_type` | Plot type to show: `op`, `dc`, `ac`, `tran`, `noise`, `sp`, ... |
| `rawfile` | Name of the results to show; matches the `write` name of Requirement "Control block translation" |
| `autoload` | 1: load the named results automatically when the schematic opens |
| `digital`, `mode`, `sweep`, `legend`, `cursor1_x`, `cursor2_x`, `hcursor1_y`, `hcursor2_y`, `hilight_wave`, `linewidth_mult`, `rainbow`, ... | Display options, preserved; supported as in capability `simulation-ui-and-crossprobe` where it has an equivalent |

Interacting with a graph (zoom, cursors) MAY update `x1`, `x2`, `y1`, `y2` and cursor keys, and that SHALL mark the document modified, as in xschem. Graph boxes inside a schematic used as a symbol SHALL be dropped (capability `schematic-symbols-and-libraries`).

Compatibility: behavioral

(xschem ref: draw.c:4047-4200, draw.c:5641-5800, save.c:5274-5277)

#### Scenario: IHP DC graph
- **GIVEN** the graph box of `dc_lv_nmos.sch` with `node=i(vd)`, `sim_type=dc`, `x1=0`, `x2=1.2`, `autoload=1`
- **WHEN** the testbench has been simulated
- **THEN** the graph SHALL show the drain current against the swept voltage from 0 to 1.2

### Requirement: Waveform cross-probing
Selecting a schematic net (wire, label or pin) and asking to probe it SHALL add the net's vector (Requirement "Simulator vector names") as a trace in the waveform viewer (capability `simulation-ui-and-crossprobe`). Probing a device SHALL add its current. Selecting a trace in the viewer SHALL highlight the corresponding schematic net at the current hierarchy level, and SHALL highlight it after descending or ascending (capability `schematic-editing`, Requirement "Net highlighting"). An option ("auto-highlight", xschem's `auto_hilight`) SHALL highlight nets automatically when they are selected.

Compatibility: behavioral

(xschem ref: hilight.c:1974-2100, xschem.tcl:12012-12013)

#### Scenario: Probe from schematic
- **GIVEN** a completed transient job of `inv_tb.sch`
- **WHEN** the user selects the wire of net `out` and chooses Probe
- **THEN** the trace `v(out)` SHALL be added to the waveform viewer

### Requirement: Schematic to layout cross-probing
When LVS compares a layout against a schematic (capability `lvs-command`), Hefesto SHALL be able to take the schematic side directly from a schematic file: it SHALL netlist the schematic in LVS netlist mode (capability `schematic-netlisting`, Requirement "LVS netlist mode", with option `lvs_ignore` on) in memory and pass the text to the LVS reader (capability `lvs-netlist-input`). After a comparison, Hefesto SHALL keep a map from each schematic-side netlist name to the schematic object it came from:
- net names to the hierarchical schematic net (Requirement "Simulator vector names" without lowercasing and without the `x` prefix rule)
- device and subcircuit instance names (including any `spiceprefix`) to the schematic instance and hierarchy path

Selecting a schematic net or instance SHALL highlight its layout partner (capability `lvs-command`, Requirement "Cross-probing"), or show `no layout counterpart`. Selecting a layout net or device in the LVS result browser SHALL highlight the matched schematic net or instance, descending to its hierarchy level if needed. Unmatched schematic objects SHALL be listed in the result browser with a link to the schematic.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: Highlight layout net from schematic
- **GIVEN** an LVS run of layout `inv` against `inv.sch` with net `Y` matched
- **WHEN** the user selects the `Y` wire in the schematic and chooses "Show in layout"
- **THEN** the layout net matched to `Y` SHALL be highlighted in the layout window

#### Scenario: Device back to schematic
- **GIVEN** the same run, with layout device `X0` matched to schematic device `XM1`
- **WHEN** the user taps `X0` in the result browser
- **THEN** the schematic instance `M1` SHALL be selected and highlighted

### Requirement: Schematic-driven LVS command
The `lvs` command (capability `lvs-command`) SHALL accept a schematic file wherever it accepts a schematic netlist file: a `.sch` path SHALL be netlisted as in Requirement "Schematic to layout cross-probing", using the PDK's LVS conventions (`lvs_format`, `lvs_ignore`). The generated text SHALL be retrievable (`schematic netlist -lvs`, capability `schematic-command`) for comparison with the netlist xschem produces.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: LVS from schematic file
- **WHEN** the user runs `lvs inv.mag inv.sch sky130A`
- **THEN** `inv.sch` SHALL be netlisted in LVS mode in memory and compared with the layout netlist

### Requirement: Results interchange with xschem
Hefesto SHALL export job results as ngspice raw files (capability `simulation-analyses-and-results`) that xschem can load, and SHALL import raw files written by ngspice runs started from xschem, so that graphs and annotations work on results from either tool. Vector names in exported files SHALL follow Requirement "Simulator vector names".

Compatibility: behavioral

(xschem ref: save.c:925-1186, save.c:1414-1488)

#### Scenario: Import xschem raw file
- **GIVEN** `tb.raw` written by ngspice when the testbench was simulated from xschem
- **WHEN** it is imported into Hefesto and the schematic `tb.sch` is open
- **THEN** its embedded graphs and `@spice_get_voltage` annotations SHALL show the imported values

### Requirement: Simulation launchers
xschem testbenches often contain launcher instances whose `tclcommand` runs xschem Tcl commands (`xschem netlist`, `simulate`, `xschem raw_read <file> <type>`, `xschem annotate_op`, `xschem setprop ...`). Hefesto SHALL map these commands, run in the sandboxed Tcl interpreter, to its own operations: `xschem netlist` to `schematic netlist`, `simulate` to `schematic simulate`, `xschem raw_read` and `xschem annotate_op` to loading the named results and turning annotation on. Unsupported commands SHALL report `xschem command '<cmd>' is not supported` and SHALL NOT stop the rest of the script. Changes to the xschem simulator command variables (`sim(spice,...)`) SHALL be ignored, because the simulator is embedded.

Compatibility: behavioral

(xschem ref: scheduler.c:3780-3870, xschem.tcl:983-1043)

#### Scenario: IHP OP annotate launcher
- **GIVEN** the launcher `descr="OP annotate" tclcommand="xschem annotate_op"` in an IHP testbench that has been simulated
- **WHEN** the user Ctrl+clicks it and confirms
- **THEN** the operating-point annotation SHALL be turned on with the latest job's `op` plot
