// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LEF-LEX-* of the lef area, included by
// src/core/src/lef/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_LEF_CODE("LEF-LEX-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/lef.md with `hefesto-cli --diagnostic-codes=lef`
// and describe the codes in its LEX section.
//
// Owner: LEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2).

HEFESTO_LEF_CODE("LEF-LEX-001", info, "Message cap notice: `LEF Read:  Further errors will not be reported.` (or DEF; or warnings, messages) after 100 messages of one kind.")
HEFESTO_LEF_CODE("LEF-LEX-002", info, "Read totals: `LEF Read: encountered <k> errors total.` (or DEF; or warnings) at the end of a read.")
HEFESTO_LEF_CODE("LEF-LEX-003", error, "END matching: `Bad file read while looking for END statement` or `Section <N> has no END record!`.")
