# Simulation Engine Embedding

## Purpose
Defines how Hefesto embeds the ngspice circuit simulator as a library so that SPICE simulation works on Linux, macOS and iPadOS (decision P-7). It covers how ngspice is built and linked, the boundary between the Hefesto core and ngspice, initialisation, the engine lifecycle and threading model, error recovery, crash containment, running several simulations, dynamic model loading (OSDI / Verilog-A and XSPICE code models) and the iPadOS constraints.

IRSIM stays the legacy switch-level path on desktop (capability `irsim-interface`). Netlist generation is in capability `simulation-netlist-preparation`, analyses and result storage in `simulation-analyses-and-results`, viewing and probing in `simulation-ui-and-crossprobe`, and the user-visible commands in `simulation-command`.

ngspice reference: release ngspice-47, SourceForge `ngspice` tag commit `a80f6e3e` (Q-SIM-7; re-checked 2026-11-22, the specs were first written from master `7aa47164`). `(ngspice ref: ...)` paths are relative to that tree. "The ngspice API" means the shared-library interface declared in `src/include/ngspice/sharedspice.h`.

## Requirements

### Requirement: ngspice embedded as a library
Hefesto SHALL run SPICE simulation by calling the ngspice shared-library API from an ngspice build that Hefesto makes itself. It SHALL NOT depend on an ngspice executable or library installed on the host. The ngspice source SHALL be pinned to one exact upstream release tag plus a recorded, versioned set of Hefesto patches, following the dependency policy of capability `hefesto-build-and-distribution`. The pin is ngspice-47 (tag commit `a80f6e3e95d51534905b1f23410a951802666656`, Q-SIM-7). That release builds with autotools only, so Hefesto builds it with its own CMake build (`third_party/ngspice/CMakeLists.txt`); moving to a later release, for example the first one that ships an upstream CMake build, is a separate decision.

How the build is linked depends on the platform:
- **Linux and macOS.** The build SHALL produce one shared library, `libngspice-hefesto`. The engine adapter SHALL be a separate plugin, `hefesto-ngspice-adapter`, which links that library and exports only Hefesto's engine interface. Both are installed in the install tree's library directory (`lib/`, beside the `bin/` that holds the worker executable `hefesto-simd`) and found through an install-relative run path (`$ORIGIN/../lib` on Linux, `@loader_path/../lib` on macOS). The library file carries the release as its version (`libngspice-hefesto.so.47` on Linux, `libngspice-hefesto.47.dylib` on macOS, with the unversioned name as a link). In release builds only `hefesto-simd` SHALL load the adapter plugin. The editor, the headless command-line tool and the core library SHALL NOT link ngspice or the adapter plugin, and SHALL NOT contain any ngspice symbol.
- **iPadOS.** The build SHALL be linked into the app process, as decision D-28 settles: either statically, with object files and relink instructions offered, or as an embedded signed framework.

The ngspice version string reported to users SHALL be `<release>-hefesto<patch-level>`, for example `47-hefesto3`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/include/ngspice/sharedspice.h:1-160, configure.ac:178-180, src/Makefile.am:498-616, the Hefesto wrapper (third_party/ngspice/CMakeLists.txt)) (See Q-SIM-7, Q-SIM-20, Q-SIM-26.)

#### Scenario: No system ngspice installed
- **GIVEN** a macOS host with no `ngspice` executable or `libngspice` on any search path
- **WHEN** the user runs a transient simulation of an extracted inverter
- **THEN** the simulation SHALL run and produce results

#### Scenario: Version reporting
- **GIVEN** a build from ngspice-47 with patch level 3
- **WHEN** `sim engine` is executed
- **THEN** the reported engine version SHALL be `47-hefesto3`

