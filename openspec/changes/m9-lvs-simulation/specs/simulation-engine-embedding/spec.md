## MODIFIED Requirements

### Requirement: ngspice embedded as a library
Hefesto SHALL run SPICE simulation by calling the ngspice shared-library API from an ngspice build that Hefesto makes itself. It SHALL NOT depend on an ngspice executable or library installed on the host. The ngspice source SHALL be pinned to one exact upstream release tag (ngspice-47 or later) plus a recorded, versioned set of Hefesto patches, following the dependency policy of capability `hefesto-build-and-distribution`.

How the build is linked depends on the platform:
- **Linux and macOS.** The build SHALL produce one shared library, `libngspice-hefesto`. The engine adapter SHALL be a separate plugin, `hefesto-ngspice-adapter`, which links that library and exports only Hefesto's engine interface. Both are installed next to the worker executable `hefesto-simd` and found through an install-relative run path. In release builds only `hefesto-simd` SHALL load the adapter plugin. The editor, the headless command-line tool and the core library SHALL NOT link ngspice or the adapter plugin, and SHALL NOT contain any ngspice symbol.
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
- **THEN** none SHALL contain a symbol whose name starts with `ngSpice_`, only `hefesto-ngspice-adapter` SHALL have `libngspice-hefesto` as a dependency, and only `hefesto-simd` SHALL load `hefesto-ngspice-adapter`

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

Between jobs, a worker SHALL be reused: after a job's results are copied out it SHALL clear the circuit as Requirement "Single engine instance per address space" requires, and before the next load it SHALL repeat the per-load steps of Requirement "Deterministic engine initialisation". `ngSpice_Reset` SHALL NOT be used on desktop.

In-process execution on desktop SHALL exist only in developer builds, for debugging. The developer host loads the same `hefesto-ngspice-adapter` plugin behind the same engine interface. Release builds SHALL NOT offer it and SHALL NOT load the plugin into the editor process.

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

### Requirement: ngspice build configuration and license hygiene
The ngspice build SHALL use this feature set on every platform:

| Feature | Setting | Reason |
|---|---|---|
| shared-library API (`SHARED_MODULE`) | on; built as the shared library `libngspice-hefesto` on Linux and macOS, and linked into the app as decision D-28 settles on iPadOS | embedding |
| XSPICE | on | mixed-signal and digital models |
| XSPICE code models | analog, digital, spice2poly and xtradev libraries compiled in statically | no runtime `dlopen` on iPadOS, and the same registry on every platform |
| `table` code model library | off | GPLv2+ license |
| `d_cosim`, GHDL and Icarus co-simulation shims | off | load arbitrary user code |
| OSDI | on | Verilog-A compact models (see Requirement "OSDI compact models") |
| KLU sparse solver | on, bundled copy | solver performance |
| CIDER, NDEV | off | not needed |
| FFTW3 | off | GPL license; ngspice's built-in FFT is used |
| libsndfile / libsamplerate | off | not needed; LGPL |
| readline/editline, X11 plotting, help browser | off | no terminal or X11 |
| OpenMP | off on iPadOS; MAY be on for Linux and macOS desktop builds | no OpenMP runtime on iPadOS |

Hefesto SHALL ship the license texts of every ngspice component it links, including the Modified BSD license of ngspice and the separately licensed parts it contains: KLU together with the SuiteSparse parts it brings (AMD, COLAMD, BTF, SuiteSparse_config), numparam (LGPLv2 or newer), OSDI (MPL 2.0), sparse (MIT-style) and the XSPICE code-model libraries, each with the license its files carry in the pinned tree. No GPL-licensed ngspice component SHALL be linked. Static linking of the LGPL parts on iPadOS is pending Q-SIM-1.

The build manifest SHALL list every Hefesto patch applied to ngspice, including at least the static OSDI registry (Requirement "OSDI compact models"), the static code-model registration, and the portable generator seeding of capability `simulation-monte-carlo`, Requirement "Portable random numbers".

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7, docs/decisions/product.md#P-19) (ngspice ref: COPYING:1-60, cmake/NgspiceOptions.cmake:21-74) (See Q-SIM-1, Q-SIM-12, Q-SIM-20, Q-SIMADV-6.)

#### Scenario: No GPL code linked
- **WHEN** the license manifest of a release build is generated
- **THEN** it SHALL list ngspice (Modified BSD), KLU with AMD, COLAMD, BTF and SuiteSparse_config, numparam, OSDI, sparse and the XSPICE code models with their licenses, and SHALL NOT list FFTW3, the `table` code model library or readline

