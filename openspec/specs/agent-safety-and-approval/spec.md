# Agent Safety and Approval

## Purpose
Defines how the layout agent operates under human approval. The agent proposes changes, the user reviews them as a visual and structural diff, and only an explicit accept applies them. This capability also sets the limits and provenance records that keep agent output reviewable and reversible.

## Requirements

### Requirement: Proposals require explicit acceptance
Every agent task SHALL end with a proposal. A proposal contains:
- the change set, as a structural diff (capability `hefesto-collaboration-and-diff`)
- a visual diff overlay
- the verification gate results
- a summary of the steps taken

The user's cells SHALL change only when the user accepts. The user SHALL be able to accept the whole proposal, accept a subset of changes per cell, or reject it.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Reject
- **WHEN** the user rejects a proposal
- **THEN** no user cell SHALL change, and the scratch session SHALL be discarded

### Requirement: Accept is a single undo step
Applying an accepted proposal SHALL be recorded as one undo unit (capability `layout-undo`, requirement "Native undo events for Hefesto compound operations"). A single `undo` SHALL restore the state before the acceptance, including property changes and cell creation, rename and deletion, which ordinary commands do not record.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Undo agent change
- **WHEN** the user accepts a proposal that changed 3 cells and then runs `undo`
- **THEN** all 3 cells SHALL return to their prior state

### Requirement: Resource and scope limits
Each task SHALL run within configurable limits:
- maximum model calls
- maximum tokens or cost
- maximum wall time
- maximum number of tool calls
- the set of cells the task may modify

The default values are open (see Q-AGENT-6). The agent SHALL stop at a limit and return its best proposal, marked "incomplete". It SHALL NOT modify cells outside the declared scope, not even in the scratch session.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Budget reached
- **GIVEN** a cost limit of $1
- **WHEN** the task reaches $1 of model usage
- **THEN** the agent SHALL stop and present its current proposal, marked incomplete

### Requirement: No side channels
The agent SHALL NOT perform any of these actions:
- write files other than through accepted proposals
- execute scripts outside the sandbox of capability `hefesto-scripting`
- change the technology or the project settings
- change provider configuration
- send data to destinations other than the configured provider

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Model requests tech reload
- **WHEN** the model attempts to call `tech load`
- **THEN** the tool call SHALL be refused with a diagnostic, and the task SHALL continue without it

### Requirement: Provenance
Cells changed by an accepted proposal SHALL get a `HEFESTO_agent` property (capability `hefesto-mag-extensions`). The property records:
- the task id
- the provider and model identifier
- a timestamp
- the gate results

The full task log SHALL be kept with the project, subject to the retention setting.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Inspect provenance
- **WHEN** the user inspects an agent-modified cell
- **THEN** the property inspector SHALL show which task and model produced the change
