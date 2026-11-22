// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SCHEM-SYM-* of the schem area (schematic-symbols-and-libraries), included by
// src/core/src/schem/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_SCHEM_CODE("SCHEM-SYM-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/schem.md with `hefesto-cli --diagnostic-codes=schem`
// and describe the codes in its SYMLIB section.
//
// Owner: SYMLIB (openspec/changes/m8-schematic/design.md D3).
HEFESTO_SCHEM_CODE("SCHEM-SYM-001", warning, "Pins of a symbol share a `sim_pinnumber`; they keep their file order (Q-SCHEM-5).")
HEFESTO_SCHEM_CODE("SCHEM-SYM-002", warning, "A displayed-text token is undefined while an identifier prefix of it is defined: a token ends only before a blank, `@` or `%` (Q-SCHEM-26).")
