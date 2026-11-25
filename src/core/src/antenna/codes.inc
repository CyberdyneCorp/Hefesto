// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes ANTENNA-CHECK-* of the antenna area, included by
// src/core/src/antenna/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_ANTENNA_CODE("ANTENNA-CHECK-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/antenna.md with `hefesto-cli --diagnostic-codes=antenna`
// and describe the codes in its CHECK section.
//
// Owner: ANTENNA (openspec/changes/m7-extraction/design.md D2/D3). The scaffold declared no code.
HEFESTO_ANTENNA_CODE("ANTENNA-CHECK-001", info, "An `antennacheck` report line (Magic's TxPrintf): `Reading extract file.`, `Building flattened netlist.`, `Running antenna checks.`, `<n> gates analyzed.`, `antennacheck finished.`.")
HEFESTO_ANTENNA_CODE("ANTENNA-CHECK-002", warning, "An `antennacheck` message on the error stream (Magic's TxError): no plane order, no window or cell, an unrecognized run flag, and the `debug` violation details.")
