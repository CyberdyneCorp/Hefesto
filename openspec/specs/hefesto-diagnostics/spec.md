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
Every command of the command API SHALL return a status (success or failure), a result value, and the diagnostics emitted while it ran. The GUI, the CLI and plugins SHALL see this status directly. In Tcl, Magic's commands return OK even when they fail, and PDK scripts rely on it. So the Tcl binding SHALL follow the status convention of capability `scripting-api` (Requirement "Command status and error signalling") by default, and SHALL map a failure to a script error only in script strict mode (Requirement "Script strict mode switch"; see Q-SCRIPT-2).

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2)

#### Scenario: Failing command in a strict-mode script
- **GIVEN** script strict mode is in effect for the script's interpreter
- **WHEN** the script runs `load nonexistent -fail`
- **THEN** the script SHALL receive an error carrying the diagnostic code and text

#### Scenario: Failing command in a PDK script by default
- **WHEN** a PDK script runs `catch {load nonexistent -fail}` with default settings
- **THEN** `catch` SHALL return 0, as in Magic, and the diagnostic SHALL still be recorded

### Requirement: Script strict mode switch
Hefesto SHALL let scripts opt in to script strict mode, in which a command whose command-API status is failure raises a Tcl error instead of returning ok status. The switch SHALL have two levels:
- **Project setting `script-strict`.** A boolean project setting, OFF by default. It gives the initial strict state of every Tcl interpreter that Hefesto creates for the project: the main scripting interpreter and each script-plugin interpreter.
- **Command `hefesto::strict on|off`.** It sets the strict state of the interpreter that runs it, overriding `script-strict` for that interpreter only, until the interpreter is destroyed or the command is run again. Other interpreters are not affected. `hefesto::strict` with no argument SHALL return `1` if strict mode is in effect for the calling interpreter, and `0` otherwise. Any other argument SHALL raise a Tcl error with a usage message.

In strict mode, the Tcl error result SHALL carry the diagnostic code and text of the first `error` diagnostic the command emitted, and the command's diagnostics SHALL still be recorded as usual. Dispatch failures raise errors in both modes (capability `scripting-api`, Requirement "Command status and error signalling").

The switch SHALL be independent of the `magic-compat` setting (capability `hefesto-magic-compat-mode`). `magic-compat` governs output-changing quirks, and script strict mode governs only error signalling; neither changes the other. Because PDK scripts rely on `catch` returning 0, turning `script-strict` on for a project can break them. A script that needs Magic's convention SHALL be able to run `hefesto::strict off` for its own interpreter.

Compatibility: native

(Hefesto decision: docs/decisions/audit/DECISIONS.md#D-4)

#### Scenario: Default is Magic's convention
- **GIVEN** a new project, in which `script-strict` is OFF
- **WHEN** a script runs `hefesto::strict`
- **THEN** the result SHALL be `0`, and `catch {load nonexistent -fail}` SHALL return 0

#### Scenario: Per-interpreter override
- **GIVEN** `script-strict` is OFF and a script plugin's entry script runs `hefesto::strict on`
- **WHEN** that plugin's interpreter and the main interpreter each run `catch {load nonexistent -fail}`
- **THEN** `catch` SHALL return 1 in the plugin's interpreter and 0 in the main interpreter

#### Scenario: Project setting with a local opt-out
- **GIVEN** `script-strict` is ON
- **WHEN** a PDK script runs `hefesto::strict off` and then `catch {load nonexistent -fail}`
- **THEN** `catch` SHALL return 0

#### Scenario: Independent of magic-compat
- **GIVEN** `magic-compat` is OFF and `script-strict` is OFF
- **WHEN** a script runs `catch {load nonexistent -fail}`
- **THEN** `catch` SHALL return 0

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
