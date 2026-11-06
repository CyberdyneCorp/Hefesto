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
- `StefanSchippers/xschem_sky130` @ `2bfd7d654e12492d47986b78e68bb770e633b5da`
- `IHP-GmbH/IHP-Open-PDK` @ `5e6d592e4002946a4616f798c357f0f3c06cf3b6` (`ihp-sg13g2/libs.tech/xschem`)
- `efabless/globalfoundries-pdk-libs-gf180mcu_fd_pr` @ `aacc04cc30ed119baaa616dff266cca7006dc3d6` (`cells/xschem`)
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
The pin order of a symbol SHALL be the order of its layer-5 boxes in the file. If every pin has a `sim_pinnumber` key, the pins SHALL instead be sorted by the integer value of `sim_pinnumber`, ascending. Pins with equal `sim_pinnumber` have no defined relative order in xschem; Hefesto SHALL keep their file order (a stable sort; see Q-SCHEM-5). If at least one pin lacks `sim_pinnumber`, no sorting SHALL happen. Pin order SHALL define `@pinlist`, the `.subckt` port list and pin numbers used by `@#n`.

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

### Requirement: Templates and instance attributes
A new instance SHALL get the symbol's `template` value (decoded, capability `schematic-file-format`, Requirement "Property strings") as its property string, with `name` made unique (Requirement "Instance names"). When an `@key` token is resolved for an instance (capability `schematic-netlisting`, Requirement "Attribute substitution"), the instance property string SHALL be searched first, then the symbol template. Instance attributes that the template does not define SHALL be kept and SHALL be usable by `@key`. An instance MAY override `format` and `lvs_format`; an instance value SHALL be used before the symbol value.

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
- **GIVEN** a symbol text `@name=@value` and an instance `name=R7 value=4.7k`
- **WHEN** the instance is drawn
- **THEN** the text SHALL read `R7=4.7k`

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

Hefesto SHALL run this in the embedded, sandboxed Tcl interpreter of capability `hefesto-scripting` on every platform, so the PDK symbols that use `tcleval` work on iPadOS too. That interpreter SHALL provide xschem-compatible definitions of the Tcl procedures that PDK symbols call (Requirement "PDK attribute conventions"): `ev`, `ev7`, `ev0`, `to_eng`, `from_eng`, `ngspice::get_node`, `ngspice::get_current`, `xschem getprop`, `xschem get` (subset), and the variables set by the active PDK's `xschemrc`. See Q-SCHEM-7 and Q-SCHEM-8.

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

### Requirement: Script execution permission
xschem guards `tcleval` and `tclcommand` with a user setting whose values are `ask`, true or false. Hefesto SHALL instead run `tcleval` attribute values always, inside the sandbox of capability `hefesto-scripting`, because the sandbox removes process execution, sockets and file access outside granted folders. Launcher `tclcommand` scripts SHALL run only after the user confirms them once per document, and SHALL run in the same sandbox. The xschem `xschem_execute_scripts` variable in an `xschemrc` SHALL be read and ignored. See Q-SCHEM-25.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12)

#### Scenario: Launcher confirmation
- **GIVEN** a schematic with a launcher whose `tclcommand` is `xschem netlist`
- **WHEN** the user Ctrl+clicks it for the first time in this document
- **THEN** Hefesto SHALL ask for confirmation, and SHALL NOT ask again for that document in the session

### Requirement: Symbol references
A symbol reference in a `C` record SHALL be resolved to a file as follows:
1. An empty reference resolves to nothing.
2. An `http://` or `https://` reference SHALL be kept as is (informative: Hefesto does not download; see Q-SCHEM-9).
3. A reference starting with `/` is an absolute path.
4. Otherwise, runs of `/` SHALL be collapsed, `/./` removed, `dir/../` pairs removed, and a trailing `/` or `/.` removed.
5. A reference that then starts with `./` or `../` SHALL be resolved against the folder of the schematic that contains the instance; it is used if the file or its folder exists.
6. If the first path component is a library alias (Requirement "Library search path"), the alias SHALL be replaced by its folder.
7. Otherwise, each folder of the search path is tried in order, and the first `<folder>/<reference>` that exists is used.
8. If none exists, `<folder of the containing schematic>/<reference>` is used.

When Hefesto writes a reference it chose itself (placing, replacing or re-pathing a symbol), it SHALL write the shortest form: the path relative to the first search-path folder that is a prefix of the absolute path, or the alias form, or the bare file name if the file is in the schematic's folder, or else the absolute path. References read from a file SHALL be written back unchanged.

Compatibility: exact

(xschem ref: xschem.tcl:8731-8883, save.c:3350-3360)

#### Scenario: Library-relative reference
- **GIVEN** the search path `/pdk/xschem:/pdk/xschem/sg13g2_pr` and the reference `sg13g2_pr/sg13_lv_nmos.sym`
- **WHEN** it is resolved
- **THEN** the file SHALL be `/pdk/xschem/sg13g2_pr/sg13_lv_nmos.sym`

### Requirement: Library search path
The search path SHALL be an ordered list of folders built from the value of `XSCHEM_LIBRARY_PATH`:
- the value SHALL be split at `:`; on Windows hosts it is split at `;` (informative)
- an element of the form `<folder> | <alias>` (spaces around `|` optional, `\|` an escaped bar) SHALL define the alias `<alias>` for `<folder>`
- elements that start with `#` SHALL be dropped
- `~` and environment references in an element SHALL be expanded, and elements that do not exist SHALL be dropped
- the element `.` means the folder of the current schematic
- if the result is empty, it SHALL be the current schematic's folder

