## ADDED Requirements

### Requirement: In-memory netlist sources
Every reader of this capability SHALL accept a netlist either as a file or as in-memory text with a source name. In-memory text comes from the layout netlist of `lvs -layout` (capability `lvs-command`) or from a schematic netlisted in LVS mode (capability `schematic-simulation-integration`). An in-memory source SHALL produce the same database, the same diagnostics and the same report text as the same bytes read from a file with that name. Includes in an in-memory source SHALL resolve relative to the document folder.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5) (See Q-LVS-33.)

#### Scenario: Layout netlist kept in memory
- **GIVEN** `lvs -layout inv inv.spice sky130A` with the setting that writes `<cell>.spice` off
- **WHEN** the run completes
- **THEN** no `inv.spice` layout file SHALL be written, and `comp.out` SHALL be byte-identical to the run in which the layout netlist was written to a file and read back

### Requirement: Input limits
The readers SHALL enforce these limits in both `netgen-compat` modes:
- include depth 64;
- one logical line, after continuation lines are joined, 16 MiB;
- one name 4 KiB;
- 50 million devices per run.

Each limit SHALL be a setting that can be lowered, and SHALL NOT be raised above twice its default. Exceeding a limit SHALL stop the read with a diagnostic that names the limit, the file and the line, and SHALL classify the run as an input error. A reader SHALL NOT crash, recurse without bound or hang on any input.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5, docs/decisions/cross-cutting.md#X-2) (See Q-LVS-36.)

#### Scenario: Include cycle
- **GIVEN** `a.spice` includes `b.spice` and `b.spice` includes `a.spice`
- **WHEN** `a.spice` is read
- **THEN** the read SHALL stop at include depth 64 with the include-depth diagnostic naming `b.spice`, and the process SHALL keep running
