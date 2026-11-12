// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Ports of Tcl 8.6.16 Tcl_PrintDouble (generic/tclUtil.c, tcl_precision 0),
// the number grammar of TclParseNumber (generic/tclStrToD.c) and the
// int/double arithmetic of TclExecuteByteCode's binary operators
// (generic/tclExecute.c), restricted to the subset described in
// tcl_number.hpp.

#include "pcell/native/tcl_number.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace hefesto::pcell::tcl {

namespace {

bool is_space(char c) {
  return c == ' ' || (c >= '\t' && c <= '\r');
}

bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

int digit_value(char c, int radix) {
  int d = -1;
  if (is_digit(c)) {
    d = c - '0';
  } else if (c >= 'a' && c <= 'f') {
    d = c - 'a' + 10;
  } else if (c >= 'A' && c <= 'F') {
    d = c - 'A' + 10;
  }
  return d < radix ? d : -1;
}

struct Scanned {
  Result result;
  std::size_t length = 0;
};

Scanned scan_error(std::size_t length) {
  return {{Status::error, {}}, length};
}

Scanned scan_unsupported() {
  return {{Status::unsupported, {}}, 0};
}

// An integer in `radix` from s[start] (digits required).
Scanned radix_integer(std::string_view s, std::size_t start, int radix) {
  std::uint64_t value = 0;
  std::size_t i = start;
  const auto r = static_cast<std::uint64_t>(radix);
  while (i < s.size() && digit_value(s[i], radix) >= 0) {
    const auto d = static_cast<std::uint64_t>(digit_value(s[i], radix));
    if (value > (static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) - d) / r) {
      return scan_unsupported();  // a big integer
    }
    value = value * r + d;
    ++i;
  }
  if (i == start) {
    return scan_error(i);
  }
  return {{Status::ok, Number::of(static_cast<std::int64_t>(value))}, i};
}

std::size_t skip_digits(std::string_view s, std::size_t i) {
  while (i < s.size() && is_digit(s[i])) {
    ++i;
  }
  return i;
}

// digits [. digits] [e[+-]digits] or . digits [...]: the float's length, or
// 0 when the text is a plain integer.
std::size_t float_length(std::string_view s) {
  std::size_t i = skip_digits(s, 0);
  bool is_float = false;
  if (i < s.size() && s[i] == '.') {
    i = skip_digits(s, i + 1);
    is_float = true;
  }
  if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
    std::size_t j = i + 1;
    if (j < s.size() && (s[j] == '+' || s[j] == '-')) {
      ++j;
    }
    const std::size_t end = skip_digits(s, j);
    if (end > j) {
      return end;
    }
  }
  return is_float ? i : 0;
}

Scanned decimal(std::string_view s) {
  if (const std::size_t length = float_length(s); length > 0) {
    const std::string text{s.substr(0, length)};
    return {{Status::ok, Number::of(std::strtod(text.c_str(), nullptr))}, length};
  }
  const std::size_t end = skip_digits(s, 0);
  const std::string_view digits = s.substr(0, end);
  if (digits.size() > 1 && digits.front() == '0') {
    // Legacy octal; an 8 or 9 is an error ("looks like invalid octal number").
    return digits.find_first_of("89") != std::string_view::npos ? scan_error(end) : radix_integer(s, 1, 8);
  }
  return radix_integer(s, 0, 10);
}

// The radix of a 0x / 0b / 0o prefix, or 0.
int prefix_radix(char p) {
  switch (p) {
    case 'x': case 'X': return 16;
    case 'b': case 'B': return 2;
    case 'o': case 'O': return 8;
    default: return 0;
  }
}

// A number at the start of `s` (no sign, no white space).
Scanned scan_number(std::string_view s) {
  if (s.empty() || (!is_digit(s[0]) && s[0] != '.')) {
    return scan_error(0);
  }
  if (const int radix = s.size() > 2 && s[0] == '0' ? prefix_radix(s[1]) : 0; radix != 0) {
    return radix_integer(s, 2, radix);
  }
  if (s[0] == '.' && (s.size() < 2 || !is_digit(s[1]))) {
    return scan_error(0);
  }
  return decimal(s);
}