#### Scenario: Pinned source in the build manifest
- **WHEN** the build manifest of a release build is generated
- **THEN** its ngspice entry SHALL name release `ngspice-47`, commit `a80f6e3e95d51534905b1f23410a951802666656` and the Hefesto CMake build, and the build SHALL fail if the fetched release archive's SHA-256 differs from the pinned archive's SHA-256 (`third_party/ngspice/pin.json`; the archive is the ngspice-47 release tarball of that tag, and the build does not compare it with the tag's git tree)

#### Scenario: No ngspice in the editor
- **WHEN** the symbol tables of the desktop editor, `hefesto-cli` and the core library of a Linux release build are listed
- **THEN** none SHALL contain a symbol whose name starts with `ngSpice_`, only `hefesto-ngspice-adapter` SHALL have `libngspice-hefesto` as a dependency, and only `hefesto-simd` SHALL load `hefesto-ngspice-adapter`

### Requirement: ngspice build configuration and license hygiene
The ngspice build SHALL use this feature set on every platform:

| Feature | Setting | Reason |
|---|---|---|
| shared-library API (`SHARED_MODULE`) | on; built as the shared library `libngspice-hefesto` on Linux and macOS, and linked into the app as decision D-28 settles on iPadOS | embedding |
| XSPICE | on | mixed-signal and digital models |
| XSPICE code models | analog, digital, spice2poly and xtradev libraries compiled in statically | no runtime `dlopen` on iPadOS, and the same registry on every platform |
| `table` code model library | off | GPLv2+ license |
| `d_cosim`, GHDL and Icarus co-simulation shims | off | load arbitrary user code |
| `d_process` code model (in the `digital` library) | off | starts external processes |
| OSDI | on | Verilog-A compact models (see Requirement "OSDI compact models") |
| KLU sparse solver | on, bundled copy | solver performance |
| CIDER, NDEV | off | not needed |
| FFTW3 | off | GPL license; ngspice's built-in FFT is used |
| libsndfile / libsamplerate | off | not needed; LGPL |
| readline/editline, X11 plotting, help browser | off | no terminal or X11 |
| OpenMP | off on iPadOS; MAY be on for Linux and macOS desktop builds | no OpenMP runtime on iPadOS |

Hefesto SHALL ship the license texts of every ngspice component it links, including the Modified BSD license of ngspice and the separately licensed parts it contains: KLU together with the SuiteSparse parts it brings (AMD, COLAMD, BTF, SuiteSparse_config), numparam (LGPLv2 or newer), OSDI (MPL 2.0), sparse (MIT-style) and the XSPICE code-model libraries, each with the license its files carry in the pinned tree. No GPL-licensed ngspice component SHALL be linked. How the LGPL parts are linked on iPadOS (an embedded signed framework, or static linking with object files and relink instructions offered) is settled by decision D-28 before M14 (Q-SIM-1).

The build manifest SHALL list every Hefesto patch applied to ngspice, including at least the static OSDI registry (Requirement "OSDI compact models"), the static code-model registration, the portable generator seeding of capability `simulation-monte-carlo`, Requirement "Portable random numbers", the deterministic initialisation of Requirement "Deterministic engine initialisation" (patch 0004) and the once-per-load draw of top-level statistical `.param` lines of capability `simulation-monte-carlo`, Requirement "Statistical functions in ngspice (informative)" (patch 0005, finding F1).

The license manifest SHALL record, for KLU, AMD, COLAMD, BTF and SuiteSparse_config, the license of each file as an SPDX expression (`file_licenses`), and each component's license SHALL be the set of its files' licenses.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7, docs/decisions/product.md#P-19) (ngspice ref: COPYING:1-60, configure.ac:138-196, configure.ac:936-944, configure.ac:1320-1330, the Hefesto wrapper (third_party/ngspice/CMakeLists.txt)) (See Q-SIM-1, Q-SIM-12, Q-SIM-20, Q-SIMADV-6.)

#### Scenario: No GPL code linked
- **WHEN** the license manifest of a release build is generated
- **THEN** it SHALL list ngspice (Modified BSD), KLU with AMD, COLAMD, BTF and SuiteSparse_config, numparam, OSDI, sparse and the XSPICE code models with their licenses, and SHALL NOT list FFTW3, the `table` code model library or readline

#### Scenario: Patch list in the manifest
- **WHEN** the build manifest of a release build is generated
- **THEN** its ngspice entry SHALL name the portable-seeding patch, `0004-deterministic-initialisation.patch` and `0005-stat-param-once-per-load.patch` among the Hefesto patches

#### Scenario: Per-file licenses of the SuiteSparse parts
- **WHEN** the license manifest of a release build is generated
- **THEN** every compiled file of KLU, AMD, COLAMD, BTF and SuiteSparse_config SHALL have an SPDX license expression in `file_licenses`, and each of these components' license SHALL be the set of its files' expressions

#### Scenario: Built-in FFT
- **GIVEN** a build without FFTW3
- **WHEN** a deck uses the `fft` control command on a transient result
- **THEN** ngspice's internal FFT SHALL be used and the command SHALL succeed

### Requirement: Engine adapter boundary
Exactly one Hefesto component, the engine adapter, SHALL include the ngspice API header and call the ngspice API. The rest of the core SHALL see only Hefesto's own engine interface, which offers:
- load a deck (as lines of text)
- run, pause, resume and cancel
- query plots and vectors
- callbacks for output text, progress, streamed data points and state changes

ngspice types SHALL NOT appear in that interface. The adapter SHALL copy every string, name and data array it receives from ngspice into Hefesto-owned memory before the callback or API call returns. It SHALL never free memory allocated by ngspice, and ngspice SHALL never be asked to free memory allocated by Hefesto. Pointers returned by `ngGet_Vec_Info` SHALL be used only while vector reallocation is locked (`ngSpice_LockRealloc` / `ngSpice_UnlockRealloc`) or after the background thread has ended.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/include/ngspice/sharedspice.h:95-110, src/include/ngspice/sharedspice.h:475-505)

