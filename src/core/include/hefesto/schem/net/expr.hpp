// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `expr(...)` evaluation in format strings (openspec/changes/m8-schematic
// design.md D9, "Area contracts" NETLIST; capability schematic-netlisting
// "expr evaluation", Q-SCHEM-12).
//
// Owner: NETLIST.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::schem {

/// Deepest nesting of parentheses, operators and function calls an
/// expression may have; a deeper one fails like a syntax error and reports
/// SCHEM-NET-006. Proposed limit, pending approval.
inline constexpr int kMaxExprDepth = 200;

/// How an `expr` result is printed.
enum class ExprStyle : std::uint8_t {
  plain,        // expr: %.15g
  engineering,  // expr_eng: ev_precision significant digits
  engineering4  // expr_eng4: 4 significant digits
};

struct ExprOptions {
  bool xschem_compat = true;
  /// The `ev_precision` setting used by expr_eng.
  int ev_precision = 4;
};

/// The value of one expression, or nullopt on a syntax error or an unknown
/// identifier. Warnings (Q-SCHEM-12, depth limit) go to `warnings` when it
/// is not null.
[[nodiscard]] std::optional<double> evaluate_expr_value(std::string_view expression, bool xschem_compat,
                                                        std::vector<diag::Diagnostic>* warnings = nullptr);

/// The text that replaces `expr(<expression>)`, given the expression
/// between the parentheses: the %.15g text of its value, or the expression
/// itself when it cannot be evaluated. `xschem_compat` selects the
/// Q-SCHEM-12 number lexing. Identical on every platform (D20).
[[nodiscard]] std::string evaluate_expr(std::string_view expression, bool xschem_compat);

struct ExprText {
  std::string text;
  std::vector<diag::Diagnostic> warnings;
};

/// `text` with every `expr(`, `expr_eng(` and `expr_eng4(` marker and its
/// matching `)` replaced by the printed result. After the first failure the
/// rest of the text is copied with every marker and its `)` removed.
[[nodiscard]] ExprText substitute_expr(std::string_view text, const ExprOptions& options);

/// C `%.<precision>g` of `value` in the C locale.
[[nodiscard]] std::string format_g(double value, int precision);

/// Engineering notation with `digits` significant digits ("expr evaluation").
[[nodiscard]] std::string format_engineering(double value, int digits);

/// One number run as the expr lexer reads it (Q-SCHEM-12 branch point).
struct NumberRun {
  std::size_t length = 0;  // characters taken from the input
  double value = 0.0;      // after the SI suffix
  /// The run as read with xschem-compat ON contains a `-` that is not
  /// directly after `e` or `E`: the two modes read it differently.
  bool mode_dependent = false;
};

/// Reads the number run starting at `text[0]` (a digit or `.`).
[[nodiscard]] NumberRun lex_number(std::string_view text, bool xschem_compat);

}  // namespace hefesto::schem
