# Agent Architecture

## Purpose
Defines how Hefesto's AI layout agent is structured. The agent is a planner driven by an AI model. It can act on a layout only by calling tools, and every tool maps to the command API of capability `hefesto-architecture`. The agent works inside an isolated scratch session and returns proposals, which the user must approve (capability `agent-safety-and-approval`). Because the agent cannot write geometry except through layout commands, all of its output is ordinary, DRC-checkable Magic-compatible layout.

## Requirements

### Requirement: Tools map to the command API
The agent SHALL act on layout only through a declared tool set. Every tool SHALL be a typed wrapper over one or more command-API commands, for example:
- `paint`, `erase`, `box`, `label`, `port`
- `getcell`, `copy`, `move`, `flatten`
- the PCell generators
- `drc`, `extract`, `ext2spice`, `lvs`, `simulate`
- read-only queries: `what`, `select`, `cellname`, `drc listall why`, net tracing, the tech rule summary

The agent SHALL NOT access the database, files or scripts by any other path. Each tool call and its result SHALL be logged.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Agent paints through commands
- **WHEN** the agent decides to add a metal2 strap
- **THEN** it SHALL invoke the `paint` tool, which issues the `paint metal2` command on the scratch session, and the call SHALL appear in the agent log

### Requirement: Scratch session isolation
Each agent task SHALL run in a scratch session. The scratch session SHALL contain:
- copies of the target cell and the cells it needs to modify
- read-only references to the rest of the hierarchy
- the active technology

Changes in the scratch session SHALL NOT affect the user's open cells, files or undo history until the proposal is accepted.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Cancel leaves no trace
- **WHEN** the user cancels an agent task midway
- **THEN** the user's cells, files and undo history SHALL be unchanged

### Requirement: Grounding context
For each task, the agent SHALL receive a structured context containing:
- the technology summary: layers, planes, connectivity, DRC rule values in microns, and the device generators available from the PDK
- the target cell's hierarchy, ports, labels and bounding box
- the current selection and box
- for schematic-driven tasks, the source netlist

The context SHALL be generated from the specs' data models (tech, DRC rules, PCell catalog). It SHALL NOT be free text scraped from files.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Rule values provided
- **GIVEN** sky130A is loaded
- **WHEN** an agent task starts
- **THEN** its context SHALL include the metal1 minimum width and spacing in microns, taken from the loaded DRC style

### Requirement: Verification gates are tools, not opinions
The agent SHALL treat DRC, LVS, extraction and simulation results as authoritative. A proposal SHALL be marked "verified" only if the gates required by its task type have passed on the scratch session. The gate results SHALL be attached to the proposal.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Claimed clean but not
- **WHEN** the model says its layout is DRC-clean but `drc count` reports errors
- **THEN** the proposal SHALL be marked "unverified" and SHALL show the error count

### Requirement: Platform availability and task control
The agent SHALL be available on Linux, macOS and iPadOS. Tasks SHALL:
- run in the background
- report progress, meaning the current step and the gate results so far
- be cancellable at any point
- respect the limits of capability `agent-safety-and-approval`

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Background task on iPad
- **WHEN** an agent task runs on iPad while the user keeps editing another cell
- **THEN** the user's editing SHALL NOT be blocked, and the task SHALL show its progress