#### Scenario: String lifetime
- **GIVEN** a `SendChar` callback delivering `Note: Starting dynamic gmin stepping`
- **WHEN** the callback returns
- **THEN** Hefesto's log SHALL still hold the full text, even though ngspice frees its buffer immediately afterwards

#### Scenario: Reading vectors during a run
- **GIVEN** a transient run that is still in progress
- **WHEN** the waveform viewer asks for the current points of `v(out)`
- **THEN** the adapter SHALL lock reallocation, copy the points, and unlock, without reading the vector's data pointer outside that window

### Requirement: Deterministic engine initialisation
Before calling `ngSpice_Init`, the adapter SHALL call `ngSpice_nospinit` and `ngSpice_nospiceinit`, so that ngspice reads no `spinit` file and no `.spiceinit` file from the current directory, `SPICE_USERINIT_DIR`, `HOME` or `USERPROFILE`. It SHALL pass non-null `SendChar`, `SendStat`, `ControlledExit`, `SendData`, `SendInitData` and `BGThreadRunning` callbacks and a user-data pointer that identifies the engine instance. Right after initialisation, the adapter SHALL send this fixed command list, in this order:
1. `set noaskquit`
2. `set nomoremode`
3. `set filetype=binary`

Before each deck is loaded, it SHALL send the per-PDK initialisation commands of capability `simulation-netlist-preparation` (Requirement "PDK initialisation commands"), followed by `setseed <s>`, where `s` is 1 for ordinary jobs and the sample seed `s_i` for Monte Carlo samples (capability `simulation-monte-carlo`, Requirement "Sample seeds"). `setseed <n>` reseeds ngspice's generator immediately, including the pool used by `trnoise` and `trrandom` sources (stock ngspice-47's `setseed` reseeds only the main generator, and its `trnoise` pool is seeded from the process id at initialisation; Hefesto's patch 0004 makes `setseed` refill the pool and removes the process-id seed. Finding F2, carried by patch 0004, approved by the user 2026-11-25; see `docs/decisions/m9-decision-table.md` "Trace re-check against ngspice-47"); `set rndseed=<n>` only sets a variable and SHALL NOT be relied on, because the shared-library initialisation seeds the generator from the process id. Simulation results SHALL NOT depend on environment variables, the current directory, the user's home directory or the process id, on any platform.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/sharedspice.c:803-830, src/sharedspice.c:856-1091, src/sharedspice.c:930-935, src/sharedspice.c:1079-1081, src/maths/misc/randnumb.c:71-92, src/maths/misc/randnumb.c:286-322, src/frontend/trannoise/wallace.c:76) (See Q-SIMADV-7.)

