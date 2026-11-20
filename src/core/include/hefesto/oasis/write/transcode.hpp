// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Read-only cells in OASIS output (capability oasis-write "Read-only GDS
// vendor cells", "Read-only OASIS vendor cells";
// openspec/changes/m6-lef-def-oasis/design.md D13), as standalone
// functions over one cell:
// - transcode_gds_cell: a GDS structure is transcoded element by element
//   (Q-OASIS-63: an odd-width path is an error naming the cell, the round
//   ends of an even-width path become CIRCLE caps; no repetition
//   detection);
// - reencode_oasis_cell: an OASIS cell is re-encoded from its decoded
//   records (Q-OASIS-40: XNAMEs are not copied, extension payloads are).
// Both write one CELL record named `name` and its contents to `out`. A
// write applies the same rules to the read-only cells of its hierarchy
// (oasis::emit), with the write's names and string mapping.
//
// Owner: OASW (design D2; the header was created by the M6 scaffold).

#pragma once

#include <cstdint>
#include <string_view>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"

namespace hefesto::gds {
class ByteSource;
}

namespace hefesto::oasis {

class Encoder;

/// Transcodes the GDS structure whose BGNSTR record starts at `offset` in
/// `gds_stream` (SNAMEs kept when they are legal n-strings, else renamed;
/// TEXT strings mapped with the strict character table). Warnings and
/// errors go to `sink`; an element error (an odd-width path) is reported
/// and the status is an error, after the rest of the cell is written.
[[nodiscard]] diag::Status transcode_gds_cell(gds::ByteSource& gds_stream, std::uint64_t offset,
                                              std::string_view name, Encoder& out,
                                              diag::DiagnosticSink* sink = nullptr);

/// Re-encodes the cell named `cell` of the OASIS file in `oasis_stream`
/// (decoded from its current position, the first magic byte): an error
/// when the file cannot be decoded or does not define `cell`.
[[nodiscard]] diag::Status reencode_oasis_cell(gds::ByteSource& oasis_stream, std::string_view cell,
                                               std::string_view name, Encoder& out,
                                               diag::DiagnosticSink* sink = nullptr);

}  // namespace hefesto::oasis
