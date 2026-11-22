## MODIFIED Requirements

### Requirement: ngspice embedded as a library
Hefesto SHALL run SPICE simulation by calling the ngspice shared-library API from an ngspice build that Hefesto makes itself. It SHALL NOT depend on an ngspice executable or library installed on the host. The ngspice source SHALL be pinned to one exact upstream release tag (ngspice-47 or later) plus a recorded, versioned set of Hefesto patches, following the dependency policy of capability `hefesto-build-and-distribution`.

How the build is linked depends on the platform:
- **Linux and macOS.** The build SHALL produce one shared library, `libngspice-hefesto`. It is installed next to the worker executable `hefesto-simd` and is loaded only by that worker, through an install-relative run path. The editor, the headless command-line tool and the core library SHALL NOT link ngspice or contain any ngspice symbol.
- **iPadOS.** The build SHALL be linked into the app process, as decision D-28 settles: either statically, with object files and relink instructions offered, or as an embedded signed framework.

The ngspice version string reported to users SHALL be `<release>-hefesto<patch-level>`, for example `47-hefesto3`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/include/ngspice/sharedspice.h:1-160, cmake/NgspiceOptions.cmake:86) (See Q-SIM-7, Q-SIM-20, Q-SIM-26.)

#### Scenario: No system ngspice installed
- **GIVEN** a macOS host with no `ngspice` executable or `libngspice` on any search path
- **WHEN** the user runs a transient simulation of an extracted inverter
- **THEN** the simulation SHALL run and produce results

#### Scenario: Version reporting
- **GIVEN** a build from ngspice-47 with patch level 3
- **WHEN** `sim engine` is executed
- **THEN** the reported engine version SHALL be `47-hefesto3`

#### Scenario: No ngspice in the editor
- **WHEN** the symbol tables of the desktop editor, `hefesto-cli` and the core library of a Linux release build are listed
- **THEN** none SHALL contain a symbol whose name starts with `ngSpice_`, and only `hefesto-simd` SHALL have `libngspice-hefesto` as a dependency

### Requirement: Crash containment on desktop
On Linux and macOS, simulations SHALL run in a separate worker executable, `hefesto-simd`. The worker contains the engine adapter and talks to the core through a long-lived IPC channel provided by the platform services (capability `hefesto-architecture`, Requirement "Long-lived worker processes"). The channel carries:
- deck lines
- commands
- progress, log and state events
- result vectors as float64 arrays, in chunks

Any fault SHALL make the running job `failed` with the diagnostic `simulation engine terminated`, and the core SHALL replace the worker with a new one for later jobs. A fault is any of the following:
- the worker crashes or is killed;
- ngspice takes its controlled-exit path;
- the engine becomes `unavailable`;
- the worker sends a malformed or oversize frame;
- the worker misses its heartbeat.

The worker SHALL NOT be reused after a fault. The editor process and the open layouts SHALL NOT be affected.

In-process execution on desktop SHALL exist only in developer builds, for debugging. It loads the same `libngspice-hefesto` as an optional plugin behind the same engine interface. Release builds SHALL NOT offer it.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-11, Q-SIM-21, Q-SIM-22, D-25.)

#### Scenario: ngspice segmentation fault
- **GIVEN** a deck that makes ngspice crash in the worker
- **WHEN** the job runs on macOS
- **THEN** the job SHALL be `failed` with `simulation engine terminated`, unsaved layout edits SHALL remain, and the next job SHALL run in a new worker

#### Scenario: Controlled exit replaces the worker
- **GIVEN** a deck that makes ngspice take its controlled-exit path
- **WHEN** the job runs on Linux
- **THEN** the job SHALL be `failed` with the error lines as diagnostic, and the next job SHALL run in a new worker process with a different process id

## ADDED Requirements

### Requirement: Worker channel limits
The core SHALL enforce these limits on every worker channel:
- one frame at most 64 MiB, with larger vectors sent as several chunks;
- the worker's version handshake within 10 s of start;
- a heartbeat from a running worker at least every 2 s, with the worker declared stuck after 10 s without one.

Each limit SHALL be a setting that can be lowered. A violation SHALL be handled as a fault (Requirement "Crash containment on desktop"). The core SHALL parse every frame defensively: no frame content, however malformed, SHALL crash the core or make it read outside the frame.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-21, Q-SIM-25.)

#### Scenario: Stuck worker
- **GIVEN** a worker that stops sending heartbeats while a job runs
- **WHEN** 10 s pass without a heartbeat
- **THEN** the core SHALL terminate the worker, the job SHALL be `failed` with `simulation engine terminated`, and the next job SHALL run in a new worker
