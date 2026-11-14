// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The GDS-WRITE-* codes (src/core/src/gds/write/codes.inc). GDS-WRITE-001
// to 019 belong to the writer core (tasks 10.1-10.3, 10.6), 020 to 039 to
// the merged-polygon and label emitters (tasks 10.4, 10.5), 040 on to the
// verbatim copy, full dump, addendum and abstract views (task 10.7).
// Owner: GDSW.

#pragma once

#include <string_view>

namespace hefesto::gds::codes {

inline constexpr std::string_view kProgress = "GDS-WRITE-001";
inline constexpr std::string_view kNoStyle = "GDS-WRITE-002";
inline constexpr std::string_view kSubtree = "GDS-WRITE-003";
inline constexpr std::string_view kValueRange = "GDS-WRITE-004";
inline constexpr std::string_view kScaleUnits = "GDS-WRITE-005";
inline constexpr std::string_view kNameChanged = "GDS-WRITE-006";
inline constexpr std::string_view kNameCollision = "GDS-WRITE-007";
inline constexpr std::string_view kAbstract = "GDS-WRITE-008";
inline constexpr std::string_view kAbstractAllowed = "GDS-WRITE-009";
inline constexpr std::string_view kOutputFile = "GDS-WRITE-010";
inline constexpr std::string_view kUnnamed = "GDS-WRITE-011";
inline constexpr std::string_view kProblems = "GDS-WRITE-012";
inline constexpr std::string_view kCancelled = "GDS-WRITE-013";
inline constexpr std::string_view kDepth = "GDS-WRITE-014";
inline constexpr std::string_view kNotWritten = "GDS-WRITE-015";

}  // namespace hefesto::gds::codes
