// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes GDS-WRITE-* of the gds area, included by
// src/core/src/gds/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_GDS_CODE("GDS-WRITE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/gds.md with `hefesto-cli --diagnostic-codes=gds`
// and describe the codes in its GDSW section.
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D1/D2).

// Merged polygon output and labels (merge_plane.cpp, labels_text.cpp). The
// 2xx block keeps these clear of the writer core's codes while both land.
HEFESTO_GDS_CODE("GDS-WRITE-201", error, "A GDS output coordinate does not fit a 4-byte integer (Q-MASK-222); the write fails.")
HEFESTO_GDS_CODE("GDS-WRITE-202", warning, "Port labels of a type without a `labels <type> port` layer are skipped (magic-compat ON) or written on the port-text layer (OFF) (Q-MASK-206).")
