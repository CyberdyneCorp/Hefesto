# Hefesto Scripting

## Purpose
Defines how Hefesto runs scripts and PDK integration code. Open PDKs drive Magic through Tcl startup files and the `toolkit.tcl` PCell generators (capabilities `scripting-api`, `scripting-pdk-integration`, `pcell-toolkit`). Under decision X-3, Hefesto embeds a sandboxed Tcl interpreter on every platform so that these scripts run unchanged. It also provides native C++ generators as a fast path, validated against the Tcl generators.

## Requirements

### Requirement: Host-independent scripting binding
Script runtimes SHALL bind to the single command API (capability `hefesto-architecture`), never to core internals. A script command SHALL have the same:
- arguments
- return values
- errors
- tag callbacks

as specified by capability `scripting-api`, regardless of the runtime.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Return value parity
- **WHEN** a script calls `box values` in any supported runtime
- **THEN** it SHALL receive the same four-number result that Magic returns in Tcl

### Requirement: Embedded sandboxed Tcl on all platforms
Hefesto SHALL embed a Tcl interpreter on Linux, macOS and iPadOS, exposing the `magic::` command namespace, so that unmodified open_pdks startup files and PCell generator scripts for sky130, gf180mcu and IHP sg13g2 run. The interpreter SHALL be sandboxed. It SHALL NOT provide:
- process execution (`exec`, `open |`)
- sockets
- loading of native extensions (`load` of shared libraries)
- file access outside the paths granted by capability `hefesto-files-and-pdks`

A script that attempts a denied operation SHALL get a Tcl error naming the denied operation.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: sky130 PCell on any platform
- **GIVEN** sky130A is installed on macOS or iPadOS
- **WHEN** the user places an nfet through the device generator with w=1.0 and l=0.15
- **THEN** Hefesto SHALL produce the same generated cell name and geometry as Magic

#### Scenario: Denied exec
- **WHEN** a script runs `exec ls`
- **THEN** the interpreter SHALL raise an error stating that process execution is not permitted, and Hefesto SHALL continue running

### Requirement: Native PCell generators validated against Tcl
For each supported PDK device, Hefesto SHALL provide native C++ generator implementations. Their cell names, parameters, properties and geometry SHALL be identical to those of the PDK's Tcl generator for the same parameters. Hefesto SHALL use a native generator only when it exists for the installed PDK version, and SHALL otherwise fall back to the Tcl generator. CI SHALL check native and Tcl outputs against each other over a parameter sweep.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Same result native and Tcl
- **WHEN** the same nfet (w=1.0, l=0.15, nf=2) is generated natively and through Tcl
- **THEN** the cell names SHALL be equal and the `.mag` contents SHALL be byte-identical

### Requirement: App Store fallback for iPadOS
If App Review rejects running user-imported PDK scripts, the iPadOS build SHALL run only scripts bundled in curated PDK packs, together with the native generators. The desktop builds SHALL be unaffected.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Curated pack mode
- **GIVEN** the iPadOS build is in curated-pack mode
- **WHEN** a user imports a PDK folder that contains Tcl scripts
- **THEN** Hefesto SHALL use the bundled curated scripts for that PDK, and SHALL report that imported scripts are not executed
