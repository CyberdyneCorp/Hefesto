// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers for persistent display elements in .mag files
// (cell-file-format "Element records", "Writer elements").

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/db/cell.hpp"

namespace hefesto::cell::detail {

/// Parses one element record (the line, LF optional) with Magic's sscanf
/// rules; nullopt when it is malformed. Coordinates are returned unscaled.
[[nodiscard]] std::optional<db::Element> parse_element(std::string_view line);

/// DBWElementParseFlags for one flag word (unknown words are ignored).
void apply_element_flag(db::Element& element, std::string_view flag);

/// Adds `element` to `def`, replacing an element of the same name.
void put_element(db::CellDef& def, db::Element element);

/// The `<< elements >>` section of `def` (empty when no persistent
/// element exists), sorted by name with the natural comparator; values
/// divided by the write scale `r`.
[[nodiscard]] std::string element_section(const db::CellDef& def, std::int64_t r);

}  // namespace hefesto::cell::detail
