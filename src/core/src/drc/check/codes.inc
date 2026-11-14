// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes DRC-CHECK-* of the drc area, included by src/core/src/drc/codes.cpp.
// One line per code, sorted by code:
//   HEFESTO_DRC_CODE("DRC-TOPIC-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/drc.md with `hefesto-cli --diagnostic-codes=drc`
// and describe the codes in its CHECK section.
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D1/D2, tasks.md section 5).
//
// DRC-MASK-*: the mask-layer check (M5 DRCCIF, openspec/changes/m5-mask-gds-cif/design.md D14).
HEFESTO_DRC_CODE("DRC-MASK-001", warning, "Mask-layer area or maxwidth errors narrower than one internal unit were dropped (Q-DRC-13, magic-compat ON); native only.")
HEFESTO_DRC_CODE("DRC-MASK-002", info, "Mask-layer area or maxwidth errors narrower than one internal unit were reported rounded outward (Q-DRC-13, magic-compat OFF); native only.")
