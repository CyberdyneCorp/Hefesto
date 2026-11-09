// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the display area (table: src/core/src/display/codes.cpp,
// document: docs/diagnostics/codes/display.md).
//
// Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2/D3).

#pragma once

#include <string_view>

namespace hefesto::display::codes {

inline constexpr std::string_view kCmapOpen = "DISPLAY-CMAP-001";
inline constexpr std::string_view kCmapSyntax = "DISPLAY-CMAP-002";
inline constexpr std::string_view kCmapOrder = "DISPLAY-CMAP-003";
inline constexpr std::string_view kCmapShort = "DISPLAY-CMAP-004";
inline constexpr std::string_view kCmapWrite = "DISPLAY-CMAP-005";
inline constexpr std::string_view kDstyleOpen = "DISPLAY-DSTYLE-001";
inline constexpr std::string_view kDstyleVersion = "DISPLAY-DSTYLE-002";
inline constexpr std::string_view kDstyleOrder = "DISPLAY-DSTYLE-003";
inline constexpr std::string_view kDstyleEmptyLine = "DISPLAY-DSTYLE-004";
inline constexpr std::string_view kDstyleSection = "DISPLAY-DSTYLE-005";
inline constexpr std::string_view kDstyleLine = "DISPLAY-DSTYLE-006";
inline constexpr std::string_view kDstyleCount = "DISPLAY-DSTYLE-007";
inline constexpr std::string_view kDstyleMissing = "DISPLAY-DSTYLE-008";
inline constexpr std::string_view kDstylePale = "DISPLAY-DSTYLE-009";
inline constexpr std::string_view kFontOpen = "DISPLAY-FONT-001";
inline constexpr std::string_view kFontFormat = "DISPLAY-FONT-002";
inline constexpr std::string_view kGlyphOpen = "DISPLAY-GLYPH-001";
inline constexpr std::string_view kGlyphFormat = "DISPLAY-GLYPH-002";
inline constexpr std::string_view kGlyphEof = "DISPLAY-GLYPH-003";
inline constexpr std::string_view kGlyphExtra = "DISPLAY-GLYPH-004";
inline constexpr std::string_view kGlyphTooLarge = "DISPLAY-GLYPH-005";
inline constexpr std::string_view kStyleInvalid = "DISPLAY-STYLE-001";
inline constexpr std::string_view kStyleNoFamily = "DISPLAY-STYLE-002";

}  // namespace hefesto::display::codes
