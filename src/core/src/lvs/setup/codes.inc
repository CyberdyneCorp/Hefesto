// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-SETUP-* of the lvs area (lvs-setup-file), included by
// src/core/src/lvs/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_LVS_CODE("LVS-SETUP-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/lvs.md with `hefesto-cli --diagnostic-codes=lvs`
// and describe the codes in its LVSSETUP section.
//
// Owner: LVSSETUP (openspec/changes/m9-lvs-simulation/design.md D3).
HEFESTO_LVS_CODE("LVS-SETUP-001", warning, "A setup-file command failed (`Error <setupfile>:<line> (ignoring), <message>`); evaluation continues with the next command, and `Warning:  There were errors reading the setup file` follows at the end.")
HEFESTO_LVS_CODE("LVS-SETUP-002", error, "The setup file does not exist and its name does not contain `nosetup` (`Error:  Setup file <f> does not exist.`); the `lvs` command ends.")
HEFESTO_LVS_CODE("LVS-SETUP-003", error, "The setup file exists but cannot be read (`Error:  Cannot read the setup file <f>`).")
HEFESTO_LVS_CODE("LVS-SETUP-004", error, "Setup evaluation ran out of its step budget (default 50 million Tcl commands) or its wall-time backstop (default 60 s, for compiled loops that run no command); evaluation stops and the run is an error.")
HEFESTO_LVS_CODE("LVS-SETUP-005", warning, "The run was cancelled during setup evaluation; evaluation stops and the run is cancelled.")
HEFESTO_LVS_CODE("LVS-SETUP-006", error, "A setup command raised an error (the netgen message, e.g. `No such cell!`); inside a setup file it is reported as LVS-SETUP-001.")
HEFESTO_LVS_CODE("LVS-SETUP-007", error, "The setup file is larger than the setup-file limit (16 MiB, a crafted-input limit in both modes); nothing is evaluated.")
HEFESTO_LVS_CODE("LVS-SETUP-008", error, "The setup context is incomplete (a circuit is missing), or its step or time budget is zero or above twice the default.")
