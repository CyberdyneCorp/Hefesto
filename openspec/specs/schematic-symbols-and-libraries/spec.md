# Schematic Symbols and Libraries

## Purpose
Defines what a schematic symbol means and how symbols are found. Decision P-12 requires the xschem symbol libraries of the sky130, gf180mcu and IHP sg13g2 PDKs to work in Hefesto unchanged. This covers:
- symbol properties (`type`, `format`, `template`, ... in the `K` record of capability `schematic-file-format`)
- the symbol type set and how each type behaves
- pins and pin order
- instance attributes, templates and instance-name uniqueness
- `@` substitution tokens in displayed texts
- `tcleval(...)` values and their dependence on Tcl
- symbol references, the library search path and `xschemrc` startup files read as data
- schematics used as symbols, embedded symbols and missing symbols
- symbol generators
- the attribute conventions of the three PDK libraries

**Compatibility adaptation.** This capability is about xschem, not Magic. The compatibility classes in `docs/spec-conventions.md` therefore apply relative to **xschem** (`StefanSchippers/xschem` @ `4142e6d6f1eb5cd0e271bb183f14202af374ce27`, version `3.4.8RC`):
- `exact`: identical to xschem's data semantics and results
- `behavioral`: same results; the mechanism or interaction may differ
- `informative`: xschem detail that Hefesto MAY differ from
- `native`: Hefesto-only, and cites `(Hefesto decision: docs/decisions/product.md#P-12)`

Traces use `(xschem ref: path/file.c:line)`, relative to the xschem `src/` directory. They exist for traceability only. Implementers SHALL work from this spec and SHALL NOT consult the GPL sources (capability `hefesto-licensing`).

PDK reference data, read only:
- `StefanSchippers/xschem_sky130` @ `2bfd7d654e12492d47986b78e68bb770e633b5da`, as installed by open_pdks into `sky130A/libs.tech/xschem` (ciel release `sky130-5740c4a8`)
- `IHP-GmbH/IHP-Open-PDK` @ `5e6d592e4002946a4616f798c357f0f3c06cf3b6` (`ihp-sg13g2/libs.tech/xschem` and `ihp-sg13g2/libs.ref/sg13g2_stdcell/sym/xschem`, ciel release `ihp-sg13g2-5e6d592e`)
- `fossi-foundation/globalfoundries-pdk-libs-gf180mcu_fd_pr` @ `5e4be8726e2ae8be099c4131f697a353d6457874` (`cells/xschem`), as installed by open_pdks into `gf180mcuD/libs.tech/xschem` (ciel release `gf180mcu-5740c4a8`; re-pinned from `aacc04cc` on 2026-11-17, design Open Question 1 of `openspec/changes/m8-schematic`)
- `RTimothyEdwards/open_pdks` @ `c6df28c1fccb05c286aa0a15e9c4c7dcd8502607` (`sky130/custom/xschem`)

Other capabilities own related parts:
- The file grammar and the property-string grammar: capability `schematic-file-format`.
- How `format` strings become netlist lines: capability `schematic-netlisting`.
- The embedded, sandboxed Tcl interpreter: capability `hefesto-scripting`.
- PDK installation and the PDK store: capability `hefesto-files-and-pdks`.

Open questions are in `docs/decisions/open-questions/SCHEM.md` (Q-SCHEM-n).

## Requirements

### Requirement: Symbol property
A symbol's netlisting rules SHALL be its symbol property: the `K` record, or the `G` record of pre-1.2 symbols (capability `schematic-file-format`). Hefesto SHALL recognise these keys. Other keys SHALL be kept and SHALL be available to `@` substitution.

| Key | Meaning |
|---|---|
| `type` | Symbol type (Requirement "Symbol types") |
| `format` | SPICE line template (capability `schematic-netlisting`) |
| `lvs_format` | SPICE line template used in LVS netlist mode, falling back to `format` |
| `spectre_format`, `verilog_format`, `vhdl_format`, `tedax_format` | Templates for the other netlisters. Hefesto SHALL keep them; the other netlisters are outside P-12 (see Q-SCHEM-4). |
| `template` | Default instance attributes (Requirement "Templates and instance attributes") |
| `extra` | Space-separated names of extra subcircuit ports or parameters that are not pins |
| `schematic` | Schematic that implements the symbol, instead of `<symbol base name>.sch`. MAY contain `@symname`. |
| `default_schematic` | `ignore`: no schematic is netlisted for this symbol |
| `spice_sym_def` | Text printed instead of a generated `.subckt` (for example a `.include`) |
| `spice_primitive` | true: the symbol is a leaf and its schematic is not netlisted |
| `lvs_primitive` | true: as `spice_primitive`, but only in LVS netlist mode |
| `spice_stop` | true: the schematic is netlisted for its ports only, and its instances are not descended |
| `place` | For `netlist_commands`: `header`, `end` or absent (capability `schematic-netlisting`) |
| `device_model` | Model or subcircuit text to print once at the end of the netlist |
| `global` | For labels: `true` makes the net global; `ground` makes it a global ground |
| `spice_ignore`, `lvs_ignore`, `spectre_ignore`, `verilog_ignore`, `vhdl_ignore`, `tedax_ignore` | `true` or `open`: the symbol is left out of that netlist; `short`: all its pins are shorted together |
| `highlight` | true: nets attached to the symbol are highlighted with it |
| `hide` | true: draw only the bounding box |
| `drc` | Script called by the symbol check, informative (see Q-SCHEM-6) |

Compatibility: exact

(xschem ref: actions.c:825-878, spice_netlist.c:183-268, spice_netlist.c:646-760)

#### Scenario: sky130 nfet properties
- **GIVEN** `sky130_fd_pr/nfet_01v8.sym` from `xschem_sky130`
- **WHEN** it is loaded
- **THEN** its type SHALL be `nmos`, its `lvs_format` SHALL be `@spiceprefix@name @pinlist sky130_fd_pr__@model L=@L W=@W nf=@nf m=@mult`, and its template SHALL define `model=nfet_01v8` and `spiceprefix=X`

### Requirement: Symbol types
The `type` value SHALL decide how the netlister, connectivity and editor treat the instances. Any other `type` value (for example `nmos`, `resistor`, `vsource`) SHALL be a device that is netlisted through its `format`.

| Type | Behaviour |
|---|---|
| `subcircuit` | Netlisted as an instance whose definition comes from the symbol's schematic. The editor can descend into it. |
| `primitive` | Netlisted through `format`. The editor can descend into its schematic, but no `.subckt` is generated for it. |
| `ipin`, `opin`, `iopin` | Schematic port. Its single pin names the net from `lab`; it defines the subcircuit port list and direction (in, out, inout). |
| `label` | Net label. Its single pin names the net from `lab`; with `global` set it declares a global net. |
| `netlist_commands` | Its `format` (usually `@value`) is copied into the netlist as user code |
| `netlist_options` | Sets netlister options (capability `schematic-netlisting`); emits nothing |
| `launcher` | Holds `url` or `tclcommand` to open or run on Ctrl+click; emits only a comment |
| `logo` | Title block or decoration; emits nothing |
| `probe` | Probe symbol, netlisted through `format` (for example `.save`) |
| `ammeter` | Zero-volt source used to measure current |
| `noconn` | Marks a pin as intentionally unconnected |
| `show_label`, `bus_tap`, `scope` | Connectivity helpers: `bus_tap` extracts bits from a bus (capability `schematic-netlisting`); the others only show the attached net name |
| `use`, `package`, `architecture`, `arch_declarations`, `attributes`, `port_attributes`, `timescale`, `verilog_preprocessor` | VHDL or Verilog helpers; ignored by SPICE netlisting |

A schematic that is placed as a symbol SHALL ignore instances of the types `logo`, `netlist_commands`, `netlist_options`, `arch_declarations`, `architecture`, `attributes`, `package`, `port_attributes`, `use`, `launcher`, `verilog_preprocessor` and `timescale` when building the symbol graphics.

Compatibility: exact

(xschem ref: xschem.h:391-395, save.c:5490-5512, actions.c:2680-2690)

#### Scenario: Descend only into hierarchical types
- **GIVEN** a selected instance of a symbol with `type=nmos`
- **WHEN** the user asks to descend into its schematic
- **THEN** Hefesto SHALL refuse, and it SHALL accept the same request for `type=subcircuit` or `type=primitive`

### Requirement: Pins
A symbol pin SHALL be a box on layer 5 of the symbol. Its connection point is the centre of the box. Pin keys:

| Key | Meaning |
|---|---|
| `name` | Pin name. MAY be a bus (`A[3:0]`, capability `schematic-netlisting`). Two pins with the same name are one electrical pin ("pass-through"). |
| `dir` | `in`, `out` or `inout` |
| `pinnumber` | Package pin number. For a slotted instance name `U3:2`, a value `a:b:c` gives the slot's item. |
| `sim_pinnumber` | Integer port order (Requirement "Pin order") |
| `spice_ignore` | true: the pin is left out of `@pinlist` and `@@name` |
| `propag` | Pin indices to which highlighting propagates through the symbol |
| `sig_type`, `verilog_type` | Used by HDL netlisters, preserved |
| `goto` | Kept for compatibility, preserved |

Boxes on layer 5 inside an instance that is itself drawn inside a schematic-used-as-symbol SHALL NOT become pins of the outer symbol; they SHALL be moved to layer 7 for drawing.

Compatibility: exact

(xschem ref: netlist.c:777-801, save.c:5255, token.c:2559-2600)

#### Scenario: Pass-through pin
- **GIVEN** a symbol with two pins both named `A`, at (−20,0) and (20,0)
- **WHEN** a wire named `N1` touches only the left pin
- **THEN** the right pin SHALL also connect to net `N1`

