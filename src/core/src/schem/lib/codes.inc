// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SCHEM-LIB-* of the schem area (schematic-symbols-and-libraries), included by
// src/core/src/schem/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_SCHEM_CODE("SCHEM-LIB-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/schem.md with `hefesto-cli --diagnostic-codes=schem`
// and describe the codes in its SYMLIB section.
//
// Owner: SYMLIB (openspec/changes/m8-schematic/design.md D3).
HEFESTO_SCHEM_CODE("SCHEM-LIB-001", warning, "A `./` or `../` reference, or one found through the `.` search-path element, was resolved against the schematic folder, where xschem uses its start folder (Q-SCHEM-30).")
HEFESTO_SCHEM_CODE("SCHEM-LIB-002", warning, "An `http://` or `https://` symbol reference is not fetched; the instance is shown as a missing symbol (Q-SCHEM-9).")
HEFESTO_SCHEM_CODE("SCHEM-LIB-003", warning, "Symbol not found: the reference resolves to no file; the instance is shown as a missing symbol.")
HEFESTO_SCHEM_CODE("SCHEM-LIB-004", warning, "A symbol file exists but could not be read or parsed; the instance is shown as a missing symbol.")
HEFESTO_SCHEM_CODE("SCHEM-LIB-005", warning, "Symbol recursively instantiating symbol: a schematic used as a symbol nests too deep or places itself; the nested instance is skipped.")
HEFESTO_SCHEM_CODE("SCHEM-LIB-006", warning, "An `xschemrc` file raised a Tcl error; the settings it would have made after the error are lost.")
HEFESTO_SCHEM_CODE("SCHEM-LIB-007", info, "`xschem_execute_scripts` in an `xschemrc` is ignored: `tcleval` values always run in the sandbox and launchers are confirmed (Q-SCHEM-25).")
HEFESTO_SCHEM_CODE("SCHEM-LIB-008", info, "A line an `xschemrc` file printed with `puts`.")
