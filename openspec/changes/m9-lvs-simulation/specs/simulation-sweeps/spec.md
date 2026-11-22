## ADDED Requirements

### Requirement: Measured waveform retention
Run sets SHALL offer the waveform retention mode `measured`. It keeps, for every point, only the vectors that the run set's measurements reference, at full resolution. The other modes keep their current definitions. The default mode SHALL stay as specified until the storage cost of `measured` has been measured on the reference run sets and the user has approved a new default.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-11, D-24.)

#### Scenario: Only measured vectors kept
- **GIVEN** a 500-point sweep that saves 40 vectors and measures `v(out)` only
- **WHEN** it runs with retention `measured`
- **THEN** every point SHALL store `v(out)` and the scale vector, and no other vector

### Requirement: Results in point order
Run set results, tables and exports SHALL be stored and listed in point-index order, whatever the order in which jobs finish or the number of workers used. A run set run with one worker and with four workers SHALL store identical results.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-13.)

#### Scenario: Out-of-order completion
- **GIVEN** a 100-point sweep in which point 7 takes ten times longer than the others
- **WHEN** it runs on four workers
- **THEN** the exported CSV SHALL list points 0 to 99 in order, and it SHALL equal the CSV of the same sweep run on one worker