Number negate(const Number& n) {
  return n.integer ? Number::of(-n.i) : Number::of(-n.d);
}

Result integer_op(char op, std::int64_t a, std::int64_t b) {
  std::int64_t r = 0;
  switch (op) {
    case '+':
      if (__builtin_add_overflow(a, b, &r)) {
        return {Status::unsupported, {}};
      }
      return {Status::ok, Number::of(r)};
    case '-':
      if (__builtin_sub_overflow(a, b, &r)) {
        return {Status::unsupported, {}};
      }
      return {Status::ok, Number::of(r)};
    case '*':
      if (__builtin_mul_overflow(a, b, &r)) {
        return {Status::unsupported, {}};
      }
      return {Status::ok, Number::of(r)};
    default:
      break;
  }
  if (b == 0) {
    return {Status::error, {}};  // divide by zero
  }
  if (a == std::numeric_limits<std::int64_t>::min() && b == -1) {
    return {Status::unsupported, {}};
  }
  std::int64_t q = a / b;
  std::int64_t m = a % b;
  if (m != 0 && ((m < 0) != (b < 0))) {  // floor division, remainder with the divisor's sign
    --q;
    m += b;
  }
  return {Status::ok, Number::of(op == '/' ? q : m)};
}

Result double_op(char op, double a, double b) {
  switch (op) {
    case '+': return {Status::ok, Number::of(a + b)};
    case '-': return {Status::ok, Number::of(a - b)};
    case '*': return {Status::ok, Number::of(a * b)};
    case '/':
      if (b == 0.0 && (a == 0.0 || std::isnan(a))) {
        return {Status::error, {}};  // Tcl 8.6: a NaN result is a domain error
      }
      return {Status::ok, Number::of(a / b)};  // x/0.0 is +-Inf in Tcl 8.6
    default:
      return {Status::error, {}};  // % needs integers
  }
}

// ---- the expression parser --------------------------------------------------------

class Parser {
 public:
  explicit Parser(std::string_view text) : s_(text) {}

  Result parse() {
    skip();
    if (pos_ == s_.size()) {
      return {Status::error, {}};  // empty expression
    }
    Result r = sum();
    skip();
    if (r.status == Status::ok && pos_ != s_.size()) {
      return {classify(s_[pos_]), {}};
    }
    return r;
  }

 private:
  // A character the subset does not know: letters make Tcl's "invalid
  // bareword" or a function call (unsupported unless e/E, which is never a
  // function here); other characters are unsupported.
  static Status classify(char c) {
    if (c == 'e' || c == 'E') {
      return Status::error;
    }
    const bool simple = is_digit(c) || c == '.' || c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
                        c == '(' || c == ')' || is_space(c);
    return simple ? Status::error : Status::unsupported;
  }

  void skip() {
    while (pos_ < s_.size() && is_space(s_[pos_])) {
      ++pos_;
    }
  }