### Requirement: Pin order
The pin order of a symbol SHALL be the order of its layer-5 boxes in the file. If every pin has a `sim_pinnumber` key, the pins SHALL instead be sorted by the integer value of `sim_pinnumber`, ascending. Pins with equal `sim_pinnumber` have no defined relative order in xschem; Hefesto SHALL keep their file order (a stable sort) and SHALL report a warning naming the symbol and the duplicated value, whatever the `xschem-compat` setting (Q-SCHEM-5, `Resolution: fix`). If at least one pin lacks `sim_pinnumber`, no sorting SHALL happen. Pin order SHALL define `@pinlist`, the `.subckt` port list and pin numbers used by `@#n`.

Compatibility: exact

(xschem ref: save.c:3825-3876)

#### Scenario: Sorted by sim_pinnumber
- **GIVEN** pins in file order `B` (sim_pinnumber=3), `A` (sim_pinnumber=1), `C` (sim_pinnumber=2)
- **WHEN** the symbol is loaded
- **THEN** the pin order SHALL be A, C, B

#### Scenario: Partial numbering
- **GIVEN** pins `B` (sim_pinnumber=2) and `A` (no sim_pinnumber)
- **WHEN** the symbol is loaded
- **THEN** the pin order SHALL be B, A

#### Scenario: Duplicate sim_pinnumber
- **GIVEN** pins in file order `B` (sim_pinnumber=1), `A` (sim_pinnumber=1), `C` (sim_pinnumber=0)
- **WHEN** the symbol is loaded
- **THEN** the pin order SHALL be C, B, A, and a warning SHALL report the duplicated `sim_pinnumber` 1

### Requirement: Templates and instance attributes
A new instance SHALL get the symbol's `template` value (decoded, capability `schematic-file-format`, Requirement "Property strings") as its property string, with `name` made unique (Requirement "Instance names"). When an `@key` token is resolved for an instance (capability `schematic-netlisting`, Requirement "Format string substitution"), the instance property string SHALL be searched first, then the symbol template. Instance attributes that the template does not define SHALL be kept and SHALL be usable by `@key`. An instance MAY override `format` and `lvs_format`; an instance value SHALL be used before the symbol value.

Compatibility: exact

(xschem ref: token.c:2448-2470, token.c:2700-2740)

#### Scenario: Instance value wins over template
- **GIVEN** a resistor symbol with template `name=R1 value=1k` and an instance with `name=R5 value=2k`
- **WHEN** `@value` is resolved for the instance
- **THEN** it SHALL be `2k`

### Requirement: Instance names
Instance names SHALL be unique in a schematic, compared ASCII case-insensitively after bus expansion (so `X[1:0]` uses `X[1]` and `X[0]`). When a placed, pasted or copied instance gets a name that is already used, Hefesto SHALL rename it:
1. Split the name into a base (the leading characters before the first digit or `[`) and a bracket suffix (from the first `[` to the end, or empty).
2. Starting from the last index used for that base and suffix in this schematic (initially 1), try `<base><q><suffix>` for q = start, start+1, ... until a name is unused.
3. Record q as the last index used for that base and suffix.

An instance without `name` SHALL keep its properties unchanged.

Compatibility: exact

(xschem ref: token.c:704-860)

#### Scenario: Copy of R1
- **GIVEN** instances `R1` and `R2` in a schematic, and no renaming done yet
- **WHEN** `R1` is copied
- **THEN** the copy SHALL be named `R3`

#### Scenario: Vector instance copy
- **GIVEN** an instance `x1[3:0]`
- **WHEN** it is copied
- **THEN** the copy SHALL be named `x2[3:0]`

### Requirement: Floater texts
A text in a schematic whose property string has a `name` key, or a `floater` key, SHALL be a floater. Its owner SHALL be the instance whose name equals the value of `name`, or, if `name` is absent, the value of `floater`. If the owner exists, the text's tokens (Requirement "Text substitution tokens") SHALL be resolved against it. If no such instance exists, a text that starts with `tcleval(` or contains `@` or `%` SHALL be resolved with no instance (only instance-independent tokens and Tcl evaluation apply). The editor can move floaters together with their owner (capability `schematic-editing`). Inside a schematic-used-as-symbol, floater texts SHALL be dropped.

Compatibility: behavioral

(xschem ref: actions.c:100-146, save.c:5320-5335, token.c:45-57)

#### Scenario: Floater shows instance width
- **GIVEN** an instance `M1` with `W=2` and a schematic text `T {W=@W} ... {name=M1}`
- **WHEN** the schematic is displayed
- **THEN** the text SHALL read `W=2`

### Requirement: Text substitution tokens
When a symbol text is drawn for an instance, every token SHALL be replaced as follows. A token starts at an unescaped `@` or `%` and ends before the next unescaped space, `@` or `%` outside parentheses, or before an escaped character. A backslash escapes the next character and is removed.

| Token | Replacement |
|---|---|
| `@name` | instance name |
| `@symname` | symbol reference without directory and extension |
| `@symname_ext` | symbol reference without directory |
| `@symref` | symbol reference as written |
| `@path` | hierarchical path of the current schematic without the leading `.` (Requirement "Hierarchy path" of capability `schematic-editing`) |
| `@schname`, `@schname_ext`, `@topschname` | current schematic name without and with extension; top schematic name |
| `@index` | instance index in the schematic |
| `@prop_ptr` | the whole instance property string |
| `@sch_last_modified`, `@sym_last_modified`, `@time_last_modified` | file modification time as `%Y-%m-%d  %H:%M:%S` (two spaces), local time |
| `@lvs_ignore` | `1` if LVS-ignore mode is on, else `0` |
| `@@pin` | name of the net attached to pin `pin` |
| `@#n` or `@#name` | net attached to pin number n (0-based in pin order) or pin `name` |
| `@#n:attr` | value of pin attribute `attr`, looked up first as `attr(pinname)=` or `attr(n)=` in the instance, then on the pin box; `net_name` gives the attached net |
| `@spice_get_voltage`, `@spice_get_current`, `@spice_get_node ...` and variants | simulation values (capability `schematic-simulation-integration`) |
| `@key` | value of `key` in the instance, else in the template, else empty |
| `%key` | as `@key`, but if `key` is defined nowhere the text `key` itself |

Compatibility: exact

(xschem ref: token.c:4675-4795, token.c:5299-5560)

#### Scenario: Resistor label
- **GIVEN** a symbol text `@name=@value` and an instance `name=R7 value=4.7k` of `res.sym`
- **WHEN** the instance is drawn
- **THEN** the text SHALL read `4.7k`: the first token is `@name=` (it ends only before the next `@`), which is not a defined key and is empty, and Hefesto SHALL report the Q-SCHEM-26 warning naming `name=` and the defined prefix `name` (golden case SCHEM-PR-04)

#### Scenario: Resistor label written with a separator
- **GIVEN** a symbol text `@name @value` and the same instance
- **WHEN** the instance is drawn
- **THEN** the text SHALL read `R7 4.7k`

#### Scenario: Token values from xschem translate
- **GIVEN** golden case SCHEM-PR-04: instances `R7` (`value=4.7k`) and `M1` (`W=2 L=1 value=2`) of `res.sym`, whose pins are `P` and `M`
- **WHEN** the texts are substituted for them
- **THEN** `@W/@L` on `M1` SHALL give `1`, `@W @L` SHALL give `2 1`, and on `R7` `%undef` SHALL give `undef`, `@undef` the empty text, `@symname` `res`, `@symname_ext` `res.sym`, `@#0:pinnumber` `1` and `@#1:name` `M`

#### Scenario: Escaped at sign
- **GIVEN** a symbol text `a\@b @name` and an instance named `X1`
- **WHEN** the instance is drawn
- **THEN** the text SHALL read `a@b X1`

### Requirement: tcleval values
An attribute value, a `format` result or a displayed text whose text begins with `tcleval(` SHALL be evaluated in Tcl:
1. Every `\}` SHALL be replaced by `}`.
2. The leading `tcleval(` and the last `)` (with any whitespace after it) SHALL be removed.
3. In the Tcl global scope, the variables `path` (the current hierarchical path without the leading `.`, with `x` inserted before each component that does not start with `x` or `X` when the netlist type is SPICE) and `sch_basename` SHALL be set.
4. The remainder SHALL be evaluated as the body of `subst {...}` in the global scope.
5. The result replaces the value. If evaluation fails, or the user has not allowed script execution, the result SHALL be `?` followed by LF.

A format token `@tcleval(...)` SHALL be evaluated the same way, with Tcl variables `instname` and `symname` set to the instance name and symbol reference.

Hefesto SHALL run this in the embedded, sandboxed Tcl interpreter of capability `hefesto-scripting` on every platform, so the PDK symbols that use `tcleval` work on iPadOS too. That interpreter SHALL provide xschem-compatible definitions of the Tcl procedures that PDK symbols call (Requirement "PDK attribute conventions"): `ev`, `ev7`, `ev0`, `to_eng`, `from_eng`, `ngspice::get_node`, `ngspice::get_current`, `xschem getprop`, `xschem get` (subset), and the variables set by the active PDK's `xschemrc`. See Q-SCHEM-7.

The set of procedures and variables SHALL be exactly the one that an inventory script finds in the three pinned PDK trees (every `tcleval(` value, every `[`-command and every `$::` variable). The inventory SHALL be kept as a test that fails when a PDK bump adds a procedure or variable that Hefesto does not provide. Each procedure's semantics SHALL be specified from the public xschem manual and from expected values computed for the PDK symbols, never from xschem's Tcl code (capability `hefesto-licensing`, Requirement "Two-team clean-room process for specs written from GPL source"; Q-SCHEM-8, `Resolution: other`).

