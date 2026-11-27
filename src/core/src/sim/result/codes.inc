// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SIM-RESULT-* of the sim area (simulation-analyses-and-results), included by
// src/core/src/sim/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_SIM_CODE("SIM-RESULT-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/sim.md with `hefesto-cli --diagnostic-codes=sim`
// and describe the codes in its SIMRES section.
//
// Owner: SIMRES (openspec/changes/m9-lvs-simulation/design.md D3).

// M9 R2 (task 13.3): raw files, CSV export and waveform measurements.
HEFESTO_SIM_CODE("SIM-RESULT-001", error, "Malformed raw file: a line does not hold the expected token.")
HEFESTO_SIM_CODE("SIM-RESULT-002", error, "Truncated raw file: the binary data ends before the declared points.")
HEFESTO_SIM_CODE("SIM-RESULT-003", error, "Raw import above the import limit or the result budget.")
HEFESTO_SIM_CODE("SIM-RESULT-004", error, "A plot cannot be written: no vectors, or vectors of different lengths.")
HEFESTO_SIM_CODE("SIM-RESULT-005", error, "No such vector in the plot.")
HEFESTO_SIM_CODE("SIM-RESULT-006", error, "Invalid waveform measurement arguments.")
HEFESTO_SIM_CODE("SIM-RESULT-007", error, "No crossing of the level for the measurement.")
HEFESTO_SIM_CODE("SIM-RESULT-008", error, "Unknown waveform measurement.")
HEFESTO_SIM_CODE("SIM-RESULT-009", error, "A result file cannot be read or written.")
HEFESTO_SIM_CODE("SIM-RESULT-010", error, "Invalid analysis parameter.")
// M9 R3 (Q-SIM-16, approved 2026-11-27): the streaming result budget.
HEFESTO_SIM_CODE("SIM-RESULT-011", error, "Result memory budget exceeded: the job failed and keeps its partial result.")
