## ADDED Requirements

### Requirement: Layout object references in structured results
When a circuit of the run was generated from a layout (capability `lvs-command`, Requirement "Layout netlist from extraction"), the result structure that Hefesto keeps for its GUI and scripts SHALL attach a layout reference to each net, device and pin entry of that circuit: the hierarchical use path and a point on the object, taken from the layout netlist's name map. A circuit that was netlisted from a schematic SHALL carry a schematic reference instead. Adding the references SHALL NOT change `comp.out`, the `-list` result or `comp.json`: none of them carries a reference, and each is byte-identical to the output of the same run without references. Those outputs otherwise follow this capability's requirements and the resolutions of Q-LVS-2, Q-LVS-13 and Q-LVS-19.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5) (See Q-LVS-33, Q-LVS-2, Q-LVS-13, Q-LVS-19.)

#### Scenario: Unmatched net points into the layout
- **GIVEN** an `lvs -layout` run in which net `x1/n3` of the layout is unmatched
- **WHEN** the GUI reads the result
- **THEN** the net's entry SHALL carry the use path `x1` and a point inside node `n3`, and `comp.out` SHALL be byte-identical to the run without references
