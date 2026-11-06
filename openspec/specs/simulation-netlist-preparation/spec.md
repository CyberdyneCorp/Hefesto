# Simulation Netlist Preparation

## Purpose
Defines how Hefesto builds the SPICE deck that the embedded ngspice engine runs (capability `simulation-engine-embedding`). The deck is built from:
- a circuit under test (DUT): extracted layout netlisted by the SPICE netlister (capabilities `netlist-spice-options`, `netlist-spice-hierarchy`, `netlist-spice-devices`), optionally with `extresist-command` parasitic networks (capability `extresist-output`); or an imported netlist
- a testbench: stimuli, analyses and saved vectors
- PDK model libraries, resolved through the PDK store of capability `hefesto-files-and-pdks`
- process, voltage and temperature corners

This capability also defines the net-name map that capability `simulation-ui-and-crossprobe` uses to probe layout nets, and how decks are validated before they reach ngspice.

ngspice reference: SourceForge `ngspice` master @ `7aa47164` (after ngspice-47).

## Requirements

### Requirement: Circuit sources
A simulation testbench SHALL take its DUT from exactly one of these sources:

| Source | DUT text |
|---|---|
| `layout <cell>` | Netlist generated from the layout of `<cell>` (Requirement "Layout DUT netlist generation") |
| `netlist <file>` | An imported SPICE file that defines a `.subckt` named by the user |
| `schematic <file>` | An xschem-compatible testbench schematic whose netlist is the complete deck (capability `schematic-simulation-integration`, Requirement "Schematic as a circuit source") |
| `none` | No DUT. The testbench is a complete deck written by the user (Requirement "Raw decks"). |