  bool take(char c) {
    skip();
    if (pos_ < s_.size() && s_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  Result primary() {
    skip();
    if (pos_ == s_.size()) {
      return {Status::error, {}};
    }
    if (take('(')) {
      Result r = sum();
      if (r.status == Status::ok && !take(')')) {
        return {Status::error, {}};
      }
      return r;
    }
    const Scanned n = scan_number(s_.substr(pos_));
    if (n.result.status != Status::ok) {
      return n.result.status == Status::error && n.length == 0 ? Result{classify(s_[pos_]), {}} : n.result;
    }
    pos_ += n.length;
    return n.result;
  }

  Result unary() {
    if (take('-')) {
      Result r = unary();
      if (r.status == Status::ok) {
        if (r.value.integer && r.value.i == std::numeric_limits<std::int64_t>::min()) {
          return {Status::unsupported, {}};
        }
        r.value = negate(r.value);
      }
      return r;
    }
    if (take('+')) {
      return unary();
    }
    return primary();
  }

  template <typename Next>
  Result binary(Next next, std::string_view ops) {
    Result left = (this->*next)();
    while (left.status == Status::ok) {
      skip();
      if (pos_ == s_.size() || ops.find(s_[pos_]) == std::string_view::npos) {
        break;
      }
      const char op = s_[pos_++];
      const Result right = (this->*next)();
      if (right.status != Status::ok) {
        return right;
      }
      left = apply(op, left.value, right.value);
    }
    return left;
  }

  Result product() { return binary(&Parser::unary, "*/%"); }
  Result sum() { return binary(&Parser::product, "+-"); }

  std::string_view s_;
  std::size_t pos_ = 0;
};

}  // namespace

std::string format_double(double value) {
  if (std::isnan(value)) {
    return "NaN";
  }
  if (std::isinf(value)) {
    return value < 0 ? "-Inf" : "Inf";
  }
  std::array<char, 64> buffer{};
  const auto [end, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                       std::chars_format::scientific);
  const std::string_view sci(buffer.data(), static_cast<std::size_t>(end - buffer.data()));
  const bool negative = sci.front() == '-';
  const std::size_t e = sci.find('e');
  std::string digits;
  for (const char c : sci.substr(negative ? 1 : 0, e - (negative ? 1 : 0))) {
    if (c != '.') {
      digits += c;
    }
  }
  const int exponent = std::atoi(std::string{sci.substr(e + 1)}.c_str());
  std::string out = negative ? "-" : "";
  if (exponent < -4 || exponent > 16) {
    out += digits.front();
    if (digits.size() > 1) {
      out += "." + digits.substr(1);
    }
    out += (exponent < 0 ? "e-" : "e+") + std::to_string(std::abs(exponent));
    return out;
  }
  if (exponent < 0) {
    return out + "0." + std::string(static_cast<std::size_t>(-exponent - 1), '0') + digits;
  }
  const auto whole = static_cast<std::size_t>(exponent) + 1;
  if (digits.size() <= whole) {
    return out + digits + std::string(whole - digits.size(), '0') + ".0";
  }
  return out + digits.substr(0, whole) + "." + digits.substr(whole);
}

std::string format(const Number& value) {
  return value.integer ? std::to_string(value.i) : format_double(value.d);
}

Result operand(std::string_view text) {
  std::size_t i = 0;
  while (i < text.size() && is_space(text[i])) {
    ++i;
  }
  bool negative = false;
  if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
    negative = text[i] == '-';
    ++i;
  }
  const Scanned n = scan_number(text.substr(i));
  if (n.result.status != Status::ok) {
    return n.result.status == Status::unsupported ? n.result : Result{Status::error, {}};
  }
  std::size_t end = i + n.length;
  while (end < text.size() && is_space(text[end])) {
    ++end;
  }
  if (end != text.size()) {
    return {Status::error, {}};
  }
  return {Status::ok, negative ? negate(n.result.value) : n.result.value};
}

Result evaluate(std::string_view text) {
  return Parser{text}.parse();
}

Result round(const Number& value) {
  if (value.integer) {
    return {Status::ok, value};
  }
  if (!std::isfinite(value.d) || std::fabs(value.d) >= 9.2e18) {
    return {Status::unsupported, {}};
  }
  double int_part = 0.0;
  const double fract = std::modf(value.d, &int_part);
  auto r = static_cast<std::int64_t>(int_part);
  if (fract <= -0.5) {
    --r;
  } else if (fract >= 0.5) {
    ++r;
  }
  return {Status::ok, Number::of(r)};
}

Result truncate(const Number& value) {
  if (value.integer) {
    return {Status::ok, value};
  }
  if (!std::isfinite(value.d) || std::fabs(value.d) >= 9.2e18) {
    return {Status::unsupported, {}};
  }
  return {Status::ok, Number::of(static_cast<std::int64_t>(value.d))};
}

Result apply(char op, const Number& a, const Number& b) {
  if (a.integer && b.integer) {
    return integer_op(op, a.i, b.i);
  }
  return double_op(op, a.as_double(), b.as_double());
}

}  // namespace hefesto::pcell::tcl
