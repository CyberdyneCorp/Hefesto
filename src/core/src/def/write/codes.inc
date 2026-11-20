// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes DEF-WRITE-* of the def area, included by
// src/core/src/def/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_DEF_CODE("DEF-WRITE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/def.md with `hefesto-cli --diagnostic-codes=def`
// and describe the codes in its WRITE section.
//
// Owner: DEFW (openspec/changes/m6-lef-def-oasis/design.md D1/D2).
HEFESTO_DEF_CODE("DEF-WRITE-001", info, "DEF write report line: `Generating DEF output <file> for cell <c>:`, the `Diagnostic:` progress lines and the special-net notes (`Node <n> matches VDD variable definition!`).")
HEFESTO_DEF_CODE("DEF-WRITE-002", error, "DEF write: the output file cannot be opened (`Cannot open output file <name> (<reason>).`); nothing is written.")
HEFESTO_DEF_CODE("DEF-WRITE-003", error, "DEF write: the cell is unnamed (`Please name the cell before generating DEF.`); nothing is written.")
HEFESTO_DEF_CODE("DEF-WRITE-004", warning, "DEF write: the cell's `.ext` netlist cannot be read; no SPECIALNETS or NETS are written.")
HEFESTO_DEF_CODE("DEF-WRITE-005", warning, "DEF write: a `.ext` file is older than its cell (Q-LEFDEF-110, magic-compat OFF); the nets may be wrong.")
HEFESTO_DEF_CODE("DEF-WRITE-006", error, "DEF write: a pin's layer has no LEF name (Q-LEFDEF-114); its LAYER line is not written.")
HEFESTO_DEF_CODE("DEF-WRITE-007", warning, "DEF write: nothing of a node's type at its location (`Nothing of type <t> found at node <n> location (floating label?)!`).")
HEFESTO_DEF_CODE("DEF-WRITE-008", error, "DEF write: a contact region has no via in the position index (`Cannot find via name <key> in table!`).")
HEFESTO_DEF_CODE("DEF-WRITE-009", info, "def write arguments: the help, `The \"<opt>\" option is only for lef write`, `The \"-units\" option requires an argument.`.")
HEFESTO_DEF_CODE("DEF-WRITE-010", error, "def write: no cell is selected, or no technology is loaded; nothing is written.")
HEFESTO_DEF_CODE("DEF-WRITE-011", error, "def write: the `-units` value is not positive (Q-LEFDEF-104 fix); nothing is written.")
HEFESTO_DEF_CODE("DEF-WRITE-012", warning, "Hefesto-only: an input on which Magic 8.3.694 crashes in def write (Q-LEFDEF-101 blockage nets, a rule or via layer with no LEF record, a `.ext` layer the technology lacks); Hefesto writes defined output.")
