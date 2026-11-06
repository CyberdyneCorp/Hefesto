# Hefesto Diagnostics

## Purpose
Defines Hefesto's structured diagnostics model. Magic reports problems as free text, and the specs treat Magic's message wording as `informative`. Hefesto reports problems as structured records that the GUI, scripts, the CLI and tests can all consume.

## Requirements

### Requirement: Structured diagnostic records
Every message emitted by the core SHALL be a diagnostic record with these fields:
- severity: `error`, `warning`, `info` or `debug`
- a stable code, e.g. `CELL-READ-001`
- a human-readable text
- an optional source location: file and line for parsed input, or cell plus coordinates for layout issues
- an optional related-object reference: cell, instance, net, label or rule

The codes SHALL be stable across releases and documented.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-1)

#### Scenario: Tech file error with location
- **WHEN** a tech file contains an unknown section on line 42
- **THEN** an `error` diagnostic SHALL be emitted with a stable code, file and line 42, and text equivalent to Magic's message

### Requirement: Command results and errors
Every command of the command API SHALL return a status (success or failure), a result value, and the diagnostics emitted while it ran. The GUI, the CLI and plugins SHALL see this status directly. In Tcl, Magic's commands return OK even when they fail, and PDK scripts rely on it. So the Tcl binding SHALL follow the status convention of capability `scripting-api` (Requirement "Command status and error signalling") by default, and SHALL map a failure to a script error only in an opt-in strict mode (see Q-SCRIPT-2).

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2)

#### Scenario: Failing command in a strict-mode script
- **GIVEN** strict mode is enabled for the script
- **WHEN** the script runs `load nonexistent -fail`
- **THEN** the script SHALL receive an error carrying the diagnostic code and text

#### Scenario: Failing command in a PDK script by default
- **WHEN** a PDK script runs `catch {load nonexistent -fail}` with default settings
- **THEN** `catch` SHALL return 0, as in Magic, and the diagnostic SHALL still be recorded

### Requirement: Diagnostic presentation and logging
The diagnostics SHALL be presented as follows:
- The GUI SHALL show diagnostics in a filterable panel, linked to the related objects; selecting a diagnostic navigates to its location.
- The CLI SHALL print diagnostics to stderr, in text or JSON lines (`--diagnostics=json`).
- Each session SHALL keep a rotating log file within the app's storage.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-1)

#### Scenario: Navigate to DRC problem
- **WHEN** the user taps a DRC diagnostic in the panel
- **THEN** the view SHALL zoom to the error area in the corresponding cell
