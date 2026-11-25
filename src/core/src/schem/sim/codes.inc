// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SCHEM-SIM-* of the schem area (schematic-simulation-integration), included by
// src/core/src/schem/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_SCHEM_CODE("SCHEM-SIM-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/schem.md with `hefesto-cli --diagnostic-codes=schem`
// and describe the codes in its SIMINT section.
//
// Owner: SIMINT (openspec/changes/m8-schematic/design.md D3).
HEFESTO_SCHEM_CODE("SCHEM-SIM-001", error, "A `.control` line of a schematic-source deck names a command that is not allowed in simulation decks (`shell`, `cd`, `load`, `source`, `codemodel`, `osdi`, `pre_osdi`, or one named only at run time).")
HEFESTO_SCHEM_CODE("SCHEM-SIM-002", warning, "A `.include` or `.lib` path of a schematic-source deck names no file in the netlist folder, the schematic's folder or the PDK store; it is kept as written.")
HEFESTO_SCHEM_CODE("SCHEM-SIM-003", warning, "A launcher's `xschem` command (or `simulate`) is not supported or not available until M9; it was skipped and the rest of the launcher ran.")
HEFESTO_SCHEM_CODE("SCHEM-SIM-004", error, "The testbench of a schematic circuit source could not be netlisted, so no simulation deck was built.")
