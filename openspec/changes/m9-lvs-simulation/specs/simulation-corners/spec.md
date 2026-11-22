## ADDED Requirements

### Requirement: Corner jobs on the worker pool
Each corner of a corner run SHALL be one job on the shared worker pool. The pool size SHALL be `min(4, cores − 1, ⌊B_total / B_job⌋)`, where `B_total` is half the physical memory and `B_job` is the job's result budget plus its deck size estimate, and SHALL be at least 1. A user setting MAY lower the size. The corner matrix SHALL be identical whatever the pool size.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-13, Q-SIM-5.)

#### Scenario: Large post-layout deck limits the pool
- **GIVEN** a 16 GB desktop with 10 cores and a corner run whose jobs each have a 4 GiB result budget
- **WHEN** nine corners run
- **THEN** at most 2 workers SHALL run at a time, and the corner matrix SHALL equal the matrix of the same run on one worker
