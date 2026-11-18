// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the GDS writer (src/core/src/gds/write/): read-only OASIS
// cells in GDS output (capability gds-write "Read-only OASIS vendor cells";
// openspec/changes/m6-lef-def-oasis/design.md D13, Q-OASIS-35 (a)). `gds
// write` of a hierarchy holding a read-only OASIS cell (OASIS_FILE)
// transcodes the cell from its OASIS file instead of generating it from its
// abstract layout. A layer or datatype above 32767, or a polygon above 8191
// points, fails the write with an error. vendor.cpp calls it where it would
// copy a read-only GDS cell (OASW adds the call).
//
// Owner: OASW (design D2, D17).

#pragma once

#include "vendor.hpp"

namespace hefesto::gds::detail {

/// Transcodes read-only OASIS cell `def` into the GDS stream of `io`.
/// SCAFFOLD: returns CopyResult::fallback (the cell is generated from its
/// layout, as for a failed verbatim copy) until OASW lands it; nothing
/// calls it yet.
[[nodiscard]] CopyResult transcode_oasis_cell(VendorIo& io, const db::CellDef& def);

}  // namespace hefesto::gds::detail
