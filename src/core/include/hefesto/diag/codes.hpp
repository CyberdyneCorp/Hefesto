// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The diagnostic code table: every code the core and CLI emit, declared once
// with its default severity and a one-line summary. docs/diagnostics/codes.md
// is generated from this table (`hefesto-cli --diagnostic-codes`) and a unit
// test fails when the two disagree, so codes stay stable and documented.

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
inline constexpr std::string_view kCliTechNotImplemented = "CLI-TECH-001";
inline constexpr std::string_view kCliLog = "CLI-LOG-001";
}  // namespace codes

/// Every declared code, sorted by code.
[[nodiscard]] std::span<const CodeInfo> code_table() noexcept;

/// Looks up a code in the table.
[[nodiscard]] std::optional<CodeInfo> find_code(std::string_view code) noexcept;

/// True when `code` has the form AREA-TOPIC-NNN (upper-case letters, digits).
[[nodiscard]] bool is_well_formed_code(std::string_view code) noexcept;

/// The Markdown table that docs/diagnostics/codes.md carries between its
/// BEGIN/END markers.
[[nodiscard]] std::string render_code_table_markdown();

}  // namespace hefesto::diag
