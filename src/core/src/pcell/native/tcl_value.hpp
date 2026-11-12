// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tcl values for the native generators (openspec/changes/m4-scripting/
// design.md D10; D-29): the C++ ports of the PDK generator procedures keep
// each value as Tcl does, a string representation and/or a number, so
//  - a parameter read from the dictionary keeps its original text ("0.420"
//    stays "0.420" when it becomes `0.420um` in a command word),
//  - a computed value is formatted by Tcl_PrintDouble only when it is used
//    as a string ("0.21", "0.3849999913945794"),
//  - arithmetic follows the ::tcl::mathop commands and `expr` (integer
//    floor division, int/double promotion), and
//  - comparisons are numeric when both operands are numbers, else string
//    comparisons, as `expr`'s `<`, `>` and `==`.
// A value outside the reproduced number subset (a big integer) raises
// Unsupported; a non-numeric operand raises the Tcl error.
//
// Owner: PCELL (design D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "pcell/native/tcl_number.hpp"

namespace hefesto::pcell::native {

/// The generator cannot reproduce this input natively: the PDK's Tcl
/// generator runs instead. Raised only before anything is drawn.
struct Unsupported {
  std::string reason;
};

/// A Tcl error the PDK procedure would raise (it aborts the procedure).
struct TclError {
  std::string message;
};

class Val {
 public:
  Val() = default;
  // NOLINTNEXTLINE(google-explicit-constructor): Tcl literals are strings.
  Val(const char* text) : text_(text) {}
  // NOLINTNEXTLINE(google-explicit-constructor)
  Val(std::string text) : text_(std::move(text)) {}
  // NOLINTNEXTLINE(google-explicit-constructor)
  Val(std::string_view text) : text_(std::string{text}) {}

  [[nodiscard]] static Val of(const tcl::Number& n);
  [[nodiscard]] static Val integer(std::int64_t v) { return of(tcl::Number::of(v)); }
  [[nodiscard]] static Val real(double v) { return of(tcl::Number::of(v)); }

  /// The string representation (a computed number is formatted as Tcl does).
  [[nodiscard]] const std::string& str() const;
  /// The number; TclError for a non-numeric string, Unsupported outside
  /// the subset.
  [[nodiscard]] tcl::Number num() const;
  /// True when the value is a number Tcl accepts as an operand.
  [[nodiscard]] bool is_number() const;
  /// Tcl's boolean value (`if {$topc}`): a number is true when non-zero;
  /// true/false/yes/no/on/off; else a TclError.
  [[nodiscard]] bool truth() const;
  [[nodiscard]] bool empty() const { return str().empty(); }

 private:
  mutable std::optional<std::string> text_;
  std::optional<tcl::Number> number_;
};

/// `${v}um`.
[[nodiscard]] std::string um(const Val& v);

// ::tcl::mathop: [+ a b], [- a b], [* a b], [/ a b] (left to right for more operands).
[[nodiscard]] Val add(const Val& a, const Val& b);
[[nodiscard]] Val add(const Val& a, const Val& b, const Val& c);
[[nodiscard]] Val sub(const Val& a, const Val& b);
[[nodiscard]] Val sub(const Val& a, const Val& b, const Val& c);
[[nodiscard]] Val mul(const Val& a, const Val& b);
[[nodiscard]] Val div(const Val& a, const Val& b);
/// expr abs($v).
[[nodiscard]] Val abs(const Val& v);
/// expr -$v (unary minus of a number).
[[nodiscard]] Val neg(const Val& v);
/// expr min/max of two numbers.
[[nodiscard]] Val min(const Val& a, const Val& b);
[[nodiscard]] Val max(const Val& a, const Val& b);

// expr comparisons: numeric when both are numbers, else string comparison.
[[nodiscard]] bool lt(const Val& a, const Val& b);
[[nodiscard]] bool gt(const Val& a, const Val& b);
[[nodiscard]] bool le(const Val& a, const Val& b);
[[nodiscard]] bool ge(const Val& a, const Val& b);
[[nodiscard]] bool eq(const Val& a, const Val& b);
[[nodiscard]] bool ne(const Val& a, const Val& b);

/// `string is int $v` (Tcl 8.6: an integer of at most 32 bits, either sign, or
/// the empty string, which `string is` accepts without -strict).
[[nodiscard]] bool string_is_int(const Val& v);
/// `string index $v 0`.
[[nodiscard]] std::string first_char(const Val& v);

}  // namespace hefesto::pcell::native
