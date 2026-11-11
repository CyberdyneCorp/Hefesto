// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of pcell/framework, included by src/core/src/pcell/codes.cpp
// (rows are sorted there). One line per code:
//   HEFESTO_PCELL_CODE("PCELL-TOPIC-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/pcell.md with `hefesto-cli --diagnostic-codes=pcell`.
//
// Owner: PCELL (openspec/changes/m4-scripting/design.md D1/D2, tasks.md section 7).

HEFESTO_PCELL_CODE("PCELL-GEN-001", error, "A gencell framework procedure raised a script error.")
HEFESTO_PCELL_CODE("PCELL-GEN-002", warning, "A device procedure failed inside the framework; its message is printed and generation continues.")
HEFESTO_PCELL_CODE("PCELL-GEN-003", error, "The project has no script interpreter to run the device procedures.")
HEFESTO_PCELL_CODE("PCELL-LIST-001", error, "A Tcl list or dictionary value is malformed.")
HEFESTO_PCELL_CODE("PCELL-NETLIST-001", info, "SPICE netlist seeding report.")
