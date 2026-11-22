// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes NETLIST-SIM-* of the netlist area, included by
// src/core/src/netlist/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_NETLIST_CODE("NETLIST-SIM-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/netlist.md with `hefesto-cli --diagnostic-codes=netlist`
// and describe the codes in its SIM section.
//
// Owner: SIM (openspec/changes/m7-extraction/design.md D2/D3).
HEFESTO_NETLIST_CODE("NETLIST-SIM-001", info, "An `ext2sim` report line (Magic's TxPrintf): `Devices merged: <n>`, `exttosim finished.`, a merge-type list.")
HEFESTO_NETLIST_CODE("NETLIST-SIM-002", warning, "An `ext2sim` message on the error stream (Magic's TxError): a bad option value, a run-flag error, no cell to netlist, a device the `.sim` format cannot name.")
HEFESTO_NETLIST_CODE("NETLIST-SIM-003", error, "`ext2sim` cannot write one of its output files; the run stops and no file of the run is written.")
