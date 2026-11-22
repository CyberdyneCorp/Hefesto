// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `expr` evaluator (capability schematic-netlisting "expr evaluation";
// design D9, D20): number lexing with the Q-SCHEM-12 branch point and a
// recursive-descent parser over
//
//   ternary  = equality [ "?" ternary ":" ternary ]
//   equality = relation { ("==" | "!=") relation }
//   relation = additive { ("<" | ">" | "<=" | ">=") additive }
//   additive = product { ("+" | "-") product }
//   product  = unary { ("*" | "/" | "%") unary }
//   unary    = "-" unary | power
//   power    = primary [ "^" unary ]
//   primary  = number | "(" ternary ")" | function "(" ternary ")" | constant
//
// Numbers are read with std::strtod (the C `%lf` scan), so results are the same on every
// platform. Nesting is limited by kMaxExprDepth.
//
// Owner: NETLIST.

#include <cmath>
#include <limits>
#include <cstdlib>
#include <string>
#include <cstdint>
#include <span>
#include <utility>

#include "hefesto/schem/net/codes.hpp"
#include "hefesto/schem/net/expr.hpp"

namespace hefesto::schem {

namespace {

bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

bool is_alpha(char c) noexcept {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_run_char(char c) noexcept {
  return is_digit(c) || is_alpha(c) || c == '.' || c == '_' || c == '-';
}

bool is_ident_char(char c) noexcept {
  return is_digit(c) || is_alpha(c) || c == '_';
}

char lower(char c) noexcept {
  return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

/// Length of the run starting at text[0]; `minus_anywhere` lets `-` in
/// after the first character, otherwise only directly after `e` or `E`.
std::size_t run_length(std::string_view text, bool minus_anywhere) noexcept {
  std::size_t n = 1;
  while (n < text.size() && is_run_char(text[n])) {
    const bool after_exponent = text[n - 1] == 'e' || text[n - 1] == 'E';
    if (text[n] == '-' && !minus_anywhere && !after_exponent) {
      break;
    }
    ++n;
  }
  return n;
}

bool has_mode_dependent_minus(std::string_view run) noexcept {
  for (std::size_t i = 1; i < run.size(); ++i) {
    if (run[i] == '-' && run[i - 1] != 'e' && run[i - 1] != 'E') {
      return true;
    }
  }
  return false;
}

/// The C `%lf` scan of a run that starts with a digit or `.`: the value and
/// the characters it used. std::strtod is that scan (decimal and hexadecimal
/// forms, inf on overflow) and is correctly rounded on every CI platform;
/// Hefesto never changes the C locale. (std::from_chars for double is not
/// available on the macOS deployment target.)
std::pair<double, std::size_t> scan_lf(std::string_view run) {
  const std::string copy{run};
  char* end = nullptr;
  const double value = std::strtod(copy.c_str(), &end);
  return {value, static_cast<std::size_t>(end - copy.c_str())};
}

double si_multiplier(std::string_view rest) noexcept {
  if (rest.empty()) {
    return 1.0;
  }
  if (rest.size() >= 3 && lower(rest[0]) == 'm' && lower(rest[1]) == 'e' && lower(rest[2]) == 'g') {
    return 1e6;
  }
  switch (rest[0]) {
    case 'T': return 1e12;
    case 'G': return 1e9;
    case 'x':
    case 'X':
    case 'M': return 1e6;
    case 'k': return 1e3;
    case 'm': return 1e-3;
    case 'u': return 1e-6;
    case 'n': return 1e-9;
    case 'p': return 1e-12;
    case 'f': return 1e-15;
    case 'a': return 1e-18;
    default: return 1.0;
  }
}

/// round(x): half away from zero, as floor(x + 0.5) above 0 and
/// ceil(x - 0.5) below; 0, -0, NaN and infinities unchanged ("expr
/// evaluation").
double round_half_away(double x) noexcept {
  if (x > 0.0 && std::isfinite(x)) {
    return std::floor(x + 0.5);
  }
  if (x < 0.0 && std::isfinite(x)) {
    return std::ceil(x - 0.5);
  }
  return x;
}

/// round6 / round1: x unchanged when 0; else round(x / s) * s with
/// s = 10^(ceil(log10(|x|)) - digits). NaN and infinities give NaN.
double round_significant(double x, int digits) noexcept {
  if (x == 0.0) {
    return x;
  }
  if (!std::isfinite(x)) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  const double s = std::pow(10.0, std::ceil(std::log10(std::fabs(x))) - digits);
  return round_half_away(x / s) * s;
}

std::optional<double> constant(std::string_view name) noexcept {
  static constexpr std::pair<std::string_view, double> kConstants[] = {
      {"pi", 3.14159265358979323846},  {"e", 2.71828182845904523536}, {"k", 1.380649e-23},
      {"h", 6.62607e-34},              {"echarge", 1.60217646e-19},  {"abszero", 273.15},
      {"c", 2.99792458e8},
  };
  for (const auto& [key, value] : kConstants) {
    if (key == name) {
      return value;
    }
  }
  return std::nullopt;
}

using Function = double (*)(double);

double log_base10(double x) {
  return std::log10(x);
}

Function function(std::string_view name) noexcept {
  static constexpr std::pair<std::string_view, Function> kFunctions[] = {
      {"int", [](double x) { return std::trunc(x); }},
      {"sin", [](double x) { return std::sin(x); }},
      {"cos", [](double x) { return std::cos(x); }},
      {"tan", [](double x) { return std::tan(x); }},
      {"asin", [](double x) { return std::asin(x); }},
      {"acos", [](double x) { return std::acos(x); }},
      {"atan", [](double x) { return std::atan(x); }},
      {"log", log_base10},
      {"ln", [](double x) { return std::log(x); }},
      {"exp", [](double x) { return std::exp(x); }},
      {"sqrt", [](double x) { return std::sqrt(x); }},
      {"round6", [](double x) { return round_significant(x, 6); }},
      {"round1", [](double x) { return round_significant(x, 1); }},
      {"round", [](double x) { return round_half_away(x); }},
  };
  for (const auto& [key, fn] : kFunctions) {
    if (key == name) {
      return fn;
    }
  }
  return nullptr;
}

struct BinaryOp {
  std::string_view token;
  double (*apply)(double, double);
};

// Two-character operators come before their one-character prefixes.
constexpr BinaryOp kEquality[] = {
    {"==", [](double a, double b) { return a == b ? 1.0 : 0.0; }},
    {"!=", [](double a, double b) { return a != b ? 1.0 : 0.0; }},
};
constexpr BinaryOp kRelation[] = {
    {"<=", [](double a, double b) { return a <= b ? 1.0 : 0.0; }},
    {">=", [](double a, double b) { return a >= b ? 1.0 : 0.0; }},
    {"<", [](double a, double b) { return a < b ? 1.0 : 0.0; }},
    {">", [](double a, double b) { return a > b ? 1.0 : 0.0; }},
};
constexpr BinaryOp kAdditive[] = {
    {"+", [](double a, double b) { return a + b; }},
    {"-", [](double a, double b) { return a - b; }},
};
constexpr BinaryOp kProduct[] = {
    {"*", [](double a, double b) { return a * b; }},
    {"/", [](double a, double b) { return a / b; }},
    {"%", [](double a, double b) { return std::fmod(std::trunc(a), std::trunc(b)); }},
};

class Evaluator {
 public:
  Evaluator(std::string_view text, bool xschem_compat, std::vector<diag::Diagnostic>* warnings)
      : text_{text}, compat_{xschem_compat}, warnings_{warnings} {}

  std::optional<double> run() {
    auto value = ternary();
    skip_space();
    if (!value || pos_ != text_.size()) {
      return std::nullopt;
    }
    return value;
  }

 private:
  using Value = std::optional<double>;

  /// `parse` one nesting level deeper: a parenthesis, a function argument,
  /// a unary operand, or the right operand of `?:` or `^` (the parts that
  /// recurse). Past kMaxExprDepth it fails like a syntax error.
  Value nested(Value (Evaluator::*parse)()) {
    const Guard guard{*this};
    return guard.ok() ? (this->*parse)() : std::nullopt;
  }

  Value ternary() {
    auto cond = equality();
    if (!cond || !accept("?")) {
      return cond;
    }
    auto yes = nested(&Evaluator::ternary);
    if (!yes || !accept(":")) {
      return std::nullopt;
    }
    auto no = nested(&Evaluator::ternary);
    if (!no) {
      return std::nullopt;
    }
    return *cond != 0.0 ? yes : no;
  }

  /// One left-associative precedence level: `next { op next }`.
  Value binary(Value (Evaluator::*next)(), std::span<const BinaryOp> ops) {
    auto left = (this->*next)();
    while (left) {
      const BinaryOp* op = match(ops);
      if (op == nullptr) {
        break;
      }
      left = combine(left, (this->*next)(), op->apply);
    }
    return left;
  }

  const BinaryOp* match(std::span<const BinaryOp> ops) {
    for (const BinaryOp& op : ops) {
      if (accept(op.token)) {
        return &op;
      }
    }
    return nullptr;
  }

  Value equality() { return binary(&Evaluator::relation, kEquality); }
  Value relation() { return binary(&Evaluator::additive, kRelation); }
  Value additive() { return binary(&Evaluator::product, kAdditive); }
  Value product() { return binary(&Evaluator::unary, kProduct); }

  Value unary() {
    if (accept("-")) {
      auto value = nested(&Evaluator::unary);
      return value ? Value{-*value} : std::nullopt;
    }
    return power();
  }

  Value power() {
    auto base = primary();
    if (!base || !accept("^")) {
      return base;
    }
    return combine(base, nested(&Evaluator::unary), [](double a, double b) { return std::pow(a, b); });
  }

  Value primary() {
    skip_space();
    if (pos_ >= text_.size()) {
      return std::nullopt;
    }
    const char c = text_[pos_];
    if (is_digit(c) || c == '.') {
      return number();
    }
    if (accept("(")) {
      auto value = nested(&Evaluator::ternary);
      return value && accept(")") ? value : std::nullopt;
    }
    if (is_alpha(c) || c == '_') {
      return identifier();
    }
    return std::nullopt;
  }

  Value number() {
    const NumberRun run = lex_number(text_.substr(pos_), compat_);
    if (run.mode_dependent && warnings_ != nullptr) {
      const std::size_t compat_length = run_length(text_.substr(pos_), true);
      warnings_->push_back(diag::make(net_codes::kExprMinus,
                                      "expr: the number '" + std::string{text_.substr(pos_, compat_length)} +
                                          "' is read differently with xschem-compat on and off"));
    }
    pos_ += run.length;
    return run.value;
  }

  Value identifier() {
    const std::size_t start = pos_;
    while (pos_ < text_.size() && is_ident_char(text_[pos_])) {
      ++pos_;
    }
    const std::string_view name = text_.substr(start, pos_ - start);
    skip_space();
    if (pos_ < text_.size() && text_[pos_] == '(') {
      const Function fn = function(name);
      if (fn == nullptr) {
        return std::nullopt;
      }
      ++pos_;
      auto arg = nested(&Evaluator::ternary);
      return arg && accept(")") ? Value{fn(*arg)} : std::nullopt;
    }
    return constant(name);
  }

  template <typename Op>
  static Value combine(const Value& left, const Value& right, Op op) {
    if (!left || !right) {
      return std::nullopt;
    }
    return op(*left, *right);
  }

  bool accept(std::string_view token) noexcept {
    skip_space();
    // Callers try two-character operators (`<=`, `>=`) before `<` and `>`.
    if (text_.substr(pos_, token.size()) != token) {
      return false;
    }
    pos_ += token.size();
    return true;
  }

  void skip_space() noexcept {
    while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n')) {
      ++pos_;
    }
  }

  /// Counts nesting; refuses to go deeper than kMaxExprDepth.
  class Guard {
   public:
    explicit Guard(Evaluator& e) : e_{e} {
      if (++e_.depth_ > kMaxExprDepth && !e_.depth_reported_) {
        e_.depth_reported_ = true;
        if (e_.warnings_ != nullptr) {
          e_.warnings_->push_back(diag::make(net_codes::kSizeLimit, "expr nested deeper than " + std::to_string(kMaxExprDepth) +
                                                                     " levels; text copied without evaluation"));
        }
      }
    }
    ~Guard() { --e_.depth_; }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
    [[nodiscard]] bool ok() const noexcept { return e_.depth_ <= kMaxExprDepth; }

   private:
    Evaluator& e_;
  };

  std::string_view text_;
  std::size_t pos_ = 0;
  bool compat_;
  std::vector<diag::Diagnostic>* warnings_;
  int depth_ = 0;
  bool depth_reported_ = false;
};

}  // namespace

NumberRun lex_number(std::string_view text, bool xschem_compat) {
  NumberRun out;
  if (text.empty()) {
    return out;
  }
  const std::size_t compat_length = run_length(text, true);
  out.mode_dependent = has_mode_dependent_minus(text.substr(0, compat_length));
  // Q-SCHEM-12 branch point: ON keeps xschem's run, OFF stops at a `-`
  // that is not an exponent sign.
  out.length = xschem_compat ? compat_length : run_length(text, false);
  const std::string_view run = text.substr(0, out.length);
  const auto [value, used] = scan_lf(run);
  out.value = value * si_multiplier(run.substr(used));
  return out;
}

std::optional<double> evaluate_expr_value(std::string_view expression, bool xschem_compat,
                                          std::vector<diag::Diagnostic>* warnings) {
  return Evaluator{expression, xschem_compat, warnings}.run();
}

std::string evaluate_expr(std::string_view expression, bool xschem_compat) {
  const auto value = evaluate_expr_value(expression, xschem_compat);
  return value ? format_g(*value, 15) : std::string{expression};
}

}  // namespace hefesto::schem