#### Scenario: Patch list in the manifest
- **WHEN** the build manifest of a release build is generated
- **THEN** its ngspice entry SHALL name the portable-seeding patch among the Hefesto patches

#### Scenario: Built-in FFT
- **GIVEN** a build without FFTW3
- **WHEN** a deck uses the `fft` control command on a transient result
- **THEN** ngspice's internal FFT SHALL be used and the command SHALL succeed

### Requirement: Cancellation and fault states
Each engine instance SHALL have one of these states: `idle`, `loaded`, `running`, `paused`, `faulted`, `unavailable`.

- **Cancel.** The adapter SHALL send `bg_halt` and wait at most 2 s for `BGThreadRunning(true)`. If the thread ends, the job becomes `cancelled`, the data computed so far is kept as a partial result, and the instance returns to `idle` after `remcirc`. If the thread does not end in time, the instance becomes `faulted`.
- **Fault.** On a `ControlledExit` callback, or when cancellation times out, the instance becomes `faulted`. The running job becomes `failed`, with the last 50 error-stream lines as its diagnostic.
- **Recovery on Linux and macOS.** A faulted instance is never reused: its worker process SHALL be replaced as Requirement "Crash containment on desktop" specifies, and the next job SHALL run in the new worker. `ngSpice_Reset` SHALL NOT be called.
- **Recovery on iPadOS.** Before the next job, the adapter SHALL call `ngSpice_Reset`, repeat initialisation (Requirement "Deterministic engine initialisation") and return to `idle`. If `ngSpice_Reset` returns non-zero, or a second fault happens before any job completes, the instance becomes `unavailable`, and simulation stays unavailable until the app restarts (Requirement "In-process execution on iPadOS").

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/sharedspice.c:547-590, src/sharedspice.c:1473-1479, src/sharedspice.c:2134-2190) (See Q-SIM-11, D-25.)

#### Scenario: Cancel a transient run
- **GIVEN** a transient run from 0 to 1 ms that has reached 300 µs
- **WHEN** the user cancels it
- **THEN** the job SHALL end as `cancelled`, and its partial result SHALL hold the time points up to about 300 µs

#### Scenario: Unrecoverable error
- **GIVEN** a deck that makes ngspice call its controlled-exit path
- **WHEN** the job runs on Linux or macOS
- **THEN** the job SHALL be `failed` with the error lines as diagnostic, and the next job SHALL run in a new worker process, with no engine reset

#### Scenario: Reset on iPadOS
- **GIVEN** the same deck on iPadOS
- **WHEN** the job runs
- **THEN** the job SHALL be `failed` with the error lines as diagnostic, and the next job SHALL run after `ngSpice_Reset` and repeated initialisation

### Requirement: In-process execution on iPadOS
On iPadOS, which has no process creation, the engine SHALL run in the app process with one instance (Requirement "Single engine instance per address space"), linked as decision D-28 settles: statically, with object files and relink instructions offered, or as an embedded signed framework. Because a crash inside ngspice ends the app, Hefesto SHALL protect user data as follows:
- **Autosave first.** Before loading each deck, it SHALL complete an autosave of all modified cells (capability `hefesto-files-and-pdks`, Requirement "Locking and recovery replacements").
- **Validate the deck.** It SHALL run the deck validation of capability `simulation-netlist-preparation` before passing anything to ngspice.
- **Memory budget.** It SHALL enforce the result memory budget of capability `simulation-analyses-and-results`.
- **Memory pressure.** On an iPadOS memory warning during a run, it SHALL first release caches as capability `hefesto-performance` requires. If a second warning arrives during the same run, it SHALL cancel the run.
- **Suspension.** When the app moves to the background, a running simulation SHALL be paused (`bg_halt`) and resumed when the app returns. Hefesto SHALL NOT rely on background execution time.

If the instance becomes `unavailable`, Hefesto SHALL report `simulation engine needs an app restart` and keep every non-simulation feature working.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-4, Q-SIM-19, Q-SIM-26, D-28.)

#### Scenario: Autosave before simulation
- **GIVEN** an iPad session with unsaved edits in cell `inv`
- **WHEN** the user starts a simulation
- **THEN** an autosave of `inv` SHALL complete before the deck is passed to ngspice

#### Scenario: App backgrounded
- **GIVEN** a running transient simulation on iPad
- **WHEN** the user switches to another app and returns 30 s later
- **THEN** the simulation SHALL have been paused while in the background, and SHALL then continue to completion

### Requirement: Multiple simulations and corner parallelism
Hefesto SHALL accept any number of submitted jobs, for example one per corner. On Linux and macOS it SHALL run up to `N` jobs at once, one per worker process, where

