# Agent Evaluation

## Purpose
Defines how agent quality is measured and kept from regressing. A benchmark suite of layout tasks is scored by Hefesto's own verification gates. The cases are listed in `tests/golden/AGENT/CASES.md`. Recorded model responses let CI replay the suite deterministically, without calling a live model.

## Requirements

### Requirement: Benchmark suite with objective metrics
Hefesto SHALL maintain a suite of agent tasks covering:
- natural-language edits
- device generation
- cell-from-subckt generation
- DRC and LVS repair
- schematic-driven blocks

The tasks SHALL use at least two PDKs. Each task SHALL be scored on these metrics:
- the verification gates passed
- the number of iterations
- the model usage (tokens or cost)
- wall time
- task-specific quality measures, such as area, wirelength or extracted capacitance

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Suite report
- **WHEN** the suite runs against a configured provider
- **THEN** it SHALL output a per-task table of gate results and metrics, plus an aggregate pass rate

### Requirement: Deterministic replay in CI
CI SHALL replay the suite from recorded provider responses, so that changes to tools, context generation or gates are tested without a live model. A replay run SHALL fail if a recorded tool call is no longer valid or if a gate result changes.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Tool signature change
- **WHEN** a pull request renames a parameter of the `paint` tool
- **THEN** replay of recorded tasks that used it SHALL fail, prompting re-recording

### Requirement: Provider qualification
Before a provider and model combination is listed as supported, it SHALL reach a minimum pass rate on the live suite. The threshold SHALL be stated in the provider documentation. The qualification results SHALL be published with each release.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Below threshold
- **WHEN** a new on-prem model passes 40% of the suite while the threshold is 70%
- **THEN** it SHALL be listed as experimental, not supported
