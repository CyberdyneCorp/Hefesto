// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Command result value: none, integer, real, string or a list of values, so
// results map onto Tcl results when the binding arrives (decision X-3).

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace hefesto::cmd {

class Value {
 public:
  using List = std::vector<Value>;
  using Data = std::variant<std::monostate, std::int64_t, double, std::string, List>;

  Value() = default;
  Value(std::int64_t v) : data_(v) {}               // NOLINT(google-explicit-constructor)
  Value(int v) : data_(std::int64_t{v}) {}          // NOLINT(google-explicit-constructor)
  Value(double v) : data_(v) {}                     // NOLINT(google-explicit-constructor)
  Value(std::string v) : data_(std::move(v)) {}     // NOLINT(google-explicit-constructor)
  Value(const char* v) : data_(std::string{v}) {}   // NOLINT(google-explicit-constructor)
  Value(List v) : data_(std::move(v)) {}            // NOLINT(google-explicit-constructor)

  [[nodiscard]] bool is_none() const noexcept { return std::holds_alternative<std::monostate>(data_); }
  [[nodiscard]] const Data& data() const noexcept { return data_; }

  /// Tcl-style string form: integers in decimal, reals in shortest
  /// round-trip form, lists as space-separated elements with {} quoting
  /// where needed; none is the empty string.
  [[nodiscard]] std::string to_string() const;

  friend bool operator==(const Value&, const Value&) = default;

 private:
  Data data_;
};

}  // namespace hefesto::cmd
