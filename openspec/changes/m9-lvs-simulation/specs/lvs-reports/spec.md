## ADDED Requirements

### Requirement: Layout object references in structured results
When a circuit of the run was generated from a layout (capability `lvs-command`, Requirement "Layout netlist from extraction"), the result structure that Hefesto keeps for its GUI and scripts SHALL attach a layout reference to each net, device and pin entry of that circuit: the hierarchical use path and a point on the object, taken from the layout netlist's name map. A circuit that was netlisted from a schematic SHALL carry a schematic reference instead. `comp.out`, the `-list` result and `comp.json` SHALL stay exactly as netgen writes them, with no reference added.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5) (See Q-LVS-33.)

#### Scenario: Unmatched net points into the layout
- **GIVEN** an `lvs -layout` run in which net `x1/n3` of the layout is unmatched
- **WHEN** the GUI reads the result
- **THEN** the net's entry SHALL carry the use path `x1` and a point inside node `n3`, and `comp.out` SHALL be byte-identical to the run without references
