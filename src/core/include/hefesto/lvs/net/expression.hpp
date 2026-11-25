// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Property-value expressions (capability lvs-netlist-input "Globals,
// includes and parameters"): a value that is an expression is reduced to a
// number when its operands resolve through the instance's properties, the
// parent cell's property defaults and the file's .PARAM values (in that
// order: the nearest definition wins). An unresolved expression stays an
// expression-typed value. Evaluation is iterative in its parameter
// references and bounded in nesting, so no input recurses without bound.
//
// Owner: LVSREAD.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "hefesto/lvs/net/netlist_db.hpp"

namespace hefesto::lvs {

/// Parenthesis nesting and parameter-reference depth of one evaluation.
inline constexpr std::size_t kMaxExpressionDepth = 64;

/// A property value after evaluation: `value` with `number` set, or
/// `string` (not numeric, not an expression) or `expression` (unresolved)
/// with `text` set.
struct ResolvedValue {
  PropertyType type = PropertyType::string;
  double number = 0.0;
  std::string text;
};

/// Where the operands of an expression are looked up. Every pointer may be
/// null.
struct ParameterScope {
  const PropertyRecord* instance = nullptr;
  const Cell* parent = nullptr;
  const ParameterTable* parameters = nullptr;
};

/// Whether `text` is written as an expression: quoted with ' or ", or in
/// braces.
[[nodiscard]] bool is_expression_text(std::string_view text) noexcept;

/// Evaluates `text` (a number with SI suffix, or an expression of numbers,
/// parameter names, + - * / and parentheses).
[[nodiscard]] ResolvedValue resolve_value(std::string_view text, const ParameterScope& scope);

}  // namespace hefesto::lvs
