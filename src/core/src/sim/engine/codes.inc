// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SIM-ENGINE-* of the sim area (simulation-engine-embedding), included by
// src/core/src/sim/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_SIM_CODE("SIM-ENGINE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/sim.md with `hefesto-cli --diagnostic-codes=sim`
// and describe the codes in its SIMENG section.
//
// Owner: SIMENG (openspec/changes/m9-lvs-simulation/design.md D3).
HEFESTO_SIM_CODE("SIM-ENGINE-030", error, "Simulation engine terminated: the job failed and the worker is replaced.")
HEFESTO_SIM_CODE("SIM-ENGINE-031", error, "Simulation engine unavailable: no engine could be started.")
HEFESTO_SIM_CODE("SIM-ENGINE-032", error, "An error-stream line of a failed job.")
HEFESTO_SIM_CODE("SIM-ENGINE-033", error, "Worker count above the pool maximum of this host refused.")
HEFESTO_SIM_CODE("SIM-ENGINE-034", error, "No job with this id.")
HEFESTO_SIM_CODE("SIM-ENGINE-035", error, "Job not in the state the command needs.")
HEFESTO_SIM_CODE("SIM-ENGINE-036", error, "Run-set point job id already in use.")
