// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes EXTFLAT-READ-* of the extflat area, included by
// src/core/src/extflat/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/extflat.md with `hefesto-cli --diagnostic-codes=extflat`
// and describe the codes in its READ section.
//
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D1/D2, D10).
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-001", error, "An `.ext` file of the hierarchy cannot be found or read, so the read fails (`Cannot read extract file <name>.ext`).")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-002", warning, "A `.ext` read message (Magic's efReadError text, with its `<file>, line <n>: ` prefix).")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-003", error, "A `tech` record names another technology than the loaded one; the read fails.")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-004", warning, "A `style` record names an extraction style the technology does not have.")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-005", warning, "A `use` record repeats a use id of its cell; the later use replaces the earlier.")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-006", warning, "An `equiv` record joins two ports of one cell (`Ports \"a\" and \"b\" are electrically shorted.`).")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-007", warning, "A merge or kill names a node the flattened hierarchy does not have (`connect(1): no such node <name>`).")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-008", info, "A global node name is split over unconnected nodes, which are merged (`*** Global name <g> not fully connected:`).")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-009", error, "The `.ext` input passes a resource cap of the read (file size, line length, cells, depth, instances, names or connections); the read fails.")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-010", error, "The `use` records of the `.ext` files form a cycle; the read fails.")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-011", warning, "A `.ext` line has more than 128 tokens; the line is ignored with this warning, in both modes.")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-012", warning, "A `device` record overlaps an incompatible device at the same place and is ignored.")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-013", warning, "A cell of the flattened hierarchy was extracted as an abstract view (`Error:  Cell <name> was extracted as an abstract view.`).")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-014", warning, "A device of a hierarchical netlist is connected to a killed node and is written anyway (`Error:  Device at (<x> <y>) is connected to one or more eliminated nodes.`).")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-015", error, "A netlister argument is not recognised or lacks its value; the command prints its usage and fails.")
HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-016", warning, "A netlister argument is ignored (a second root cell, or a `-s` symbol that is malformed or already defined).")
