// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of pcell/native, included by src/core/src/pcell/codes.cpp
// (rows are sorted there). One line per code:
//   HEFESTO_PCELL_CODE("PCELL-TOPIC-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/pcell.md with `hefesto-cli --diagnostic-codes=pcell`.
//
// Owner: PCELL (openspec/changes/m4-scripting/design.md D1/D2, tasks.md section 7).

HEFESTO_PCELL_CODE("PCELL-NATIVE-001", info, "No native generator for this PDK version; the device's Tcl generator runs.")
