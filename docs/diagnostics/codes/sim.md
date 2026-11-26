# Diagnostic codes: sim area

Codes `SIM-<TOPIC>-<NNN>` emitted by `src/core/src/sim/` (simulation: the
engine interface and job manager, deck preparation, results, probes, the
`sim` command, run sets and measurements;
`openspec/changes/m9-lvs-simulation/design.md` D3). Conventions and the
renderings are described in [../codes.md](../codes.md).

The area's table is assembled by `src/core/src/sim/codes.cpp` (owner:
SCAFFOLD) from one fragment per subdirectory, each owned by its workstream.
The rows are sorted while compiling (`src/core/src/diag/sorted_codes.hpp`):

| Fragment | Topics | Owner |
|---|---|---|
| `src/core/src/sim/engine/codes.inc` | `SIM-ENGINE-*` | SIMENG |
| `src/core/src/sim/deck/codes.inc` | `SIM-DECK-*` | SIMDECK |
| `src/core/src/sim/result/codes.inc` | `SIM-RESULT-*` | SIMRES |
| `src/core/src/sim/probe/codes.inc` | `SIM-PROBE-*` | SIMPROBE |
| `src/core/src/sim/cmd/codes.inc` | `SIM-CMD-*` | SIMCMD |
| `src/core/src/sim/runset/codes.inc` | `SIM-RUNSET-*` | SIMADV |
| `src/core/src/sim/meas/codes.inc` | `SIM-MEAS-*` | SIMMEAS |

The M9 scaffold (tasks.md 4.1, 4.2) declared no code. While M9 is in
progress, a function the scaffold declared before its workstream implemented
it fails with the core code `CORE-NOTIMPL-009` ([../codes.md](../codes.md));
INTEGRATION retires it at M9 exit.

Each resource cap of design D18 (prepared deck, deck include depth, IPC
frame, worker start-up and heartbeat, result memory, points per run set,
Monte Carlo samples, raw import) gets its own code from the workstream that
enforces it.

## SIMENG

Codes of the engine interface, the worker client, the job manager and the
worker pool (`sim/engine/`; capability simulation-engine-embedding).

`SIM-ENGINE-001` to `-029` belong to the worker client of SIMD
(`sim/engine/worker_engine.cpp`, task 5.4) and the developer-build
in-process host (`src/simd/devhost/`); `-030` to `-059` to SIMENG (the job
manager and the pool, task 5.5).

- `SIM-ENGINE-001`: `make_worker_engine` got no worker host, or the
  developer host could not load the plugin or create the engine (one per
  process). A host that cannot create processes reports the platform's
  `PLATFORM-UNSUPPORTED-001` instead.
