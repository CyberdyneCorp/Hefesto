// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes ROUTE-TECH-* of the route area, included by
// src/core/src/route/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_ROUTE_CODE("ROUTE-TECH-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/route.md with `hefesto-cli --diagnostic-codes=route`
// and describe the codes in its MZTECH section.
//
// Owner: MZTECH (openspec/changes/m10-plot-route/design.md D3).

HEFESTO_ROUTE_CODE("ROUTE-TECH-001", error, "An mzrouter statement comes before any style line.")
HEFESTO_ROUTE_CODE("ROUTE-TECH-002", error, "An mzrouter style line has no name; the lines up to the next style are skipped.")
HEFESTO_ROUTE_CODE("ROUTE-TECH-003", error, "Unrecognized keyword in the mzrouter section.")
HEFESTO_ROUTE_CODE("ROUTE-TECH-004", error, "Bad mzrouter layer statement; the layer is not defined.")
HEFESTO_ROUTE_CODE("ROUTE-TECH-005", error, "Bad mzrouter contact statement; the contact is not defined.")
HEFESTO_ROUTE_CODE("ROUTE-TECH-006", error, "Bad mzrouter notactive statement; nothing is marked inactive.")
HEFESTO_ROUTE_CODE("ROUTE-TECH-007", error, "Bad mzrouter spacing statement; none of its spacings is set.")
HEFESTO_ROUTE_CODE("ROUTE-TECH-008", error, "Bad mzrouter search statement; the search parameters are unchanged.")
HEFESTO_ROUTE_CODE("ROUTE-TECH-009", error, "Bad search penalty value; it must be a non-negative decimal.")
HEFESTO_ROUTE_CODE("ROUTE-TECH-010", error, "Search penalty value out of range; the statement is ignored.")
HEFESTO_ROUTE_CODE("ROUTE-TECH-011", error, "Bad mzrouter width statement; the width is not recorded.")