#### Scenario: Stray .spiceinit ignored
- **GIVEN** a `.spiceinit` file in the user's home directory containing `set ngbehavior=ps`
- **WHEN** the engine is initialised and a sky130 deck is run
- **THEN** the compatibility mode in effect SHALL be the one from the sky130 PDK descriptor, not `ps`

#### Scenario: Reproducible random sources
- **GIVEN** a deck with a `trnoise` source and a top-level `.param p = 'agauss(1, 0.1, 1)'`, and no seed setting
- **WHEN** it is simulated twice in fresh engine instances (different process ids) on the same machine
- **THEN** `setseed 1` SHALL have been sent before each load, and both runs SHALL produce bit-identical vectors and the same value of `p`

#### Scenario: Reused engine repeats trnoise
- **GIVEN** a deck with a `trnoise` source and no seed setting
- **WHEN** it is simulated twice in one reused engine instance, and once in a fresh instance
- **THEN** the three runs SHALL produce bit-identical `trnoise` vectors, because `setseed 1` before each load also refills the `trnoise` pool (patch 0004)

### Requirement: Single engine instance per address space
ngspice keeps its simulator state in process-wide globals. Hefesto SHALL therefore create at most one ngspice engine instance per process, and SHALL serialise all API calls to it. Jobs SHALL go through one FIFO job queue per engine instance. At most one job SHALL be loaded or running in an instance at a time. When a job's results have been copied out (see capability `simulation-analyses-and-results`), the adapter SHALL send `remcirc` and `destroy all`. This keeps the instance's memory from growing from job to job. Hefesto SHALL NOT load several copies of the ngspice library into one process.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/sharedspice.c:390-470)

#### Scenario: Second job waits
- **GIVEN** a running transient job A in the in-process engine
- **WHEN** the user starts job B
- **THEN** job B SHALL be queued with state `queued` and SHALL start only after job A reaches a final state

#### Scenario: Memory released between jobs
- **GIVEN** job A completed and its results were stored
- **WHEN** job B is loaded
- **THEN** `ngSpice_AllPlots` SHALL list no plot belonging to job A

### Requirement: Engine threading model
The engine adapter SHALL own a dedicated engine thread that makes every ngspice API call for its instance. The UI thread and the command thread SHALL NOT call ngspice. Every analysis SHALL be started as `bg_run`, so that ngspice runs it on its own background thread. Pause SHALL use `bg_halt`, resume SHALL use `bg_resume`, and cancellation SHALL follow Requirement "Cancellation and fault states". The engine thread SHALL learn that a run has ended from the `BGThreadRunning(true)` callback. While the background thread is running, the adapter SHALL send no command other than `bg_halt`.

Callbacks can arrive on the engine thread, on ngspice's background thread or on ngspice's print thread. Every callback handler SHALL:
- copy its data
- post an event to the engine thread
- return within 1 ms in the common case

A callback handler SHALL NOT call the ngspice API, take a layout-database lock or wait on the UI thread. The layout database and the UI SHALL stay responsive during simulation, as capability `hefesto-architecture` requires for background work.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/sharedspice.c:24-40, src/sharedspice.c:640-730, src/sharedspice.c:795-800)

#### Scenario: UI stays responsive
- **GIVEN** a 10-second transient simulation in progress
- **WHEN** the user pans and zooms the layout
- **THEN** frames SHALL keep the rate required by capability `hefesto-performance`

