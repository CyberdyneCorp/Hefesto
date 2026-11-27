# Hefesto Performance

## Purpose
Sets measurable performance targets for interactive editing, rendering and batch operations. Each target has a reference workload, so CI benchmarks can catch regressions. The numbers are initial targets and will be recalibrated once the first benchmarks run.

## Requirements

### Requirement: Interactive frame rate
The 2D view SHALL sustain at least 60 frames per second during pan and zoom of the reference large layout (a full-chip sky130 GDS with at least 10 million rectangles, read as a read-only view). The reference devices are:
- an Apple M1 Mac
- an M1 iPad Pro
- a Linux x86_64 machine with a mid-range discrete GPU

The 3D view SHALL sustain at least 30 frames per second for a meshed region of up to 1 million rectangles.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-3)

#### Scenario: Pan large chip on iPad
- **GIVEN** the reference full-chip layout is open on an M1 iPad Pro
- **WHEN** the user pans continuously for 10 seconds
- **THEN** the median frame time SHALL be at most 16.7 ms

### Requirement: Edit latency
A paint, erase, move or copy command affecting up to 10,000 rectangles SHALL update the view within 50 ms. Background DRC SHALL start on the affected area within 100 ms of the edit.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-1)

#### Scenario: Paint response
- **WHEN** a 100×100 µm box is painted in a cell of 1 million rectangles
- **THEN** the screen SHALL show the change within 50 ms

### Requirement: Batch throughput relative to Magic
On the same reference workloads, single-threaded Hefesto SHALL be no slower than Magic 8.3.694 for:
- full-hierarchy DRC
- GDS read and write
- extraction
- SPICE generation

How the target is judged:
- **Full-hierarchy DRC, GDS read and GDS write: instruction counts.** The measure is the callgrind instruction count of each reference workload on the x86_64 CI runner, counting the operation alone: a script with the operation minus the same script without it, or one pass minus the set-up. Instruction counts are stable across runner types, while the wall-clock ratio moves by about 0.1 ×.
  - GDS read and GDS write: each workload SHALL take at most Magic's instruction count, measured on the same runner. Once Hefesto's count for a workload is recorded as its baseline, the workload SHALL take at most that baseline + 2 %.
  - Full-hierarchy DRC: one pass of the reference block SHALL take at most the recorded baseline + 2 %. The baseline is 12.14 billion instructions, the M3 count, so the limit is 12.38 billion. Magic's count is reported for information.
  - The wall time of both tools on the same runner is reported for information. It does not gate.
- **Extraction and SPICE generation: instruction counts.** Each reference workload, counting the operation alone as above, SHALL take at most Magic's callgrind instruction count on the same x86_64 runner. Once Hefesto's count for a workload is recorded as its baseline, the workload SHALL take at most that baseline + 2 %. Wall time is reported for information. The workloads are `extract all` of the reference all-cells block of `sky130_fd_sc_hd`, `ext2spice` flat with `lvs`, `ext2spice hierarchy on`, and `ext2sim` on the same block (m7-extraction design D23; user decision 2026-11-21, m7-extraction design Open Question 5 (a)).

Multithreaded runs SHOULD be faster. CI SHALL track these benchmarks.

Compatibility: native

(Hefesto decisions: docs/decisions/product.md#P-1, docs/decisions/product.md#P-24)

#### Scenario: GDS write benchmark
- **GIVEN** the reference all-cells block of `sky130_fd_sc_hd` with no recorded Hefesto baseline for the workload
- **WHEN** `gds write` of the block is run under callgrind in Hefesto and in Magic on the same x86_64 runner
- **THEN** Hefesto's instruction count for the write SHALL be less than or equal to Magic's
- **AND** the wall times of both tools SHALL be reported without gating

#### Scenario: GDS read benchmark after the baseline is recorded
- **GIVEN** a recorded Hefesto baseline of B instructions for `gds read` of `sky130_fd_sc_hd.gds`
- **WHEN** the workload is run under callgrind on the x86_64 runner
- **THEN** Hefesto's instruction count SHALL be at most B + 2 %, whatever Magic's count is

#### Scenario: Extraction benchmark
- **GIVEN** the reference all-cells block of `sky130_fd_sc_hd` with no recorded Hefesto baseline for the workload
- **WHEN** `extract all` of the block is run under callgrind in Hefesto and in Magic, single-threaded, on the same x86_64 runner
- **THEN** Hefesto's instruction count for the extraction SHALL be less than or equal to Magic's
- **AND** the wall times of both tools SHALL be reported without gating

#### Scenario: SPICE generation benchmark after the baseline is recorded
- **GIVEN** a recorded Hefesto baseline of B instructions for `ext2spice hierarchy on` of the reference block
- **WHEN** the workload is run under callgrind on the x86_64 runner
- **THEN** Hefesto's instruction count SHALL be at most B + 2 %, whatever Magic's count is

### Requirement: Memory bound on iPad
On iPadOS, Hefesto SHALL open the reference full-chip layout as a read-only view without exceeding 3 GB resident memory. When the system warns of memory pressure, Hefesto SHALL release GPU caches and 3D meshes before failing.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-2)

