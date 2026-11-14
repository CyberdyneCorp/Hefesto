// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CIF-WRITE-* codes (src/core/src/cif/write/codes.inc).
// Owner: CIF.

#pragma once

#include <string_view>

namespace hefesto::cif::write_codes {

inline constexpr std::string_view kNoStyle = "CIF-WRITE-001";
inline constexpr std::string_view kSubtree = "CIF-WRITE-002";
inline constexpr std::string_view kCalmaOnly = "CIF-WRITE-003";
inline constexpr std::string_view kOutputFile = "CIF-WRITE-004";
inline constexpr std::string_view kProblems = "CIF-WRITE-005";
inline constexpr std::string_view kLabelText = "CIF-WRITE-006";
inline constexpr std::string_view kCancelled = "CIF-WRITE-007";
inline constexpr std::string_view kDepth = "CIF-WRITE-008";

}  // namespace hefesto::cif::write_codes
