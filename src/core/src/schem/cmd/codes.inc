// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SCHEM-CMD-* of the schem area (schematic-command), included by
// src/core/src/schem/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_SCHEM_CODE("SCHEM-CMD-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/schem.md with `hefesto-cli --diagnostic-codes=schem`
// and describe the codes in its CMD section.
//
// Owner: CMD (openspec/changes/m8-schematic/design.md D3).
HEFESTO_SCHEM_CODE("SCHEM-CMD-001", error, "Unknown or ambiguous `schematic` option or keyword argument.")
HEFESTO_SCHEM_CODE("SCHEM-CMD-002", error, "Wrong arguments: the usage line of the `schematic` option, or of the `xschem` subcommand.")
HEFESTO_SCHEM_CODE("SCHEM-CMD-003", error, "The `schematic` option needs an open schematic document and none is open.")
HEFESTO_SCHEM_CODE("SCHEM-CMD-004", error, "The active document has unsaved changes; it is not closed or replaced.")
HEFESTO_SCHEM_CODE("SCHEM-CMD-005", error, "The netlist run reported errors; the message lists them.")
HEFESTO_SCHEM_CODE("SCHEM-CMD-006", error, "No instance, symbol, wire or box with that name or index in the active document.")
HEFESTO_SCHEM_CODE("SCHEM-CMD-007", error, "The PDK named by `schematic pdk` has no xschem data (`libs.tech/xschem/xschemrc`).")
HEFESTO_SCHEM_CODE("SCHEM-CMD-008", error, "`xschem`: unsupported subcommand, `get` key or `set` variable.")
HEFESTO_SCHEM_CODE("SCHEM-CMD-009", error, "`xschem set netlist_type`: only `spice` is supported.")
HEFESTO_SCHEM_CODE("SCHEM-CMD-010", error, "`xschem place_symbol` needs the interactive editor.")