#### Scenario: Command while running
- **GIVEN** a running background analysis
- **WHEN** a script asks for the engine to `alter` a source value
- **THEN** the adapter SHALL NOT forward the command, and SHALL report `engine busy` to the caller

### Requirement: Callback contract
The adapter SHALL handle each ngspice callback as follows:

| Callback | Hefesto behaviour |
|---|---|
| `SendChar(text)` | If `text` starts with `stderr `, remove that 7-character prefix and record the rest as an error-stream line; otherwise record it as an output-stream line. Each line is appended to the job log. |
| `SendStat(text)` | Parse the progress string as specified in capability `simulation-analyses-and-results`, Requirement "Progress reporting". |
| `SendInitData(info)` | Record the plot name and the ordered list of vector names and real/complex flags of the starting analysis. |
| `SendData(values, count)` | Append one data point per vector to the live stream of the current plot, for live plotting. |
| `BGThreadRunning(flag)` | `false` means the background thread started; `true` means it ended. Drives the job state machine. |
| `ControlledExit(status, immediate, quit)` | Mark the engine instance `faulted`; see Requirement "Cancellation and fault states". |
| `GetVSRCData`, `GetISRCData`, `GetSyncData` | Not registered: `ngSpice_Init_Sync` SHALL NOT be called unless co-simulation is specified by a later capability. |

The live stream SHALL be used only for progress and live plotting. Stored results SHALL come from the final vectors, as specified in capability `simulation-analyses-and-results`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/include/ngspice/sharedspice.h:240-328, src/sharedspice.c:1698-1718, src/sharedspice.c:1808-1818) (See Q-SIM-15.)

#### Scenario: Error stream classification
- **WHEN** `SendChar` delivers `stderr Error: unknown subckt: xinv`
- **THEN** the job log SHALL contain the error-stream line `Error: unknown subckt: xinv`

#### Scenario: Thread state
- **WHEN** `BGThreadRunning(false)` and later `BGThreadRunning(true)` are received
- **THEN** the job SHALL move from `loading` to `running` and then out of `running`

### Requirement: Cancellation and fault states
Each engine instance SHALL have one of these states: `idle`, `loaded`, `running`, `paused`, `faulted`, `unavailable`.

- **Cancel.** The adapter SHALL send `bg_halt` and wait at most 2 s for `BGThreadRunning(true)`. If the thread ends, the job becomes `cancelled`, the data computed so far is kept as a partial result, and the instance returns to `idle` after `remcirc`. If the thread does not end in time, the instance becomes `faulted`.
- **Fault.** On a `ControlledExit` callback, or when cancellation times out, the instance becomes `faulted`. The running job becomes `failed`, with the last 50 error-stream lines as its diagnostic.
- **Recovery on Linux and macOS.** A faulted instance is never reused: its worker process SHALL be replaced as Requirement "Crash containment on desktop" specifies, and the next job SHALL run in the new worker. `ngSpice_Reset` SHALL NOT be called.
- **Recovery on iPadOS.** Before the next job, the adapter SHALL call `ngSpice_Reset`, repeat initialisation (Requirement "Deterministic engine initialisation") and return to `idle`. If `ngSpice_Reset` returns non-zero, or a second fault happens before any job completes, the instance becomes `unavailable`, and simulation stays unavailable until the app restarts (Requirement "In-process execution on iPadOS").

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/sharedspice.c:547-590, src/sharedspice.c:1471-1477, src/sharedspice.c:2132-2188) (See Q-SIM-11, D-25.)

#### Scenario: Cancel a transient run
- **GIVEN** a transient run from 0 to 1 ms that has reached 300 µs
- **WHEN** the user cancels it
- **THEN** the job SHALL end as `cancelled`, and its partial result SHALL hold the time points up to about 300 µs

