# LVS Command

## Purpose
Defines Hefesto's LVS entry points:
- the netgen-compatible `lvs` command and its arguments
- the lower-level netgen command set exposed to scripts
- obtaining the layout netlist from Hefesto's own extraction, with no external tools
- locating schematic netlists and PDK setup files
- execution on every platform, including iPadOS
- the result browser and cross-probing from LVS results to layout geometry
- headless use

Decision P-5 requires LVS to work on iPadOS without external tools, with results compatible with netgen.

**Compatibility adaptation.** This capability is about netgen, not Magic. The classes in `docs/spec-conventions.md` apply relative to **netgen** (`RTimothyEdwards/netgen` @ `93dcda14d36349d0b0ca63168d3cbbab20fe2ece`, version 1.5.327):
- `exact`: same arguments and results as the netgen `lvs` Tcl procedure
- `behavioral`: same semantics
- `informative`: netgen detail
- `native`: Hefesto product behavior, citing `(Hefesto decision: docs/decisions/product.md#P-5)`

Traces use `(netgen ref: path/file.c:line)`. They are for traceability only (capability `hefesto-licensing`).

Integrates with these capabilities:
- `hefesto-architecture`: single command API, background work, headless tool
- `hefesto-scripting`: the Tcl runtime of decision X-3
- `extract-commands`, `netlist-spice-options` (`ext2spice lvs`), `netlist-spice-hierarchy` and `netlist-spice-devices`: the layout netlist
- `hefesto-files-and-pdks`: the PDK store and documents
- `ui-feedback-and-highlights`: highlighting

## Requirements

### Requirement: lvs command syntax
Hefesto SHALL provide `lvs <name1> <name2> [<setup> [<log>]] [option …]`, with netgen's semantics.

**Defaults.** `<setup>` defaults to `setup.tcl` and `<log>` to `comp.out`.

**Names.** Each name SHALL be one of:
- a cell reference accepted by capability `lvs-setup-file`, Requirement "Cell reference syntax", if that netlist is already loaded
- `{file cell}`
- a single file name, whose top cell takes the file's name

**Options.** Options SHALL be matched exactly:
- `-list`: return the structured result
- `-json`: also write the JSON file and print `Generating JSON file result`
- `-blackbox`: turn automatic black-boxing on and print `Treating empty subcircuits as black-box cells`
- `-noflatten=<file-or-list>`: see capability `lvs-hierarchy`

`-list` prints `Generating list result`. The options SHALL be processed before the netlists are read.

**Reading and checks.**
- An unread file SHALL be read with automatic format detection, printing `Reading netlist file <file> for <name>`.
- Two names in the same netlist SHALL print `Both cells are in the same netlist:  Cannot compare!` and return.
- A cell missing from its file SHALL print `Cannot find cell <cell> in file <file>` and return.

**Sequence.** The command SHALL then:
1. assign circuits 1 and 2
2. evaluate the setup file (capability `lvs-setup-file`)
3. start the log (capability `lvs-reports`)
4. run the hierarchical flow (capability `lvs-hierarchy`)
5. print `LVS Done.`

With `-list` it SHALL return the list result. If the compare queue is empty at the start, it SHALL log `No cells in queue!` and return.
Compatibility: exact
(netgen ref: tcltk/netgen.tcl.in:357-731)

#### Scenario: netgen-style invocation
- **WHEN** `lvs "inv.spice inv" "inv_schem.spice inv" $PDK_ROOT/sky130A/libs.tech/netgen/sky130A_setup.tcl comp.out -json` is run in Hefesto's Tcl console
- **THEN** `comp.out` and `comp.json` SHALL be written with the content specified in capability `lvs-reports`

### Requirement: netgen command namespace
The commands of capabilities `lvs-netlist-input`, `lvs-setup-file`, `lvs-comparison-algorithm` and `lvs-hierarchy` SHALL be available in the `netgen::` namespace of Hefesto's Tcl interpreter, together with `lvs`. These include:
- `readnet`, `readlib`, `compare`, `run`, `verify`, `equate`, `permute`, `property`, `ignore`, `model`, `global`, `convert`, `flatten`, `format`, `log`, `cells`, `ports`, `print`, `iterate`, `restart`, `exhaustive`, `symmetries`, `matching`, `reinitialize`
- the diagnostic commands

`reinitialize` SHALL clear the LVS database.

These commands SHALL be imported unqualified only inside setup-file evaluation and an explicit LVS console mode. They SHALL NOT shadow Magic-compatible commands of the same name in the `magic::` namespace (capability `scripting-api`). `netgen::writenet` MAY support only `spice` and `verilog`; other formats are informative.
Compatibility: behavioral
(netgen ref: tcltk/tclnetgen.c:117-262, tcltk/netgen.tcl.in:736-764)

#### Scenario: No clash with Magic
- **GIVEN** Magic's `flatten` command in the `magic::` namespace
- **WHEN** a setup file calls `flatten class "-circuit1 x"`
- **THEN** the netgen `flatten` SHALL run inside setup evaluation, and Magic's `flatten` SHALL be unaffected elsewhere

### Requirement: Layout netlist from extraction
When `lvs` is invoked through Hefesto's layout-aware form, `lvs -layout <cell> <schematic> [<setup>] [<log>] [option …]`, the layout side SHALL be generated internally:
1. Extract `<cell>` and its hierarchy as `extract all` would (capability `extract-commands`).
2. Netlist it with the `ext2spice lvs` preset as defined by capability `netlist-spice-options` (hierarchy on, ngspice format, infinite cthresh and rthresh, renumber off, scale off, blackbox on, global off, subcircuits top auto).
3. Load the result as circuit 1.

