// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SCHEM-EDIT-* of the schem area (schematic-editing), included by
// src/core/src/schem/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_SCHEM_CODE("SCHEM-EDIT-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/schem.md with `hefesto-cli --diagnostic-codes=schem`
// and describe the codes in its EDIT section.
//
// Owner: EDIT (openspec/changes/m8-schematic/design.md D3).
HEFESTO_SCHEM_CODE("SCHEM-EDIT-001", error, "The active document is in the PDK store or the system library and is read-only; save it in a user folder to edit it.")
HEFESTO_SCHEM_CODE("SCHEM-EDIT-002", error, "The instance cannot be descended into: its symbol is not a `subcircuit` or `primitive`.")
HEFESTO_SCHEM_CODE("SCHEM-EDIT-003", error, "Descending would pass the hierarchy depth limit of 40 levels.")
HEFESTO_SCHEM_CODE("SCHEM-EDIT-004", error, "`schematic ascend` at the top of the hierarchy.")
HEFESTO_SCHEM_CODE("SCHEM-EDIT-005", warning, "Bus width mismatch: the net on a pin is neither the pin width nor the pin width times the instance width.")
HEFESTO_SCHEM_CODE("SCHEM-EDIT-006", error, "The schematic or symbol file to descend into does not exist or cannot be read.")
HEFESTO_SCHEM_CODE("SCHEM-EDIT-007", error, "`schematic symbol make` or `toschematic` would overwrite an existing file (`make -force` overwrites).")
HEFESTO_SCHEM_CODE("SCHEM-EDIT-008", error, "`schematic paste`: the clipboard holds no schematic objects.")
