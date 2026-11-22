## ADDED Requirements

### Requirement: Single-threaded comparison per pair
Each cell pair SHALL be compared on one thread. The comparison state SHALL hold no process-global data, so that two pairs could be compared concurrently without sharing state. The report, the list result and the JSON file of a run SHALL be identical whatever the number of threads available to the process and on every supported platform.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5) (See Q-LVS-34, Q-LVS-4.)

#### Scenario: Repeated runs
- **GIVEN** a pair with symmetric nets that need arbitrary pairing
- **WHEN** the same comparison is run 100 times, on 1 and on 8 hardware threads
- **THEN** every `comp.out` and `comp.json` SHALL be byte-identical
