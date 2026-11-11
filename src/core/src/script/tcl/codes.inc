// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SCRIPT-SANDBOX-* and SCRIPT-TCL-* of the script area,
// included by src/core/src/script/codes.cpp (rows are sorted there). One
// line per code:
//   HEFESTO_SCRIPT_CODE("SCRIPT-TOPIC-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/script.md with `hefesto-cli --diagnostic-codes=script`
// and describe the codes in its TCL section.
//
// Owner: TCL (openspec/changes/m4-scripting/design.md D1/D2, tasks.md section 4).
HEFESTO_SCRIPT_CODE("SCRIPT-TCL-001", error, "A Tcl script raised an error; the text is Tcl's error message.")
HEFESTO_SCRIPT_CODE("SCRIPT-TCL-002", error, "A script evaluation was interrupted.")
HEFESTO_SCRIPT_CODE("SCRIPT-TCL-003", error, "A script evaluation exceeded its command count or time limit.")
HEFESTO_SCRIPT_CODE("SCRIPT-SANDBOX-001", error, "The script sandbox refused to run a process (exec, or open of a command pipeline).")
HEFESTO_SCRIPT_CODE("SCRIPT-SANDBOX-002", error, "The script sandbox refused to open a network socket.")
HEFESTO_SCRIPT_CODE("SCRIPT-SANDBOX-003", error, "The script sandbox refused to load or unload a native extension.")
HEFESTO_SCRIPT_CODE("SCRIPT-SANDBOX-004", error, "The script sandbox refused a file access outside the granted roots.")
HEFESTO_SCRIPT_CODE("SCRIPT-SANDBOX-005", error, "The script sandbox refused an unsafe interpreter operation.")
HEFESTO_SCRIPT_CODE("SCRIPT-SANDBOX-006", error, "The script sandbox refused a package that Tcl's embedded library does not provide.")
HEFESTO_SCRIPT_CODE("SCRIPT-SANDBOX-007", error, "The script sandbox refused a removed command.")
HEFESTO_SCRIPT_CODE("SCRIPT-SANDBOX-008", error, "The script sandbox refused a command that changes PDK trust or the PDK store.")
