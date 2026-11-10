// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The diagnostic code table: every code the core and CLI emit, declared once
// with its default severity and a one-line summary. The table is split into
// areas so that each M1 workstream owns its own codes (openspec/changes/
// m1-core-viewer/design.md, D3): the "core" area lives in src/core/src/diag/
// codes.cpp and is documented in docs/diagnostics/codes.md; every other area
// lives in src/core/src/<area>/codes.cpp and is documented in
// docs/diagnostics/codes/<area>.md. Each document is generated with
// `hefesto-cli --diagnostic-codes[=<area>]` and a unit test fails when a
// document and its table disagree, so codes stay stable and documented.

#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::diag {

struct CodeInfo {
  std::string_view code;
  Severity severity;
  std::string_view summary;
};

namespace codes {
// Command API
inline constexpr std::string_view kCmdDispatch = "CMD-DISPATCH-001";
inline constexpr std::string_view kCmdUsage = "CMD-USAGE-001";
inline constexpr std::string_view kCmdInternal = "CMD-INTERNAL-001";
inline constexpr std::string_view kCmdRegistryDuplicate = "CMD-REGISTRY-001";
inline constexpr std::string_view kCmdParseQuote = "CMD-PARSE-001";
inline constexpr std::string_view kCmdParseBrace = "CMD-PARSE-002";
inline constexpr std::string_view kCmdParseExtra = "CMD-PARSE-003";
inline constexpr std::string_view kCmdSession = "CMD-SESSION-001";
// Platform services
inline constexpr std::string_view kPlatformUnsupported = "PLATFORM-UNSUPPORTED-001";
inline constexpr std::string_view kPlatformRead = "PLATFORM-IO-001";
inline constexpr std::string_view kPlatformWrite = "PLATFORM-IO-002";
inline constexpr std::string_view kPlatformDirectory = "PLATFORM-IO-003";
inline constexpr std::string_view kPlatformNotFound = "PLATFORM-IO-004";
inline constexpr std::string_view kPlatformProcess = "PLATFORM-PROCESS-001";
inline constexpr std::string_view kPlatformTechNotFound = "PLATFORM-PDK-001";
inline constexpr std::string_view kPlatformLog = "PLATFORM-LOG-001";
// Project settings
inline constexpr std::string_view kSettingsName = "SETTINGS-NAME-001";
inline constexpr std::string_view kSettingsValue = "SETTINGS-VALUE-001";
inline constexpr std::string_view kSettingsFile = "SETTINGS-FILE-001";
// Headless CLI
inline constexpr std::string_view kCliUsage = "CLI-USAGE-001";
inline constexpr std::string_view kCliScript = "CLI-SCRIPT-001";
inline constexpr std::string_view kCliLog = "CLI-LOG-001";
// Retired codes keep their table rows (never reused) but have no constant:
// CLI-TECH-001 (M1: `-T` loads the technology), CORE-NOTIMPL-001 (M1: no stub left),
// CORE-NOTIMPL-002 (M2: no stub left).
}  // namespace codes

/// One area of the code table. `prefix` is the AREA part every code of the
/// area starts with ("TECH" for TECH-...); the core area has several prefixes
/// and an empty `prefix`.
struct CodeArea {
  std::string_view name;    // "core", "tech", "db", "cell", "display", "view", "pdk", "app",
                            // "undo", "select", "connect", "ui", "wiring"
  std::string_view prefix;  // required AREA part of each code; empty for "core"
  std::string_view doc;     // repository-relative path of the area's document
  std::span<const CodeInfo> codes;  // sorted by code
};

/// Every area, "core" first, then in the order listed above.
[[nodiscard]] std::span<const CodeArea> code_areas() noexcept;

/// Looks up an area by name.
[[nodiscard]] std::optional<CodeArea> find_code_area(std::string_view name) noexcept;

/// Every declared code of every area, sorted by code.
[[nodiscard]] std::span<const CodeInfo> code_table() noexcept;

/// Looks up a code in the table.
[[nodiscard]] std::optional<CodeInfo> find_code(std::string_view code) noexcept;

/// True when `code` has the form AREA-TOPIC-NNN (upper-case letters, digits).
[[nodiscard]] bool is_well_formed_code(std::string_view code) noexcept;

/// The Markdown table that docs/diagnostics/codes.md carries between its
/// BEGIN/END markers (the "core" area).
[[nodiscard]] std::string render_code_table_markdown();

/// The Markdown table of one area, as carried by the area's document between
/// its BEGIN/END markers. An unknown area renders the header rows only.
[[nodiscard]] std::string render_code_table_markdown(std::string_view area);



}  // namespace hefesto::diag
