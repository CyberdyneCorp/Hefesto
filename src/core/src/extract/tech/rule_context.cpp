// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// RuleContext helpers and Magic's number tests (rule_context.hpp).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c (aToCap),
// utils/strdup.c (StrIsInt, StrIsNumeric) and database/DBtechname.c
// (DBTechNoisyNamePlane), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: EXTTECH.

#include "rule_context.hpp"

#include <cstdio>
#include <cstdlib>

namespace hefesto::extract::detail {

bool str_is_numeric(const std::string& s) {
  char* end = nullptr;
  (void)std::strtod(s.c_str(), &end);
  return end != s.c_str();
}

bool str_is_int(std::string_view s) noexcept {
  if (!s.empty() && (s.front() == '-' || s.front() == '+')) {
    s.remove_prefix(1);
  }
  for (const char c : s) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  return true;
}

std::optional<double> scan_double(const std::string& s) {
  double value = 0.0;
  // NOLINTNEXTLINE(cert-err34-c): sscanf is Magic's parser; the result is checked.
  if (std::sscanf(s.c_str(), "%lf", &value) != 1) {
    return std::nullopt;
  }
  return value;
}

std::optional<std::int32_t> scan_int(const std::string& s) {
  int value = 0;
  // NOLINTNEXTLINE(cert-err34-c): sscanf is Magic's parser; the result is checked.
  if (std::sscanf(s.c_str(), "%d", &value) != 1) {
    return std::nullopt;
  }
  return value;
}

tech::TypeMask RuleContext::noisy_types(std::string_view text) const {
  auto parsed = tech.parse_type_list(text);
  if (!parsed.ok()) {
    report(parsed.error().code, parsed.error().text, Print::tech_error);
    return {};
  }
  mentioned |= parsed.value();
  return parsed.value();
}

std::optional<tech::TypeMask> RuleContext::quiet_types(std::string_view text) const {
  auto parsed = tech.parse_type_list(text);
  if (!parsed.ok()) {
    return std::nullopt;
  }
  return parsed.value();
}

std::optional<tech::PlaneId> RuleContext::noisy_plane(std::string_view name) const {
  const tech::Lookup<tech::PlaneId> plane = tech.find_plane(name);
  if (plane.ok()) {
    return plane.id;
  }
  const bool ambiguous = plane.error == tech::LookupError::ambiguous;
  report(kCodeBadPlane,
         std::string{ambiguous ? "Ambiguous" : "Unrecognized"} + " plane name \"" + std::string{name} + "\"",
         Print::tech_error);
  return std::nullopt;
}

double RuleContext::cap_value(const std::string& text) const {
  const std::optional<double> value = scan_double(text);
  if (!value) {
    report(kCodeBadNumber, "Capacitance value " + text + " must be a number", Print::tech_error);
    return 0.0;
  }
  return *value;
}

tech::PlaneId RuleContext::plane_of(tech::TileType type) const {
  return tech.home_plane(type).value_or(tech::PlaneId{0});
}

}  // namespace hefesto::extract::detail