The generated deck SHALL be held in memory and passed to ngspice as lines (Requirement "Deck delivery"). Writing it to disk SHALL NOT be required on any platform. A copy SHALL be written only when the user asks for it (`sim deck write`, capability `simulation-command`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7, docs/decisions/product.md#P-12)

#### Scenario: Schematic source
- **GIVEN** an IHP testbench schematic `dc_lv_nmos.sch`
- **WHEN** a testbench with source `schematic dc_lv_nmos.sch` is prepared
- **THEN** the deck SHALL be the schematic's netlist, generated in memory

#### Scenario: Layout source on iPad
- **GIVEN** cell `inv` open on iPad and a testbench with source `layout inv`
- **WHEN** the simulation starts
- **THEN** the netlist SHALL be generated in memory and no `inv.spice` file SHALL be created in the user's folders

### Requirement: Layout DUT netlist generation
For source `layout <cell>`, Hefesto SHALL bring extraction up to date and then netlist the cell. If the `.ext` data of `<cell>` or of any descendant is missing or older than its layout (capabilities `cell-timestamps` and `extract-hierarchy`), Hefesto SHALL first re-extract those cells, as incremental `extract all` does. A testbench setting `extraction: reuse` skips this and uses the existing `.ext` data, with the warning `Simulating stale extraction of <cell>`.

The SPICE netlister SHALL then run with these settings. They override the user's interactive `ext2spice` settings for this run only, and do not change them:
- `format ngspice`
- `hierarchy on`
- `subcircuits top on`
- `subcircuits descend on`
- `scale off`
- `renumber off`
- `global on`
- `blackbox off`
- `merge none`
- `short resistor`
- the parasitic settings of Requirement "Parasitic modes"

The DUT text SHALL be exactly what the netlister produces with these settings, as specified by the `netlist-spice-*` capabilities.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Stale extraction refreshed
- **GIVEN** `inv.ext` older than the last edit to `inv`
- **WHEN** a simulation with source `layout inv` starts
- **THEN** `inv` SHALL be re-extracted before netlisting, and the DUT SHALL reflect the edit

#### Scenario: User settings preserved
- **GIVEN** the user had set `ext2spice format hspice`
- **WHEN** a layout simulation runs and then the user runs `ext2spice`
- **THEN** the interactive run SHALL still use format hspice

### Requirement: Parasitic modes
A layout DUT SHALL be netlisted in one of three parasitic modes:

| Mode | Netlister settings | Content |
|---|---|---|
| `none` | `cthresh infinite`, `extresist off` | Devices only |
| `c` (default) | `cthresh <t>` where `<t>` is the testbench capacitance threshold in fF (default 0), `extresist off` | Devices plus node and coupling capacitors above the threshold |
| `rc` | as `c`, plus `extresist on` | As `c`, plus the resistor networks from the `.res.ext` overlays (capability `extresist-output`, Requirement "Consumption by netlisters") |

In mode `rc`, if a cell with resistance extraction enabled has no `.res.ext` file, or that file is older than its `.ext`, Hefesto SHALL run the resistance extraction of capability `extresist-command` with the testbench's `extresist-command` options (default: the tech file defaults) before netlisting.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: RC mode includes overlay resistors
- **GIVEN** a `.res.ext` overlay line `resist "a.t0" "a.n0" 2000` with resistScale 1
- **WHEN** a DUT is netlisted in mode `rc`
- **THEN** the DUT text SHALL contain `R0 a.t0 a.n0 2.0005`

#### Scenario: Mode none has no capacitors
- **WHEN** a DUT is netlisted in mode `none`
- **THEN** it SHALL contain no capacitor line whose name starts with `C` and whose value comes from extraction

### Requirement: Net name map
Every DUT generation SHALL also produce a net-name map with one entry per SPICE node of the DUT netlist. Each entry has three fields:
- `layoutName`: the node's hierarchical layout name as computed by capability `irsim-interface`, Requirement "Hierarchical node names", relative to the DUT cell
- `spiceName`: the node name as written in the netlist
- `simName`: the ngspice vector name of that node once it is instantiated as `Xdut`

`simName` SHALL be `xdut.` followed by the instance path and the local node name. The instance path is the sequence of subcircuit instance names, lowercased and each followed by `.`. All letters are lowercased, because ngspice lowercases names. A DUT port maps to the testbench node it is bound to, not to an `xdut.` name.

For each resistor network node created by an `rnode` record in mode `rc`, the map SHALL record the parent net's `layoutName` together with the rnode name, so that probes can reach individual network points.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-6.)

#### Scenario: Internal node of a subcell
- **GIVEN** DUT cell `buf` containing instance `inv_0` of cell `inv`, whose local net `mid` is not a port
- **WHEN** the map is built
- **THEN** the entry for layout name `inv_0/mid` SHALL have `simName` `xdut.xinv_0.mid`

#### Scenario: Port net
- **GIVEN** DUT port `A` bound to testbench node `in`
- **WHEN** the map is built
- **THEN** layout name `A` SHALL map to `simName` `in`

### Requirement: Case-collision handling
ngspice treats node and instance names case-insensitively, but layout names are case-sensitive. Before a deck is passed to ngspice, Hefesto SHALL detect any two distinct SPICE node names in the same subcircuit that are equal when lowercased. Each name after the first, in netlist order, SHALL be renamed to `<name>__c<k>`, with `k` starting at 1 for each collision group. The renaming SHALL be applied to the deck text and recorded in the net-name map. The job log SHALL get the warning `Renamed node <old> to <new> (case collision)`. Instance names SHALL be checked in the same way.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-6.)

#### Scenario: Nets A and a
- **GIVEN** a DUT subcircuit containing distinct nodes `A` and `a`, with `A` first in netlist order
- **WHEN** the deck is prepared
- **THEN** `a` SHALL become `a__c1`, the net-name map SHALL map layout `a` to `xdut.a__c1`, and one warning SHALL be logged

### Requirement: Testbench composition
For DUT sources `layout` and `netlist`, Hefesto SHALL compose the deck in exactly this order:

