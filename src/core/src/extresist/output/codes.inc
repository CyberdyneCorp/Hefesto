// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes EXTRESIST-OUT-* of the extresist area, included by
// src/core/src/extresist/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_EXTRESIST_CODE("EXTRESIST-OUT-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/extresist.md with `hefesto-cli --diagnostic-codes=extresist`
// and describe the codes in its OUT section.
//
// Owner: RESCMD (openspec/changes/m7-extraction/design.md D2/D3). The scaffold declared no code.
HEFESTO_EXTRESIST_CODE("EXTRESIST-OUT-001", error, "`extresist` cannot open the cell's `.ext` file (`Cannot open file <cell>.ext`); the cell is not processed.")
HEFESTO_EXTRESIST_CODE("EXTRESIST-OUT-002", warning, "A message of the `.ext` input read on the error stream (Magic's TxError): an unrecognized record, a bad tile type or node name, a bad device record, an `attr` record with no node.")
HEFESTO_EXTRESIST_CODE("EXTRESIST-OUT-003", info, "A line of the `.ext` input read on the output stream (Magic's TxPrintf): the `extresist debug` lines `Added port at <rect>`, `Added sink at <rect>`, `Added driver at <rect>`.")
