// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A schematic loaded as a symbol (capability schematic-symbols-and-libraries
// "Schematic used as symbol").
//
// Owner: SYMLIB.

#pragma once

#include <functional>
#include <string>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/sym/symbol.hpp"

namespace hefesto::schem {

/// The symbol an instance of the schematic places (the cache resolves it one
/// nesting level deeper, or gives the placeholder).
using NestedSymbol = std::function<const Symbol&(const InstanceRecord&)>;

/// The symbol of `schematic` placed as a symbol: its `K` record, its
/// graphics and texts (floaters and `symbol_ignore=true` objects dropped),
/// its wires as layer-1 lines, the graphics of the symbols its instances
/// place, transformed by their orientation (their pin boxes moved to layer
/// 7; instances of the types ignored_in_schematic_symbol() left out), and one
/// pin per `ipin`, `opin` or `iopin` instance, named by its `lab`, at the
/// port symbol's transformed pin box. When `order_from` (the `.sym` with the
/// same base name next to the schematic) is given, the pins follow its pin
/// order by name; pins it does not name follow in file order.
[[nodiscard]] Symbol schematic_symbol(std::string reference, platform::Path file, const Document& schematic,
                                      const NestedSymbol& nested, const Symbol* order_from);

}  // namespace hefesto::schem