**Where the inventoried names come from.** The names of `tests/data/schem/inventory.json` (`tcleval_commands`, `tcleval_variables`, `rc_commands`, `rc_variables`) fall into four groups, and only the first needs semantics here:
1. Procedures that xschem provides, specified in the table below: `ev`, `ev7`, `ev0`, `to_eng`, `from_eng`, `sim_is_ngspice`, `sim_is_xyce`, `tabulate`, `ngspice::get_node`, `ngspice::get_current`, `ngspice::get_voltage`, `abs_sym_path`, `textwindow`, `write_data`. `ngspice::get_current` and `ngspice::get_voltage` are not in the inventory: the sky130 example schematics call them in the `node` attribute of `ngspice_get_expr.sym` instances, which only the symbol's displayed text evaluates (the known gap of task 2.3 of `openspec/changes/m8-schematic`). `ev0` and `from_eng` are used by none of the pinned trees (the sky130 generator `sky130_tests/res.tcl` defines its own `from_eng`) and are provided because Q-SCHEM-8 names them.
2. Procedures that the PDK defines in its own `xschemrc` or in the Tcl files it sources (the inventory's `pdk_procs`: for example `display_fet_params`, `sky130_display_fet_params`, `display_bip_params`, `cap_cmomi_C`, `symtype`, `hierarchy_config`). They SHALL run as the PDK wrote them, in the sandbox; their results follow from the procedures they call.
3. Tcl and Tk built-in commands (`append`, `expr`, `foreach`, `format`, `if`, `return`, `set`, `subst`, `string`, `file`, `menu`, `toplevel`, ...), with Tcl 8.6 semantics as the sandbox of capability `hefesto-scripting` provides them. Tk commands in an `xschemrc` only build PDK menus and dialogs (capability `hefesto-scripting` decides how they are served; they never change a netlist).
4. The `xschem` command, whose subcommands capability `schematic-command` specifies (Requirement "xschem compatibility command").

The variables are set by the active PDK's `xschemrc`, read as data (Requirement "xschemrc as data"). Their expected values are in the table "Expected values of the inventoried variables".

**Procedures that xschem provides**

Numbers below are printed as Tcl `format` prints them (C `printf` conventions: `%.<N>g` gives at most N significant digits, drops trailing zeros and uses the exponent form `1e-06` when the exponent is below −4 or at least N). An expression is in Tcl `expr` syntax and is evaluated in the global scope.

| Procedure | Behaviour | Source | Compatibility |
|---|---|---|---|
| `ev <arg> ...` | The arguments joined with single spaces are evaluated as an expression. On success the result is the value printed with `%.<N>g`; an integer result prints as the integer. If the expression has an error or does not evaluate, the result is the argument text unchanged. N is not documented publicly; it SHALL be pinned by capture (open item of task 4.3); every pinned PDK use gives the same text for any N from 4 to 7 (table "Expected values of the PDK uses"). | Manual, "XSCHEM TCL PROCEDURES": "evaluate 'expr'. if 'expr' has errors or does not evaluate return 'expr' as is" | behavioral |
| `ev7 <arg> ...` | As `ev`, with N = 7. | Captured: SCHEM-NL-07 `dc_res_temp.spice` line `XR4 ... R=262.8473` (the IHP `ptap1` format, exact value 262.84733397704105) | exact |
| `ev0 <arg> ...` | As `ev`, with the number format pinned by the same capture. No pinned PDK uses it. | Q-SCHEM-8 | informative until captured |
| `to_eng <number>` | The number in engineering form with the SPICE scale suffixes (for example `131u` for `131e-6`). In the pinned PDKs every argument is a simulation result (operating-point annotation, Monte Carlo statistics, plots), so the exact digits, the suffix set and the result for an empty or non-numeric argument SHALL be pinned with the simulation-result captures of M9 (SCHEM-SIM-02, deferred). | Manual, "tutorial_ngspice_backannotation": "wrap the whole expression inside a [to_eng ... ] to have the value displayed in engineering form", "131u for 131e-6" | behavioral |
| `from_eng <text>` | The inverse of `to_eng`: a number optionally followed by a scale suffix, read case-insensitively (`a` 1e-18, `f` 1e-15, `p` 1e-12, `n` 1e-9, `u` 1e-6, `m` 1e-3, `k` 1e3, `meg` 1e6, `g` 1e9, `t` 1e12; any other text after the number scales by 1), returns the plain number. | The Apache-2.0 definition of the same name in the pinned sky130 tree (`sky130_tests/res.tcl`); manual lists the name only | informative |
| `sim_is_ngspice`, `sim_is_xyce` | 1 when the selected SPICE simulator is ngspice (respectively Xyce), otherwise 0; no arguments. Hefesto's simulator is the embedded ngspice (capability `schematic-simulation-integration`; changes to xschem's `sim(spice,...)` variables are ignored), so `sim_is_ngspice` SHALL return 1 and `sim_is_xyce` SHALL return 0. | Manual lists the names; the sky130 `test_multisim.sch` text: "Helper procedures sim_is_xyce and sim_is_ngspice" select per-simulator code | behavioral |
| `tabulate <text> <separator>` | Lays out `text`, whose rows are separated by LF and whose columns by `separator`, as an aligned text table. The pinned PDKs call it only on simulation results (sky130 `test_customplot.sch`, after `xschem raw switch`), so its exact layout SHALL be pinned with the M9 captures; before results are loaded the enclosing text fails, which gives `?` LF (step 5 above). | sky130 `test_customplot.sch` | informative until captured |
| `ngspice::get_node <expr>` | The value of the ngspice expression `expr` (a node voltage, `i(...)` current or device parameter such as `@m.xm1.msky130_fd_pr__nfet_01v8[gm]`) at the loaded operating point, as a plain number. | Manual, "@spice_get_node": the same lookup as the `@spice_get_node` token | behavioral |
| `ngspice::get_current <name>` | The current of the voltage source or device `name` at the loaded operating point. | sky130 `n_diffamp.sch`, `test_analog.sch` | behavioral |
| `ngspice::get_voltage <net>` | The voltage of net `net` at the loaded operating point. | sky130 `n_diffamp.sch` | behavioral |
| `abs_sym_path <ref>` | The absolute path of the file that symbol reference `ref` resolves to (Requirement "Symbol references"). | Manual, "XSCHEM TCL PROCEDURES": "given a symbol reference 'sym' return its absolute path" | exact |
| `textwindow <text>`, `write_data <text> <file>` | Used only by PDK menu procedures. `textwindow` shows `text` in a read-only text window; `write_data` writes `text` to `file`, inside the sandbox's granted folders only (capability `hefesto-scripting`). | Manual lists the names | informative |

The three `ngspice::` procedures read simulation results, which are M9's (capability `schematic-simulation-integration`, Requirement "Operating-point annotation"). Before results are loaded they SHALL return the empty string, as the `@spice_get_*` tokens show nothing; SCHEM-SIM-02 (deferred to M9) pins this against xschem.

**Expected values of the PDK uses**

Every inventoried use of an xschem procedure in a `tcleval(` value, with the template defaults of its symbol (instance attributes, when an example schematic sets them, change only the inputs). Uses whose inputs are simulation results have no value in M8 and are listed with their M8 result.

| PDK | Where | Use | Inputs | Expected text |
|---|---|---|---|---|
| ihp | `sg13g2_pr/ptap1.sym`, `ntap1.sym` `format` | `R=[ev7 { 1.0 / ( 1.0 / ( 9.8e-10 / ( @w * @l ) ) + 1.0 / ( 9.8e-4 / ( 2.0 * ( @w + @l ) ) ) )}]` | w=0.78e-6 l=0.78e-6 | `R=262.8473` (captured, SCHEM-NL-07) |
| ihp | `ptap1.sym`, `ntap1.sym` `lvs_format` | `A=[ev7 { @w * @l } ] P=[ev7 { 2 * ( @w + @l ) } ]` | w=0.78e-6 l=0.78e-6 | `A=6.084e-13 P=3.12e-06` |
| ihp | `sg13g2_pr/pnpMPA.sym` `format` and `lvs_format` | `a=[ev7 { @w * @l }] p=[ev7 { ( @w + @l ) * 2 }] m=[ev { @m }]` | w=0.7e-6 l=2.0e-6 m=1 | `a=1.4e-12 p=5.4e-06 m=1` |
| ihp | `sg13g2_pr/schottky_nbl1.sym` `lvs_format` | `m=[ev { @Nx * @Ny } ]` | Nx=1 Ny=1 | `m=1` |
| ihp | `sg13g2_pr/npn13G2v.sym`, `npn13G2v_5t.sym`, `npn13G2l.sym` `lvs_format` | `le=[ev { @El * 1.0e-6 } ]` | El=1.0 | `le=1e-06` |
| ihp | `sg13g2_pr/npn13G2l_5t.sym` `lvs_format` | `le=[ev { @El * 1.0e-6 } ]` | El=2.5 | `le=2.5e-06` |
| ihp | `sg13g2_pr/cap_cmim.sym` displayed text | `C=[ev {@m * (@w * @l * 1.5e-3 + 2*( @w + @l ) * 40e-12)}]` | m=1 w=7.0e-6 l=7.0e-6 | `C=7.462e-14` |
| ihp | `sg13g2_pr/cap_rfcmim.sym` displayed text | as `cap_cmim.sym` | m=1 w=10.0e-6 l=10.0e-6 | `C=1.516e-13` |
| ihp | `sg13g2_pr/cap_cmomf.sym` displayed text | `C=[ev {@m * ( ((@mmin == 1) ? 0.372 : 0.305) + max(0, @mmax - @mmin ) * 0.305 ) * ( @l * 1e6 ) * ( @w * 1e6 ) * 1e-15}]` | m=1 w=5e-6 l=5e-6 mmin=1 mmax=5 | `C=3.98e-14` |
| ihp | `sg13g2_pr/cap_cmomi.sym` displayed text | `C=[cap_cmomi_C @m @l @w @mmin @mmax @feed ]` (PDK procedure, `sg13g2_pr/cap_cmomi.tcl`) | m=1 l=5e-6 w=5e-6 mmin=1 mmax=5 feed=double | `C=2.616e-14` |
| ihp | `libs.ref/sg13g2_stdcell/sym/xschem/*.sym` `type` and `schematic` | `tcleval([symtype])`, `tcleval([hierarchy_config @symname])` (PDK procedures) | `stdcell_sym_type` and `stdcell_view` as set by the `xschemrc`; cell `sg13g2_inv_1` | `subcircuit`; `<R>/ihp-sg13g2/libs.ref/sg13g2_stdcell/sch/xschem/sg13g2_inv_1.sch` |
| sky130 | `sky130_tests/test_multisim.sch`, `test_inv.sch` code blocks | `[if [sim_is_ngspice] {return {...}}]`, `[if [sim_is_xyce] {...}]` | embedded ngspice | the ngspice block is emitted, the Xyce block is not |
| sky130 | `simulator_commands.sym`, `simulator_commands_shown.sym` instances (system library) | `sim_is_<simulator>` | simulator=ngspice | LF LF value LF; simulator=xyce gives LF |
| sky130 | `sky130_tests/test_customplot.sch` displayed text | `tabulate`, `to_eng`, `xschem raw ...` | no results loaded | `?` LF |
| sky130 | `sky130_tests/n_diffamp.sch`, `test_analog.sch` `ngspice_get_expr.sym` instances | `[ngspice::get_current v3]`, `[to_eng [ngspice::get_current {...}] * [ngspice::get_voltage s] ]W` | no results loaded | M9 (SCHEM-SIM-02) |
| gf180mcu | `gf180mcu_fd_pr/*fet*.sym` displayed texts | `gm=[to_eng [ngspice::get_node [subst -nocommand {...}]]]`, `id=...` | no results loaded | M9 (SCHEM-SIM-02) |
| gf180mcu, ihp, sky130 | `annotate_fet_params.sym`, `annotate_bip_params.sym` displayed texts | PDK procedures (group 2) that call `xschem raw value` and `to_eng` | no results loaded | M9 (SCHEM-SIM-02) |

