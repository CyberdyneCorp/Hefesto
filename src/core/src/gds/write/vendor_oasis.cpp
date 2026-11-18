// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Read-only OASIS cells in GDS output, scaffold stub
// (openspec/changes/m6-lef-def-oasis/design.md D13).
//
// Owner: OASW (design D2, D17).

#include "vendor_oasis.hpp"

namespace hefesto::gds::detail {

CopyResult transcode_oasis_cell(VendorIo& /*io*/, const db::CellDef& /*def*/) {
  return CopyResult::fallback;
}

}  // namespace hefesto::gds::detail
