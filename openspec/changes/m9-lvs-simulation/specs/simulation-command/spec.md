## ADDED Requirements

### Requirement: Schematic launcher calls
A `sim` call made by a schematic launcher (capability `schematic-simulation-integration`, Requirement "Simulation launchers") SHALL run with the launcher's testbench schematic as its circuit source. It SHALL return the same values and errors as the same call typed at the console. It SHALL run in the launcher's sandbox and confirmation context. No `sim` subcommand SHALL answer with a not-implemented diagnostic.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7, docs/decisions/product.md#P-12)

#### Scenario: Launcher runs the testbench
- **GIVEN** the IHP testbench `dc_lv_nmos.sch` with its simulation launcher, confirmed once for the document
- **WHEN** the user activates the launcher
- **THEN** a job SHALL run with the schematic as circuit source, and `sim jobs` SHALL list it with state `completed`