#### Scenario: Cancel a paused job
- **GIVEN** a transient run that the user paused, so its background thread has ended
- **WHEN** the user cancels it
- **THEN** the job SHALL end as `cancelled` at once, with the partial result of the pause, and its engine SHALL take the next job

#### Scenario: Pause as the run ends
- **GIVEN** a run whose background thread ends just before the user's pause reaches the engine
- **WHEN** the engine refuses the pause because no analysis is running
- **THEN** the refusal SHALL be reported to the user, the job SHALL end as it would have without the pause, and the engine SHALL NOT be treated as faulted

#### Scenario: Unrecoverable error
- **GIVEN** a deck that makes ngspice call its controlled-exit path
- **WHEN** the job runs on Linux or macOS
- **THEN** the job SHALL be `failed` with the error lines as diagnostic, and the next job SHALL run in a new worker process, with no engine reset

#### Scenario: Reset on iPadOS
- **GIVEN** the same deck on iPadOS
- **WHEN** the job runs
- **THEN** the job SHALL be `failed` with the error lines as diagnostic, and the next job SHALL run after `ngSpice_Reset` and repeated initialisation

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

`B_total` is half the physical memory, and `B_job` is the job's result memory budget (capability `simulation-analyses-and-results`) plus an estimate of its prepared deck size. A user setting MAY lower `N` and SHALL NOT raise it. Degenerate inputs SHALL still give a pool of at least one worker: a user setting of 0 SHALL count as 1, a `B_job` of 0 (no estimate) SHALL drop the memory term from the minimum, and an unknown core count (0) SHALL count as one core. On iPadOS, jobs SHALL run one at a time in submission order. A job's results SHALL NOT depend on the number of workers, on which worker ran it, or on the order in which jobs finish. Sweeps, corners and Monte Carlo runs share this pool (capabilities `simulation-sweeps`, `simulation-corners`, `simulation-monte-carlo`).

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
- **THEN** the setting SHALL be refused with `SIM-ENGINE-033`, whose message gives the maximum 3, and at most 3 jobs SHALL run at once

#### Scenario: Large post-layout deck limits the pool
- **GIVEN** a Linux host with 10 physical cores and 16 GiB of memory, and a 4-point sweep of a post-layout deck whose jobs each have the default 4 GiB result budget plus a 300 MiB prepared deck
- **WHEN** the sweep is started
- **THEN** `N` SHALL be 1 (half of 16 GiB divided by a `B_job` above 4 GiB rounds down to 1), the points SHALL run one at a time, and the results SHALL equal those of the same sweep with `N` set to 1

#### Scenario: Worker setting of zero
- **GIVEN** a host on which the formula gives `N = 3`
- **WHEN** the user setting is 0
- **THEN** `N` SHALL be 1, and the jobs SHALL run one at a time and complete

#### Scenario: Job with no memory estimate
- **GIVEN** an 8-physical-core host with 32 GB of memory and a job whose `B_job` is 0
- **WHEN** the pool size is computed
- **THEN** `N` SHALL be 4 (the memory term is dropped, `min(4, 7)`), with no division by zero

### Requirement: No runtime loading of executable code on iPadOS
On iPadOS, the engine SHALL NOT load executable code at runtime. Every OSDI model and XSPICE code model SHALL be compiled into the app at build time and registered statically. The ngspice commands that would load code (`osdi`, `pre_osdi`, `codemodel`) SHALL be resolved against the built-in registry, as Requirement "OSDI compact models" specifies, and SHALL NEVER reach a dynamic loader. The ngspice `shell` command and `.control`-section execution of host programs SHALL be rejected on every platform; see capability `simulation-netlist-preparation`, Requirement "Deck validation and sanitisation".

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/osdi/osdiregistry.c:31, src/spicelib/devices/dev.c:290-420) (See Q-SIM-2, Q-SIM-3.)

#### Scenario: User-supplied OSDI file on iPad
- **GIVEN** a deck containing `pre_osdi /path/mymodel.osdi`, where `mymodel` is not a built-in module
- **WHEN** it is run on iPad
- **THEN** the job SHALL fail before loading with `OSDI model 'mymodel' is not built into this app`, and no file SHALL be opened as code

