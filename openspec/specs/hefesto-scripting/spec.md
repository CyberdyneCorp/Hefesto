# Hefesto Scripting

## Purpose
Defines how Hefesto runs scripts and PDK integration code. Open PDKs drive Magic through Tcl startup files and the `toolkit.tcl` PCell generators (capabilities `scripting-api`, `scripting-pdk-integration`, `pcell-toolkit`). Hefesto must keep them working on desktop platforms and offer an equivalent on iPadOS. The per-platform runtime is pending cross-cutting decision X-3. This spec fixes only the parts that follow from the decisions already made.

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

### Requirement: Tcl compatibility on desktop
On Linux and macOS, Hefesto SHALL be able to run unmodified open_pdks Tcl startup files and PCell generator scripts for sky130, gf180mcu and IHP sg13g2. This follows from full data compatibility. The mechanism SHALL be an embedded Tcl interpreter with the `magic::` command namespace, unless decision X-3 chooses an equivalent with proven script compatibility.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: sky130 PCell on desktop
- **GIVEN** sky130A is installed on macOS
- **WHEN** the user places an nfet through the device generator with w=1.0 and l=0.15
- **THEN** Hefesto SHALL produce the same generated cell name and geometry as Magic

### Requirement: PDK equivalence on iPadOS
On iPadOS, Hefesto SHALL provide PDK startup configuration and PCell generation that give the same results as the desktop Tcl path for the supported PDKs. The mechanism is pending decision X-3. The recommended option is a declarative PDK profile plus native generators, bound to the same command API.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Same device on iPad
- **WHEN** the same nfet (w=1.0, l=0.15) is generated on iPad
- **THEN** the cell name and geometry SHALL match the desktop result
