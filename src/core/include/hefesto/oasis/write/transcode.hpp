// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Read-only cells in OASIS output (capability oasis-write "Read-only vendor
// cells"; openspec/changes/m6-lef-def-oasis/design.md D13):
// - transcode_gds_cell: a read-only GDS cell (GDS_FILE) is transcoded from
//   its GDS structure with gds::RecordReader (Q-OASIS-63). An odd-width path
//   is a write error naming the cell, and the round ends of an even-width
//   path become CIRCLE caps;
// - reencode_oasis_cell: a read-only OASIS cell (OASIS_FILE) is re-encoded
//   from its decoded records; XNAME, XELEMENT and XGEOMETRY records are kept
//   with the Q-OASIS-40 warning.
// Both write one CELL record and its contents to `out` under `name` (the
// write's structure name for the cell, after Q-OASIS-61 renaming).
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASW fills
// in the implementation in src/core/src/oasis/write/ (design D2).

#pragma once

#include <cstdint>
#include <string_view>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::gds {
class ByteSource;
}

namespace hefesto::oasis {

// The record encoder (oasis/codec/encoder.hpp, scaffold task 2.5).
class Encoder;

/// Transcodes the GDS structure that starts at `offset` in `gds_stream`.
/// SCAFFOLD: fails with CORE-NOTIMPL-006 until OASW lands it.
[[nodiscard]] diag::Status transcode_gds_cell(gds::ByteSource& gds_stream, std::uint64_t offset,
                                              std::string_view name, Encoder& out);

/// Re-encodes the OASIS CELL whose record starts at `offset` in the
/// decompressed `oasis_stream`. SCAFFOLD: fails with CORE-NOTIMPL-006 until
/// OASW lands it.
[[nodiscard]] diag::Status reencode_oasis_cell(gds::ByteSource& oasis_stream, std::uint64_t offset,
                                               std::string_view name, Encoder& out);

}  // namespace hefesto::oasis
