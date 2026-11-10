// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes DRC-CMD-* of the drc area, included by src/core/src/drc/codes.cpp.
// One line per code, sorted by code:
//   HEFESTO_DRC_CODE("DRC-TOPIC-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/drc.md with `hefesto-cli --diagnostic-codes=drc`
// and describe the codes in its COMMANDS section.
//
// Owner: COMMANDS (openspec/changes/m3-drc/design.md D1/D2, tasks.md section 7).
HEFESTO_DRC_CODE("DRC-CMD-001", info, "A report the drc command prints (Magic's TxPrintf text).")
HEFESTO_DRC_CODE("DRC-CMD-002", warning, "A warning the drc command prints while it still runs (Magic's TxError text).")
