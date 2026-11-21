// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SCHEM-NET-* of the schem area (schematic-netlisting), included by
// src/core/src/schem/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_SCHEM_CODE("SCHEM-NET-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/schem.md with `hefesto-cli --diagnostic-codes=schem`
// and describe the codes in its NETLIST section. The constants are in
// hefesto/schem/net/codes.hpp.
//
// Owner: NETLIST (openspec/changes/m8-schematic/design.md D3).
HEFESTO_SCHEM_CODE("SCHEM-NET-001", error, "Two differently named nets are shorted (`Error: <tag> shorted: <a> - <b>`); the first name is kept and the netlist is still written.")
HEFESTO_SCHEM_CODE("SCHEM-NET-002", warning, "A net, pin or instance name is not valid bus notation (`syntax error in <string>`); it is used unchanged with unknown width.")
HEFESTO_SCHEM_CODE("SCHEM-NET-003", warning, "A pin has no net and is written as `__UNCONNECTED_PIN__<k>`.")
HEFESTO_SCHEM_CODE("SCHEM-NET-004", warning, "An `expr` number contains a `-` that xschem-compat ON and OFF read differently (Q-SCHEM-12).")
HEFESTO_SCHEM_CODE("SCHEM-NET-005", warning, "A format token is undefined while a shorter identifier prefix of it is defined, for example `@W/` and `W` (Q-SCHEM-26).")
HEFESTO_SCHEM_CODE("SCHEM-NET-006", warning, "A bus expansion or `expr` nesting exceeds a netlisting size limit; the text is used unchanged.")
