# m9-lvs-simulation

M9 is "LVS, simulation and advanced analog simulation", which leads to the Desktop Beta release. It has 17 capabilities with 184 requirements in total: 87 for LVS and 97 for simulation.

- LVS capabilities: `lvs-netlist-input`, `lvs-setup-file`, `lvs-comparison-algorithm`, `lvs-device-reduction`, `lvs-property-matching`, `lvs-hierarchy`, `lvs-reports`, `lvs-command`.
- Simulation capabilities: `simulation-engine-embedding`, `simulation-netlist-preparation`, `simulation-analyses-and-results`, `simulation-ui-and-crossprobe`, `simulation-command`, `simulation-sweeps`, `simulation-corners`, `simulation-monte-carlo`, `simulation-measurements-and-statistics`.

The plan covers:
- **LVS.** A built-in, clean-room LVS that is compatible with netgen 1.5.327 under `netgen-compat`. It writes `comp.out` and JSON reports and cross-probes from the report to the layout and the schematic.
- **Simulation engine.** ngspice runs in the worker executable `hefesto-simd` on desktop. That worker is the licence and crash boundary. The core talks to it through a new platform worker service.
- **Simulation front end.** Deck preparation from layout, schematic and raw sources; analyses, results, raw and CSV files; the waveform viewer; probing from layout and schematic; and the `sim` command.
- **Advanced analog simulation.** Sweeps, corners, Monte Carlo and mismatch with portable seeds, and measurements, specs, yield and statistics.
- **Work deferred from M8.** Running schematic testbenches, operating-point annotation, embedded graphs, waveform and layout cross-probing, schematic-driven LVS and raw interchange with xschem. This includes SCHEM-SIM-01/02 and, if Q-SCHEM-39 is approved, SCHEM-LVS-01.
- **Golden cases.** The 77 LVS, 53 SIM and 40 SIMADV cases (5 SIMADV cases are M11), captured from netgen and from a patched reference ngspice.
- **Exit.** The Desktop Beta flow on the three PDKs, and the M9 exit check.

The plan depends on the M7 entry points (`extract::ensure_extracted`, `netlist::spice::render`, `extresist::run`; m7-extraction design D18) and on the M8 netlister and deck builder (`schem::netlist`, `schem::build_deck`; m8-schematic design D13).

**Status: plan drafted 2026-11-22. No decision is approved yet.**
- `check_milestone_readiness.py --milestone M9` reports 34 blocking entries.
- Audit decisions D-22 to D-28 are open.
- This plan proposes 18 new entries: Q-LVS-30 to -36, Q-SIM-20 to -28, Q-SIMADV-13 and Q-SCHEM-39.
- Design Open Questions 1–4 are also open.

All of these, each with a recommended option, are in `docs/decisions/m9-decision-table.md`. Nothing in sections 2–20 of `tasks.md` starts before gate G0 (design D1).
