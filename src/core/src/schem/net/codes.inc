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
HEFESTO_SCHEM_CODE("SCHEM-NET-006", warning, "A bus expansion, `expr` nesting or vector element exceeds a netlisting size limit; the text is used unchanged.")
HEFESTO_SCHEM_CODE("SCHEM-NET-007", warning, "Top-level ports share a `sim_pinnumber`; they keep their instance order (Q-SCHEM-5).")
HEFESTO_SCHEM_CODE("SCHEM-NET-008", error, "The pins of a symbol and the ports of its schematic differ; the netlist is written and the run fails.")
HEFESTO_SCHEM_CODE("SCHEM-NET-009", warning, "Two subcircuit symbols have one cell name; xschem-compat ON writes the first only, OFF renames the later one (Q-SCHEM-14).")
HEFESTO_SCHEM_CODE("SCHEM-NET-010", warning, "A template key occurs in `extra` only inside a word (Q-SCHEM-19).")
HEFESTO_SCHEM_CODE("SCHEM-NET-011", warning, "A subcircuit template spans several lines (Q-SCHEM-21).")
HEFESTO_SCHEM_CODE("SCHEM-NET-012", warning, "The schematic of a subcircuit does not exist; its definition has an empty body (Q-SCHEM-27).")
HEFESTO_SCHEM_CODE("SCHEM-NET-013", warning, "An instance has no `name` attribute and its template has none either.")
HEFESTO_SCHEM_CODE("SCHEM-NET-014", warning, "A symbol has no `type` attribute.")
HEFESTO_SCHEM_CODE("SCHEM-NET-015", warning, "Every pin of an instance is unconnected.")
HEFESTO_SCHEM_CODE("SCHEM-NET-016", warning, "Two identical instances of one symbol overlap.")
HEFESTO_SCHEM_CODE("SCHEM-NET-017", warning, "The flat netlist stopped at the depth limit of 64 levels or after 268435456 bytes of output (Q-SCHEM-42); what was written so far is kept and the run does not fail.")
HEFESTO_SCHEM_CODE("SCHEM-NET-018", error, "The netlist run was interrupted (the environment's `interrupted` callback returned true); no netlist text is returned.")
