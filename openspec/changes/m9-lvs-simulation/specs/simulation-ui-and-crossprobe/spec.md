## ADDED Requirements

### Requirement: Probe names without length limits
The hierarchical node names that probes use (Requirement "Probing a layout net") SHALL be computed without any fixed-size buffer, on every platform and in both `magic-compat` modes, whatever the hierarchy depth or label length. The naming function SHALL be available without the IRSIM interface, which is not part of this capability.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7, docs/decisions/cross-cutting.md#X-2) (See Q-SIM-24.)

#### Scenario: Deep hierarchy
- **GIVEN** a layout DUT whose probed net is 12 levels deep, with a hierarchical name of 600 bytes
- **WHEN** the user probes the net after a run that saved it
- **THEN** the full 600-byte name SHALL be resolved through the net name map, and the trace SHALL be added

### Requirement: Simulate button
On desktop, the top bar's Simulate button SHALL open the testbench panel of the active document. When no engine can run, the button SHALL be disabled and its tooltip SHALL read `simulation engine unavailable: <reason>`. Examples are a missing `hefesto-simd` or `libngspice-hefesto`, or a worker that fails its handshake. Every other feature SHALL keep working.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Worker missing
- **GIVEN** an install from which `hefesto-simd` was removed
- **WHEN** the editor starts
- **THEN** the Simulate button SHALL be disabled with the tooltip `simulation engine unavailable: hefesto-simd not found`, and layout editing, DRC, extraction and LVS SHALL work
