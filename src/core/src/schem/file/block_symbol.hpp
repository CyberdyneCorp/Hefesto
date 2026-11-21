// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The records of an embedded symbol block as the symbol keeps them
// (capability schematic-file-format "Embedded symbols"): what a block
// drops, and how a nested block that defines an instance's symbol in place
// is merged into the symbol of the level above it.
//
// Owner: FORMAT.

#pragma once

#include <string_view>

#include "hefesto/schem/file/document.hpp"
#include "hefesto/schem/file/transform.hpp"

namespace hefesto::schem::file_detail {

/// Pin boxes are on layer 5; the pins of nested levels move to layer 7.
inline constexpr int kPinLayer = 5;
inline constexpr int kNestedPinLayer = 7;
/// `N` records of a block become lines on this layer.
inline constexpr int kWireLineLayer = 1;

/// The reader's canonical endpoint order of lines and wires, and corner
/// order of boxes ("Line, box, arc and polygon records", "Wire records").
void order_endpoints(Point& p1, Point& p2) noexcept;
void order_corners(Point& p1, Point& p2) noexcept;

/// `symbol_ignore=true`: the object is not part of the symbol.
[[nodiscard]] bool symbol_ignored(std::string_view props);
/// A floater text: one with a `name` or `floater` key.
[[nodiscard]] bool floater_text(const TextRecord& text);
/// A graph box: the value of its `flags` key contains `graph`.
[[nodiscard]] bool graph_box(const BoxRecord& box);

/// The placement equal to placing with `inner`, then with `outer` (an
/// instance of a nested level seen from the level above). Rotations other
/// than 0, 1 and 2 act as 3 and any non-zero flip mirrors ("Orientation
/// transform"), so the result is always in 0..3 / 0..1.
[[nodiscard]] Placement compose(const Placement& outer, const Placement& inner) noexcept;

/// Moves the records of `nested` (the symbol a nested block defined in
/// place) into `parent`, transformed by `at`, in their list order, with
/// layer-5 boxes put on layer 7. Instances still in `nested` (their symbol
/// is a file, which SYMLIB places) keep their place through `compose`.
void merge_nested(Document& nested, const Placement& at, Document& parent);

}  // namespace hefesto::schem::file_detail
