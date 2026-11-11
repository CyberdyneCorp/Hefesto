// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tcl 8.6 numbers and the arithmetic subset of `expr` that the PDK helper
// procedures use (openspec/changes/m4-scripting/design.md D10; Q-SCRIPT-40):
// integer and double operands with Tcl's int/double promotion, integer
// floor division, and Tcl_PrintDouble's shortest round-trip formatting. An
// expression outside the subset (functions, variables, comparisons, big
// integers) is reported as unsupported, so a native generator falls back
// to the PDK's Tcl generator instead of guessing.
//
// Owner: PCELL (design D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace hefesto::pcell::tcl {

struct Number {
  bool integer = true;
  std::int64_t i = 0;
  double d = 0.0;

  [[nodiscard]] double as_double() const noexcept { return integer ? static_cast<double>(i) : d; }
  [[nodiscard]] static Number of(std::int64_t v) { return {true, v, 0.0}; }
  [[nodiscard]] static Number of(double v) { return {false, 0, v}; }
};

enum class Status : std::uint8_t {
  ok,
  error,        // Tcl raises an error (a `catch` in the PDK code sees it)
  unsupported,  // outside the reproduced subset: use the Tcl generator
};

struct Result {
  Status status = Status::ok;
  Number value;
};

/// Tcl_PrintDouble with tcl_precision 0 (shortest round-trip digits, "2.0",
/// "1e-06", "Inf", "NaN").
[[nodiscard]] std::string format_double(double value);
/// The string form of a number.
[[nodiscard]] std::string format(const Number& value);

/// A value used as an `expr` operand (`expr {$v * 2}`): a number with
/// optional surrounding white space and sign; error for other text.
[[nodiscard]] Result operand(std::string_view text);

/// `expr <text>` for the arithmetic subset: numbers, unary + and -,
/// binary + - * / %, parentheses and white space.
[[nodiscard]] Result evaluate(std::string_view text);

/// Tcl's round(): half away from zero, as an integer.
[[nodiscard]] Result round(const Number& value);
/// Tcl's int(): truncation toward zero.
[[nodiscard]] Result truncate(const Number& value);

/// a op b with Tcl's promotion rules ('+', '-', '*', '/', '%').
[[nodiscard]] Result apply(char op, const Number& a, const Number& b);

}  // namespace hefesto::pcell::tcl
