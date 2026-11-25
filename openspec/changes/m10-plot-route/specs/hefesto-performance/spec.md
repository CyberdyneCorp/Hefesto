## ADDED Requirements

### Requirement: Plot and maze-route throughput
Plotting and maze routing SHALL meet these gates, tracked in CI:
- **Plot.** On the Linux x86_64 runner, `plot postscript` and `plot pnm <file> 4000` of the `sky130_fd_sc_hd` all-cells block (the DRC benchmark's block) SHALL each take at most Magic 8.3.694's callgrind instruction count for the same command, measured by subtraction (the script with the plot minus the same script without it). Peak memory of `plot pnm` SHALL stay within `pnmmaxmem` plus 64 MiB.
- **Maze route.** On the same runner, each routed LEG-MZ golden case and the 20-net `mazeroute` workload on scmos SHALL take at most Magic's callgrind instruction count for the same commands, measured by subtraction. Once Hefesto's count for a workload is recorded as its baseline, the workload SHALL take at most that baseline + 2 %.
- **Agent routing.** A 2-terminal route of 200 µm across a sky130A cell of 10 000 rectangles with the derived `irouter` style (capability `router-maze-iroute`, "Derived irouter style") SHALL finish in at most 2 s on the Apple M1 Mac and use at most 256 MiB of search memory.

Wall times SHALL be reported for information.

Compatibility: native

(Hefesto decisions: docs/decisions/product.md#P-13, docs/decisions/product.md#P-19)

#### Scenario: PostScript plot against Magic
- **GIVEN** the `sky130_fd_sc_hd` all-cells block and no recorded Hefesto baseline
- **WHEN** `plot postscript` runs under callgrind in Hefesto and in Magic on the same x86_64 runner
- **THEN** Hefesto's instruction count SHALL be at most Magic's

#### Scenario: Agent route time
- **WHEN** the 200 µm sky130A route runs on the M1 Mac with `magic-compat` OFF
- **THEN** it SHALL finish within 2 s
