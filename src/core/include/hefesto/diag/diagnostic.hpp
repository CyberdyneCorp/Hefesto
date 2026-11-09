// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Structured diagnostic records (capability hefesto-diagnostics,
// requirement "Structured diagnostic records").

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace hefesto::diag {

enum class Severity { error, warning, info, debug };

/// Lower-case name used in every rendering: "error", "warning", "info", "debug".
[[nodiscard]] std::string_view severity_name(Severity severity) noexcept;

/// Parses a severity name; std::nullopt when it is not one of the four names.
[[nodiscard]] std::optional<Severity> parse_severity(std::string_view name) noexcept;

/// File and line of parsed input. Lines are 1-based; 0 means "unknown line".
struct SourceLocation {
  std::string file;
  std::uint32_t line = 0;

  friend bool operator==(const SourceLocation&, const SourceLocation&) = default;
};

/// A point in a cell, in internal database units.
struct LayoutLocation {
  std::string cell;
  std::int64_t x = 0;
  std::int64_t y = 0;

  friend bool operator==(const LayoutLocation&, const LayoutLocation&) = default;
};

/// No location, a source location, or a layout location.
using Location = std::variant<std::monostate, SourceLocation, LayoutLocation>;

/// `section` names the technology-file section a source-located diagnostic
/// was reported in (Magic's TechPrintLine context; "(none)" outside sections).
enum class ObjectKind { cell, instance, net, label, rule, section };

[[nodiscard]] std::string_view object_kind_name(ObjectKind kind) noexcept;

/// The object a diagnostic is about, for navigation in the GUI.
struct ObjectRef {
  ObjectKind kind = ObjectKind::cell;
  std::string name;

  friend bool operator==(const ObjectRef&, const ObjectRef&) = default;
};

/// One diagnostic. `code` has the form AREA-TOPIC-NNN and is declared in the
/// code table (hefesto/diag/codes.hpp).
struct Diagnostic {
  Severity severity = Severity::error;
  std::string code;
  std::string text;
  Location location;
  std::optional<ObjectRef> related;

  friend bool operator==(const Diagnostic&, const Diagnostic&) = default;
};

/// Builds a diagnostic with the code's default severity from the code table.
/// An undeclared code is a programming error: it yields severity `error`
/// and is caught by the code-table unit test.
[[nodiscard]] Diagnostic make(std::string_view code, std::string text, Location location = {});

}  // namespace hefesto::diag
