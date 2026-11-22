## ADDED Requirements

### Requirement: Sample independence from worker assignment
The result of Monte Carlo sample `k` SHALL depend only on the deck, the base seed and `k`. It SHALL NOT depend on which worker ran the sample, on how many workers ran, or on which samples a worker ran before. A sample MAY run in a worker that ran other jobs before. Before each sample is loaded, the worker SHALL have cleared the previous circuit and repeated the per-load initialisation with the sample's own seed, as capability `simulation-engine-embedding` specifies (Requirements "Crash containment on desktop" and "Deterministic engine initialisation").

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-13.)

#### Scenario: Same sample on different workers
- **GIVEN** a 200-sample run of the sky130 inverter with base seed 42
- **WHEN** it runs once on one worker and once on four workers
- **THEN** sample 137 SHALL have bit-identical stored vectors and measurements in both runs

### Requirement: Sample count limit
A Monte Carlo run SHALL have at most 100 000 samples, and a run set at most 100 000 points in total. The limit is a setting that can be lowered. A larger request SHALL be refused before any job starts, with a diagnostic that gives the limit.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIM-25.)

#### Scenario: Too many samples
- **WHEN** `sim mc set -mode process -runs 200000` is executed
- **THEN** the command SHALL fail with the sample-limit diagnostic, and no job SHALL be queued