```
deck        = title_line
              init_section
              option_lines
              temp_line
              param_lines
              model_lines
              dut_text
              dut_instance
              stimulus_lines
              extra_lines
              save_lines
              analysis_lines
              ".end" NL ;
title_line  = "* hefesto testbench " tb_name " corner " corner_name NL ;
option_lines= { ".option " option_text NL } ;          (* user options, in insertion order *)
temp_line   = ".temp " number NL ;
param_lines = { ".param " ident " = " number NL } ;   (* corner parameters, sorted by ident *)
model_lines = { lib_line | include_line } ;           (* PDK descriptor order, see Requirement "Model library resolution" *)
lib_line    = ".lib " quoted_abs_path " " section NL ;
include_line= ".include " quoted_abs_path NL ;
dut_instance= "Xdut" { " " tb_node } " " dut_subckt NL ;  (* one tb_node per subckt port, in port order *)
save_lines  = { ".save " vector { " " vector } NL } ; (* omitted when saving "all", see capability simulation-analyses-and-results *)
quoted_abs_path = '"' absolute_path '"' ;
```

`init_section` is empty in the deck text; its commands are sent separately (Requirement "PDK initialisation commands"). DUT ports SHALL be bound to testbench nodes by name. A port with no binding SHALL be bound to a new node `nc_<port>`, connected to ground through a 1 TΩ resistor `Rnc_<port>`. These resistor lines go in `extra_lines`, together with any user-supplied extra lines, and the job log SHALL get the warning `Port <port> left unconnected`. A binding to a name that is not a DUT port SHALL be an error, `Unknown DUT port <name>`.

Example: a sky130 inverter in corner `tt_27_1v8`:

```
* hefesto testbench inv_tran corner tt_27_1v8
.temp 27
.lib "/pdk/sky130A/libs.tech/ngspice/sky130.lib.spice" tt
.subckt inv A Y VPWR VGND
X0 Y A VGND VGND sky130_fd_pr__nfet_01v8 w=0.65u l=0.15u
X1 Y A VPWR VPWR sky130_fd_pr__pfet_01v8_hvt w=1u l=0.15u
C0 Y VGND 1.2f
.ends
Xdut in out vdd 0 inv
Vdd vdd 0 1.8
Vin in 0 PULSE(0 1.8 1e-09 1e-10 1e-10 5e-09 1e-08)
.save v(in) v(out)
.tran 1e-11 2e-08
.end
```

(The DUT text above is illustrative. The real text is whatever the SPICE netlister produces.)

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Port bound by name
- **GIVEN** DUT `.subckt inv A Y VPWR VGND` and bindings `A=in`, `Y=out`, `VPWR=vdd`, `VGND=0`
- **WHEN** the deck is composed
- **THEN** the DUT instance line SHALL be `Xdut in out vdd 0 inv`

#### Scenario: Unbound port
- **GIVEN** the same DUT with no binding for `Y`
- **WHEN** the deck is composed
- **THEN** the instance line SHALL be `Xdut in nc_Y vdd 0 inv`, `Rnc_Y nc_Y 0 1e+12` SHALL appear in the extra lines, and the warning `Port Y left unconnected` SHALL be logged

### Requirement: Stimulus definitions
A testbench SHALL support independent voltage (`V`) and current (`I`) sources, connected between two testbench nodes, with these waveforms. All values are in SI base units (V, A, s, Hz, degrees for phase):

| Kind | Parameters | Emitted text |
|---|---|---|
| `dc` | `v` | `<v>` |
| `pulse` | `v1 v2 td tr tf pw per` | `PULSE(<v1> <v2> <td> <tr> <tf> <pw> <per>)` |
| `sin` | `vo va freq [td [theta [phase]]]` | `SIN(<vo> <va> <freq> ...)` |
| `pwl` | `(t, v)` pairs with non-decreasing `t` | `PWL(<t1> <v1> <t2> <v2> ...)` |
| `ac` | `mag [phase]`, combined with any of the above | the waveform text, then ` AC <mag> [<phase>]` |

