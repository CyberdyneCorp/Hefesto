# Agent Natural-Language Commands

## Purpose
Lets users describe layout operations in natural language, for example "make a 2 µm wide metal2 strap from pin A to pin B". The agent translates the request into a reviewable sequence of commands from the command API. It also explains results and diagnostics in plain language.

## Requirements

### Requirement: Translate requests into command sequences
For a natural-language request, the agent SHALL produce an explicit command sequence for the scratch session, resolving:
- layer names against the loaded tech, including aliases
- dimensions with units, following capability `cmd-coordinates-and-units`
- object references (pins, labels, instances, the selection, the box) against the current cell

Before running the sequence, the agent SHALL show it to the user. Ambiguous references SHALL produce a clarifying question; the agent SHALL NOT guess.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Strap between pins
- **GIVEN** a cell with ports `A` at (0,0) and `B` at (20 µm,0)
- **WHEN** the user asks for "a 2 µm metal2 strap from A to B"
- **THEN** the proposal SHALL contain commands that paint metal2 2 µm wide from A to B, with the vias needed to connect to the pins' layers, and DRC results attached

#### Scenario: Ambiguous layer
- **WHEN** the user asks for "a poly strap" and the tech defines two poly layers
- **THEN** the agent SHALL ask which layer is meant

### Requirement: Explain results and diagnostics
On request, the agent SHALL explain the following in plain language, citing the underlying data:
- DRC errors: the why-text, the rule values, the location
- extraction results: node capacitance and devices
- LVS mismatches
- diagnostics

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Why is this an error
- **WHEN** the user selects a DRC error and asks "why?"
- **THEN** the agent SHALL state the violated rule, its required value, the measured value, and a suggested fix
