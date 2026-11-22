## ADDED Requirements

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
