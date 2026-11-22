## ADDED Requirements

### Requirement: Hierarchy depth limit
Building the compare queue and flattening SHALL traverse the hierarchy without recursion, using an explicit stack. A hierarchy deeper than 256 levels SHALL stop the run with a diagnostic that names the deepest cell, and the run SHALL be classified as an input error. This SHALL hold in both `netgen-compat` modes. The limit SHALL be a setting that can be lowered, and SHALL NOT be raised above 512.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5, docs/decisions/cross-cutting.md#X-2) (See Q-LVS-36.)

#### Scenario: Recursive subcircuit
- **GIVEN** a netlist in which subcircuit `a` instantiates `b` and `b` instantiates `a`
- **WHEN** it is compared
- **THEN** the run SHALL stop with the hierarchy-depth diagnostic, and the process SHALL keep running