A source line SHALL be `<name> <n+> <n-> <text>`. Numbers SHALL be written in C `%.12g` format. Source names SHALL start with `V` or `I` to match their type and SHALL be unique within the testbench, case-insensitively. A `pwl` with decreasing time, a `pulse` with negative `tr`, `tf` or `pw`, or a missing node SHALL be rejected with an error that names the source.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Pulse line
- **GIVEN** source `Vin` from `in` to `0`, kind pulse, with v1 0, v2 1.8, td 1 ns, tr 100 ps, tf 100 ps, pw 5 ns, per 10 ns
- **WHEN** the deck is composed
- **THEN** the line SHALL be `Vin in 0 PULSE(0 1.8 1e-09 1e-10 1e-10 5e-09 1e-08)`

#### Scenario: Decreasing PWL rejected
- **GIVEN** a pwl source `Vx` with times 0, 2e-9, 1e-9
- **WHEN** the testbench is validated
- **THEN** the error `Source Vx: PWL time values must be non-decreasing` SHALL be reported and no job SHALL start

### Requirement: Simulation model descriptor in the PDK store
Every PDK entry in the PDK store (capability `hefesto-files-and-pdks`) that supports simulation SHALL provide a simulation model descriptor. The descriptor holds:
- the model root directory
- an ordered list of model-line templates, each a `.lib <file> <section-variable>` or a `.include <file>`, with `<file>` relative to the model root
- the corners: for each, the section name each section variable takes, the temperature (°C), the supply voltage, and any `.param` values
- the default corner
- the ngspice initialisation commands
- the OSDI modules required

Hefesto SHALL bundle descriptors for sky130A/B, gf180mcuC/D and IHP sg13g2, with the initial content below. The exact files and section names are to be verified against open_pdks (Q-SIM-8):

| PDK | Model lines | Process sections | Init commands | OSDI |
|---|---|---|---|---|
| sky130A | `.lib sky130.lib.spice <proc>` | `tt ff ss sf fs` | `set ngbehavior=hsa`, `set ng_nomodcheck` | none |
| gf180mcuD | `.include design.ngspice`, `.lib sm141064.ngspice <proc>` | `typical ff ss fs sf` | `set ngbehavior=hsa` | none |
| sg13g2 | `.lib cornerMOSlv.lib <mos>`, `.lib cornerMOShv.lib <moshv>`, `.lib cornerHBT.lib <hbt>`, `.lib cornerRES.lib <res>`, `.lib cornerCAP.lib <cap>` | mos: `mos_tt mos_ff mos_ss mos_sf mos_fs`; others from the PDK | `set ngbehavior=hsa` | `psp103_nqs`, `r3_cmc`, `mosvar` |

A user MAY add or edit descriptors. On Linux and macOS, a descriptor MAY be generated from a PDK's `libs.tech/ngspice` directory found by the PDK store's search rules.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-8.)

#### Scenario: Default corner for sky130A
- **GIVEN** sky130A installed with its bundled descriptor
- **WHEN** a testbench is created with no corner selected
- **THEN** the deck SHALL contain `.lib "<root>/sky130.lib.spice" tt` and `.temp 27`

### Requirement: Model library resolution
Hefesto SHALL resolve every file reference in a deck to an absolute path before passing the deck to ngspice. This covers `.lib` and `.include` lines in model templates, in imported netlists and in raw decks, applied recursively to included files. ngspice SHALL never need to search for a file or expand an environment variable. A reference `f` found in a file whose directory is `d` resolves as follows; the first rule that applies wins:

