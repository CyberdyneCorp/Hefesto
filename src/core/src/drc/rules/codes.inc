// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes DRC-LINT-*, DRC-TECH-* of the drc area, included by src/core/src/drc/codes.cpp.
// One line per code, sorted by code:
//   HEFESTO_DRC_CODE("DRC-TOPIC-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/drc.md with `hefesto-cli --diagnostic-codes=drc`
// and describe the codes in its RULES section.
//
// Owner: RULES (openspec/changes/m3-drc/design.md D1/D2, tasks.md section 4).

HEFESTO_DRC_CODE("DRC-LINT-001", warning, "Multiplane `spacing` rule: Magic compiles only the first plane pair (Q-DRC-3); native only.")
HEFESTO_DRC_CODE("DRC-LINT-002", warning, "`surround ... exact_width` between types that share no plane checks absence only (Q-DRC-18); native only.")
HEFESTO_DRC_CODE("DRC-LINT-003", warning, "`maxwidth` exclude types with `bend_illegal`/`both` are ignored (Q-DRC-19); native only.")
HEFESTO_DRC_CODE("DRC-TECH-001", error, "Error in a `drc` section line (Magic's message); the line or rule is skipped.")
HEFESTO_DRC_CODE("DRC-TECH-002", warning, "Warning for a `drc` section line (Magic's message); the rule is still compiled.")
HEFESTO_DRC_CODE("DRC-TECH-003", error, "Unknown or ambiguous DRC style (or cifoutput style) name.")
HEFESTO_DRC_CODE("DRC-TECH-004", info, "`DRC style is now \"<name>\"`.")
HEFESTO_DRC_CODE("DRC-TECH-005", error, "`tech drc surr` or `tech drc direc` without a second layer type.")
HEFESTO_DRC_CODE("DRC-TECH-006", info, "`Loading DRC CIF style.`: the first check under a `cifstyle` naming a cifoutput style that is not current.")