- `SIM-ENGINE-002` to `-008`: the faults of "Crash containment on desktop"
  and "Worker channel limits". The worker is terminated, `on_state` reports
  `failed`, and every later call fails with the same diagnostic, whose text
  starts with `simulation engine terminated` and ends with the worker's last
  error-stream lines (at most 50). `-002` no handshake within 10 s, `-003`
  another protocol version, `-004` a malformed frame (also a second hello, a
  frame of the core's direction, an inconsistent vector chunk or count),
  `-005` a frame above the 64 MiB limit, `-006` an unknown frame kind,
  `-007` no heartbeat for 10 s, `-008` the worker exited or crashed or its
  engine faulted (controlled exit, cancellation timeout).
- `SIM-ENGINE-009`, `-010`: the engine rejected the deck or a command; the
  text is the engine's message. The engine stays healthy: the job manager
  fails a job whose deck was rejected (with its error lines) and keeps the
  worker, and returns a rejected pause or resume to the caller without
  changing the job.
- `SIM-ENGINE-011`: `engine busy`, a command other than a halt while the
  background analysis runs ("Engine threading model").
- `SIM-ENGINE-012`: `vectors()` before any run has ended.
- `SIM-ENGINE-013`: the worker sent more result vectors before `VectorsDone`
  than the result budget (4 GiB by default; each vector counts its values
  and 64 bytes); a fault like `-002` to `-008`.
- `SIM-ENGINE-030` `simulation engine terminated`: a fault of the job's
  engine (a crash, a controlled exit, a missed heartbeat or a bad frame,
  reported as `EngineState::failed`, or an error returned by an engine call
  other than `-009` to `-011`). The job fails, the engine is discarded, and
  the next job runs in a new one (D-25 (a)). The cause (the engine's own
  diagnostic, first line) and the job's error lines follow as further
  diagnostics.
- `SIM-ENGINE-031` `simulation engine unavailable: <reason>`: no engine could
  be started for the job (no engine host, or the host failed).
- `SIM-ENGINE-032`: one error-stream line of a failed job (ngspice's
  `stderr ` lines, the last 50; for a completed run only those starting with
  `Error`).
- `SIM-ENGINE-033` `worker count <k> is above the maximum <N> of this host;
  the pool keeps <N> workers`: a worker setting larger than the pool formula
  allows is refused ("Setting cannot raise the pool").
- `SIM-ENGINE-034` `no job <id>`; `SIM-ENGINE-035` `job <id> is not running`
  (or `not paused`) for cancel, pause and resume; `SIM-ENGINE-036` `job <id>
  already exists` for a run-set point id in use.
- `SIM-ENGINE-037` `the run produced no plot`: the run ended without `Error`
  lines but left no result vectors, so the job fails ("Job lifecycle").

M9 R2: no new SIMENG code. A worker that floods state frames is bounded by
coalescing, not by a diagnostic: past `kMaxQueuedStateEvents` (4096) state
events between two polls, a connection's later states collapse into
`running` and its last state (or `failed`), so the job ends as the full
sequence would end it. A job cancelled while it loads ends `cancelled`
without a diagnostic; the factory's failure on the loader thread is still
`SIM-ENGINE-031`.

## SIMDECK

Codes of deck preparation (`sim/deck/`; capability
simulation-netlist-preparation). M9 R2 (task 13.1): the `spice` and `raw`
sources.

- `SIM-DECK-001` `File not found: <path> (<file>:<line>)`: an `.include`
  that does not resolve (relative paths against the including file's
  directory, the deck's against the testbench folder), or one that names no
  file.
- `SIM-DECK-002` `include depth limit <n> exceeded by <path> (<file>:<line>)`
  ("Deck limits": 32 by default, lowered by `DeckLimits`); an include loop
  ends here. For a `spice` source the DUT file is the first level.
- `SIM-DECK-003` `prepared deck larger than the limit of <n> bytes in <file>`
  ("Deck limits": 256 MiB by default).
- `SIM-DECK-004` `Control command '<cmd>' is not allowed in simulation decks`
  (Q-SIM-9): `shell`, `cd`, `codemodel`, `osdi`, `pre_osdi`, `load`,
  `source`, `write`, `wrdata` or `quit` in a `.control` section, also in an
  included file and also with `allowControl` on.
- `SIM-DECK-005` (warning) `Removed .control section (lines <a>-<b>)`.
- `SIM-DECK-006`: a `.subckt` without its `.ends`, or an `.ends` without its
  `.subckt`.
- `SIM-DECK-007` `undefined subcircuit '<name>' called at line <n>`; not
  checked in a deck with `.lib` lines (PDK subcircuits are exempt).
- `SIM-DECK-008` `Deck has no analysis`; `SIM-DECK-009` `Deck does not end
  with .end` ("Raw decks").
- `SIM-DECK-010` `Source <name>: <reason>` ("Stimulus definitions": a
  decreasing PWL, a negative PULSE tr, tf or pw, a missing node, a name not
  starting with V or I, a name used twice, a wrong value count).
- `SIM-DECK-011` `Unknown DUT port <name>`; `SIM-DECK-012` (warning) `Port
  <port> left unconnected` ("Testbench composition").
- `SIM-DECK-013`: the DUT file does not define the testbench's subcircuit,
  or the deck context has no file system.

## SIMRES

Codes of result storage, raw files, CSV export and waveform measurements
(`sim/result/`; capability simulation-analyses-and-results). M9 R2 (task
13.3).

- `SIM-RESULT-001` `<file>:<line>: expected "<token>"`: a malformed raw file
  ("Raw file import").
- `SIM-RESULT-002` `<file>: binary data ends after point <k> of <n>`.
- `SIM-RESULT-003`: a raw file above the import limit (4 GiB), or one whose
  declared plots would take more than the result budget once loaded; refused
  before anything is allocated for them (design D18, Q-SIM-25).
- `SIM-RESULT-004`: a plot without vectors, or with vectors of different
  lengths, cannot be written as raw or CSV.
- `SIM-RESULT-005`: no such vector in the plot.
- `SIM-RESULT-006`: invalid measurement arguments (`value_at` outside the
  scale, an empty range, a missing argument).
- `SIM-RESULT-007` `no crossing of <level> (<dir>, k=<k>)`.
- `SIM-RESULT-008`: an unknown measurement function.
- `SIM-RESULT-009`: a result file cannot be read or written (or the store
  has no file system).
- `SIM-RESULT-010` `<analysis>: <parameter> ...`: an invalid analysis
  ("Supported analyses"), e.g. `ac: fstart must be > 0 for dec sweep`.

## SIMPROBE

Codes of node naming, probes, operating-point annotation and the 3D feed
(`sim/probe/`; capability simulation-ui-and-crossprobe).

- `SIM-PROBE-001` `Net <name> is not in the netlist of this run`: neither
  the net-name map's `layout_name` index nor its points on the selected node
  give an entry ("Probing a layout net", step 3).
- `SIM-PROBE-002`: node naming was called without a root cell, a
  technology or an extraction style.
- `SIM-PROBE-003` `no instance <element> in cell <cell>`: an element of the
  probe's use path does not exist.
- `SIM-PROBE-004` `no paint at (<x>, <y>) in cell <cell>`: the probe point
  lies on no paint.

## SIMCMD

Codes of the `sim` command (`sim/cmd/`; capability simulation-command). None
yet.

## SIMADV

Codes of run sets, sweeps, corners and Monte Carlo (`sim/runset/`;
capabilities simulation-sweeps, simulation-corners, simulation-monte-carlo).
None yet.

## SIMMEAS

Codes of measurements, specifications and statistics (`sim/meas/`;
capability simulation-measurements-and-statistics). None yet.

## Table

The table below is generated from the fragments:

```sh
hefesto-cli --diagnostic-codes=sim
```

The unit test `each area's code table matches its document and prefix`
fails when the two disagree.

<!-- BEGIN DIAGNOSTIC CODES (generated by `hefesto-cli --diagnostic-codes=sim`) -->
| Code | Default severity | Meaning |
|---|---|---|
| `SIM-DECK-001` | error | A file the deck includes cannot be read. |
| `SIM-DECK-002` | error | Deck include depth limit exceeded. |
| `SIM-DECK-003` | error | Prepared deck size limit exceeded. |
| `SIM-DECK-004` | error | A forbidden command in a .control section of a simulation deck. |
| `SIM-DECK-005` | warning | A .control section was removed from the deck. |
| `SIM-DECK-006` | error | A .subckt without its .ends, or an .ends without its .subckt. |
| `SIM-DECK-007` | error | An instance calls an undefined subcircuit. |
| `SIM-DECK-008` | error | Deck has no analysis. |
| `SIM-DECK-009` | error | Raw deck does not end with .end. |
| `SIM-DECK-010` | error | Invalid stimulus source. |
| `SIM-DECK-011` | error | Unknown DUT port in a binding. |
| `SIM-DECK-012` | warning | A DUT port was left unconnected. |
| `SIM-DECK-013` | error | The DUT netlist or the deck context is missing. |
| `SIM-ENGINE-001` | error | No worker host or engine plugin is available on this platform; simulation is unavailable. |
| `SIM-ENGINE-002` | error | Simulation engine terminated: the worker did not complete its version handshake in time. |
| `SIM-ENGINE-003` | error | Simulation engine terminated: the worker speaks another protocol version. |
| `SIM-ENGINE-004` | error | Simulation engine terminated: the worker sent a malformed frame. |
| `SIM-ENGINE-005` | error | Simulation engine terminated: the worker sent a frame larger than the frame limit. |
| `SIM-ENGINE-006` | error | Simulation engine terminated: the worker sent a frame of an unknown kind. |
| `SIM-ENGINE-007` | error | Simulation engine terminated: the worker missed its heartbeat. |
| `SIM-ENGINE-008` | error | Simulation engine terminated: the worker crashed, exited, or its engine faulted. |
| `SIM-ENGINE-009` | error | The simulation engine rejected the deck. |
| `SIM-ENGINE-010` | error | The simulation engine rejected a command. |
| `SIM-ENGINE-011` | error | Engine busy: the command is not accepted while the analysis runs. |
| `SIM-ENGINE-012` | error | No result vectors are available: no run has ended. |
| `SIM-ENGINE-013` | error | Simulation engine terminated: the worker sent more result data than the job's result budget. |
| `SIM-ENGINE-030` | error | Simulation engine terminated: the job failed and the worker is replaced. |
| `SIM-ENGINE-031` | error | Simulation engine unavailable: no engine could be started. |
| `SIM-ENGINE-032` | error | An error-stream line of a failed job. |
| `SIM-ENGINE-033` | error | Worker count above the pool maximum of this host refused. |
| `SIM-ENGINE-034` | error | No job with this id. |
| `SIM-ENGINE-035` | error | Job not in the state the command needs. |
| `SIM-ENGINE-036` | error | Run-set point job id already in use. |
| `SIM-ENGINE-037` | error | The run produced no plot, so the job failed. |
| `SIM-PROBE-001` | error | Net is not in the netlist of this run. |
| `SIM-PROBE-002` | error | Node naming needs a root cell, a technology and an extraction style. |
| `SIM-PROBE-003` | error | Instance of the probe's use path not found. |
| `SIM-PROBE-004` | error | No paint at the probe point. |
| `SIM-RESULT-001` | error | Malformed raw file: a line does not hold the expected token. |
| `SIM-RESULT-002` | error | Truncated raw file: the binary data ends before the declared points. |
| `SIM-RESULT-003` | error | Raw import above the import limit or the result budget. |
| `SIM-RESULT-004` | error | A plot cannot be written: no vectors, or vectors of different lengths. |
| `SIM-RESULT-005` | error | No such vector in the plot. |
| `SIM-RESULT-006` | error | Invalid waveform measurement arguments. |
| `SIM-RESULT-007` | error | No crossing of the level for the measurement. |
| `SIM-RESULT-008` | error | Unknown waveform measurement. |
| `SIM-RESULT-009` | error | A result file cannot be read or written. |
| `SIM-RESULT-010` | error | Invalid analysis parameter. |
<!-- END DIAGNOSTIC CODES -->
