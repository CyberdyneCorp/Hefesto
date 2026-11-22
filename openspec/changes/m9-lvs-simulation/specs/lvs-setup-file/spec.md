## ADDED Requirements

### Requirement: PDK setup files evaluate unchanged
The installed netgen setup files of the three supported PDKs SHALL evaluate with no error and no warning that the same file does not give in netgen 1.5.327, with `netgen-compat` ON:
- `sky130A/libs.tech/netgen/sky130A_setup.tcl`
- `gf180mcuD/libs.tech/netgen/gf180mcuD_setup.tcl`
- `ihp-sg13g2/libs.tech/netgen/ihp-sg13g2_setup.tcl`

The PDK files SHALL NOT be edited, wrapped or replaced by Hefesto-specific copies.

Compatibility: behavioral

(Hefesto decision: docs/decisions/product.md#P-5) (See Q-LVS-31.)

#### Scenario: IHP setup file
- **GIVEN** the IHP sg13g2 PDK installed in the PDK store from IHP-Open-PDK @ `5e6d592e`
- **WHEN** a comparison is run with setup file `ihp-sg13g2_setup.tcl`
- **THEN** setup evaluation SHALL complete with no error, and the console SHALL show the same setup messages as netgen 1.5.327 for that file

### Requirement: Setup evaluation is bounded and cancellable
Setup evaluation SHALL run in a fresh sandboxed interpreter for each run (capability `hefesto-security`, Requirement "Script sandbox enforcement"), with a step budget and a check of the run's cancellation between commands. Exceeding the budget or cancelling the run SHALL stop evaluation with a diagnostic and classify the run as `error` or `cancelled`. In both `netgen-compat` modes, setup evaluation SHALL NOT hang the run.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5, docs/decisions/cross-cutting.md#X-3) (See Q-LVS-36.)

#### Scenario: Endless loop in a setup file
- **GIVEN** a setup file containing `while {1} {}`
- **WHEN** a comparison is run with it
- **THEN** evaluation SHALL stop when the step budget runs out, the run SHALL be classified `error` with the budget diagnostic, and the GUI SHALL stay responsive