Hefesto SHALL build `XSCHEM_LIBRARY_PATH` from the active PDK's `xschemrc` (Requirement "xschemrc as data"), and SHALL also accept a search path set in Hefesto settings (capability `hefesto-settings`) that is prepended to it.

Compatibility: behavioral

(xschem ref: xschem.tcl:11467-11530)

#### Scenario: Missing folder dropped
- **GIVEN** `XSCHEM_LIBRARY_PATH` = `:/a:/missing:/b` where `/missing` does not exist
- **WHEN** the search path is built
- **THEN** it SHALL be `/a`, `/b`

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
Hefesto SHALL ship a clean-room symbol library that is reference-compatible with the xschem `xschem_library/devices` library: the same file names (for example `res.sym`, `capa.sym`, `vsource.sym`, `isource.sym`, `gnd.sym`, `vdd.sym`, `lab_pin.sym`, `lab_wire.sym`, `ipin.sym`, `opin.sym`, `iopin.sym`, `code.sym`, `code_shown.sym`, `launcher.sym`, `title.sym`, `noconn.sym`, `ammeter.sym`, `spice_probe.sym`, `netlist_options.sym`), the same pin names and order, pin positions and symbol properties (`type`, `format`, `template`, `global`, ...) that produce identical netlists. Their graphics MAY differ. The library SHALL be available as both `devices/<name>` and `<name>`, because PDK schematics use both forms. The exact list SHALL be fixed by an inventory of the references used by the three PDK libraries (see Q-SCHEM-10).

Netlisting-relevant properties of the core symbols SHALL be:

| Symbol | type | format | template |
|---|---|---|---|
| `ipin.sym` | `ipin` | `*.ipin @lab` | `name=p1 lab=xxx` |
| `opin.sym` | `opin` | `*.opin @lab` | `name=p1 lab=xxx` |
| `iopin.sym` | `iopin` | `*.iopin @lab` | `name=p1 lab=xxx` |
| `lab_pin.sym`, `lab_wire.sym` | `label` | `*.alias @lab` | `name=p1 sig_type=std_logic lab=xxx` |
| `gnd.sym` | `label`, `global=ground` | `*.alias @lab` | `name=l1 lab=0` |
| `vdd.sym` | `label`, `global=true` | `*.alias @lab` | `name=l1 lab=VDD` |
| `code.sym`, `code_shown.sym` | `netlist_commands` | LF `@value` LF | `name=s1 only_toplevel=false value=blabla` |
| `res.sym` | `resistor` | `@name @pinlist @value m=@m` | `name=R1 value=1k ... m=1` |
| `capa.sym` | `capacitor` | `@name @pinlist @value m=@m` | `name=C1 m=1 value=1p ...` |
| `vsource.sym` | `vsource` | `@name @pinlist @value@savecurrent` | `name=V1 value=3 savecurrent=false` |
| `ammeter.sym` | `ammeter` | `@name @pinlist 0 @savecurrent` | `name=Vmeas savecurrent=true spice_ignore=0` |
| `spice_probe.sym` | `probe` | `.save @attrs v( @@p )` | `name=p1 attrs=""` |
| `noconn.sym` | `noconn` | `* noconn @#0:net_name` | `name=l1` |
| `launcher.sym` | `launcher`, `spice_ignore=true` | `** @descr : @url` | `name=h1 descr=... url=...` |
| `title.sym` | `logo`, `spice_ignore=true` | none | `name=l1 author="..."` |
| `netlist_options.sym` | `netlist_options` | none | `bus_replacement_char="[]" lvs_netlist=false spiceprefix=true hiersep=.` |

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
- **gf180mcu** (`gf180mcu_fd_pr/cells/xschem/symbols`): `format` with `+` continuation lines and SPICE expressions in single quotes (`ad='int((nf+1)/2) * W/nf * 0.18u'`) passed through unchanged; `lvs_format` without `@spiceprefix`.

Compatibility: exact

(xschem ref: token.c:2426-2816; PDK data: xschem_sky130 sky130_fd_pr/nfet_01v8.sym, sky130_fd_pr/corner.sym; IHP-Open-PDK ihp-sg13g2/libs.tech/xschem/sg13g2_pr/sg13_lv_nmos.sym, xschemrc:390-460; gf180mcu_fd_pr cells/xschem/symbols/nfet_03v3.sym)

#### Scenario: IHP LVS device line
- **GIVEN** an `sg13_lv_nmos` instance `name=M1 l=0.45u w=1.0u ng=1 m=1` with pins D=D, G=G, S=GND, B=GND, in LVS netlist mode
- **WHEN** it is netlisted
- **THEN** the line SHALL be `MM1 D G GND GND sg13_lv_nmos w=1.0u l=0.45u ng=1`, because a trailing ` m=1` is removed (capability `schematic-netlisting`, Requirement "Element line post-processing")

#### Scenario: gf180 expression passed through
- **GIVEN** a gf180 `nfet_03v3` instance with the template defaults
- **WHEN** it is netlisted in SPICE mode
- **THEN** the line SHALL contain `ad='int((nf+1)/2) * W/nf * 0.18u'` unchanged
