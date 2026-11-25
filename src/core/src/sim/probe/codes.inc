// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SIM-PROBE-* of the sim area (simulation-ui-and-crossprobe), included by
// src/core/src/sim/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_SIM_CODE("SIM-PROBE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/sim.md with `hefesto-cli --diagnostic-codes=sim`
// and describe the codes in its SIMPROBE section.
//
// Owner: SIMPROBE (openspec/changes/m9-lvs-simulation/design.md D3).
HEFESTO_SIM_CODE("SIM-PROBE-001", error, "Net is not in the netlist of this run.")
HEFESTO_SIM_CODE("SIM-PROBE-002", error, "Node naming needs a root cell, a technology and an extraction style.")
HEFESTO_SIM_CODE("SIM-PROBE-003", error, "Instance of the probe's use path not found.")
HEFESTO_SIM_CODE("SIM-PROBE-004", error, "No paint at the probe point.")
