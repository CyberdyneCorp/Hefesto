// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The symbol model: a loaded `.sym` (or schematic used as a symbol) and its
// pins (openspec/changes/m8-schematic design.md D7, "Area contracts" SYMLIB;
// capability schematic-symbols-and-libraries "Symbol property", "Pins",
// "Pin order").
//
// Owner: SYMLIB.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/schem/file/document.hpp"

namespace hefesto::diag {
class DiagnosticSink;
}

namespace hefesto::schem {

/// Index into Symbol::pins.
using PinIndex = std::size_t;

/// The layer whose boxes are a symbol's pins ("Pins").
inline constexpr int kPinLayer = 5;

/// A pin: a layer-5 box of the symbol, in file order ("Pins").
struct Pin {
  std::string name;
  std::string direction;  // the `dir` attribute: in, out, inout
  BoxRecord box;          // geometry and full attribute string
  /// The decoded `sim_pinnumber` attribute; nullopt when the pin has none
  /// ("Pin order").
  std::optional<std::string> sim_pinnumber;

  friend bool operator==(const Pin&, const Pin&) = default;
};

struct Symbol {
  /// The reference it was resolved from, as written in the instance.
  std::string reference;
  /// The resolved absolute file; empty for an embedded symbol or the
  /// missing-symbol placeholder.
  platform::Path file;
  Document document;
  std::vector<Pin> pins;
  bool embedded = false;
  /// The missing-symbol placeholder ("Missing symbols").
  bool placeholder = false;

  /// The symbol property ("Symbol property"): the `K` record. The reader
  /// already moves a pre-1.2 `G` symbol property there (schematic-file-format
  /// "Global property records").
  [[nodiscard]] const std::string& property() const noexcept { return document.globals.k; }
};

/// Builds a symbol from a document read as a `.sym`: the pins are its
/// layer-5 boxes in file order, named by their `name` attribute and with
/// their `dir` and `sim_pinnumber` attributes ("Pins").
[[nodiscard]] Symbol make_symbol(std::string reference, platform::Path file, Document document);

/// The pin of `box` (a layer-5 box): its `name`, `dir` and `sim_pinnumber`.
[[nodiscard]] Pin make_pin(const BoxRecord& box);

/// The pin order of "Pin order": file order, or ascending `sim_pinnumber`
/// (stable, Q-SCHEM-5) when every pin has one. Each duplicated
/// `sim_pinnumber` of a sorted symbol is reported once to `warnings`
/// (SCHEM-SYM-001, in both modes of `xschem-compat`); nullptr drops them.
[[nodiscard]] std::vector<PinIndex> pin_order(const Symbol& symbol, diag::DiagnosticSink* warnings);

/// pin_order() without warnings.
[[nodiscard]] std::vector<PinIndex> pin_order(const Symbol& symbol);

/// The integer value of a `sim_pinnumber` text: an optional sign and the
/// decimal digits that follow, after leading blanks; 0 when there are none,
/// and saturated at the range of `long long`.
[[nodiscard]] long long sim_pinnumber_value(std::string_view text) noexcept;

}  // namespace hefesto::schem
