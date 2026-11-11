// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SCRIPT-PDK-* and SCRIPT-TRUST-* of the script area,
// included by src/core/src/script/codes.cpp (rows are sorted there). One
// line per code:
//   HEFESTO_SCRIPT_CODE("SCRIPT-TOPIC-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/script.md with `hefesto-cli --diagnostic-codes=script`
// and describe the codes in its PDKINT section.
//
// Owner: PDKINT (openspec/changes/m4-scripting/design.md D1/D2, tasks.md section 6).
HEFESTO_SCRIPT_CODE("SCRIPT-PDK-001", error, "A startup file raised a Tcl error; Magic's `Error parsing ...` text, the rest of that file is skipped.")
HEFESTO_SCRIPT_CODE("SCRIPT-PDK-002", info, "A local startup file failed; startup continues without it (Magic's `Bad local startup file`).")
HEFESTO_SCRIPT_CODE("SCRIPT-PDK-003", warning, "A `magic::` procedure defined by one PDK's script was redefined by another PDK's script.")
HEFESTO_SCRIPT_CODE("SCRIPT-PDK-004", warning, "A Tk widget statement in a PDK menu callback was skipped; evaluation continues.")
HEFESTO_SCRIPT_CODE("SCRIPT-PDK-005", error, "Curated-pack mode refused a PDK without bundled scripts.")
HEFESTO_SCRIPT_CODE("SCRIPT-PDK-006", info, "A command-line file argument handled by the script host (`handling file entry ...`).")
HEFESTO_SCRIPT_CODE("SCRIPT-PDK-007", error, "A command-line file argument the script host cannot load (`don't know how to load file ...`).")
HEFESTO_SCRIPT_CODE("SCRIPT-PDK-008", error, "A PDK script could not run: the project has no interpreter.")
HEFESTO_SCRIPT_CODE("SCRIPT-PDK-009", info, "Curated-pack mode uses the bundled curated scripts of a PDK; the imported pack's own scripts are not executed.")
HEFESTO_SCRIPT_CODE("SCRIPT-TRUST-001", warning, "A PDK script was declined; the layout opens read-only with the `minimum` technology.")
HEFESTO_SCRIPT_CODE("SCRIPT-TRUST-002", info, "A PDK version is now trusted; its scripts run without asking.")
HEFESTO_SCRIPT_CODE("SCRIPT-TRUST-003", warning, "The trust record of a PDK version could not be saved in the PDK store.")
HEFESTO_SCRIPT_CODE("SCRIPT-TRUST-004", info, "A PDK version is no longer trusted (`pdk untrust`); its scripts ask before running.")
HEFESTO_SCRIPT_CODE("SCRIPT-TRUST-005", error, "A cell opened read-only because its PDK scripts were declined cannot be written.")
HEFESTO_SCRIPT_CODE("SCRIPT-TRUST-006", error, "After Review scripts, the PDK's technology could not be loaded; the declined cells stay read-only.")