1. **Absolute path.** If `f` is absolute, `f` is used.
2. **`$PDK_ROOT` or `$PDKPATH`.** If `f` starts with one of them, the variable is replaced by the PDK store's location for the PDK in use. This is done on every platform. On Linux and macOS the process environment SHALL NOT be consulted for these two variables.
3. **Other `$VAR`.** Resolution fails with `Unresolved variable $VAR in <file>`.
4. **Relative to the including file.** `d/f`, if it exists.
5. **Relative to the PDK model root.** `<model root>/f`, if it exists.
6. **Relative to the project folder.** `<project folder>/f`, if it exists.
7. **Not found.** Resolution fails with `Cannot find included file <f> (from <file>)`.

The rewritten line SHALL quote the absolute path. On iPadOS, every resolved file SHALL lie inside the PDK store or a folder the user has granted access to. Security-scoped access to each such folder SHALL be held from deck loading until the job reaches a final state.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/frontend/inpcom.c:2040-2130, src/frontend/inpcom.c:2160-2314) (See Q-SIM-13.)

#### Scenario: Relative include inside a PDK library
- **GIVEN** `sky130.lib.spice` in `/store/sky130A/libs.tech/ngspice/`, which contains `.include "corners/tt.spice"` inside section `tt`
- **WHEN** the deck is prepared
- **THEN** that line SHALL be passed to ngspice as `.include "/store/sky130A/libs.tech/ngspice/corners/tt.spice"`

#### Scenario: Unknown environment variable
- **GIVEN** an imported netlist containing `.include $MYLIB/cells.sp`
- **WHEN** the deck is prepared
- **THEN** preparation SHALL fail with `Unresolved variable $MYLIB in <netlist file>`

### Requirement: PDK initialisation commands
Before each deck is loaded, the engine adapter SHALL send the initialisation commands of the PDK descriptor in use, in descriptor order. These SHALL be followed by `set ngbehavior=<value>` exactly once: the descriptor's value, or `hsa` if the descriptor has none. A raw deck with no PDK SHALL get `set ngbehavior=hsa`. Because the compatibility mode is re-read every time a circuit is parsed, these commands apply to the next `ngSpice_Circ` call. Descriptor commands SHALL be limited to `set`, `unset` and `option`; any other command in a descriptor SHALL be rejected when the PDK is imported.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/frontend/inpcompat.c:61-95, src/frontend/inpcom.c:1048-1056)

#### Scenario: Compatibility mode per job
- **GIVEN** job A uses sky130A (`hsa`), and job B is a raw deck whose PDK descriptor sets `ngbehavior=ps`
- **WHEN** A and then B run in the same engine instance
- **THEN** A SHALL be parsed in `hsa` mode and B in `ps` mode

#### Scenario: Shell command in descriptor
- **WHEN** a PDK descriptor containing the init command `shell rm -rf ~` is imported
- **THEN** the import SHALL fail with `PDK descriptor command not allowed: shell`

### Requirement: Corners
A corner SHALL be a named tuple of:
- a section value for each section variable of the PDK descriptor
- a temperature in °C
- a supply scale factor `k` (default 1.0)
- a set of `.param` overrides

The supply scale multiplies the DC value of every stimulus marked `supply`. A corner sweep SHALL be the cartesian product of the chosen process sections, temperatures and supply factors. Each element is named `<section>_<temp>_<supply>`, where:
- `<temp>` is the integer °C, prefixed `m` when negative
- `<supply>` is the scaled supply in volts in `%g` format, with `.` replaced by `v`, and the supply is that of the first `supply` stimulus

Each corner SHALL produce one job. Decks for different corners of one testbench SHALL differ only in their title line, `.temp`, `.param` and section words, and in the values of `supply` stimuli.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Corner names
- **GIVEN** process sections `tt ss`, temperatures `27 -40` and supply 1.8 V with factors `1.0 0.9`
- **WHEN** the sweep is generated
- **THEN** 8 jobs SHALL be created, including `tt_27_1v8`, `ss_m40_1v62` and `tt_m40_1v62`