**Expected values of the inventoried variables**

With the PDK store root `<R>` (capability `hefesto-files-and-pdks`) passed as `PDK_ROOT` and no `PDK` override (golden captures use `<R>` = `/pdk`):

| PDK | Variable | Expected value | Used by |
|---|---|---|---|
| sky130 | `SKYWATER_MODELS` | `<R>/sky130A/libs.tech/combined` | `sky130_fd_pr/corner.sym` (`tcleval(.lib $::SKYWATER_MODELS/sky130.lib.spice @corner )`, scenario "sky130 corner include"), example code blocks |
| sky130 | `SKYWATER_STDCELLS` | `<R>/sky130A/libs.ref/sky130_fd_sc_hd/spice` | `sky130_tests/LACG.sch` and other code blocks |
| ihp | `PDK_ROOT` | `<R>` | the PDK's `hierarchy_config` procedure |
| ihp | `PDK` | `ihp-sg13g2` | as above |
| ihp | `MODELS_NGSPICE`, `SG13G2_MODELS` | `<R>/ihp-sg13g2/libs.tech/ngspice/models` | testbench code blocks (`.lib $::MODELS_NGSPICE/cornerRES.lib ...`) |
| ihp | `MODELS_XYCE`, `SG13G2_MODELS_XYCE` | `<R>/ihp-sg13g2/libs.tech/xyce/models` | Xyce testbenches, `start_page.sch` |
| ihp | `stdcell_view` | `subcircuit` | the PDK's `hierarchy_config` procedure |
| ihp | `stdcell_sym_type` | `subcircuit` | the PDK's `symtype` procedure (standard-cell `type=tcleval([symtype])`) |

Compatibility: behavioral

(xschem ref: token.c:59-81, xschem.tcl:7224-7298, xschem.tcl:677-698)

#### Scenario: sky130 corner include
- **GIVEN** the sky130 `corner.sym` instance with `corner=tt` and the Tcl variable `SKYWATER_MODELS` set to `/pdk/sky130A/libs.tech/combined`
- **WHEN** its format `tcleval(.lib $::SKYWATER_MODELS/sky130.lib.spice @corner )` is netlisted
- **THEN** the netlist text SHALL be `.lib /pdk/sky130A/libs.tech/combined/sky130.lib.spice tt`

#### Scenario: IHP ev7 expression
- **GIVEN** an IHP resistor format containing `A=[ev7 \{ @w * @l \} ]` inside `tcleval(...)`, with w=2e-6 and l=5e-6
- **WHEN** it is netlisted
- **THEN** the area SHALL be printed as `A=1e-11`, which is Tcl `format %.7g` of the product

#### Scenario: IHP ptap1 resistance
- **GIVEN** IHP `sg13g2_tests/dc_res_temp.sch` with the IHP `xschemrc`, whose `ptap1` instance `R4` has w=0.78e-6 l=0.78e-6
- **WHEN** it is netlisted
- **THEN** the element line SHALL be `XR4 GND sub! ptap1 R=262.8473 w=0.78e-6 l=0.78e-6` (golden case SCHEM-NL-07)

#### Scenario: Simulator selection in a code block
- **GIVEN** a `code_shown.sym` instance with `format="tcleval( @value )"` and value `[if [sim_is_xyce] {return {X}} else {return {N}}]`
- **WHEN** it is netlisted
- **THEN** the text SHALL contain `N`, because the embedded simulator is ngspice

### Requirement: Script execution permission
xschem guards `tcleval` and `tclcommand` with a user setting whose values are `ask`, true or false. Hefesto SHALL instead run `tcleval` attribute values always, inside the sandbox of capability `hefesto-scripting`, because the sandbox removes process execution, sockets and file access outside granted folders. Launcher `tclcommand` scripts SHALL run only after the user confirms them once per document, and SHALL run in the same sandbox. The xschem `xschem_execute_scripts` variable in an `xschemrc` SHALL be read and ignored, and the log SHALL say that it was ignored. This permission model SHALL be signed off by a security review against capability `hefesto-security` (Requirement "Schematic script permission model") before M8 exits (Q-SCHEM-25, `Resolution: platform`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12)

#### Scenario: Launcher confirmation
- **GIVEN** a schematic with a launcher whose `tclcommand` is `xschem netlist`
- **WHEN** the user Ctrl+clicks it for the first time in this document
- **THEN** Hefesto SHALL ask for confirmation, and SHALL NOT ask again for that document in the session

#### Scenario: xschem_execute_scripts ignored
- **GIVEN** the sky130 `xschemrc`, which sets `xschem_execute_scripts` to `yes`
- **WHEN** it is read
- **THEN** the value SHALL have no effect, `tcleval` values SHALL still run in the sandbox, launchers SHALL still be confirmed, and the log SHALL report that the variable was ignored

### Requirement: Symbol references
A symbol reference in a `C` record SHALL be resolved to a file as follows:
1. An empty reference resolves to nothing.
2. An `http://` or `https://` reference SHALL be kept as written and SHALL NOT be downloaded. The instance SHALL be shown as a missing symbol (Requirement "Missing symbols") and a warning naming the reference SHALL be reported (Q-SCHEM-9, `Resolution: platform`).
3. A reference starting with `/` is an absolute path.
4. Otherwise, runs of `/` SHALL be collapsed, `/./` removed, `dir/../` pairs removed, and a trailing `/` or `/.` removed.
5. A reference that then starts with `./` or `../` SHALL be resolved against the folder of the schematic that contains the instance, whatever the `xschem-compat` setting; it is used if the file or its folder exists (Q-SCHEM-30, `Resolution: fix`; paragraph "Working folder").
6. If the first path component is a library alias (Requirement "Library search path"), the alias SHALL be replaced by its folder.
7. Otherwise, each folder of the search path is tried in order, and the first `<folder>/<reference>` that exists is used.
8. If none exists, `<folder of the containing schematic>/<reference>` is used.

When Hefesto writes a reference it chose itself (placing, replacing or re-pathing a symbol), it SHALL write the shortest form: the path relative to the first search-path folder that is a prefix of the absolute path, or the alias form, or the bare file name if the file is in the schematic's folder, or else the absolute path. A folder that has an alias SHALL be written in the alias form (golden case SCHEM-PR-05: with the element `/work/input/libB | lib`, `/work/input/libB/x.sym` is written `lib/x.sym`). References read from a file SHALL be written back unchanged.

**Working folder (Q-SCHEM-30, `Resolution: fix`).** The `./` and `../` references of step 5 and the search-path element `.` of Requirement "Library search path" SHALL be resolved against the folder of the schematic that contains the instance, never against the folder Hefesto was started in, in both modes of `xschem-compat`, so a document resolves the same way on every platform and from every start folder. xschem resolves them against the folder it was started in (golden case SCHEM-PR-05, started in `/work` and loading `/work/input/sub/top.sch`: `./x.sym` resolves to `/work/x.sym`, `../x.sym` to `//x.sym`, and the instance `../x.sym` of `top.sch` is reported as a missing symbol although `/work/input/x.sym` exists); the two agree when xschem is started in the schematic's folder. Because xschem may resolve such a reference elsewhere, each reference resolved through `./`, `../` or the `.` element SHALL report, once per reference, the warning `SCHEM-LIB-001` `Relative symbol reference <reference> resolved against the schematic folder <folder>; xschem resolves it against its start folder`. SCHEM-PR-05 differs from xschem in its `./` and `../` lines by this decision (a recorded deviation).

Compatibility: exact

(xschem ref: xschem.tcl:8731-8883, save.c:3350-3360)

#### Scenario: Library-relative reference
- **GIVEN** the search path `/pdk/xschem:/pdk/xschem/sg13g2_pr` and the reference `sg13g2_pr/sg13_lv_nmos.sym`
- **WHEN** it is resolved
- **THEN** the file SHALL be `/pdk/xschem/sg13g2_pr/sg13_lv_nmos.sym`