`N = max(1, min(4, physical_cores − 1, ⌊B_total / B_job⌋))`

`B_total` is half the physical memory, and `B_job` is the job's result memory budget (capability `simulation-analyses-and-results`) plus an estimate of its prepared deck size. A user setting MAY lower `N` and SHALL NOT raise it. On iPadOS, jobs SHALL run one at a time in submission order. A job's results SHALL NOT depend on the number of workers, on which worker ran it, or on the order in which jobs finish. Sweeps, corners and Monte Carlo runs share this pool (capabilities `simulation-sweeps`, `simulation-corners`, `simulation-monte-carlo`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7, docs/decisions/product.md#P-19) (See Q-SIM-5, Q-SIMADV-13.)

#### Scenario: Five corners on an 8-core Mac
- **GIVEN** an 8-physical-core Mac with 32 GB of memory, default settings and a corner sweep of 5 corners with the default 4 GiB result budget and small decks
- **WHEN** the sweep is started
- **THEN** 3 jobs SHALL run at once (half of 32 GB divided by `B_job` rounds down to 3), and the next SHALL start when one finishes

#### Scenario: Same result with one worker
- **GIVEN** the same 5-corner sweep run with `N = 1` and with `N = 3`
- **WHEN** the results are compared
- **THEN** every vector SHALL be bit-identical between the two runs

#### Scenario: Single-core host
- **GIVEN** a Linux host with one physical core
- **WHEN** a 10-point sweep is started
- **THEN** `N` SHALL be 1, and the sweep SHALL complete

#### Scenario: Setting cannot raise the pool
- **GIVEN** a host on which the formula gives `N = 3`
- **WHEN** the user sets the worker count to 8
- **THEN** the setting SHALL be refused with a diagnostic that gives the maximum 3, and at most 3 jobs SHALL run at once

## ADDED Requirements

### Requirement: Worker channel limits
The core SHALL enforce these limits on every worker channel:
- one frame at most 64 MiB, with larger vectors sent as several chunks;
- the worker's version handshake within 10 s of start;
- a heartbeat from a running worker at least every 2 s, with the worker declared stuck after 10 s without one.

The worker SHALL send its heartbeat from a thread that does not call the engine and is not blocked by it, so that a long deck load or analysis does not stop the heartbeat. A stuck analysis is detected by cancellation (Requirement "Cancellation and fault states"), not by the heartbeat.

Each limit SHALL be a setting that can be lowered. A violation SHALL be handled as a fault (Requirement "Crash containment on desktop"). The core SHALL parse every frame defensively: no frame content, however malformed, SHALL crash the core or make it read outside the frame.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-21, Q-SIM-25.)

#### Scenario: Stuck worker
- **GIVEN** a worker that stops sending heartbeats while a job runs
- **WHEN** 10 s pass without a heartbeat
- **THEN** the core SHALL terminate the worker, the job SHALL be `failed` with `simulation engine terminated`, and the next job SHALL run in a new worker

#### Scenario: Large deck load
- **GIVEN** a 200 MiB post-layout deck whose load in the engine takes 25 s
- **WHEN** it is loaded in a worker
- **THEN** the core SHALL keep receiving heartbeats every 2 s during the load, and the job SHALL NOT be failed for a missed heartbeat

### Requirement: Worker confinement
On Linux and macOS, `hefesto-simd` SHALL confine itself before it loads the engine adapter, using the operating system's own mechanisms (a sandbox profile on macOS; no-new-privileges, a seccomp filter and, where the kernel supports it, a Landlock ruleset on Linux). Once confined, the worker SHALL NOT be able to:
- open network sockets;
- create processes;
- write outside the job directory the core passed to it;
- write to the PDK store or the installation directory.

It MAY read the PDK store, the installed libraries and the job directory. When a mechanism is unavailable on the host (for example Landlock on an older Linux kernel), the worker SHALL apply the others and record the missing one in the job log. On macOS, `hefesto-simd` SHALL be the only Hefesto binary that disables library validation, so that a user-rebuilt `libngspice-hefesto` can be loaded (capability `hefesto-licensing`, Requirement "Process boundary for external engines").

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-20, Q-SIM-21.)

#### Scenario: Deck tries to reach the network
- **GIVEN** a test build whose adapter, when a deck asks it to, tries to open a TCP socket and to write a file in the user's home directory
- **WHEN** such a deck runs on Linux
- **THEN** both attempts SHALL be denied by the operating system, and the job SHALL complete or fail without either effect