### Requirement: Deck validation and sanitisation
Before passing a deck to ngspice, Hefesto SHALL check it and reject it with a specific error when any of these hold:
- No `.end` line can be produced.
- A `.subckt` is missing its `.ends`, or a `.ends` has no matching `.subckt`.
- An instance calls a subcircuit or model that is undefined after includes are resolved. Model names defined only inside PDK libraries are exempt from this check.
- A file reference does not resolve (Requirement "Model library resolution").
- A `.control` … `.endc` section contains `shell`, `cd`, `codemodel`, `osdi`, `pre_osdi`, `load`, `source`, `write`, `wrdata` or `quit`. Each SHALL be rejected with `Control command '<cmd>' is not allowed in simulation decks`.

Other `.control` sections in imported netlists and raw decks SHALL be removed, with the warning `Removed .control section (lines <a>-<b>)`, unless the testbench setting `allowControl` is on. `allowControl` defaults to off. Even when it is on, the forbidden commands above remain rejected.

For a deck from a `schematic` source, `.control` sections SHALL be kept. They SHALL first be translated as defined by capability `schematic-simulation-integration`, Requirement "Control block translation", which maps `write` and `quit` to job-result capture and job completion. This validation SHALL run after that translation, so translated `write` and `quit` lines are not rejected. Every other forbidden command SHALL still be rejected.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-9.)

#### Scenario: Shell in control section
- **GIVEN** an imported netlist with `.control`, `shell ls`, `.endc`
- **WHEN** the deck is prepared with `allowControl` on
- **THEN** preparation SHALL fail with `Control command 'shell' is not allowed in simulation decks`

#### Scenario: xschem testbench write and quit
- **GIVEN** a schematic-source deck whose `.control` block contains `run`, `write tb.raw`, `quit`
- **WHEN** the deck is prepared
- **THEN** preparation SHALL succeed, `write tb.raw` SHALL become a capture of the plot as result `tb.raw`, and `quit` SHALL end the job normally

#### Scenario: Control section stripped
- **GIVEN** an imported netlist whose lines 40-44 are `.control`, `run`, `plot v(out)`, `.endc`
- **WHEN** the deck is prepared with default settings
- **THEN** those lines SHALL be removed and `Removed .control section (lines 40-44)` SHALL be logged

### Requirement: Raw decks
A testbench with source `none` SHALL hold a complete user-written deck, which is passed after file resolution and sanitisation. The deck SHALL contain at least one analysis line (`.op`, `.dc`, `.ac`, `.tran`, `.noise`, `.tf`, `.pz`, `.sp`, `.pss` or `.disto`) and SHALL end with `.end`. Hefesto SHALL NOT change anything else in the deck text. No net-name map is produced for a raw deck. Probing (capability `simulation-ui-and-crossprobe`) then works only on vector names.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Raw deck without analysis
- **GIVEN** a raw deck containing only an RC circuit and `.end`
- **WHEN** it is run
- **THEN** the job SHALL be rejected with `Deck has no analysis`

### Requirement: Deck delivery
The engine adapter SHALL pass the final deck with `ngSpice_Circ`. The deck is an array of lines without line terminators: the first is the title line, the last non-null entry is `.end`, and a null pointer ends the array. Lines SHALL be UTF-8. Lines longer than 65 535 bytes SHALL be split with `+` continuation lines at a space boundary. If XSPICE file-reading code models are used, `ngCM_Input_Path` SHALL be set to the project folder before loading.

The exact deck text, its SHA-256 hash, the PDK descriptor identity and the corner SHALL be recorded with the job result (capability `simulation-analyses-and-results`). Re-running a stored job SHALL re-send the identical deck.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/include/ngspice/sharedspice.h:118-140, src/include/ngspice/sharedspice.h:470-480)

#### Scenario: Re-run is identical
- **GIVEN** a stored job with deck hash `h`
- **WHEN** the user re-runs it after the layout has changed
- **THEN** the re-run SHALL use the stored deck with hash `h`, not a regenerated netlist, unless the user chooses "regenerate"
