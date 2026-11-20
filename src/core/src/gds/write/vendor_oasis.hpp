// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the GDS writer (src/core/src/gds/write/): read-only OASIS
// cells in GDS output (capability gds-write "Read-only OASIS vendor cells";
// openspec/changes/m6-lef-def-oasis/design.md D13, Q-OASIS-35 (a)). `gds
// write` of a hierarchy holding a read-only OASIS cell (OASIS_FILE and
// OASIS_CELL) transcodes the cell from its OASIS file instead of
// generating it from its abstract layout. A layer or datatype above 32767,
// a BOUNDARY or PATH above 8191 XY points or a coordinate outside I4 fails
// the cell with an error, and nothing of its structure is written.
// LibraryWriter::emit (library.cpp) calls it where it would generate the
// cell.
//
// Under an OASIS write (oasis::VendorCells::active(), oasis/write/
// vendor_cells.hpp) the cell is only checked (file, cell, unit) and an
// empty placeholder structure is written, which the OASIS translation
// re-encodes from the source file.
//
// Owner: OASW (design D2, D17).

#pragma once

#include <cstdint>

#include "vendor.hpp"

namespace hefesto::gds::detail {

/// Whether `def` is a read-only OASIS cell.
[[nodiscard]] bool oasis_vendor_cell(const db::CellDef& def);

/// The outcome of transcode_oasis_cell.
enum class OasisVendorResult : std::uint8_t {
  written,   // the structure is in the output
  skipped,   // no structure (an unopenable file inside a vendor cell, or with `undefined allow`)
  fallback,  // generate the cell from its layout (an error was reported)
  failed,    // the cell fails (its parents fail too)
};

/// Under an OASIS write only (oasis-write "Read-only GDS vendor cells" rule
/// 3): whether the vendor GDS file `source` of read-only cell `def` has
/// UNITS (meters per database unit) that do not round to the output unit
/// (1e-9, or 1e-10 for `units angstroms`); the error is reported and the
/// copy falls back to generating the cell. False under a GDS write.
[[nodiscard]] bool oasis_units_refused(VendorIo& io, const db::CellDef& def, ByteSource& source);

/// Transcodes read-only OASIS cell `def` into the GDS stream of `io`.
[[nodiscard]] OasisVendorResult transcode_oasis_cell(VendorIo& io, const db::CellDef& def);

}  // namespace hefesto::gds::detail
