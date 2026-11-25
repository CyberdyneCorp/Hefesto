## ADDED Requirements

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
