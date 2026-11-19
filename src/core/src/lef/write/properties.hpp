// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The cell properties the LEF writer reads (capability lef-write "Library
// header", "SITE blocks", "Macro boundary determination", "Macro header
// lines", "Macro PROPERTY lines"; Q-LEFDEF-42): string properties
// (DBPropGetString: only string-typed values count), FIXED_BBOX, and the
// two parsers of the LEFproperties string (properties.cpp).
//
// Owner: LEFW (design D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"

namespace hefesto::lef::write_detail {

/// DBPropGetString: the value of a string-typed property; nullopt when the
/// property is missing or of another type.
[[nodiscard]] std::optional<std::string> string_property(const db::CellDef& cell, std::string_view name);

/// FIXED_BBOX as four integers (a dimension list of four values); nullopt
/// otherwise. The caller checks the cell's fixed-bounding-box flag.
[[nodiscard]] std::optional<db::Rect> fixed_bbox(const db::CellDef& cell);

/// lefWriteMacro's PROPERTY parse: each `<key> <value>` pair as written
/// (quotes included). Stops, without the pair, at a key with no value or
/// an unterminated quote.
[[nodiscard]] std::vector<std::string> property_pairs(std::string_view value);

/// The keys lefGetProperties collects for PROPERTYDEFINITIONS, in the
/// order it inserts them. With magic-compat ON its parse is copied: a key
/// is inserted before its value is checked, and after a value it skips one
/// character only, so a run of spaces yields an empty key. With it OFF the
/// keys are those of property_pairs().
[[nodiscard]] std::vector<std::string> property_keys(std::string_view value, bool magic_compat);

}  // namespace hefesto::lef::write_detail
