// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The property-string (attribute) grammar shared by every record
// (openspec/changes/m8-schematic design.md "Area contracts" FORMAT;
// capability schematic-file-format "Property strings", "Boolean values").
//
// Owner: FORMAT.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::schem {

/// One attribute as stored: `key` and `raw_value` keep quotes and
/// backslashes. `has_value` is false for an attribute written without `=`.
struct Property {
  std::string key;
  std::string raw_value;
  bool has_value = false;

  friend bool operator==(const Property&, const Property&) = default;
};

/// Attributes in written order (a key may repeat; lookup takes the first).
using PropertyList = std::vector<Property>;

/// How a looked-up value is returned ("Property strings").
enum class ValueForm : std::uint8_t {
  decoded,  // unescaped quotes and backslashes removed
  raw,      // the characters as stored
};

/// Splits a property string into its attributes.
[[nodiscard]] PropertyList parse_properties(std::string_view props);

/// The value of the first attribute named `key` (exact, case-sensitive);
/// nullopt when there is none. An attribute without `=` gives the empty
/// string. The value is never passed to Tcl: a caller that wants
/// `tcleval(...)` evaluated hands the decoded value to schem::TclBridge.
[[nodiscard]] std::optional<std::string> get_token(std::string_view props, std::string_view key,
                                                   ValueForm form = ValueForm::decoded);

/// `raw` (a stored value) with every unescaped `"` and `\` removed and each
/// escaped character kept literally: the decoded form of "Property strings".
[[nodiscard]] std::string decode_value(std::string_view raw);

/// `props` with `key` set to `value` by xschem's update rules ("Property
/// strings", setting rules 1-5). nullopt and the empty value both remove the
/// first occurrence of `key`.
[[nodiscard]] std::string set_token(std::string_view props, std::string_view key,
                                    std::optional<std::string_view> value);

/// "Boolean values": a value of decimal digits only is true when non-zero and
/// false when zero; `true`, `on`, `yes` / `false`, `off`, `no` (ASCII case
/// insensitive) are true / false; any other value is neither.
[[nodiscard]] bool is_true(std::string_view value) noexcept;
[[nodiscard]] bool is_false(std::string_view value) noexcept;

}  // namespace hefesto::schem
