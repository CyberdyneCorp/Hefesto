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

`cd` SHALL be allowed within the granted roots (the project folder, the PDK store and the session's temporary area) and SHALL fail with a sandbox error for any other directory (decision D-33).

The interpreter's `env` array SHALL be synthesized, not read from the process environment: it holds `PDK_ROOT`, `PDK` and `CAD_ROOT`, with the values `hefesto-pdk-manager` uses for the project's PDK and the install root, taken from the same path-variable table as Q-CELL-17 (capability `cell-library-resolution`). Writes to `env` stay local to the interpreter and never change the process environment. This is the same on every platform, including iPadOS, which has no environment variables (decision D-31).

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: sky130 PCell on any platform
- **GIVEN** sky130A is installed on macOS or iPadOS
- **WHEN** the user places an nfet through the device generator with w=1.0 and l=0.15
- **THEN** Hefesto SHALL produce the same generated cell name and geometry as Magic

#### Scenario: Denied exec
- **WHEN** a script runs `exec ls`
- **THEN** the interpreter SHALL raise an error stating that process execution is not permitted, and Hefesto SHALL continue running

#### Scenario: cd outside the granted roots
- **GIVEN** a project in `/home/u/proj`
- **WHEN** a script runs `cd /home/u/proj/lib` and then `cd /etc`
- **THEN** the first `cd` SHALL succeed and the second SHALL fail with a sandbox error naming `cd`

#### Scenario: Synthesized environment
- **GIVEN** a project pinned to sky130A installed in the PDK store at `/store/ciel/sky130`
- **WHEN** a script reads `$env(PDK_ROOT)` and `$env(PDK)`, and then runs `set env(PDK_ROOT) /x`
- **THEN** the values SHALL be the store path of the version directory and `sky130A`, and after the write the process environment SHALL be unchanged

### Requirement: One interpreter per open project
Each open project SHALL have its own embedded Tcl interpreter, created when the project opens and deleted when it closes. Its `magic::` commands act on that project's session. Procedures, namespaces and variables defined by one project's scripts, including the PDK helpers in `magic::` and the `Opts`/`Winopts` arrays, SHALL NOT be visible to another project's interpreter. Inside each interpreter Tcl semantics are unchanged: a later `proc` replaces an earlier one, with the cross-PDK warning of capability `scripting-api` (Requirement "Script-defined procedures in the engine namespace"). This holds on desktop (several windows) and on iPadOS (several scenes). See Q-SCRIPT-41 (resolution: one interpreter per open project).

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3; docs/decisions/open-questions/SCRIPT.md#Q-SCRIPT-41)

#### Scenario: Two projects on different PDKs
- **GIVEN** project A uses sky130A and project B uses ihp-sg13g2, both open in one Hefesto process, and each has run its PDK's startup and device scripts
- **WHEN** `magic::i2u 34` is evaluated in each project
- **THEN** project A SHALL get the sky130A helper's result and project B the sg13g2 helper's result, and no redefinition warning SHALL be printed

### Requirement: Native PCell generators validated against Tcl
For each supported PDK device, Hefesto SHALL provide native C++ generator implementations. Their cell names, parameters, properties and geometry SHALL be identical to those of the PDK's Tcl generator for the same parameters: the generated cells SHALL have the same name, and their saved `.mag` files SHALL be byte-identical except for the `timestamp` line, which records the save time (decision D-29). Hefesto SHALL use a native generator only when it exists for the installed PDK version, and SHALL otherwise fall back to the Tcl generator. CI SHALL check native and Tcl outputs against each other over a parameter sweep.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Same result native and Tcl
- **WHEN** the same nfet (w=1.0, l=0.15, nf=2) is generated natively and through Tcl
- **THEN** the cell names SHALL be equal and the saved `.mag` files SHALL be byte-identical except for the `timestamp` line

### Requirement: App Store fallback for iPadOS
If App Review rejects running user-imported PDK scripts, the iPadOS build SHALL run only scripts bundled in curated PDK packs, together with the native generators. In that curated-pack mode a PDK whose scripts are not bundled SHALL be refused (capability `scripting-pdk-integration`, Requirement "PDKs without bundled scripts in curated-pack mode"). There is no declarative (non-script) PDK profile (Q-SCRIPT-46, decision D-30). The desktop builds SHALL be unaffected.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Curated pack mode
- **GIVEN** the iPadOS build is in curated-pack mode
- **WHEN** a user imports an unsigned offline PDK pack (capability `hefesto-pdk-manager`) that contains Tcl scripts
- **THEN** Hefesto SHALL use the bundled curated scripts for that PDK, and SHALL report that imported scripts are not executed

#### Scenario: PDK without a curated pack
- **GIVEN** the iPadOS build is in curated-pack mode
- **WHEN** a user imports a PDK for which no curated scripts are bundled
- **THEN** Hefesto SHALL refuse to use that PDK and SHALL report that only curated PDKs run in this mode
