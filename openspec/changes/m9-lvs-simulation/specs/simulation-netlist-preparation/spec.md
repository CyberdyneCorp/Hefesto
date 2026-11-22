## ADDED Requirements

### Requirement: sg13g2 mismatch flag on extracted devices
When the DUT is a layout of an IHP sg13g2 design and the job's Monte Carlo mode includes mismatch (capability `simulation-monte-carlo`), deck preparation SHALL add `mm_ok=1` to every sg13g2 MOS instance of the DUT netlist that does not already set `mm_ok`, inside the prepared deck only. The SPICE that `ext2spice` writes, and the files in the project, SHALL NOT change. Without a mismatch mode, the instances SHALL be passed unchanged.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-5, D-23.)

#### Scenario: Mismatch on an extracted sg13g2 pair
- **GIVEN** an extracted sg13g2 differential pair with no `mm_ok` parameter on its devices
- **WHEN** a Monte Carlo run in mode `mismatch` with 50 samples is prepared and run
- **THEN** every MOS instance line of the prepared deck SHALL carry `mm_ok=1`, the input offset SHALL vary between samples, and the project's `.spice` file SHALL be unchanged

### Requirement: Deck limits
Deck preparation SHALL enforce these limits:
- a prepared deck of at most 256 MiB;
- include depth 32.

Each limit SHALL be a setting that can be lowered. Exceeding one SHALL stop preparation with a diagnostic that names the limit and the file, before anything reaches the engine.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-25.)

#### Scenario: Include loop in a user deck
- **GIVEN** a raw deck whose `.include` files include each other
- **WHEN** it is submitted
- **THEN** preparation SHALL fail with the include-depth diagnostic, and no job SHALL be started in the engine
