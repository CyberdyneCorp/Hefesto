// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Property-value expressions (lvs-netlist-input "Globals, includes and
// parameters"). A small recursive-descent evaluator: + - * /, unary signs,
// parentheses, SI numbers and parameter names. Nesting and parameter
// references share one depth budget (kMaxExpressionDepth), so a deep or
// cyclic input ends as an unresolved expression instead of exhausting the
// stack.
//
// Owner: LVSREAD.

#include "hefesto/lvs/net/expression.hpp"

#include <cmath>
#include <optional>

#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/net/si_value.hpp"

namespace hefesto::lvs {

namespace {

bool is_alpha(char c) noexcept {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

bool is_space(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string_view strip_delimiters(std::string_view text) noexcept {
  if (text.size() >= 2) {
    const char first = text.front();
    const char last = text.back();
    if ((first == '\'' && last == '\'') || (first == '"' && last == '"') || (first == '{' && last == '}')) {
      return text.substr(1, text.size() - 2);
    }
  }
  return text;
}

std::optional<std::string> lookup(std::string_view name, const ParameterScope& scope) {
  if (scope.instance != nullptr) {
    if (const auto* property = find_property(*scope.instance, name)) {
      return property->value;
    }
  }
  if (scope.parent != nullptr) {
    if (const auto* definition = scope.parent->find_property_def(name)) {
      return definition->default_value;
    }
  }
  if (scope.parameters != nullptr) {
    const auto it = scope.parameters->find(name_key(name));
    if (it != scope.parameters->end()) {
      return it->second;
    }
  }
  return std::nullopt;
}

std::optional<double> evaluate(std::string_view text, const ParameterScope& scope, std::size_t depth);

class Parser {
 public:
  Parser(std::string_view text, const ParameterScope& scope, std::size_t depth)
      : text_(text), scope_(scope), depth_(depth) {}

  std::optional<double> run() {
    auto value = sum();
    skip_spaces();
    if (!value || pos_ != text_.size()) {
      return std::nullopt;
    }
    return value;
  }

 private:
  void skip_spaces() noexcept {
    while (pos_ < text_.size() && is_space(text_[pos_])) {
      ++pos_;
    }
  }

  bool accept(char c) noexcept {
    skip_spaces();
    if (pos_ < text_.size() && text_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  std::optional<double> sum() {
    auto value = product();
    while (value) {
      if (accept('+')) {
        const auto rhs = product();
        value = rhs ? std::optional{*value + *rhs} : std::nullopt;
      } else if (accept('-')) {
        const auto rhs = product();
        value = rhs ? std::optional{*value - *rhs} : std::nullopt;
      } else {
        break;
      }
    }
    return value;
  }

  std::optional<double> product() {
    auto value = signed_primary();
    while (value) {
      if (accept('*')) {
        const auto rhs = signed_primary();
        value = rhs ? std::optional{*value * *rhs} : std::nullopt;
      } else if (accept('/')) {
        const auto rhs = signed_primary();
        value = (rhs && *rhs != 0.0) ? std::optional{*value / *rhs} : std::nullopt;
      } else {
        break;
      }
    }
    return value;
  }

  std::optional<double> signed_primary() {
    double sign = 1.0;
    for (;;) {
      if (accept('-')) {
        sign = -sign;
      } else if (!accept('+')) {
        break;
      }
    }
    const auto value = primary();
    return value ? std::optional{sign * *value} : std::nullopt;
  }

  std::optional<double> primary() {
    skip_spaces();
    if (accept('(')) {
      if (depth_ + 1 >= kMaxExpressionDepth) {
        return std::nullopt;
      }
      ++depth_;
      auto value = sum();
      --depth_;
      return (value && accept(')')) ? value : std::nullopt;
    }
    if (pos_ >= text_.size()) {
      return std::nullopt;
    }
    if (is_digit(text_[pos_]) || text_[pos_] == '.') {
      return number();
    }
    if (is_alpha(text_[pos_])) {
      return parameter();
    }
    return std::nullopt;
  }

  std::optional<double> number() {
    const std::size_t start = pos_;
    while (pos_ < text_.size()) {
      const char c = text_[pos_];
      const bool exponent_sign = (c == '+' || c == '-') && pos_ > start &&
                                 (text_[pos_ - 1] == 'e' || text_[pos_ - 1] == 'E') && pos_ - 1 > start &&
                                 (is_digit(text_[pos_ - 2]) || text_[pos_ - 2] == '.');
      if (!(is_digit(c) || is_alpha(c) || c == '.' || exponent_sign)) {
        break;
      }
      ++pos_;
    }
    return parse_si_value(text_.substr(start, pos_ - start));
  }

  std::optional<double> parameter() {
    const std::size_t start = pos_;
    while (pos_ < text_.size() && (is_alpha(text_[pos_]) || is_digit(text_[pos_]))) {
      ++pos_;
    }
    const auto value = lookup(text_.substr(start, pos_ - start), scope_);
    if (!value) {
      return std::nullopt;
    }
    return evaluate(*value, scope_, depth_ + 1);
  }

  std::string_view text_;
  const ParameterScope& scope_;
  std::size_t depth_;
  std::size_t pos_ = 0;
};

std::optional<double> evaluate(std::string_view text, const ParameterScope& scope, std::size_t depth) {
  if (depth >= kMaxExpressionDepth) {
    return std::nullopt;
  }
  if (const auto number = parse_si_value(text)) {
    return number;
  }
  const auto value = Parser{strip_delimiters(text), scope, depth}.run();
  if (value && !std::isfinite(*value)) {
    return std::nullopt;
  }
  return value;
}

}  // namespace

bool is_expression_text(std::string_view text) noexcept {
  return strip_delimiters(text).size() != text.size();
}

ResolvedValue resolve_value(std::string_view text, const ParameterScope& scope) {
  ResolvedValue result;
  if (const auto number = parse_si_value(text)) {
    result.type = PropertyType::value;
    result.number = *number;
    return result;
  }
  if (const auto number = evaluate(text, scope, 0)) {
    result.type = PropertyType::value;
    result.number = *number;
    return result;
  }
  result.type = is_expression_text(text) ? PropertyType::expression : PropertyType::string;
  result.text = std::string{text};
  return result;
}

}  // namespace hefesto::lvs