### Requirement: OSDI compact models
Hefesto SHALL ship a built-in set of OSDI compact models, compiled from Verilog-A at Hefesto build time with a pinned OpenVAF-reloaded compiler (OSDI descriptor version 0.4). The set SHALL include at least the modules the supported PDKs need: for IHP sg13g2, the six modules its `.spiceinit` loads: PSP 103 (`psp103`, `psp103_nqs`), `r3_cmc`, `mosvar`, `cap_cmomi` and `cap_cmomf`, plus any HICUM module the PDK's model files instantiate. The adapter SHALL resolve `pre_osdi <path>` and `osdi <path>` by the file's basename without the `.osdi` extension:
- **Built-in name.** The built-in module is used on every platform, so results do not depend on any `.osdi` files installed on the host.
- **Not built in, on Linux or macOS.** The file MAY be loaded dynamically if the user setting "allow external OSDI models" is on. The default is off, and then the job fails with `OSDI model '<name>' is not built into this app`.
- **Not built in, on iPadOS.** See Requirement "No runtime loading of executable code on iPadOS".

Hefesto SHALL NOT compile Verilog-A at run time on any platform.

**Static registration.** Each built-in module SHALL be linked into the engine library and registered in the engine's static OSDI table, on every platform. Because every OpenVAF module exports the same OSDI symbol names (`OSDI_DESCRIPTORS`, `OSDI_NUM_DESCRIPTORS`, `OSDI_VERSION_MAJOR`, `OSDI_VERSION_MINOR`, `OSDI_DESCRIPTOR_SIZE`), the build SHALL rename each module's symbols with a prefix generated from the module name, and the table SHALL map the module's basename to its prefixed symbols. A built-in module SHALL NOT be loaded from a `.osdi` file at run time (Q-SIM-2, amendment of 2026-11-27).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/osdi/osdiregistry.c:339-451, src/frontend/commands.c:288-293) (See Q-SIM-2, Q-SIM-8, Q-SIMADV-5.)

#### Scenario: sg13g2 PSP model on iPad
- **GIVEN** the sg13g2 PDK descriptor requires `psp103_nqs.osdi`
- **WHEN** an sg13g2 inverter is simulated on iPad
- **THEN** the built-in `psp103_nqs` module SHALL be used and the simulation SHALL complete

#### Scenario: Version mismatch avoided
- **GIVEN** a desktop host whose PDK directory holds a `psp103_nqs.osdi` built with OSDI 0.3
- **WHEN** an sg13g2 deck is simulated
- **THEN** the built-in module SHALL be used and the host file SHALL be ignored

#### Scenario: Statically registered modules with per-module symbol prefixes
- **GIVEN** an engine built with the built-in modules `psp103` and `psp103_nqs`
- **WHEN** its static OSDI table and exported symbols are listed
- **THEN** each module SHALL appear once under its basename with its own prefixed OSDI symbols, no two modules SHALL share a symbol name, and `pre_osdi psp103_nqs.osdi` SHALL resolve to the built-in module without opening a file

### Requirement: Cross-platform result consistency
For the same deck, corner and Hefesto version:
- Results on one machine SHALL be bit-identical across runs.
- Results on two supported platforms SHALL agree within these tolerances, compared at the points of the reference platform's scale (other platforms' values are linearly interpolated):
  - node voltages: `|a − b| ≤ 1e-3 × max(|a|, |b|) + 1e-4 V`
  - branch currents: `|a − b| ≤ 1e-3 × max(|a|, |b|) + 1e-9 A`
  - AC complex values: `|a − b| ≤ 1e-3 × max(|a|, |b|) + 1e-12` of the vector's unit
  - a NaN or infinite value SHALL NOT agree with a finite value; two non-finite values agree only when equal (the same infinity; NaN agrees with nothing)

