## ADDED Requirements

### Requirement: Reduction cost
Series and parallel reduction of a cell with `n` devices SHALL take O(n log n) time and O(n) additional memory, and SHALL check the run's cancellation at least every 65 536 devices. The result SHALL NOT depend on the order of the devices in the input.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5) (See Q-LVS-35.)

#### Scenario: Many parallel fingers
- **GIVEN** a cell with 1 000 000 identical parallel transistors between the same four nets
- **WHEN** reduction runs
- **THEN** it SHALL produce one device with a merged count of 1 000 000, and it SHALL complete within the `lvs-bench` limit of capability `hefesto-performance`
