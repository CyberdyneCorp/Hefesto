// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes EXTRESIST-CMD-* of the extresist area, included by
// src/core/src/extresist/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_EXTRESIST_CODE("EXTRESIST-CMD-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/extresist.md with `hefesto-cli --diagnostic-codes=extresist`
// and describe the codes in its CMD section.
//
// Owner: RESCMD (openspec/changes/m7-extraction/design.md D2/D3). The scaffold declared no code.
HEFESTO_EXTRESIST_CODE("EXTRESIST-CMD-001", info, "An `extresist` report line (Magic's TxPrintf): an option query (`on`/`off`, a net list), `Extracting <net>`, the run statistics.")
HEFESTO_EXTRESIST_CODE("EXTRESIST-CMD-002", warning, "An `extresist` message on the error stream (Magic's TxError): an option value error, a bad `skip` type, a deprecated option, no box cell, an output file that cannot be opened.")
HEFESTO_EXTRESIST_CODE("EXTRESIST-CMD-004", warning, "An `extresist` run was cancelled; the cell's `.res.ext` and `.res.lump` keep their previous contents and no temporary file remains (Q-EXTRACT-27).")
HEFESTO_EXTRESIST_CODE("EXTRESIST-CMD-003", error, "An `extresist` output file cannot be written or committed; the previous file is kept and no temporary file remains.")
