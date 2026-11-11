// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tcl 8.6 value semantics used by the native generators: ::tcl::mathop
// (generic/tclMathOp... via tclExecute.c), `expr` comparisons
// (TclCompareTwoNumbers / string comparison), Tcl_GetBooleanFromObj and
// `string is int` (generic/tclCmdMZ.c StringIsCmd).

#include "pcell/native/tcl_value.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>

namespace hefesto::pcell::native {

namespace {

tcl::Number checked(const tcl::Result& r, std::string_view op) {
  if (r.status == tcl::Status::unsupported) {
    throw Unsupported{"arithmetic outside the reproduced subset"};
  }
  if (r.status == tcl::Status::error) {
    throw TclError{std::string{"divide by zero ("} + std::string{op} + ")"};
  }
  return r.value;
}

Val binary(char op, const Val& a, const Val& b) {
  return Val::of(checked(tcl::apply(op, a.num(), b.num()), std::string_view{&op, 1}));
}

// -1, 0, 1 for a numeric comparison; nullopt when either is not a number.
std::optional<int> numeric_compare(const Val& a, const Val& b) {
  if (!a.is_number() || !b.is_number()) {
    return std::nullopt;
  }
  const tcl::Number x = a.num();
  const tcl::Number y = b.num();
  if (x.integer && y.integer) {
    return x.i < y.i ? -1 : (x.i > y.i ? 1 : 0);
  }
  const double dx = x.as_double();
  const double dy = y.as_double();
  if (std::isnan(dx) || std::isnan(dy)) {
    throw Unsupported{"NaN comparison"};
  }
  return dx < dy ? -1 : (dx > dy ? 1 : 0);
}

int compare(const Val& a, const Val& b) {
  if (const auto n = numeric_compare(a, b)) {
    return *n;
  }
  const int c = a.str().compare(b.str());
  return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

std::string lower(std::string_view text) {
  std::string out{text};
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

bool is_prefix(std::string_view word, std::string_view full, std::size_t min_length) {
  return word.size() >= min_length && word.size() <= full.size() && full.substr(0, word.size()) == word;
}

}  // namespace

Val Val::of(const tcl::Number& n) {
  Val v;
  v.text_.reset();
  v.number_ = n;
  return v;
}

const std::string& Val::str() const {
  if (!text_) {
    text_ = tcl::format(*number_);
  }
  return *text_;
}

tcl::Number Val::num() const {
  if (number_) {
    return *number_;
  }
  const tcl::Result r = tcl::operand(str());
  if (r.status == tcl::Status::unsupported) {
    throw Unsupported{"number outside the reproduced subset: " + str()};
  }
  if (r.status == tcl::Status::error) {
    throw TclError{"can't use non-numeric string \"" + str() + "\" as operand"};
  }
  return r.value;
}

bool Val::is_number() const {
  if (number_) {
    return true;
  }
  const tcl::Result r = tcl::operand(str());
  if (r.status == tcl::Status::unsupported) {
    throw Unsupported{"number outside the reproduced subset: " + str()};
  }
  return r.status == tcl::Status::ok;
}

bool Val::truth() const {
  if (is_number()) {
    const tcl::Number n = num();
    return n.integer ? n.i != 0 : n.d != 0.0;
  }
  const std::string word = lower(str());
  if (is_prefix(word, "true", 1) || is_prefix(word, "yes", 1) || word == "on") {
    return true;
  }
  if (is_prefix(word, "false", 1) || is_prefix(word, "no", 1) || is_prefix(word, "off", 2)) {
    return false;
  }
  throw TclError{"expected boolean value but got \"" + str() + "\""};
}

std::string um(const Val& v) {
  return v.str() + "um";
}

Val add(const Val& a, const Val& b) {
  return binary('+', a, b);
}

Val add(const Val& a, const Val& b, const Val& c) {
  return add(add(a, b), c);
}

Val sub(const Val& a, const Val& b) {
  return binary('-', a, b);
}

Val sub(const Val& a, const Val& b, const Val& c) {
  return sub(sub(a, b), c);
}

Val mul(const Val& a, const Val& b) {
  return binary('*', a, b);
}

Val div(const Val& a, const Val& b) {
  return binary('/', a, b);
}

Val abs(const Val& v) {
  const tcl::Number n = v.num();
  if (n.integer) {
    if (n.i == std::numeric_limits<std::int64_t>::min()) {
      throw Unsupported{"abs of the smallest integer"};
    }
    return Val::integer(n.i < 0 ? -n.i : n.i);
  }
  return Val::real(std::fabs(n.d));
}

Val neg(const Val& v) {
  return sub(Val::integer(0), v);
}

Val min(const Val& a, const Val& b) {
  // Tcl's min() returns the smaller operand as a number (ties: the first).
  return Val::of(compare(b, a) < 0 ? b.num() : a.num());
}

Val max(const Val& a, const Val& b) {
  return Val::of(compare(b, a) > 0 ? b.num() : a.num());
}

bool lt(const Val& a, const Val& b) {
  return compare(a, b) < 0;
}

bool gt(const Val& a, const Val& b) {
  return compare(a, b) > 0;
}

bool le(const Val& a, const Val& b) {
  return compare(a, b) <= 0;
}

bool ge(const Val& a, const Val& b) {
  return compare(a, b) >= 0;
}

bool eq(const Val& a, const Val& b) {
  return compare(a, b) == 0;
}

bool ne(const Val& a, const Val& b) {
  return compare(a, b) != 0;
}

bool string_is_int(const Val& v) {
  if (v.str().empty()) {
    return true;  // `string is` without -strict accepts the empty string
  }
  const tcl::Result r = tcl::operand(v.str());
  constexpr std::int64_t kLimit = 4294967295;
  return r.status == tcl::Status::ok && r.value.integer && r.value.i >= -kLimit && r.value.i <= kLimit;
}

std::string first_char(const Val& v) {
  return v.str().empty() ? std::string{} : v.str().substr(0, 1);
}

}  // namespace hefesto::pcell::native
