// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-REDUCE-* of the lvs area (lvs-device-reduction), included by
// src/core/src/lvs/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_LVS_CODE("LVS-REDUCE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/lvs.md with `hefesto-cli --diagnostic-codes=lvs`
// and describe the codes in its LVSRED section.
//
// Owner: LVSRED (openspec/changes/m9-lvs-simulation/design.md D3).
HEFESTO_LVS_CODE("LVS-REDUCE-001", warning, "A permutable pair of a class names a pin the class does not have (`Class <c> does not have pin <p>.`); no permutation of that class is applied.")