The loaded result SHALL be identical to netgen reading the `.spice` file that Hefesto's `ext2spice` would write with that preset. The file MAY be kept in memory. The netlist SHALL be written to `<cell>.spice` in the document folder only when the user's settings request it.

The layout netlist SHALL retain a mapping from every device instance and net name back to layout objects (Requirement "Cross-probing"). The schematic side SHALL be any file accepted by capability `lvs-netlist-input`. In this form and in the netgen form, a name whose file ends in `.sch` SHALL instead be netlisted in memory in LVS netlist mode, as capability `schematic-simulation-integration` (Requirements "Schematic to layout cross-probing" and "Schematic-driven LVS command") specifies, and the text SHALL be read as SPICE.
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: One-step LVS on iPad
- **GIVEN** cell `inv` is open on iPadOS and `inv.spice` (schematic) is in the project
- **WHEN** the user runs `lvs -layout inv inv.spice sky130A_setup.tcl`
- **THEN** extraction, netlisting and comparison SHALL complete with no external process, and the result SHALL equal what netgen reports for Hefesto's `ext2spice lvs` output against `inv.spice`

### Requirement: Setup file and input resolution
A setup or netlist path SHALL be resolved through the platform file services (capability `hefesto-files-and-pdks`):
1. As given, relative to the current document folder.
2. For setup files with no directory part, in the active PDK's `libs.tech/netgen/` folder in the PDK store, also trying `<PDK>_setup.tcl` when the name is the bare PDK name.

The `$PDK_ROOT` variable and `$env(PDK_ROOT)` SHALL resolve to the PDK store root on every platform (decision X-4).
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: Bare PDK setup name
- **GIVEN** the active PDK is sky130A
- **WHEN** `lvs -layout inv inv.spice sky130A` is run
- **THEN** the setup file SHALL be `<PDK store>/sky130A/libs.tech/netgen/sky130A_setup.tcl`

### Requirement: Execution model
LVS SHALL run entirely inside the Hefesto core:
- with no `fork`/`exec`
- with no external netgen
- on Linux, macOS and iPadOS

It SHALL run on a background worker (capability `hefesto-architecture`, Requirement "Concurrency model"), so the GUI stays responsive. The layout database SHALL be read-only while extraction for LVS is in progress. Progress SHALL be reported per compared pair as `<k>/<n> <cell1> vs <cell2>`. Cancellation SHALL be supported (capability `lvs-comparison-algorithm`).

Only one LVS run at a time is required per document. Starting another SHALL either queue it or replace the current run, with the user's confirmation.
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: Responsive UI
- **WHEN** a 50k-device LVS runs on iPad
- **THEN** panning the layout SHALL remain interactive, and a progress indicator SHALL show the current pair

### Requirement: Result browser
After a run, the GUI SHALL show the structured result (capability `lvs-reports`, Requirement "Result classification"):
- the overall status and per-pair statuses, in queue order
- for each pair: device and net counts, mismatch fragments, pin table, property errors
- the raw `comp.out` text

Entries SHALL be selectable. The browser SHALL use the same command API as scripts. The same data SHALL be available from `lvs … -list` in Tcl.
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: Navigate to failing cell
- **GIVEN** pair `(nand2, nand2)` is `mismatch` and was flattened into `top`
- **WHEN** the user opens the result browser
- **THEN** `nand2` SHALL be listed as mismatched and flattened, and `top` SHALL be listed with its own status

### Requirement: Cross-probing
Selecting a layout-side net or device in the result browser SHALL highlight it in the layout window, using the highlight layer of capability `ui-feedback-and-highlights`:
- **Net.** The geometry of the extracted net SHALL be highlighted. The net is resolved through the hierarchical name, with flattening prefixes `<inst>/` mapped back through the layout instance hierarchy.
- **Device.** The device's terminal and gate geometry SHALL be highlighted. A device merged by parallel or series reduction SHALL highlight all of its original devices (capability `lvs-device-reduction`).

Selecting a schematic-side object SHALL highlight its layout partner when the pair is matched, and SHALL show `no layout counterpart` otherwise. Cross-probing SHALL be a command, `lvs probe net|device <circuit> <name>`, so scripts can use it. Highlights SHALL be cleared by `lvs probe clear`.
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: Highlight unmatched net
- **GIVEN** fragment `Net: /x1/n3` in the layout circuit
- **WHEN** the user taps it
- **THEN** the paint of net `n3` inside instance `x1` SHALL be highlighted, and the view SHALL zoom to its bounding box

### Requirement: Headless LVS
The headless tool (capability `hefesto-architecture`, Requirement "Headless command-line tool") SHALL run LVS from a command script. It SHALL exit with:
- 0 when the overall status is `clean`
- 1 for any other completed comparison
- 2 when inputs or setup could not be read

`-json` and the log SHALL be written as specified. This tool SHALL be the harness for golden tests against netgen (`tests/golden/LVS/CASES.md`).
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: CI gate
- **WHEN** `hefesto-cli -T sky130A -c "load inv; lvs -layout inv inv.spice sky130A comp.out -json; quit"` runs on a clean design
- **THEN** it SHALL exit 0 and write `comp.out` and `comp.json`
