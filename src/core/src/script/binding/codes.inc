// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SCRIPT-BIND-* of the script area, included by
// src/core/src/script/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_SCRIPT_CODE("SCRIPT-BIND-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/script.md with `hefesto-cli --diagnostic-codes=script`
// and describe the codes in its BINDING section.
//
// Owner: BINDING (openspec/changes/m4-scripting/design.md D1/D2, tasks.md section 5).
HEFESTO_SCRIPT_CODE("SCRIPT-BIND-001", warning, "A path variable's value changed after its first use; the expansion keeps the first value (magic-compat ON) or uses the new one (OFF).")
HEFESTO_SCRIPT_CODE("SCRIPT-BIND-002", error, "This `cif` option needs the CIF reader and writer, which arrive in M5.")
HEFESTO_SCRIPT_CODE("SCRIPT-BIND-003", info, "`netgen` is not available; Hefesto's built-in `lvs` command compares netlists.")
HEFESTO_SCRIPT_CODE("SCRIPT-BIND-004", info, "`xcircuit` is not available in Hefesto.")
HEFESTO_SCRIPT_CODE("SCRIPT-BIND-005", info, "`irsim` is not available on this host or in this milestone.")
