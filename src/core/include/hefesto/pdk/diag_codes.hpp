// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic code constants of the "pdk" area (table: src/core/src/pdk/codes.cpp,
// document: docs/diagnostics/codes/pdk.md).
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2/D3).

#pragma once

#include <string_view>

namespace hefesto::pdk::codes {

inline constexpr std::string_view kEnableConflict = "PDK-ENABLE-001";
inline constexpr std::string_view kFetch = "PDK-FETCH-001";
inline constexpr std::string_view kFetchNotFound = "PDK-FETCH-002";
inline constexpr std::string_view kInfo = "PDK-INFO-001";
inline constexpr std::string_view kInstallIo = "PDK-INSTALL-001";
inline constexpr std::string_view kUnknownVersion = "PDK-INSTALL-002";
inline constexpr std::string_view kMissingAsset = "PDK-INSTALL-003";
inline constexpr std::string_view kNotInstalled = "PDK-INSTALL-004";
inline constexpr std::string_view kLibrary = "PDK-LIBRARY-001";
inline constexpr std::string_view kLicense = "PDK-LICENSE-001";
inline constexpr std::string_view kManifest = "PDK-MANIFEST-001";
inline constexpr std::string_view kPack = "PDK-PACK-001";
inline constexpr std::string_view kRepin = "PDK-PIN-001";
inline constexpr std::string_view kProgress = "PDK-PROGRESS-001";
inline constexpr std::string_view kSelect = "PDK-SELECT-001";
inline constexpr std::string_view kPinnedByProject = "PDK-UNINSTALL-001";
inline constexpr std::string_view kUnsafeEntry = "PDK-UNPACK-001";
inline constexpr std::string_view kUnpackLimit = "PDK-UNPACK-002";
inline constexpr std::string_view kBadArchive = "PDK-UNPACK-003";
inline constexpr std::string_view kSkippedEntry = "PDK-UNPACK-004";
inline constexpr std::string_view kVerify = "PDK-VERIFY-001";
inline constexpr std::string_view kUnverified = "PDK-VERIFY-002";

}  // namespace hefesto::pdk::codes
