# Hefesto Security

## Purpose
Defines Hefesto's security posture. Hefesto treats every opened file as untrusted data and confines script execution to a sandbox. It also requires user consent before a file triggers script execution. In Magic, opening a layout can implicitly source a PDK `.magicrc`, and Magic offers a socket command server that evaluates anything it receives.

## Requirements

### Requirement: Untrusted input handling
All file readers SHALL treat their input as untrusted. Every reader SHALL:
- bound memory allocation by input size and configured limits
- reject malformed records with a diagnostic
- never execute content from a data file

This applies to layout, tech, netlist, simulation-result and style files. The fuzzing requirements of capability `hefesto-testing-strategy` SHALL apply to every reader.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2)

#### Scenario: Oversized count
- **WHEN** a GDS file declares a record length larger than the remaining file
- **THEN** the reader SHALL stop with an error, without allocating the declared size

### Requirement: Consent before implicit script execution
Opening a layout file whose technology triggers automatic loading of a PDK startup script SHALL execute that script only in one of these cases:
- the PDK is already trusted in the PDK store
- the user has approved it in a one-time consent prompt that shows the script's origin

Trust SHALL be recorded per PDK and version.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Unknown PDK script
- **GIVEN** a `.mag` file whose `tech` line names a PDK found in a downloaded folder that is not yet trusted
- **WHEN** the file is opened
- **THEN** Hefesto SHALL ask before running that PDK's startup script, and SHALL open the file read-only with the `minimum` tech if the user declines

### Requirement: No remote command server by default
Hefesto SHALL NOT listen on any network port by default. If a remote-control feature is offered on desktop, it SHALL require:
- explicit enabling
- binding to localhost only
- an authentication token

The feature SHALL NOT exist on iPadOS.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-4)

#### Scenario: Default install
- **WHEN** Hefesto runs with default settings
- **THEN** no listening sockets SHALL be open

### Requirement: Script sandbox enforcement
The embedded Tcl sandbox of capability `hefesto-scripting` SHALL be enforced in the core. A shell SHALL NOT be able to bypass it. Every denied operation SHALL produce a diagnostic that identifies the script file and line.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Script tries a socket
- **WHEN** a PDK script calls `socket example.com 80`
- **THEN** the call SHALL fail with a sandbox diagnostic naming the script and line
