## MODIFIED Requirements

### Requirement: Layout netlist from extraction
When `lvs` is invoked through Hefesto's layout-aware form, `lvs -layout <cell> <schematic> [<setup>] [<log>] [option …]`, the layout side SHALL be generated internally:
1. Bring the extraction of `<cell>` and its hierarchy up to date as `extract` does (capability `extract-commands`, Requirement "Incremental extraction"): only cells whose `.ext` file is missing or older than the cell, and their parents, are re-extracted. `extract all`, which ignores timestamps, SHALL NOT be used.
2. Netlist it with the `ext2spice lvs` preset as defined by capability `netlist-spice-options` (hierarchy on, ngspice format, infinite cthresh and rthresh, renumber off, scale off, blackbox on, global off, subcircuits top auto). The preset SHALL be passed as a value, and the user's `ext2spice` settings SHALL be left unchanged.
3. Load the result as circuit 1.

The loaded result SHALL be identical to netgen reading the `.spice` file that Hefesto's `ext2spice` would write with that preset after a full `extract all`, whenever the `.ext` files are current. The file MAY be kept in memory. The netlist SHALL be written to `<cell>.spice` in the document folder only when the user's settings request it.

The netlister SHALL return, together with the text, a name map from every device instance and net name back to layout objects (a hierarchical use path and a point on the object), which the comparison result and Requirement "Cross-probing" use. Names SHALL NOT be parsed back from the text. The schematic side SHALL be any file accepted by capability `lvs-netlist-input`. In this form and in the netgen form, a name whose file ends in `.sch` SHALL instead be netlisted in memory in LVS netlist mode, as capability `schematic-simulation-integration` (Requirements "Schematic to layout cross-probing" and "Schematic-driven LVS command") specifies, and the text SHALL be read as SPICE.
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5) (See Q-LVS-32, Q-LVS-33.)

#### Scenario: One-step LVS on iPad
- **GIVEN** cell `inv` is open on iPadOS and `inv.spice` (schematic) is in the project
- **WHEN** the user runs `lvs -layout inv inv.spice sky130A_setup.tcl`
- **THEN** extraction, netlisting and comparison SHALL complete with no external process, and the result SHALL equal what netgen reports for Hefesto's `ext2spice lvs` output against `inv.spice`

#### Scenario: Rerun after editing one cell
- **GIVEN** a completed `lvs -layout top top.spice sky130A` run, after which only cell `buf` (used by `top`) was edited
- **WHEN** the same command is run again
- **THEN** only `buf` and its parents up to `top` SHALL be re-extracted, and the result SHALL equal a run after `extract all`

#### Scenario: User settings untouched
- **GIVEN** the user has set `ext2spice format hspice` and `ext2spice cthresh 0`
- **WHEN** `lvs -layout inv inv.spice sky130A` runs
- **THEN** the layout netlist SHALL use the `lvs` preset, and afterwards `ext2spice format` SHALL still report `hspice` and `ext2spice cthresh` SHALL still report `0`

### Requirement: Setup file and input resolution
A setup or netlist path SHALL be resolved through the platform file services (capability `hefesto-files-and-pdks`):
1. As given, relative to the current document folder.
2. For setup files with no directory part, in the active PDK's `libs.tech/netgen/` folder in the PDK store, also trying `<PDK>_setup.tcl` when the name is the bare PDK name. `<PDK>` is the PDK's installed name as the PDK manager reports it (`sky130A`, `gf180mcuD`, `ihp-sg13g2`).

The `$PDK_ROOT` variable and `$env(PDK_ROOT)` SHALL resolve to the PDK store root on every platform (decision X-4).
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5) (See Q-LVS-31.)

#### Scenario: Bare PDK setup name
- **GIVEN** the active PDK is sky130A
- **WHEN** `lvs -layout inv inv.spice sky130A` is run
- **THEN** the setup file SHALL be `<PDK store>/sky130A/libs.tech/netgen/sky130A_setup.tcl`

#### Scenario: Bare IHP setup name
- **GIVEN** the active PDK is `ihp-sg13g2`
- **WHEN** `lvs -layout sg13g2_inv_1 inv.spice ihp-sg13g2` is run
- **THEN** the setup file SHALL be `<PDK store>/ihp-sg13g2/libs.tech/netgen/ihp-sg13g2_setup.tcl`

## ADDED Requirements

### Requirement: LVS status chip
On desktop, the top bar's LVS chip SHALL show the state of the last LVS run of the active layout:

| State | Text |
|---|---|
| no run | `LVS not run` |
| match | `LVS match` |
| mismatch | `LVS mismatch (<n>)`, where `<n>` is the number of mismatched pairs |
| error or cancelled | `LVS error` or `LVS cancelled` |
| stale | `LVS stale` |

The state SHALL become stale when the layout or either input netlist changes after the run. Activating the chip SHALL open the result browser (Requirement "Result browser").

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: Edit after a match
- **GIVEN** the chip shows `LVS match` for cell `inv`
- **WHEN** the user paints metal1 in `inv`
- **THEN** the chip SHALL show `LVS stale`
