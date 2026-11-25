// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes EXTRACT-DEV-* of the extract area, included by
// src/core/src/extract/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_EXTRACT_CODE("EXTRACT-DEV-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/extract.md with `hefesto-cli --diagnostic-codes=extract`
// and describe the codes in its DEV section.
//
// Owner: EXTDEV (openspec/changes/m7-extraction/design.md D2/D3; tasks.md 7.1-7.6).
HEFESTO_EXTRACT_CODE("EXTRACT-DEV-001", warning, "A device region matches no extraction record after terminal discovery (`Warning:  No matching extraction type for device at (x y)`); no device is written.")
HEFESTO_EXTRACT_CODE("EXTRACT-DEV-002", warning, "A device has more terminals than any record of its type declares (`Warning:  Device has more terminals than defined for type!`); the record is kept.")
HEFESTO_EXTRACT_CODE("EXTRACT-DEV-003", error, "The substrate layer under a device matches no record of its type (`No matching device for <type> with substrate layer <layer>`); no device is written.")
HEFESTO_EXTRACT_CODE("EXTRACT-DEV-004", error, "No substrate node is found under a device whose record needs one (`Device <type> does not have a compatible substrate node!`); no device is written.")
HEFESTO_EXTRACT_CODE("EXTRACT-DEV-005", error, "A device without identifier layer matches no record without one (`No matching device for <type> with no ID layer`); no device is written.")
HEFESTO_EXTRACT_CODE("EXTRACT-DEV-006", warning, "An identifier layer over a device matches no record of its type (`ID layer <layer> on non-matching device <type> was ignored.`); the record is kept.")
HEFESTO_EXTRACT_CODE("EXTRACT-DEV-007", warning, "No compatible record's parameter ranges accept a device's dimensions (`Device parameters do not match any extraction model.`).")
HEFESTO_EXTRACT_CODE("EXTRACT-DEV-008", warning, "A non-rectangular gate has several lengths (`Device has multiple lengths:  scaling all widths to length L`); the widths are scaled to the shortest.")
HEFESTO_EXTRACT_CODE("EXTRACT-DEV-009", error, "A capacitor's or diode's terminal boundary is empty (`extract:  Can't get capacitor L and W`); L and W are the equivalent square of the terminal area.")
HEFESTO_EXTRACT_CODE("EXTRACT-DEV-010", error, "A `device` cell property names an unknown device class or is malformed (`Extract error:  \"device\" property has unknown device type: ...`, `... property syntax error`); the normal device output is used.")
HEFESTO_EXTRACT_CODE("EXTRACT-DEV-011", info, "Two terminals of a symmetric device have the same position while sorting (`Extract error:  Duplicate tile position, ignoring`); they keep their order.")
HEFESTO_EXTRACT_CODE("EXTRACT-DEV-012", error, "The device pass was called without its cell, technology, style or node list (an internal error); nothing is extracted.")
