// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-CMD-* of the lvs area (lvs-command), included by
// src/core/src/lvs/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_LVS_CODE("LVS-CMD-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/lvs.md with `hefesto-cli --diagnostic-codes=lvs`
// and describe the codes in its LVSCMD section.
//
// Owner: LVSCMD (openspec/changes/m9-lvs-simulation/design.md D3).
HEFESTO_LVS_CODE("LVS-CMD-001", info, "Console output of the `lvs` command (netgen's messages: reading, logging, `LVS Done.`, ...).")
HEFESTO_LVS_CODE("LVS-CMD-002", error, "`lvs` usage error: wrong number of names, an unknown option, or a `-noflatten` list that cannot be read.")
HEFESTO_LVS_CODE("LVS-CMD-003", error, "A netlist of the `lvs` command cannot be found or read; the run ends as an input error.")
HEFESTO_LVS_CODE("LVS-CMD-004", error, "`Both cells are in the same netlist:  Cannot compare!`")
HEFESTO_LVS_CODE("LVS-CMD-005", error, "`Cannot find cell <cell> in file <file>`")
HEFESTO_LVS_CODE("LVS-CMD-006", error, "Retired (M9 R3): a setup file that cannot be read or evaluated ends the run with its LVS-SETUP-* code (input error, headless status 2).")
HEFESTO_LVS_CODE("LVS-CMD-007", error, "The log file or the JSON file of the `lvs` command could not be written.")
HEFESTO_LVS_CODE("LVS-CMD-008", error, "An LVS run is already in progress for this document.")
HEFESTO_LVS_CODE("LVS-CMD-009", warning, "Retired (M9 R3): a setup file that does not exist ends the run with LVS-SETUP-002 (lvs-setup-file \"Setup file evaluation\").")
HEFESTO_LVS_CODE("LVS-CMD-011", error, "The result object has no layout counterpart in this run (`no layout counterpart`).")
HEFESTO_LVS_CODE("LVS-CMD-010", error, "A `-noflatten` entry is longer than 4096 bytes, or the list has more than 65536 entries (crafted-input limit, both netgen-compat modes).")
