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

// SIM-ENGINE-001..029: SIMD's worker client (sim/engine/worker_engine.cpp,
// src/simd/devhost); 030..059 belong to SIMENG.
HEFESTO_SIM_CODE("SIM-ENGINE-001", error, "No worker host or engine plugin is available on this platform; simulation is unavailable.")
HEFESTO_SIM_CODE("SIM-ENGINE-002", error, "Simulation engine terminated: the worker did not complete its version handshake in time.")
HEFESTO_SIM_CODE("SIM-ENGINE-003", error, "Simulation engine terminated: the worker speaks another protocol version.")
HEFESTO_SIM_CODE("SIM-ENGINE-004", error, "Simulation engine terminated: the worker sent a malformed frame.")
HEFESTO_SIM_CODE("SIM-ENGINE-005", error, "Simulation engine terminated: the worker sent a frame larger than the frame limit.")
HEFESTO_SIM_CODE("SIM-ENGINE-006", error, "Simulation engine terminated: the worker sent a frame of an unknown kind.")
HEFESTO_SIM_CODE("SIM-ENGINE-007", error, "Simulation engine terminated: the worker missed its heartbeat.")
HEFESTO_SIM_CODE("SIM-ENGINE-008", error, "Simulation engine terminated: the worker crashed, exited, or its engine faulted.")
HEFESTO_SIM_CODE("SIM-ENGINE-009", error, "The simulation engine rejected the deck.")
HEFESTO_SIM_CODE("SIM-ENGINE-010", error, "The simulation engine rejected a command.")
HEFESTO_SIM_CODE("SIM-ENGINE-011", error, "Engine busy: the command is not accepted while the analysis runs.")
HEFESTO_SIM_CODE("SIM-ENGINE-012", error, "No result vectors are available: no run has ended.")
HEFESTO_SIM_CODE("SIM-ENGINE-013", error, "Simulation engine terminated: the worker sent more result data than the job's result budget.")
HEFESTO_SIM_CODE("SIM-ENGINE-030", error, "Simulation engine terminated: the job failed and the worker is replaced.")
HEFESTO_SIM_CODE("SIM-ENGINE-031", error, "Simulation engine unavailable: no engine could be started.")
HEFESTO_SIM_CODE("SIM-ENGINE-032", error, "An error-stream line of a failed job.")
HEFESTO_SIM_CODE("SIM-ENGINE-033", error, "Worker count above the pool maximum of this host refused.")
HEFESTO_SIM_CODE("SIM-ENGINE-034", error, "No job with this id.")
HEFESTO_SIM_CODE("SIM-ENGINE-035", error, "Job not in the state the command needs.")
HEFESTO_SIM_CODE("SIM-ENGINE-036", error, "Run-set point job id already in use.")
HEFESTO_SIM_CODE("SIM-ENGINE-037", error, "The run produced no plot, so the job failed.")
