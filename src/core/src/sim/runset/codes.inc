// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SIM-RUNSET-* of the sim area (simulation-sweeps, simulation-corners, simulation-monte-carlo), included by
// src/core/src/sim/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_SIM_CODE("SIM-RUNSET-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/sim.md with `hefesto-cli --diagnostic-codes=sim`
// and describe the codes in its SIMADV section.
//
// Owner: SIMADV (openspec/changes/m9-lvs-simulation/design.md D3).

// M9 R3 (tasks 15.1-15.3).
HEFESTO_SIM_CODE("SIM-RUNSET-001", error, "Run set above the point (or level) limit refused before any job is created.")
HEFESTO_SIM_CODE("SIM-RUNSET-002", error, "Run set has no points.")
HEFESTO_SIM_CODE("SIM-RUNSET-003", error, "Invalid sweep definition (the dimension and the parameter are named).")
HEFESTO_SIM_CODE("SIM-RUNSET-004", error, "Invalid sweep variable, duplicate variable, or a variable the corner dimension sets.")
HEFESTO_SIM_CODE("SIM-RUNSET-005", error, "Swept parameter defined more than once at top level.")
HEFESTO_SIM_CODE("SIM-RUNSET-006", error, "Invalid design-of-experiments block.")
HEFESTO_SIM_CODE("SIM-RUNSET-007", error, "Corner table error: unknown corner or set, duplicate corner, or a section the library lacks.")
HEFESTO_SIM_CODE("SIM-RUNSET-008", error, "Unknown section word for a corner variable.")
HEFESTO_SIM_CODE("SIM-RUNSET-009", error, "Monte Carlo mode not defined by the PDK or at the corner.")
HEFESTO_SIM_CODE("SIM-RUNSET-010", error, "Invalid Monte Carlo settings: run count, base seed, stop target or statistical parameter.")
HEFESTO_SIM_CODE("SIM-RUNSET-011", error, "A Monte Carlo deck sets a variable Monte Carlo runs do not support.")
HEFESTO_SIM_CODE("SIM-RUNSET-012", warning, "Seed setting removed from a Monte Carlo deck.")
HEFESTO_SIM_CODE("SIM-RUNSET-013", warning, "Testbench corner overrides the PDK corner of the same name.")
HEFESTO_SIM_CODE("SIM-RUNSET-014", warning, "Monte Carlo mismatch: a PDK device instance has mm_ok=0 and will not vary.")
HEFESTO_SIM_CODE("SIM-RUNSET-015", error, "Run-set limit setting of zero or above twice its default refused.")
HEFESTO_SIM_CODE("SIM-RUNSET-016", error, "Run-set storage cannot be written.")
HEFESTO_SIM_CODE("SIM-RUNSET-017", error, "Mismatch statistical parameter needs a layout or netlist DUT.")
HEFESTO_SIM_CODE("SIM-RUNSET-018", error, "Run set or point not in the state the command needs.")
