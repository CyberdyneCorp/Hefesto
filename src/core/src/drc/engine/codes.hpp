// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The DRC-ENG-* codes (codes.inc) as constants for the engine's sources.
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D1/D2).

#pragma once

#include <string_view>

namespace hefesto::drc::codes {

/// DRCCheck: DBCellReadArea failed ("Failure to read in entire subtree of cell.").
inline constexpr std::string_view kSubtreeReadFailure = "DRC-ENG-001";

}  // namespace hefesto::drc::codes
