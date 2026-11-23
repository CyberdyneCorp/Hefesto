// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A schematic loaded as a symbol (capability schematic-symbols-and-libraries
// "Schematic used as symbol"): its records and the graphics of everything it
// places, flattened into one symbol, to at most kMaxSymbolDepth levels,
// kMaxFlattenedRecords records (Q-SCHEM-32) and kMaxFlattenedBytes bytes
// (Q-SCHEM-40, M8-D2).
//
// Owner: SYMLIB.

#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/sym/symbol.hpp"

namespace hefesto::schem {

/// The level an instance may not open: the schematic loaded as the symbol
/// is level 0, and an instance whose symbol is a schematic opens the next
/// level ("Schematic used as symbol", "Recursion").
inline constexpr int kMaxSymbolDepth = 40;

/// The most records a schematic used as a symbol holds when flattened, over
/// all levels (input limit approved 2026-11-21, Q-SCHEM-32).
inline constexpr std::size_t kMaxFlattenedRecords = 1'000'000;

/// The most bytes a schematic used as a symbol holds when flattened, over
/// all levels, as estimated by flattened_bytes() (input limit approved
/// 2026-11-22, Q-SCHEM-40, M8-D2): 256 MiB.
inline constexpr std::size_t kMaxFlattenedBytes = std::size_t{256} << 20;

/// What an instance inside a schematic used as a symbol places: a schematic
/// (opened one level deeper and flattened in place) or a symbol (a `.sym`,
/// an embedded block or the missing-symbol placeholder). Exactly one is set;
/// both null places nothing.
struct NestedTarget {
  const Document* schematic = nullptr;
  const Symbol* symbol = nullptr;
};

/// Resolves an instance of the schematic `from` (its folder and embedded
/// blocks apply). The pointers stay valid until schematic_symbol() returns.
using NestedResolver = std::function<NestedTarget(const InstanceRecord& instance, const Document& from)>;

/// The limits a flattening stopped at.
struct FlattenReport {
  /// An instance would have opened level kMaxSymbolDepth.
  bool depth_reached = false;
  /// The record after kMaxFlattenedRecords was not added.
  bool size_reached = false;
  /// The record that would have passed kMaxFlattenedBytes was not added.
  bool bytes_reached = false;
};

struct FlattenLimits {
  int max_depth = kMaxSymbolDepth;
  std::size_t max_records = kMaxFlattenedRecords;
  std::size_t max_bytes = kMaxFlattenedBytes;
};

/// The estimated size of one flattened record (Q-SCHEM-40): 16 bytes per
/// point plus the size of every string it carries. Boxes are LineRecords.
[[nodiscard]] std::size_t flattened_bytes(const LineRecord& line) noexcept;
[[nodiscard]] std::size_t flattened_bytes(const ArcRecord& arc) noexcept;
[[nodiscard]] std::size_t flattened_bytes(const PolygonRecord& polygon) noexcept;
[[nodiscard]] std::size_t flattened_bytes(const TextRecord& text) noexcept;
[[nodiscard]] std::size_t flattened_bytes(const Pin& pin) noexcept;

/// The symbol of `schematic` placed as a symbol: its `K` record; at every
/// level its lines, boxes, arcs, polygons and texts, its wires as layer-1
/// lines, and the graphics of the symbols its instances place, transformed
/// by their orientation, with floater texts, graph boxes (`flags` containing
/// `graph`), `symbol_ignore=true` objects and instances of the types
/// ignored_in_schematic_symbol() left out, and nested layer-5 boxes moved to
/// layer 7. Its pins are the layer-5 boxes of `schematic` itself, then one
/// 5x5 box centred on each `ipin`, `opin` or `iopin` instance of
/// `schematic` itself, with `name=<lab> dir=<d> ` and the instance's other
/// attributes. When `order_from` (the `.sym` with the same base name next to
/// the schematic) is given, the pins follow its pin order by name; pins it
/// does not name follow in their order.
///
/// Reading stops, keeping what was added (depth-first), at the first
/// instance that would open level `limits.max_depth`, at the record that
/// would pass `limits.max_records`, or at the record whose flattened_bytes()
/// would take the total past `limits.max_bytes`; `report` (may be null)
/// says which.
[[nodiscard]] Symbol schematic_symbol(std::string reference, platform::Path file, const Document& schematic,
                                      const NestedResolver& nested, const Symbol* order_from,
                                      FlattenReport* report = nullptr, FlattenLimits limits = {});

/// True when a box's `flags` value contains `graph` ("Schematic used as
/// symbol": graph boxes are dropped).
[[nodiscard]] bool is_graph_box(const BoxRecord& box);

}  // namespace hefesto::schem
