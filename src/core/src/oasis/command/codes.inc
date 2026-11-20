// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes OASIS-CMD-* of the oasis area, included by
// src/core/src/oasis/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_OASIS_CODE("OASIS-CMD-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/oasis.md with `hefesto-cli --diagnostic-codes=oasis`
// and describe the codes in its CMD section.
//
// Owner: OASCMD (openspec/changes/m6-lef-def-oasis/design.md D1/D2).
HEFESTO_OASIS_CODE("OASIS-CMD-001", error, "Wrong number of arguments, or an unrecognized value, in an `oasis` command; nothing changes.")
HEFESTO_OASIS_CODE("OASIS-CMD-002", error, "`oasis compress` outside 0 to 9 or `oasis circlepoints` outside 8 to 4096; the setting is kept.")
HEFESTO_OASIS_CODE("OASIS-CMD-003", error, "`oasis contacts yes`: the option is accepted and does nothing, as for `gds`.")
HEFESTO_OASIS_CODE("OASIS-CMD-004", error, "`oasis read`: no file of the given name (with .oas, .oas.gz, .oasis or no extension) is on the search path.")
HEFESTO_OASIS_CODE("OASIS-CMD-005", error, "A file opened by content (picker, drag and drop) is not OASIS, GDS or a Magic cell file; nothing is read.")
