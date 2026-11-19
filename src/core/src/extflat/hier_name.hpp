// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Hierarchical names (capability ext-flattening-names "Hierarchical name
// structure", "Component hash", "Instance and array element names", "Name
// preference order"; Magic's HierName, EFname.c).
//
// A name is held as one string, its components joined with '\0' from the
// root side: a component never holds a NUL byte (the reader stops a line
// at one), and comparing two such strings bytewise orders them as
// efHNLexOrder orders the component lists, since '\0' sorts below every
// other byte. A use id may hold '/', so a component may too.
//
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::extflat::detail {

inline constexpr char kSep = '\0';

/// EFStrToHN(NULL, s): `s` split at every '/', empty components kept.
[[nodiscard]] std::string name_from_string(std::string_view s);
/// EFHNConcat: `prefix` (possibly empty, the root) then `suffix`.
[[nodiscard]] std::string concat_names(std::string_view prefix, std::string_view suffix);
/// EFHNToStr: the components joined with `sep`.
[[nodiscard]] std::string name_to_string(std::string_view key, char sep = '/');
/// The components from the root side.
[[nodiscard]] std::vector<std::string> name_components(std::string_view key);
/// The last component.
[[nodiscard]] std::string_view name_leaf(std::string_view key) noexcept;
[[nodiscard]] std::size_t name_component_count(std::string_view key) noexcept;

/// HASHADDVAL over one component: sum = ((sum >> 28) | (sum << 4)) + c,
/// each byte taken as a signed char (Magic's x86_64 build).
[[nodiscard]] std::uint32_t component_hash(std::string_view component) noexcept;

/// efHNFromUse: the path component of a use element: the use id, then
/// `[y,x]`, `[x]` or `[y]` for an array spanning both axes, x or y.
[[nodiscard]] std::string use_component(std::string_view use_id, bool has_x, bool has_y, std::int32_t x,
                                        std::int32_t y);

/// EFHNBest: true when `a` is preferred to `b` (both non-empty names).
/// An empty leaf compares as if its last character were NUL (Magic reads
/// the byte before the name, undefined behaviour).
[[nodiscard]] bool prefer_name(std::string_view a, std::string_view b) noexcept;

}  // namespace hefesto::extflat::detail