The reference platform for golden comparisons SHALL be Linux x86_64. Compiler flags that change floating-point semantics (`-ffast-math`, `-Ofast`) SHALL NOT be used for ngspice on any platform.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-10.)

#### Scenario: Divider on iPad and Linux (verify against ngspice)
- **GIVEN** the deck `V1 in 0 1.8`, `R1 in out 1k`, `R2 out 0 2k`, `.op`
- **WHEN** it is run on Linux x86_64 and on iPad
- **THEN** both SHALL report `v(out)` = 1.2 V within the voltage tolerance

#### Scenario: A diverged value does not agree
- **GIVEN** a reference `v(out)` of 1.8 V at a time point
- **WHEN** another platform reports NaN or infinity for `v(out)` at that point
- **THEN** the results SHALL NOT agree

### Requirement: ngspice host mechanics (informative)
The specification SHALL record these ngspice mechanics for reference. They explain the requirements above, and Hefesto MAY rely on them only through those requirements:
- **Global state.** All state is process-global, and there is a single background thread (`fl_running`/`fl_exited` flags), so only one simulation runs per loaded library image. ngspice's own way to get parallel instances is to load separately named copies of the shared library.
- **Error recovery.** Unrecoverable errors use `setjmp`/`longjmp` back to `ngSpice_Command` or `ngSpice_Circ`, after the `ControlledExit` callback. A background-thread error ends the thread with `pthread_exit`.
- **Stopping a run.** `bg_halt` sets the interrupt flag and polls 100 times at 10 ms intervals for the thread to end.
- **SIGSEGV handler.** `ngSpice_Init` installs a temporary SIGSEGV handler while it reads `.spiceinit`, unless `nosighandling` is set, and restores the previous handler afterwards.
- **`$VAR` in file names.** `.include` and `.lib` file names that start with `$VAR` are expanded with `getenv`.
- **Caller detection.** `ngSpice_Init` inspects the calling process name, and turns on KiCad compatibility when that name starts with `eeschema`.
- **Status throttling.** Status strings are throttled to one per 150 ms per thread. At most two thread identities are tracked for progress.

Compatibility: informative

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/sharedspice.c:24-40, src/sharedspice.c:547-590, src/sharedspice.c:914-1052, src/sharedspice.c:1953-2108, src/sharedspice.c:2132-2188, src/frontend/inpcom.c:2136-2290) (See Q-SIM-11, Q-SIM-14.)

#### Scenario: Process name heuristic neutralised
- **GIVEN** Hefesto's process is named `hefesto-simd`
- **WHEN** the engine initialises
- **THEN** KiCad compatibility SHALL NOT be enabled, because the name does not start with `eeschema`

### Requirement: Worker channel limits
The core SHALL enforce these limits on every worker channel:
- one frame at most 64 MiB, with larger vectors sent as several chunks;
- the worker's version handshake within 10 s of start;
- a heartbeat from a running worker at least every 2 s, with the worker declared stuck after 10 s without one.

The worker SHALL send its heartbeat from a thread that does not call the engine and is not blocked by it, so that a long deck load or analysis does not stop the heartbeat. A stuck analysis is detected by cancellation (Requirement "Cancellation and fault states"), not by the heartbeat.

Each limit SHALL be a setting that can be lowered. A violation SHALL be handled as a fault (Requirement "Crash containment on desktop"). The core SHALL parse every frame defensively: no frame content, however malformed, SHALL crash the core or make it read outside the frame.

A worker SHALL NOT be able to grow the core's memory or hold the core's thread by the number of frames it sends: the events queued between two applications on the owning thread SHALL be bounded, and one application SHALL handle a bounded number of event batches and return, leaving later events for the next one.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-21, Q-SIM-25.)

#### Scenario: Flooding worker
- **GIVEN** a worker that keeps sending state frames while its job runs
- **WHEN** the core applies the queued events
- **THEN** the application SHALL return while the worker is still sending, and the event queue SHALL stay within its bound

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
