// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SIM-DECK-* of the sim area (simulation-netlist-preparation), included by
// src/core/src/sim/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_SIM_CODE("SIM-DECK-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/sim.md with `hefesto-cli --diagnostic-codes=sim`
// and describe the codes in its SIMDECK section.
//
// Owner: SIMDECK (openspec/changes/m9-lvs-simulation/design.md D3).

// M9 R2 (task 13.1): the spice and raw sources.
HEFESTO_SIM_CODE("SIM-DECK-001", error, "A file the deck includes cannot be read.")
HEFESTO_SIM_CODE("SIM-DECK-002", error, "Deck include depth limit exceeded.")
HEFESTO_SIM_CODE("SIM-DECK-003", error, "Prepared deck size limit exceeded.")
HEFESTO_SIM_CODE("SIM-DECK-004", error, "A forbidden command in a .control section of a simulation deck.")
HEFESTO_SIM_CODE("SIM-DECK-005", warning, "A .control section was removed from the deck.")
HEFESTO_SIM_CODE("SIM-DECK-006", error, "A .subckt without its .ends, or an .ends without its .subckt.")
HEFESTO_SIM_CODE("SIM-DECK-007", error, "An instance calls an undefined subcircuit.")
HEFESTO_SIM_CODE("SIM-DECK-008", error, "Deck has no analysis.")
HEFESTO_SIM_CODE("SIM-DECK-009", error, "Raw deck does not end with .end.")
HEFESTO_SIM_CODE("SIM-DECK-010", error, "Invalid stimulus source.")
HEFESTO_SIM_CODE("SIM-DECK-011", error, "Unknown DUT port in a binding.")
HEFESTO_SIM_CODE("SIM-DECK-012", warning, "A DUT port was left unconnected.")
HEFESTO_SIM_CODE("SIM-DECK-013", error, "The DUT netlist or the deck context is missing.")
HEFESTO_SIM_CODE("SIM-DECK-014", error, "Deck include count limit exceeded.")
HEFESTO_SIM_CODE("SIM-DECK-015", error, "A deck line holds a newline, carriage return or NUL character.")
HEFESTO_SIM_CODE("SIM-DECK-016", error, "A .lib file outside the PDK store (until model library resolution).")
HEFESTO_SIM_CODE("SIM-DECK-017", error, "A deck limit setting is zero or above twice its default.")
