## MODIFIED Requirements

### Requirement: Probing a layout net
Selecting a net in the layout and choosing "probe" SHALL resolve the net to a simulation vector and add it as a trace. Selection works as in `getnode` (capability `irsim-interface`); this and the naming rule below apply on every platform, including iPadOS, where the IRSIM interface itself is unavailable (capability `hefesto-ipad-feature-scope`). Resolution SHALL work as follows:
1. Compute the selected node's hierarchical name relative to the root cell of the layout window, as specified by capability `irsim-interface`, Requirement "Hierarchical node names", including the handling of global names.
2. If the root cell is not the DUT cell of the selected job, fail with `Layout window shows <root>, but job simulated <dut>`.
3. Find the job's net-name map entry (capability `simulation-netlist-preparation`, Requirement "Net name map") whose `layoutName` equals that name. If there is none, find the entry whose layout point lies on the selected node, following the node's connectivity through the extracted hierarchy. Names SHALL NOT be compared in any other way: the netlist name chosen by the netlister and the hierarchical node name can differ (a case-collision rename, a node named by a transistor terminal, a different preferred label), and both resolve the same node. The entry gives `simName`. If no entry is found, the probe fails with `Net <name> is not in the netlist of this run`.
4. Find the vector whose name equals `simName` case-insensitively in the job's plot. If none, the probe fails with `Net <name> was not saved in this run`. Hefesto SHALL then offer to add `simName` to the testbench's saved vectors and run again.
5. If the job has no net-name map (raw deck or imported), compare the layout name, with `/` replaced by `.`, to vector names case-insensitively.

Selecting paint that covers several nets SHALL probe each net.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-24, Q-LVS-33.)

#### Scenario: Probe an internal net
- **GIVEN** a completed job of DUT `buf` in mode `all`, and a layout window rooted at `buf`
- **WHEN** the user selects paint of net `mid` inside instance `inv_0` and probes it
- **THEN** a trace of vector `xdut.xinv_0.mid` SHALL be added

#### Scenario: Net not saved
- **GIVEN** a job in mode `default` that did not save `xdut.xinv_0.mid`
- **WHEN** the same net is probed
- **THEN** `Net inv_0/mid was not saved in this run` SHALL be shown, with the option to save it and run again

#### Scenario: Netlist name differs from the node name
- **GIVEN** DUT cell `top` with two distinct nets labelled `OUT` and `out`, so that the netlist renames the second one `out__c1` (Q-SIM-6), and an unlabelled node whose hierarchical name is a transistor-terminal name `@=...`
- **WHEN** the user probes the net labelled `out` and then the unlabelled node
- **THEN** the first probe SHALL add the trace of the vector for `out__c1`, and the second SHALL add the trace of the vector for the netlist name of that node, both found through the net-name map

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
