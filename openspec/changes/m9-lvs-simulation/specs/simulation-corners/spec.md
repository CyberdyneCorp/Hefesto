## ADDED Requirements

### Requirement: Corner jobs on the worker pool
Each corner of a corner run SHALL be one job on the shared worker pool, whose size is set by capability `simulation-engine-embedding`, Requirement "Multiple simulations and corner parallelism". The corner matrix SHALL be identical whatever the pool size.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-13, Q-SIM-5.)

#### Scenario: Large post-layout deck limits the pool
- **GIVEN** a 16 GB desktop with 10 cores and a corner run whose jobs each have a 4 GiB result budget
- **WHEN** nine corners run
- **THEN** exactly 1 worker SHALL run at a time (half of 16 GB divided by a `B_job` above 4 GiB rounds down to 1), and the corner matrix SHALL equal the matrix of the same run on one worker
