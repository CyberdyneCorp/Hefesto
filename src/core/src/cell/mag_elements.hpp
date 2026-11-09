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
#include <vector>

#include "hefesto/db/cell.hpp"

namespace hefesto::cell::detail {

/// Parses one element record (the line, LF optional) with Magic's sscanf
/// rules; nullopt when it is malformed. Coordinates are returned unscaled.
/// The message of each flag Magic does not know ("No such text element
/// flag "<flag>"") is appended to `errors` when given.
[[nodiscard]] std::optional<db::Element> parse_element(std::string_view line,
                                                       std::vector<std::string>* errors = nullptr);

/// DBWElementParseFlags for one flag of a comma-separated list: false when
/// the flag is unknown for the element's kind (the element is unchanged).
bool apply_element_flag(db::Element& element, std::string_view flag);

/// DBWElementParseFlags' message for a flag apply_element_flag rejected.
[[nodiscard]] std::string unknown_element_flag(const db::Element& element, std::string_view flag);

/// Adds `element` to `def`, replacing an element of the same name.
void put_element(db::CellDef& def, db::Element element);

/// The `<< elements >>` section of `def` (empty when no persistent
/// element exists), sorted by name with the natural comparator; values
/// divided by the write scale `r`.
[[nodiscard]] std::string element_section(const db::CellDef& def, std::int64_t r);

}  // namespace hefesto::cell::detail
