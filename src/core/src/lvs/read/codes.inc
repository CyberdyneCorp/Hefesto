// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-READ-* of the lvs area (lvs-netlist-input), included by
// src/core/src/lvs/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_LVS_CODE("LVS-READ-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/lvs.md with `hefesto-cli --diagnostic-codes=lvs`
// and describe the codes in its LVSREAD section.
//
// Owner: LVSREAD (openspec/changes/m9-lvs-simulation/design.md D3).
HEFESTO_LVS_CODE("LVS-READ-001", error, "The netlist file does not exist (`ReadNetlist: unable to find file '<f>'`).")
HEFESTO_LVS_CODE("LVS-READ-002", error, "The netlist format cannot be detected from the suffix or the first character (`ReadNetlist: don't know type of file '<f>'`).")
HEFESTO_LVS_CODE("LVS-READ-003", error, "The format keyword is unknown or ambiguous, or names a netgen format Hefesto does not read (ntk, prm, netgen, actel, xilinx).")
HEFESTO_LVS_CODE("LVS-READ-004", error, "The explicit file number is negative or differs from the number the file was loaded with (`File is already loaded as a different file number.`).")
HEFESTO_LVS_CODE("LVS-READ-005", warning, "An unrecognized card is skipped (`Ignoring line starting with token: <tok>`).")
HEFESTO_LVS_CODE("LVS-READ-006", warning, "More than 100 warnings in one file (`Too many warnings. . . will not report any more.`); later warnings are counted but not reported.")
HEFESTO_LVS_CODE("LVS-READ-007", error, "A token is mostly non-ASCII bytes (`Input file \"<f>\" appears to be binary. . . bailing out`); reading stops.")
HEFESTO_LVS_CODE("LVS-READ-008", info, "The summary of a file read with warnings (`File <f> read with <n> warning[s].`).")
HEFESTO_LVS_CODE("LVS-READ-009", warning, "A `.SUBCKT` inside an unclosed definition, or the end of the file inside one (`Missing .ENDS statement on subcircuit.`).")
HEFESTO_LVS_CODE("LVS-READ-010", warning, "`.ENDS` outside any definition (`.ENDS occurred outside of a subcircuit!`).")
HEFESTO_LVS_CODE("LVS-READ-011", warning, "A subcircuit is defined twice in one file; the earlier one is renamed `<name>[[k]]` and flagged duplicate.")
HEFESTO_LVS_CODE("LVS-READ-012", warning, "An element card is malformed or its model has the wrong number of ports (`Badly formed line in input.`); the card is skipped.")
HEFESTO_LVS_CODE("LVS-READ-013", warning, "A token after the parameters of a subcircuit call has no `=` (`Token \"<t>\" is not a parameter!`); it is ignored.")
HEFESTO_LVS_CODE("LVS-READ-014", error, "A subcircuit calls itself (`Fatal: Class \"<c>\" is instanced inside of itself!`); reading stops.")
HEFESTO_LVS_CODE("LVS-READ-015", info, "A call is dropped because its class is ignored, or is ignored when shorted and all its connections are one net.")
HEFESTO_LVS_CODE("LVS-READ-016", warning, "A call to an undefined subcircuit creates a placeholder module (`Call to undefined subcircuit <c>`).")
HEFESTO_LVS_CODE("LVS-READ-017", warning, "A call has more or fewer connections than its class has ports (`Parameter list mismatch in <inst>: ...`).")
HEFESTO_LVS_CODE("LVS-READ-018", warning, "A SPICE netlist is matched case-sensitively because a case-sensitive format was read earlier.")
HEFESTO_LVS_CODE("LVS-READ-019", error, "The include depth limit (default 64, Q-LVS-36) is exceeded; the read stops.")
HEFESTO_LVS_CODE("LVS-READ-020", error, "A logical line, after continuation lines are joined, exceeds the line limit (default 16 MiB, Q-LVS-36); the read stops.")
HEFESTO_LVS_CODE("LVS-READ-021", error, "A name exceeds the name-length limit (default 4 KiB, Q-LVS-36); the read stops.")
HEFESTO_LVS_CODE("LVS-READ-022", error, "A read limit setting is zero or above twice its default (Q-LVS-36).")
HEFESTO_LVS_CODE("LVS-READ-023", warning, "An `.INCLUDE` file cannot be read; the line is skipped.")
HEFESTO_LVS_CODE("LVS-READ-024", error, "A path cannot be read: no file service was given, or a `~user` prefix cannot be expanded on this platform (Q-LVS-14).")
HEFESTO_LVS_CODE("LVS-READ-025", error, "One read performs more than 65536 `.INCLUDE` reads, or its includes hold more than 64 times the logical-line limit in bytes (an include tree that multiplies, below the depth limit); the read stops.")
