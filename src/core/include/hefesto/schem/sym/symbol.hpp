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
#include <string>
#include <vector>

#include "hefesto/schem/file/document.hpp"

namespace hefesto::schem {

/// Index into Symbol::pins.
using PinIndex = std::size_t;

/// A pin: a layer-5 box of the symbol, in file order ("Pins").
struct Pin {
  std::string name;
  std::string direction;  // the `dir` attribute: in, out, inout
  BoxRecord box;          // geometry and full attribute string

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
};

/// The pin order of "Pin order": file order, or ascending `sim_pinnumber`
/// (stable, Q-SCHEM-5) when every pin has one.
[[nodiscard]] std::vector<PinIndex> pin_order(const Symbol& symbol);

}  // namespace hefesto::schem
