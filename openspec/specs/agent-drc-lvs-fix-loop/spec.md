# Agent DRC/LVS Fix Loop

## Purpose
Lets the agent repair layout by iterating. It reads DRC errors and LVS mismatches, proposes edits, re-runs the checks, and repeats until the checks are clean or a limit is reached. The checks themselves are always Hefesto's own DRC and LVS. The model only proposes edits.

## Requirements

### Requirement: Iterative repair loop
For a DRC or LVS repair task, the agent SHALL repeat these steps:
1. Run the requested checks in the scratch session.
2. Pick a set of violations.
3. Apply edits through tools.
4. Re-run the checks.

The loop SHALL end when the target checks are clean, the limits of capability `agent-safety-and-approval` are reached, or no progress is made for a configurable number of iterations. Every iteration SHALL record the violation counts before and after.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Spacing violations fixed
- **GIVEN** a cell with 5 metal1 spacing errors
- **WHEN** a DRC repair task runs
- **THEN** the proposal SHALL report 0 metal1 spacing errors, with the per-iteration counts recorded

#### Scenario: No progress
- **WHEN** three consecutive iterations do not reduce the violation count
- **THEN** the agent SHALL stop and present the best proposal it found, marked incomplete

### Requirement: No regressions
The agent SHALL present a proposal as an improvement only if it introduces no new violations of any kind compared with the starting state:
- DRC errors
- LVS mismatches
- shorts or opens detected by extraction

Any newly introduced violation SHALL be listed prominently in the proposal.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Fix creates short
- **WHEN** an edit fixes a width error but shorts two nets
- **THEN** the proposal SHALL flag the new short, and SHALL NOT mark the result verified

### Requirement: Preserve intent
Repair edits SHALL keep the following unchanged unless the user allows changing them:
- port positions and names
- the cell bounding box when `FIXED_BBOX` is set
- instance placements outside the scope

The agent SHALL prefer minimal edits, measured by the area of the geometric XOR against the original.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Ports untouched
- **WHEN** a repair runs on a cell with ports
- **THEN** the accepted result SHALL keep every port's name, layer and position unchanged
