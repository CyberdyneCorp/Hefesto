# Agent Schematic-Driven Layout

## Purpose
Lets the agent turn a schematic netlist into a verified layout. It generates the devices (capability `agent-device-and-cell-generation`), places them, routes the nets, and closes the loop with DRC, LVS, parasitic extraction and, optionally, simulation against the schematic. The target is analog and custom blocks of modest size, not full-chip digital place-and-route.

## Requirements

### Requirement: End-to-end flow
Given a SPICE netlist of a block, or a schematic netlisted in LVS mode by capability `schematic-netlisting`, plus optional constraints, the agent SHALL produce a proposal for a cell. The constraints MAY include:
- an outline
- pin locations
- matching or symmetry groups
- the power-grid layers

The agent SHALL build the proposal with these stages, recording each stage's result:
1. Device generation.
2. Placement that respects the constraints.
3. Routing of every net using the routing tools, which wrap capabilities `router-maze`, `router-maze-iroute` and `wiring-tool`, or direct paint commands. These are available on every platform, including iPadOS (capability `hefesto-ipad-feature-scope`). The routing engine for analog nets is still open (see Q-AGENT-1).
4. DRC.
5. LVS against the source netlist.
6. Parasitic extraction.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Current mirror
- **GIVEN** a 4-transistor current mirror netlist and a matching group {M1, M2}
- **WHEN** the agent runs the flow
- **THEN** the proposal SHALL place M1 and M2 symmetrically, and SHALL report DRC clean, an LVS unique match, and extracted parasitics

### Requirement: Analog-aware placement constraints
Placement SHALL honor user-declared constraints:
- symmetry axes
- common-centroid or interdigitated matching groups
- proximity groups
- keep-out regions
- orientation restrictions

The proposal SHALL report each constraint as satisfied or violated.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Common centroid
- **WHEN** a differential pair is declared common-centroid with 2 fingers each
- **THEN** the placement SHALL arrange the fingers ABBA (or an equivalent common-centroid pattern), and the report SHALL mark the constraint satisfied

### Requirement: Post-layout comparison against the schematic
If the user provides a testbench, the agent SHALL simulate the pre-layout and post-layout netlists with the embedded simulator, through the evaluation interface of capability `simulation-optimization-hooks`. The post-layout netlist SHALL include the extracted parasitics. The proposal SHALL report the differences in the user-selected measurements, such as gain, bandwidth, delay and offset.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10, docs/decisions/product.md#P-19)

#### Scenario: Delay degradation reported
- **WHEN** post-layout delay is 12% worse than pre-layout
- **THEN** the proposal SHALL show both values and the 12% delta next to the gate results

### Requirement: Iterative improvement
After a verified layout exists, the user MAY ask the agent to optimize a metric: area, wirelength, parasitic capacitance on named nets, matching, or a simulated measurement. Simulation-based candidates SHALL be evaluated, budgeted and recorded as capability `simulation-optimization-hooks` specifies. The agent SHALL keep every intermediate verified layout as a selectable alternative. It SHALL NOT present an unverified layout as better than a verified one.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Reduce net capacitance
- **WHEN** the user asks to reduce the capacitance on net `out`
- **THEN** the agent SHALL present alternatives, each with its extracted capacitance on `out` and its gate results
