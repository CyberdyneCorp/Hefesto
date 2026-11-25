// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes EXTRESIST-NET-* of the extresist area, included by
// src/core/src/extresist/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_EXTRESIST_CODE("EXTRESIST-NET-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/extresist.md with `hefesto-cli --diagnostic-codes=extresist`
// and describe the codes in its NET section.
//
// Owner: RESNET (openspec/changes/m7-extraction/design.md D2/D3).
HEFESTO_EXTRESIST_CODE("EXTRESIST-NET-001", error, "A network function was called without the technology.")
HEFESTO_EXTRESIST_CODE("EXTRESIST-NET-002", error, "A plane that is not a scratch copy reached the stitch editor.")