#### Scenario: Relative reference resolved against the schematic folder
- **GIVEN** golden case SCHEM-PR-05: `/work/input/sub/top.sch` with the instance `../x.sym`, the files `/work/input/x.sym` and `/work/input/sub/x.sym`, and Hefesto started in `/work`
- **WHEN** `top.sch` is opened and `./x.sym` and `../x.sym` are resolved, with `xschem-compat` ON or OFF
- **THEN** `../x.sym` SHALL resolve to `/work/input/x.sym` and `./x.sym` to `/work/input/sub/x.sym`, the instance SHALL NOT be reported as a missing symbol, and the warning `SCHEM-LIB-001` SHALL be reported once for each of the two references

#### Scenario: Web reference not fetched
- **GIVEN** an instance whose reference is `https://example.com/lib/amp.sym`
- **WHEN** the schematic is opened
- **THEN** no network request SHALL be made, the instance SHALL show the missing-symbol placeholder, a warning SHALL name the reference, and saving SHALL write the reference unchanged

### Requirement: Library search path
The search path SHALL be an ordered list of folders built from the value of `XSCHEM_LIBRARY_PATH`:
- the value SHALL be split at `:`; on Windows hosts it is split at `;` (informative)
- an element of the form `<folder> | <alias>` (spaces around `|` optional, `\|` an escaped bar) SHALL define the alias `<alias>` for `<folder>`
- elements that start with `#` SHALL be dropped
- `~` and environment references in an element SHALL be expanded, and elements that do not exist SHALL be dropped
- the element `.` means the folder of the current schematic, never the folder Hefesto was started in, whatever the `xschem-compat` setting; a reference found through it reports the warning `SCHEM-LIB-001` (Q-SCHEM-30, Requirement "Symbol references", paragraph "Working folder")
- if the result is empty, it SHALL be the current schematic's folder

Hefesto SHALL build `XSCHEM_LIBRARY_PATH` from the active PDK's `xschemrc` (Requirement "xschemrc as data"), and SHALL also accept a search path set in Hefesto settings (capability `hefesto-settings`) that is prepended to it.

Compatibility: behavioral

(xschem ref: xschem.tcl:11467-11530)

#### Scenario: Missing folder dropped
- **GIVEN** `XSCHEM_LIBRARY_PATH` = `:/a:/missing:/b` where `/missing` does not exist
- **WHEN** the search path is built
- **THEN** it SHALL be `/a`, `/b`

#### Scenario: Dot element is the schematic folder
- **GIVEN** `XSCHEM_LIBRARY_PATH` = `/a:.`, the schematic `/w/s/top.sch` with the instance `y.sym`, the file `/w/s/y.sym`, no `/a/y.sym`, and Hefesto started in `/w`
- **WHEN** the instance is resolved
- **THEN** its symbol SHALL be `/w/s/y.sym`, in both modes of `xschem-compat`, and the warning `SCHEM-LIB-001` SHALL name `y.sym` and `/w/s`

### Requirement: xschemrc as data
PDKs configure xschem through a Tcl file named `xschemrc`. Hefesto SHALL read it as follows:
- The `xschemrc` of the active PDK (`<PDK root>/<PDK>/libs.tech/xschem/xschemrc` in the PDK store of capability `hefesto-files-and-pdks`) SHALL be evaluated in the sandboxed Tcl interpreter (capability `hefesto-scripting`).
- Before evaluation, Tcl variables SHALL be set: `XSCHEM_SHAREDIR` (the Hefesto copy of the xschem system library, Requirement "System library"), `USER_CONF_DIR`, `PDK_ROOT`, `PDK`, `env(PDK_ROOT)`, `env(PDK)` and `env(HOME)`.
- `XSCHEM_LIBRARY_PATH` and every other global variable SHALL be read from the interpreter afterwards; Tcl variables such as `SKYWATER_MODELS`, `SG13G2_MODELS` and `MODELS_NGSPICE` SHALL stay defined for `tcleval`.
- Commands that only affect the xschem GUI (colours, toolbar, `dircolor`, `XSCHEM_START_WINDOW`, `bespice_listen_port`, `to_pdf`, ...) SHALL be accepted and ignored.
- `source` of a Tcl file inside the PDK (for example IHP `cap_cmomi.tcl`) SHALL be allowed.
- An `xschemrc` in the document's folder SHALL be evaluated after the PDK's one, as xschem does when started in that folder.
- Environment variables read by the file SHALL be synthetic values mapped to the PDK store and the document folder on every platform (see Q-SCHEM-23).

In xschem, the files evaluated are, in order: the system `xschemrc`, then the one given on the command line, or else `./xschemrc`, or else the user's `~/.xschem/xschemrc` (informative).

Compatibility: behavioral

(xschem ref: xinit.c:3552-3610)

#### Scenario: IHP library path
- **GIVEN** the IHP sg13g2 PDK installed in the PDK store
- **WHEN** a schematic is opened with sg13g2 active
- **THEN** the search path SHALL contain `<PDK root>/ihp-sg13g2/libs.tech/xschem`, `<PDK root>/ihp-sg13g2/libs.tech/xschem/sg13g2_pr` and `<PDK root>/ihp-sg13g2/libs.ref/sg13g2_stdcell/sym/xschem`, in that order after the system library folders

### Requirement: System library
Hefesto SHALL ship a clean-room symbol library that is reference-compatible with the xschem `xschem_library/devices` library: the same file names (for example `res.sym`, `capa.sym`, `vsource.sym`, `isource.sym`, `gnd.sym`, `vdd.sym`, `lab_pin.sym`, `lab_wire.sym`, `ipin.sym`, `opin.sym`, `iopin.sym`, `code.sym`, `code_shown.sym`, `launcher.sym`, `title.sym`, `noconn.sym`, `ammeter.sym`, `spice_probe.sym`, `netlist_options.sym`), the same pin names and order, pin positions and symbol properties (`type`, `format`, `template`, `global`, ...) that produce identical netlists. Their graphics MAY differ. The library SHALL be available as both `devices/<name>` and `<name>`, because PDK schematics use both forms. The library SHALL contain only the symbols that the three PDK libraries and their example schematics reference: the list SHALL be generated by an inventory script over the pinned Apache-2.0 PDK trees and kept as a test that fails when a PDK bump references a symbol that the library lacks (Q-SCHEM-10, `Resolution: other`).

Clean-room provenance (capability `hefesto-licensing`, Requirement "Two-team clean-room process for specs written from GPL source"): the interface of each symbol (file name, pin names, pin order, pin positions and the netlisting properties) has no public source other than xschem's GPL `devices/*.sym` files. It SHALL be extracted from them by the spec side only and recorded in the four interface tables below; implementers SHALL take it from this spec, and SHALL draw the graphics independently. The tables are covered by the one counsel review that `hefesto-licensing` requires before Desktop 1.0.

