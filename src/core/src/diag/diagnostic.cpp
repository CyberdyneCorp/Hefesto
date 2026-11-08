// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/diag/diagnostic.hpp"

#include <array>
#include <utility>

#include "hefesto/diag/codes.hpp"

namespace hefesto::diag {

namespace {
constexpr std::array<std::string_view, 4> kSeverityNames = {"error", "warning", "info", "debug"};
constexpr std::array<std::string_view, 5> kObjectKindNames = {"cell", "instance", "net", "label",
                                                              "rule"};
}  // namespace

std::string_view severity_name(Severity severity) noexcept {
  return kSeverityNames.at(static_cast<std::size_t>(severity));
}

std::optional<Severity> parse_severity(std::string_view name) noexcept {
  for (std::size_t i = 0; i < kSeverityNames.size(); ++i) {
    if (kSeverityNames[i] == name) {
      return static_cast<Severity>(i);
    }
  }
  return std::nullopt;
}

std::string_view object_kind_name(ObjectKind kind) noexcept {
  return kObjectKindNames.at(static_cast<std::size_t>(kind));
}

Diagnostic make(std::string_view code, std::string text, Location location) {
  const auto info = find_code(code);
  return Diagnostic{
      .severity = info ? info->severity : Severity::error,
      .code = std::string{code},
      .text = std::move(text),
      .location = std::move(location),
      .related = std::nullopt,
  };
}

}  // namespace hefesto::diag