#### Scenario: Memory warning
- **WHEN** iPadOS sends a memory warning while a large layout and a 3D view are open
- **THEN** Hefesto SHALL free cached 3D meshes and continue running

### Requirement: LVS and simulation throughput
LVS and simulation SHALL meet these gates, tracked in CI.

**LVS.** These are callgrind instruction counts on the x86_64 runner, counting the comparison alone. The comparison's count is isolated by subtraction, for Hefesto and for netgen alike: the count of a script that reads both netlists, evaluates the setup file and compares them, minus the count of the same script without the comparison command. No per-function filter on either tool's internal names SHALL be used. Each workload SHALL take at most netgen 1.5.327's count for the same comparison on the same runner. Once Hefesto's count for a workload is recorded as its baseline, the workload SHALL take at most that baseline + 2 %. netgen runs only as a subprocess in the benchmark job. The workloads are:
- the `sky130_fd_sc_hd` library compared cell by cell against its schematic netlists;
- a 50 000-device flat block;
- a hierarchical block of 1 000 000 devices.

**Simulation.** Two gates:
- Hefesto's overhead is the job's wall time minus ngspice's own analysis time, as ngspice's `rusage` command reports it after each analysis (not the worker process's resource usage, which includes Hefesto's adapter and transfer time). It SHALL be at most 10 % of ngspice's time on the sky130 inverter transient deck, a 1 000-device post-layout `rc` deck and a 1 000-point sweep. The sweep's time SHALL include the worker reuse steps between points.
- A 1 000-sample Monte Carlo run SHALL complete at least 0.6 × `w` times faster with `w` workers than with one, for every `w` from 2 up to `min(4, physical_cores − 1)` of the runner that measures it.

Wall times SHALL be reported for information.

Compatibility: native

(Hefesto decisions: docs/decisions/product.md#P-5, docs/decisions/product.md#P-7, docs/decisions/product.md#P-19) (See Q-LVS-35, Q-SIM-28.)

#### Scenario: LVS benchmark before a baseline
- **GIVEN** the 50 000-device flat block with no recorded Hefesto baseline
- **WHEN** the comparison is run under callgrind in Hefesto and in netgen on the same x86_64 runner
- **THEN** Hefesto's instruction count SHALL be less than or equal to netgen's

#### Scenario: Simulation overhead
- **WHEN** the 1 000-device `rc` deck is run in `sim-bench`
- **THEN** the job's wall time minus ngspice's analysis time SHALL be at most 10 % of ngspice's analysis time

### Requirement: Plot and maze-route throughput
Plotting and maze routing SHALL meet these gates, tracked in CI (`perf.yml` jobs `plot-bench` and `route-bench`):
- **Plot.** On the Linux x86_64 runner, `plot postscript` and `plot pnm <file> 4000` of the `block` workload of `bench/drc/drc_bench.py` (every `sky130_fd_sc_hd` cell placed once, in rows) SHALL each take at most Magic 8.3.694's callgrind instruction count for the same command, measured by subtraction (the script with the plot minus the same script without it). Once Hefesto's count for a command is recorded as its baseline, the command SHALL take at most that baseline + 2 %. Peak memory of `plot pnm` SHALL stay within `pnmmaxmem` plus 64 MiB.
- **Maze route.** On the same runner, each routed LEG-MZ golden case and the 20-net `mazeroute` workload on scmos (`bench/route/`) SHALL take at most Magic's callgrind instruction count for the same commands, measured by subtraction. Once Hefesto's count for a workload is recorded as its baseline, the workload SHALL take at most that baseline + 2 %.
- **Agent route.** The `agent-route` workload of `bench/route/` is one 2-terminal route between two labels that `bench/route/` places 200 µm apart across the rows of the same `block` workload, with the derived `irouter` style (capability `router-maze-iroute`, "Derived irouter style") and `magic-compat` OFF. It SHALL finish in at most 2 s on the Apple M3 Pro development machine and SHALL complete with its search memory under 256 MiB (the iPadOS default of `router.max-search-memory`), so that it never ends on the memory limit on iPadOS. The same workload SHALL be measured on the Apple M1 Mac and on an iPad when those reference runs happen; their times are recorded, not gated, until the reference hardware harness exists.

The Plot, Maze route and Agent route gates block the milestone exit. Wall times are reported for information.

Compatibility: native

(Hefesto decisions: docs/decisions/product.md#P-13, docs/decisions/product.md#P-19, docs/decisions/product.md#P-24)

#### Scenario: PostScript plot against Magic
- **GIVEN** the `block` workload and no recorded Hefesto baseline
- **WHEN** `plot postscript` runs under callgrind in Hefesto and in Magic on the same x86_64 runner
- **THEN** Hefesto's instruction count SHALL be at most Magic's

#### Scenario: Maze route after the baseline is recorded
- **GIVEN** a recorded Hefesto baseline for the 20-net `mazeroute` workload
- **WHEN** the workload runs under callgrind on the x86_64 runner
- **THEN** its instruction count SHALL be at most the baseline + 2 %

#### Scenario: Agent route time and memory
- **WHEN** the `agent-route` workload runs on the M3 Pro with `magic-compat` OFF
- **THEN** it SHALL finish within 2 s, and its peak search memory SHALL be under 256 MiB
