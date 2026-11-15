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
- **Extraction and SPICE generation: wall time.** Single-threaded wall time on the same machine SHALL be no worse than Magic's.

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
- **WHEN** `extract all` is run on the reference standard-cell block in Hefesto and in Magic on the same machine
- **THEN** Hefesto's single-threaded time SHALL be less than or equal to Magic's

### Requirement: Memory bound on iPad
On iPadOS, Hefesto SHALL open the reference full-chip layout as a read-only view without exceeding 3 GB resident memory. When the system warns of memory pressure, Hefesto SHALL release GPU caches and 3D meshes before failing.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-2)

#### Scenario: Memory warning
- **WHEN** iPadOS sends a memory warning while a large layout and a 3D view are open
- **THEN** Hefesto SHALL free cached 3D meshes and continue running
