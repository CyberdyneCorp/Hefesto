# Hefesto Architecture

## Purpose
Defines how Hefesto is structured. A platform-independent C++20 core holds the layout database, all algorithms and the command API. Thin per-platform app shells provide windows, input and platform services. A headless command-line tool runs the core for batch flows and compatibility tests.

## Requirements

### Requirement: Platform-independent core
All of the following SHALL live in a core library that depends only on the C++20 standard library and vendored portable libraries (for example zlib):
- layout data, technology handling, file I/O
- DRC, mask generation, extraction, netlisting and routing
- the command API

The core SHALL NOT include any windowing, GPU, OS-UI or process-spawning API. It SHALL reach every host facility through the platform-services interfaces defined in this capability.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-1)

#### Scenario: Core builds without UI
- **WHEN** the core target is built with all app shells disabled
- **THEN** it SHALL compile and link on Linux, macOS and iPadOS without any UI or GPU framework

### Requirement: Platform services abstraction
The core SHALL access host facilities only through abstract service interfaces that each shell implements:
- file access and document storage
- PDK and technology storage
- autosave and recovery
- clock
- logging
- background task execution
- optional external-process execution

A platform without process execution (iPadOS) SHALL report that the capability is unavailable. Features that depend on process execution, such as the IRSIM interface, SHALL then degrade as their own capabilities specify.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-4)

#### Scenario: No process execution
- **GIVEN** the iPadOS shell
- **WHEN** the core asks to start an external simulator
- **THEN** the services layer SHALL return "unsupported", and the core SHALL report this without crashing

### Requirement: Single command API
Every user-visible operation SHALL be exposed as a command in one command API. This covers every command specified by the `cmd-*`, `ui-*`, `drc-commands`, `extract-commands` and related capabilities. Every front end SHALL invoke operations through this API and SHALL NOT call algorithms directly:
- GUI actions
- touch gestures
- key and button bindings
- scripts
- the headless CLI

This makes every interactive action scriptable, loggable and replayable.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Touch action is a command
- **WHEN** a user drags the box with a finger on iPad
- **THEN** the shell SHALL issue the same `box` command that a script would, and command logging SHALL record it

### Requirement: Headless command-line tool
Hefesto SHALL ship a headless executable on Linux and macOS that runs the core with no display. It SHALL:
- load a technology
- run command scripts
- read and write all supported formats
- run DRC and extraction
- exit with a status code

Golden compatibility tests SHALL use this tool.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-6)

#### Scenario: Batch extraction
- **WHEN** `hefesto-cli -T sky130A -c "load inv; extract all; ext2spice; quit"` is run
- **THEN** `inv.spice` SHALL be written and the tool SHALL exit 0

### Requirement: Concurrency model
The layout database SHALL support:
- one writer at a time, which is the command thread
- concurrent readers, used by rendering and by background DRC and extraction

Background work (continuous DRC, extraction, 3D mesh generation) SHALL run off the UI thread and SHALL NOT block input. Results SHALL equal the results of single-threaded execution, so the outputs required by `exact` requirements stay deterministic.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-1)

#### Scenario: Responsive UI during DRC
- **WHEN** background DRC is processing a large region
- **THEN** pan and zoom SHALL remain responsive, and the final DRC errors SHALL match a single-threaded run

### Requirement: App shells per platform
Hefesto SHALL have two thin app shells over the same core:
- **Desktop shell (Linux and macOS):** written in C++20 on the Raven GUI library. Raven's C API SHALL be wrapped in a C++20 UI-shell layer, and no Raven type or header SHALL appear outside that layer. The layout canvas and the 3D view SHALL be Raven viewports in which Hefesto's own renderer draws.
- **iPadOS shell:** written in Swift with SwiftUI and Metal. It SHALL call the C++20 core through Swift/C++ interoperability.

Each shell SHALL contain only presentation, input mapping and platform services. The desktop shell SHALL be delivered first. The iPadOS shell SHALL follow the desktop release criteria of the roadmap.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-11)

#### Scenario: Raven confined to the shell
- **WHEN** the core and CLI targets are built
- **THEN** they SHALL compile and link without Raven headers or libraries

#### Scenario: Shared behavior
- **WHEN** the same command sequence is run in the desktop shell and in the iPadOS shell
- **THEN** the resulting layout SHALL be identical