The interface tables SHALL be read as follows:
- **Rows.** The tables hold one symbol for every entry of the `devices` lists of `tests/data/schem/inventory.json` (the inventory of design D18 of `openspec/changes/m8-schematic`), and `netlist_options.sym`, which no PDK references but golden case SCHEM-NL-17 places (Q-SCHEM-31, proposed). Column "Referenced by" names the PDKs whose inventory lists the symbol.
- **Values.** Every value in the property and note tables is written as a JSON string literal (RFC 8259: `\n` is LF, `\"` a quote, `\\` a backslash, ``` a backtick, `|` a vertical bar). A property value is the **decoded value** of that key (capability `schematic-file-format`, Requirement "Property strings"): the value a lookup returns, after the brace-string escapes of the `K` record are resolved and with the unescaped quotes and backslashes of the property string removed. A library file SHALL hold each listed key with a text whose decoded value is exactly the listed value, and SHALL hold no other key that changes SPICE netlisting or display.
- **Keys not listed.** Keys that only other netlisters or tools read are not part of the interface and are left out: `spectre_format`, `spectre_device_model`, `verilog_format`, `vhdl_format`, `tedax_format`, `verilog_ignore`, `vhdl_ignore`, `tedax_ignore`, `function0`, `function1` (SPICE is the only M8 netlister, Q-SCHEM-4). A library file MAY hold them.
- **Pins.** Pins are listed in pin order: index 0 is the first `B` record on layer 5 of the file, and `@pinlist` and `@#<n>:` tokens follow this order. No listed symbol sets `sim_pinnumber`. "Box" is the pin rectangle `x1 y1 x2 y2` in symbol coordinates (y down, Requirement "Coordinate system" of capability `schematic-file-format`), corners sorted. "Other pin properties" lists the pin's attributes besides `name` and `dir`, in file order, or `-` when there are none. Two pins of `short.sym` share the name `A`.
- **Bounding box.** `x1 y1 x2 y2` of the symbol's lines, rectangles (pins included), polygon points and arcs (the swept part of each arc only), texts excluded. It is informative: Hefesto's graphics SHOULD keep within it so that placement and selection match xschem; it does not change netlists.
- **Notes.** A note states behaviour that a property value alone does not show. Where a note replaces a property value (the `format` of `simulator_commands.sym` and `simulator_commands_shown.sym`), the library file SHALL hold a value with the behaviour the note states, and the property table does not list that key.

**Interface table: symbols**

| Symbol | Referenced by | Bounding box |
|---|---|---|
| `ammeter.sym` | gf180mcu ihp sky130 | `-7.5 -32.5 7.5 32.5` |
| `architecture.sym` | sky130 | `0 -10 355 -10` |
| `bsource.sym` | sky130 | `-20 -32.5 15 32.5` |
| `bus_tap.sym` | sky130 | `-0.625 -10.625 10.625 0.625` |
| `capa-2.sym` | ihp | `-10 -32.5 10 32.5` |
| `capa.sym` | ihp sky130 | `-10 -32.5 10 32.5` |
| `cccs.sym` | sky130 | `-20 -32.5 20 32.5` |
| `code.sym` | sky130 | `10 0 110 110` |
| `code_shown.sym` | gf180mcu ihp sky130 | `0 -10 70 10` |
| `gnd.sym` | gf180mcu ihp sky130 | `-5 -2.5 5 17.5` |
| `ind.sym` | ihp sky130 | `-7.5 -32.5 10 32.5` |
| `intuitive_interface_cheatsheet.sym` | ihp sky130 | `-260 -60 280 30` |
| `iopin.sym` | sky130 | `-1.25 -5 17.5 5` |
| `ipin.sym` | sky130 | `-17.5 -5 1.25 5` |
| `isource.sym` | ihp sky130 | `-15 -32.5 15 32.5` |
| `lab_pin.sym` | gf180mcu ihp sky130 | `-1.25 -1.25 1.25 1.25` |
| `lab_show.sym` | sky130 | `-20 -10 1.25 1.25` |
| `lab_wire.sym` | ihp sky130 | `-1.25 -1.25 1.25 1.25` |
| `launcher.sym` | gf180mcu ihp sky130 | `-60 -12.5 0 12.5` |
| `netlist_options.sym` | - | `0 -10 355 -10` |
| `ngspice_get_expr.sym` | sky130 | `-1.875 -1.875 1.875 1.875` |
| `ngspice_get_value.sym` | sky130 | `-1.875 -1.875 1.875 1.875` |
| `ngspice_probe.sym` | sky130 | `-0.46875 -5 5 0.46875` |
| `njfet.sym` | sky130 | `-22.5 -32.5 22.5 32.5` |
| `noconn.sym` | ihp sky130 | `-3.75 -1.25 1.25 1.25` |
| `opin.sym` | sky130 | `-1.25 -5 17.5 5` |
| `parax_cap.sym` | ihp sky130 | `-5 -12.5 5 15` |
| `pjfet.sym` | sky130 | `-22.5 -32.5 22.5 32.5` |
| `res.sym` | gf180mcu ihp sky130 | `-7.5 -32.5 7.5 32.5` |
| `short.sym` | sky130 | `-2.5 -32.5 2.5 32.5` |
| `simulator_commands.sym` | sky130 | `10 0 110 110` |
| `simulator_commands_shown.sym` | ihp sky130 | `0 -10 70 10` |
| `spice_probe.sym` | sky130 | `-0.46875 -27.5 28.75 0.46875` |
| `spice_probe_vdiff.sym` | sky130 | `-2.5 -21.25 2.5 21.25` |
| `sqwsource.sym` | sky130 | `-15 -32.5 15 32.5` |
| `switch_ngspice.sym` | sky130 | `-42.5 -32.5 2.5 32.5` |
| `title-3.sym` | ihp | `0 -1770 2500 30` |
| `title.sym` | gf180mcu ihp sky130 | `-160 -30 1020 30` |
| `vcvs.sym` | sky130 | `-42.5 -32.5 20 32.5` |
| `vdd.sym` | gf180mcu sky130 | `-10 -20 10 2.5` |
| `verilog_preprocessor.sym` | sky130 | `0 -10 70 10` |
| `vsource.sym` | gf180mcu ihp sky130 | `-15 -32.5 15 32.5` |
| `vsource_arith.sym` | sky130 | `-20 -32.5 15 32.5` |

**Interface table: properties**

| Symbol | Key | Value |
|---|---|---|
| `ammeter.sym` | `type` | `"ammeter"` |
| `ammeter.sym` | `format` | `"@name @pinlist 0 @savecurrent"` |
| `ammeter.sym` | `template` | `"name=Vmeas savecurrent=true spice_ignore=0"` |
| `architecture.sym` | `type` | `"architecture"` |
| `architecture.sym` | `spice_ignore` | `"true"` |
| `architecture.sym` | `template` | `"name=a1 code=ARCHITECTURE"` |
| `bsource.sym` | `type` | `"source"` |
| `bsource.sym` | `format` | `"@name @pinlist @VAR = @FUNC m=@m"` |
| `bsource.sym` | `template` | `"name=B1 VAR=I FUNC=\"'pwl(V(plus,minus),0,0, 1,10m, 2, 100m)'\" m=1"` |
| `bus_tap.sym` | `type` | `"bus_tap"` |
| `bus_tap.sym` | `template` | `"name=l1 lab=[0]"` |
| `bus_tap.sym` | `format` | `"* tap: @#1:net_name --> @#0:net_name"` |
| `capa-2.sym` | `type` | `"polarized_capacitor"` |
| `capa-2.sym` | `format` | `"@name @pinlist @value m=@m"` |
| `capa-2.sym` | `template` | `"name=C1\nm=1\nvalue=1p\nfootprint=1206\ndevice=polarized_capacitor"` |
| `capa.sym` | `type` | `"capacitor"` |
| `capa.sym` | `format` | `"@name @pinlist @value m=@m"` |
| `capa.sym` | `template` | `"name=C1\nm=1\nvalue=1p\nfootprint=1206\ndevice=\"ceramic capacitor\""` |
| `cccs.sym` | `type` | `"cccs"` |
| `cccs.sym` | `format` | `"@name @pinlist @vnam @value"` |
| `cccs.sym` | `template` | `"name=F1 vnam=v1 value=1"` |
| `code.sym` | `type` | `"netlist_commands"` |
| `code.sym` | `template` | `"name=s1 only_toplevel=false value=blabla"` |
| `code.sym` | `format` | `"\n@value\n"` |
| `code_shown.sym` | `type` | `"netlist_commands"` |
| `code_shown.sym` | `template` | `"name=s1 only_toplevel=false value=blabla"` |
| `code_shown.sym` | `format` | `"\n@value\n"` |
| `gnd.sym` | `type` | `"label"` |
| `gnd.sym` | `global` | `"ground"` |
| `gnd.sym` | `format` | `"*.alias @lab"` |
| `gnd.sym` | `template` | `"name=l1 lab=0"` |
| `ind.sym` | `type` | `"inductor"` |
| `ind.sym` | `format` | `"@name @pinlist @value m=@m"` |
| `ind.sym` | `template` | `"name=L1\nm=1\nvalue=1n\nfootprint=1206\ndevice=inductor"` |
| `intuitive_interface_cheatsheet.sym` | `type` | `"subcircuit"` |
| `intuitive_interface_cheatsheet.sym` | `format` | `"@name @pinlist @symname"` |
| `intuitive_interface_cheatsheet.sym` | `template` | `"name=x1"` |
| `iopin.sym` | `type` | `"iopin"` |
| `iopin.sym` | `format` | `"*.iopin @lab"` |
| `iopin.sym` | `template` | `"name=p1 lab=xxx"` |
| `ipin.sym` | `type` | `"ipin"` |
| `ipin.sym` | `format` | `"*.ipin @lab"` |
| `ipin.sym` | `template` | `"name=p1 lab=xxx"` |
| `isource.sym` | `type` | `"isource"` |
| `isource.sym` | `format` | `"@name @pinlist @value"` |
| `isource.sym` | `template` | `"name=I0 value=1m"` |
| `lab_pin.sym` | `type` | `"label"` |
| `lab_pin.sym` | `format` | `"*.alias @lab"` |
| `lab_pin.sym` | `template` | `"name=p1 sig_type=std_logic lab=xxx"` |
| `lab_show.sym` | `type` | `"show_label"` |
| `lab_show.sym` | `template` | `"name=l1"` |
| `lab_show.sym` | `highlight` | `"true"` |
| `lab_wire.sym` | `type` | `"label"` |
| `lab_wire.sym` | `format` | `"*.alias @lab"` |
| `lab_wire.sym` | `template` | `"name=p1 sig_type=std_logic lab=xxx"` |
| `launcher.sym` | `type` | `"launcher"` |
| `launcher.sym` | `format` | `"** @descr : @url"` |
| `launcher.sym` | `spice_ignore` | `"true"` |
| `launcher.sym` | `template` | `"name=h1\ndescr=google \nurl=www.google.com"` |
| `netlist_options.sym` | `type` | `"netlist_options"` |
| `netlist_options.sym` | `template` | `"\nbus_replacement_char=[]\nlvs_netlist=false\nspiceprefix=true\nhiersep=.\n"` |
| `ngspice_get_expr.sym` | `type` | `"probe"` |
| `ngspice_get_expr.sym` | `spice_ignore` | `"false"` |
| `ngspice_get_expr.sym` | `template` | `"name=r1 node=xxx\ndescr=\"I=\""` |
| `ngspice_get_value.sym` | `type` | `"probe"` |
| `ngspice_get_value.sym` | `spice_ignore` | `"false"` |
| `ngspice_get_value.sym` | `template` | `"name=r1 node=xxx\ndescr=\"I=\""` |
| `ngspice_probe.sym` | `type` | `"probe"` |
| `ngspice_probe.sym` | `spice_ignore` | `"false"` |
| `ngspice_probe.sym` | `template` | `"name=r1"` |
| `njfet.sym` | `type` | `"njfet"` |
| `njfet.sym` | `format` | `"@spiceprefix@name @pinlist @model area=@area @extra m=@m"` |
| `njfet.sym` | `template` | `"name=J1 model=njfet area=1 m=1"` |
| `noconn.sym` | `type` | `"noconn"` |
| `noconn.sym` | `format` | `"* noconn @#0:net_name"` |
| `noconn.sym` | `template` | `"name=l1"` |
| `opin.sym` | `type` | `"opin"` |
| `opin.sym` | `format` | `"*.opin @lab"` |
| `opin.sym` | `template` | `"name=p1 lab=xxx"` |
| `parax_cap.sym` | `type` | `"parax_cap"` |
| `parax_cap.sym` | `format` | `"@name @pinlist @gnd @value m=@m"` |
| `parax_cap.sym` | `template` | `"name=C1 gnd=0 value=4f m=1"` |
| `pjfet.sym` | `type` | `"pjfet"` |
| `pjfet.sym` | `format` | `"@spiceprefix@name @pinlist @model area=@area @extra m=@m"` |
| `pjfet.sym` | `template` | `"name=J1 model=pjfet area=1 m=1"` |
| `res.sym` | `type` | `"resistor"` |
| `res.sym` | `format` | `"@name @pinlist @value m=@m"` |
| `res.sym` | `template` | `"name=R1\nvalue=1k\nfootprint=1206\ndevice=resistor\nm=1"` |
| `short.sym` | `type` | `"short"` |
| `short.sym` | `template` | `"name=x1 value=0.1"` |
| `short.sym` | `format` | `"* short @name : @#0:net_name <--> @#1:net_name"` |
| `short.sym` | `highlight` | `"true"` |
| `simulator_commands.sym` | `type` | `"netlist_commands"` |
| `simulator_commands.sym` | `template` | `"name=COMMANDS\nsimulator=ngspice\nonly_toplevel=false \nvalue=\"\n* ngspice commands\n\""` |
| `simulator_commands_shown.sym` | `type` | `"netlist_commands"` |
| `simulator_commands_shown.sym` | `template` | `"name=COMMANDS\nsimulator=ngspice\nonly_toplevel=false \nvalue=\"\n* ngspice commands\n\""` |
| `spice_probe.sym` | `type` | `"probe"` |
| `spice_probe.sym` | `spice_ignore` | `"false"` |
| `spice_probe.sym` | `format` | `".save @attrs v( @@p )"` |
| `spice_probe.sym` | `template` | `"name=p1 attrs=\"\""` |
| `spice_probe_vdiff.sym` | `type` | `"probe"` |
| `spice_probe_vdiff.sym` | `format` | `".save v(@@p\\) v(@@m\\)"` |
| `spice_probe_vdiff.sym` | `template` | `"name=p1"` |
| `sqwsource.sym` | `type` | `"vsource"` |
| `sqwsource.sym` | `format` | `"@name @pinlist \\\npulse 0 @vhi \\\n'0.495/ @freq ' \\\n'0.01/@freq ' \\\n'0.01/@freq ' \\\n'0.49/@freq ' \\\n'1/@freq '"` |
| `sqwsource.sym` | `template` | `"name=V1 vhi=3 freq=1e6"` |
| `switch_ngspice.sym` | `type` | `"switch"` |
| `switch_ngspice.sym` | `format` | `"@name @@P @@M @@CP @@CM @model"` |
| `switch_ngspice.sym` | `template` | `"name=S1 model=SW1\ndevice_model=\".MODEL SW1 SW \n+ VT=0.9 VH=0.01\n+ RON=0.01 ROFF=10G \""` |
| `title-3.sym` | `type` | `"logo"` |
| `title-3.sym` | `template` | `"name=l1 author=\"Stefan Schippers\" rev=1.0 lock=false"` |
| `title-3.sym` | `spice_ignore` | `"true"` |
| `title.sym` | `type` | `"logo"` |
| `title.sym` | `template` | `"name=l1 author=\"Stefan Schippers\""` |
| `title.sym` | `spice_ignore` | `"true"` |
| `vcvs.sym` | `type` | `"vcvs"` |
| `vcvs.sym` | `format` | `"@name @pinlist @value"` |
| `vcvs.sym` | `template` | `"name=E1 value=3"` |
| `vdd.sym` | `type` | `"label"` |
| `vdd.sym` | `global` | `"true"` |
| `vdd.sym` | `format` | `"*.alias @lab"` |
| `vdd.sym` | `template` | `"name=l1 lab=VDD"` |
| `verilog_preprocessor.sym` | `type` | `"verilog_preprocessor"` |
| `verilog_preprocessor.sym` | `spice_ignore` | `"true"` |
| `verilog_preprocessor.sym` | `template` | `"name=s1  string=\"`include \\\"file\\\"\""` |
| `vsource.sym` | `type` | `"vsource"` |
| `vsource.sym` | `format` | `"@name @pinlist @value@savecurrent"` |
| `vsource.sym` | `template` | `"name=V1 value=3 savecurrent=false"` |
| `vsource_arith.sym` | `type` | `"vsource"` |
| `vsource_arith.sym` | `format` | `"@name @pinlist VOL=' @VOL '"` |
| `vsource_arith.sym` | `template` | `"name=E1 VOL=cos(V(IN))"` |

**Interface table: pins**

| Symbol | Pin | Name | dir | Box | Other pin properties |
|---|---|---|---|---|---|
| `ammeter.sym` | 0 | `plus` | `inout` | `-2.5 -32.5 2.5 -27.5` | `propag=1` |
| `ammeter.sym` | 1 | `minus` | `inout` | `-2.5 27.5 2.5 32.5` | `propag=0` |
| `bsource.sym` | 0 | `p` | `inout` | `-2.5 -32.5 2.5 -27.5` | - |
| `bsource.sym` | 1 | `m` | `inout` | `-2.5 27.5 2.5 32.5` | - |
| `bus_tap.sym` | 0 | `tap` | `inout` | `9.375 -10.625 10.625 -9.375` | - |
| `bus_tap.sym` | 1 | `bus` | `inout` | `-0.625 -0.625 0.625 0.625` | - |
| `capa-2.sym` | 0 | `p` | `inout` | `-2.5 -32.5 2.5 -27.5` | `propag=1 pinnumber=1` |
| `capa-2.sym` | 1 | `m` | `inout` | `-2.5 27.5 2.5 32.5` | `propag=0 pinnumber=2` |
| `capa.sym` | 0 | `p` | `inout` | `-2.5 -32.5 2.5 -27.5` | `pinnumber=1` |
| `capa.sym` | 1 | `m` | `inout` | `-2.5 27.5 2.5 32.5` | `pinnumber=2` |
| `cccs.sym` | 0 | `p` | `in` | `-2.5 -32.5 2.5 -27.5` | - |
| `cccs.sym` | 1 | `m` | `in` | `-2.5 27.5 2.5 32.5` | - |
| `gnd.sym` | 0 | `p` | `inout` | `-2.5 -2.5 2.5 2.5` | `goto=0` |
| `ind.sym` | 0 | `p` | `inout` | `-2.5 -32.5 2.5 -27.5` | `propag=1 pinnumber=1` |
| `ind.sym` | 1 | `m` | `inout` | `-2.5 27.5 2.5 32.5` | `propag=0 pinnumber=2` |
| `iopin.sym` | 0 | `p` | `inout` | `-1.25 -1.25 1.25 1.25` | - |
| `ipin.sym` | 0 | `p` | `out` | `-1.25 -1.25 1.25 1.25` | - |
| `isource.sym` | 0 | `p` | `inout` | `-2.5 -32.5 2.5 -27.5` | - |
| `isource.sym` | 1 | `m` | `inout` | `-2.5 27.5 2.5 32.5` | - |
| `lab_pin.sym` | 0 | `p` | `in` | `-1.25 -1.25 1.25 1.25` | - |
| `lab_show.sym` | 0 | `p` | `none` | `-1.25 -1.25 1.25 1.25` | - |
| `lab_wire.sym` | 0 | `p` | `in` | `-1.25 -1.25 1.25 1.25` | - |
| `ngspice_probe.sym` | 0 | `p` | `xxx` | `-0.46875 -0.46875 0.46875 0.46875` | - |
| `njfet.sym` | 0 | `d` | `inout` | `17.5 -32.5 22.5 -27.5` | - |
| `njfet.sym` | 1 | `g` | `in` | `-22.5 -2.5 -17.5 2.5` | - |
| `njfet.sym` | 2 | `s` | `inout` | `17.5 27.5 22.5 32.5` | - |
| `noconn.sym` | 0 | `p` | `inout` | `-1.25 -1.25 1.25 1.25` | - |
| `opin.sym` | 0 | `p` | `in` | `-1.25 -1.25 1.25 1.25` | - |
| `parax_cap.sym` | 0 | `p` | `in` | `-2.5 -12.5 2.5 -7.5` | - |
| `pjfet.sym` | 0 | `s` | `inout` | `17.5 27.5 22.5 32.5` | - |
| `pjfet.sym` | 1 | `g` | `in` | `-22.5 -2.5 -17.5 2.5` | - |
| `pjfet.sym` | 2 | `d` | `inout` | `17.5 -32.5 22.5 -27.5` | - |
| `res.sym` | 0 | `P` | `inout` | `-2.5 -32.5 2.5 -27.5` | `propag=1 pinnumber=1 goto=1` |
| `res.sym` | 1 | `M` | `inout` | `-2.5 27.5 2.5 32.5` | `propag=0 pinnumber=2 goto=0` |
| `short.sym` | 0 | `A` | `inout` | `-2.5 -32.5 2.5 -27.5` | - |
| `short.sym` | 1 | `A` | `inout` | `-2.5 27.5 2.5 32.5` | - |
| `spice_probe.sym` | 0 | `p` | `xxx` | `-0.46875 -0.46875 0.46875 0.46875` | - |
| `spice_probe_vdiff.sym` | 0 | `p` | `in` | `-1.25 -21.25 1.25 -18.75` | - |
| `spice_probe_vdiff.sym` | 1 | `m` | `in` | `-1.25 18.75 1.25 21.25` | - |
| `sqwsource.sym` | 0 | `p` | `inout` | `-2.5 -32.5 2.5 -27.5` | - |
| `sqwsource.sym` | 1 | `m` | `inout` | `-2.5 27.5 2.5 32.5` | - |
| `switch_ngspice.sym` | 0 | `CP` | `in` | `-42.5 -2.5 -37.5 2.5` | - |
| `switch_ngspice.sym` | 1 | `P` | `inout` | `-2.5 -32.5 2.5 -27.5` | - |
| `switch_ngspice.sym` | 2 | `M` | `inout` | `-2.5 27.5 2.5 32.5` | - |
| `switch_ngspice.sym` | 3 | `CM` | `in` | `-42.5 17.5 -37.5 22.5` | - |
| `vcvs.sym` | 0 | `p` | `inout` | `-2.5 -32.5 2.5 -27.5` | - |
| `vcvs.sym` | 1 | `m` | `inout` | `-2.5 27.5 2.5 32.5` | - |
| `vcvs.sym` | 2 | `cp` | `in` | `-42.5 -22.5 -37.5 -17.5` | - |
| `vcvs.sym` | 3 | `cm` | `in` | `-42.5 17.5 -37.5 22.5` | - |
| `vdd.sym` | 0 | `p` | `inout` | `-2.5 -2.5 2.5 2.5` | `verilog_type=wire goto=0` |
| `vsource.sym` | 0 | `p` | `inout` | `-2.5 -32.5 2.5 -27.5` | - |
| `vsource.sym` | 1 | `m` | `inout` | `-2.5 27.5 2.5 32.5` | - |
| `vsource_arith.sym` | 0 | `p` | `inout` | `-2.5 -32.5 2.5 -27.5` | - |
| `vsource_arith.sym` | 1 | `m` | `inout` | `-2.5 27.5 2.5 32.5` | - |

Symbols with no row in the pin table have no pins: `architecture.sym`, `code.sym`, `code_shown.sym`, `intuitive_interface_cheatsheet.sym`, `launcher.sym`, `netlist_options.sym`, `ngspice_get_expr.sym`, `ngspice_get_value.sym`, `simulator_commands.sym`, `simulator_commands_shown.sym`, `title-3.sym`, `title.sym`, `verilog_preprocessor.sym`.

**Interface table: notes**

| Symbol | Note |
|---|---|
| `simulator_commands.sym` | `"format: a tcleval value (Requirement \"tcleval values\") whose netlist text is LF, LF, the instance's value, LF when the Tcl command sim_is_<simulator> (simulator: the instance's simulator attribute) runs without error and returns true, and a single LF when it returns false or fails. The value is placed in a Tcl braced word, so its braces must balance and a backslash-LF in it becomes one space."` |
| `simulator_commands_shown.sym` | `"format: as simulator_commands.sym. The symbol also shows the instance's value as text."` |
| `code_shown.sym` | `"Shows the instance's value as text; code.sym does not."` |
| `ngspice_get_expr.sym` | `"Shows the instance's descr, LF, and the result of the instance's node evaluated as a tcleval value (the PDKs put [ngspice::get_current ...] or [to_eng ...] expressions in node). Emits nothing in netlists (no format)."` |
| `ngspice_get_value.sym` | `"Shows @spice_get_node with the instance's node, and the instance's descr (capability schematic-simulation-integration). Emits nothing in netlists (no format)."` |
| `ngspice_probe.sym` | `"Shows @spice_get_voltage of the net at its pin. Emits nothing in netlists (no format)."` |
| `lab_show.sym` | `"Shows the name of the net at its pin (@#0:net_name). Emits nothing in netlists (no format)."` |
| `intuitive_interface_cheatsheet.sym` | `"A help panel whose content is text (graphics, not described); it has no pins and no schematic."` |
| `netlist_options.sym` | `"Not in the inventory: golden case SCHEM-NL-17 places it (Q-SCHEM-31, proposed). Its keys are applied as Requirement \"Netlist options\" of capability schematic-netlisting says."` |

Compatibility: behavioral

(xschem ref: xschem_library/devices/*.sym)

#### Scenario: IHP testbench with devices prefix
- **GIVEN** IHP `sg13g2_tests/dc_lv_nmos.sch`, which references `devices/code_shown.sym`, `devices/gnd.sym` and `lab_pin.sym`
- **WHEN** it is opened
- **THEN** every reference SHALL resolve to a Hefesto system-library symbol and no placeholder SHALL be shown

### Requirement: Schematic used as symbol
If a symbol reference names a `.sch` file, the schematic SHALL be loaded as a symbol:
- its `K` record is the symbol property
- its graphics are the symbol graphics
- its wires are drawn as lines on layer 1
- the instances inside it are drawn transformed by their orientation (Requirement "Orientation transform" of capability `schematic-file-format`), recursively, to at most 40 levels
- the pins of the outer symbol are generated from its `ipin`, `opin` and `iopin` instances, at their positions, named by their `lab`
- floater texts and graph boxes inside it are dropped
- objects with `symbol_ignore=true` are dropped

If a `.sym` with the same base name exists next to the `.sch`, its pin order SHALL be applied to the generated pins.

Compatibility: exact

(xschem ref: save.c:5455-5600, save.c:4656-4705, xschem.h:200)

#### Scenario: Recursion limit
- **GIVEN** a schematic `a.sch` that places itself as a symbol
- **WHEN** it is opened
- **THEN** loading SHALL stop at 40 levels with the warning `Symbol recursively instantiating symbol: max depth reached, skipping`

### Requirement: Missing symbols
An instance whose symbol cannot be found SHALL be drawn with a placeholder symbol that has no pins. The warning `Symbol not found: <reference>` SHALL be reported once per reference. The instance SHALL keep its reference and properties, SHALL be saved unchanged, and SHALL be netlisted as nothing (it has no `format`). A later change of the search path SHALL re-resolve it.

Compatibility: behavioral

(xschem ref: save.c:4950-4965)

#### Scenario: Missing PDK symbol
- **GIVEN** a sky130 schematic opened with no PDK active
- **WHEN** it is displayed
- **THEN** each `sky130_fd_pr/*` instance SHALL show a placeholder, and saving SHALL keep all `C` records unchanged

### Requirement: Symbol generators
A symbol reference of the form `name(args)` (matching `^[^ \t()]+\([^()]*\)[ \t]*$`) is a symbol generator in xschem: the program `name` found on the search path is run with the arguments split at `(`, `)`, `,` and spaces, and its standard output is read as a `.sym` file. A generator reference in a `schematic` attribute works the same way. The symbol name used in netlists SHALL be the reference with every run of ` . ( ) ,` and surrounding spaces replaced by `_`, and a trailing `_` removed.

Hefesto SHALL support generators only when the generator is a Tcl script, run in the sandboxed interpreter with `argv` set to the arguments and with `puts` to standard output captured as the symbol text. Other generators SHALL be shown as missing symbols with the message `symbol generators that run external programs are not supported`. See Q-SCHEM-11.

Compatibility: behavioral

(xschem ref: token.c:83-182)

#### Scenario: Generator name in netlist
- **GIVEN** an instance with reference `ramdec(4, 8)`
- **WHEN** its `@symname` is netlisted
- **THEN** it SHALL print `ramdec_4_8`

### Requirement: Symbol cache and reload
Hefesto SHALL load each referenced symbol file once per open document and share it among instances. It SHALL detect changes to a loaded symbol file (file-system notification or modification time) and SHALL reload it on request (`schematic reload_symbols`, capability `schematic-command`) or automatically when the user returns to the window. A symbol edited and saved in Hefesto SHALL update every open schematic that uses it.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12)

#### Scenario: Edited symbol propagates
- **GIVEN** `top.sch` and `inv.sym` both open
- **WHEN** a pin is added to `inv.sym` and it is saved
- **THEN** the instances of `inv` in `top.sch` SHALL show the new pin

### Requirement: PDK attribute conventions
Hefesto SHALL netlist the three PDK libraries as xschem does. Conventions the netlister and display SHALL support, observed in the pinned PDK data:
- **sky130** (`xschem_sky130`, installed by open_pdks as `<PDK>/libs.tech/xschem`): MOS symbols have `type=nmos` or `pmos`; `format="@spiceprefix@name @pinlist sky130_fd_pr__@model L=@L W=@W` LF `+ nf=@nf ad=@ad ..."` with `+` continuation lines; `template` with `spiceprefix=X` and geometry defaults written as `expr('...')` (capability `schematic-netlisting`, Requirement "expr evaluation"); a separate `lvs_format`; `corner.sym` of type `netlist_commands` whose format is `tcleval(.lib $::SKYWATER_MODELS/sky130.lib.spice @corner )`; and texts with `@spice_get_node` for operating-point display.
- **IHP sg13g2** (`ihp-sg13g2/libs.tech/xschem`, symbols in `sg13g2_pr`, standard cells in `libs.ref/sg13g2_stdcell/sym/xschem`): `lvs_format` beginning with a literal device letter (`M@name ...`); `format="@spiceprefix@name @pinlist @model w=@w l=@l ng=@ng m=@m mm_ok=@mm_ok"`; resistor, capacitor and bipolar formats wrapped in `tcleval(...)` that call `ev`, `ev7` or the PDK procedure `cap_cmomi_C`; testbenches using `code_shown.sym` with `format="tcleval( @value )"`.
- **gf180mcu** (`gf180mcuD/libs.tech/xschem/symbols`, a copy of `gf180mcu_fd_pr`): `format` with `+` continuation lines and SPICE expressions in single quotes (`ad='int((nf+1)/2) * W/nf * 0.18u'`) passed through unchanged; `lvs_format` without `@spiceprefix`.

Compatibility: exact

(xschem ref: token.c:2426-2816; PDK data: xschem_sky130 sky130_fd_pr/nfet_01v8.sym, sky130_fd_pr/corner.sym; IHP-Open-PDK ihp-sg13g2/libs.tech/xschem/sg13g2_pr/sg13_lv_nmos.sym, xschemrc:390-460; gf180mcu_fd_pr cells/xschem/gf180mcu_fd_pr/nfet_03v3.sym)

#### Scenario: IHP LVS device line
- **GIVEN** an `sg13_lv_nmos` instance `name=M1 l=0.45u w=1.0u ng=1 m=1` with pins D=D, G=G, S=GND, B=GND, in LVS netlist mode
- **WHEN** it is netlisted
- **THEN** the line SHALL be `MM1 D G GND GND sg13_lv_nmos w=1.0u l=0.45u ng=1`, because a trailing ` m=1` is removed (capability `schematic-netlisting`, Requirement "Element line post-processing")

#### Scenario: gf180 expression passed through
- **GIVEN** a gf180 `nfet_03v3` instance with the template defaults
- **WHEN** it is netlisted in SPICE mode
- **THEN** the line SHALL contain `ad='int((nf+1)/2) * W/nf * 0.18u'` unchanged
