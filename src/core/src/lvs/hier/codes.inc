// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-HIER-* of the lvs area (lvs-hierarchy), included by
// src/core/src/lvs/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_LVS_CODE("LVS-HIER-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/lvs.md with `hefesto-cli --diagnostic-codes=lvs`
// and describe the codes in its LVSHIER section.
//
// Owner: LVSHIER (openspec/changes/m9-lvs-simulation/design.md D3).
HEFESTO_LVS_CODE("LVS-HIER-001", error, "The hierarchy below a compared top cell is deeper than the hierarchy-depth limit (256 by default, Q-LVS-36) or recursive; the diagnostic names the deepest cell and the run ends as an input error.")
HEFESTO_LVS_CODE("LVS-HIER-002", error, "A circuit of the run has no top cell to compare (its netlist was not read, or the named cell does not exist).")
HEFESTO_LVS_CODE("LVS-HIER-003", error, "The hierarchy-depth setting is 0 or above 512 (twice the default).")
HEFESTO_LVS_CODE("LVS-HIER-004", info, "The LVS run was cancelled; the pairs compared so far are kept.")
HEFESTO_LVS_CODE("LVS-HIER-005", warning, "Flattening of unmatched or mismatched subcells of a pair stopped after as many rounds as the hierarchy-depth limit.")
