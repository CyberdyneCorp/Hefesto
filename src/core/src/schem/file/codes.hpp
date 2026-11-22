// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The SCHEM-FILE-* codes (src/core/src/schem/file/codes.inc).
// Owner: FORMAT.

#pragma once

#include <string_view>

namespace hefesto::schem::file_codes {

inline constexpr std::string_view kMissingFields = "SCHEM-FILE-001";
inline constexpr std::string_view kBadLayer = "SCHEM-FILE-002";
inline constexpr std::string_view kUnterminatedString = "SCHEM-FILE-003";
inline constexpr std::string_view kStrayEmbeddedBlock = "SCHEM-FILE-004";
inline constexpr std::string_view kUnclosedEmbeddedBlock = "SCHEM-FILE-005";
inline constexpr std::string_view kChangedOnDisk = "SCHEM-FILE-006";
inline constexpr std::string_view kBlockTooDeep = "SCHEM-FILE-007";
inline constexpr std::string_view kBlockSymbolTooDeep = "SCHEM-FILE-008";

}  // namespace hefesto::schem::file_codes
