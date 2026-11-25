// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LVS name matching (capability lvs-netlist-input "Name matching and case
// sensitivity"; Q-LVS-3, fixed in both netgen-compat modes). The fold table
// is indexed by unsigned byte, so bytes >= 0x80 fold to themselves, and the
// lookup key folds brackets as equality does.
//
// Owner: LVSREAD.

#pragma once

#include <string>
#include <string_view>

namespace hefesto::lvs {

/// The case-insensitive fold of one byte: a-z to A-Z, `[{<` to `<`, `]}>`
/// to `>`; every other byte (including 0x80-0xFF) is unchanged.
[[nodiscard]] unsigned char fold_name_byte(unsigned char byte) noexcept;

/// Name equality. Case-sensitive: byte equality. Case-insensitive: equality
/// through fold_name_byte(), ignoring a leading backslash on exactly one of
/// the names together with one trailing space after it (Verilog escaped
/// names). An empty name stands for netgen's null name and never equals
/// anything.
[[nodiscard]] bool names_equal(std::string_view first, std::string_view second, bool case_insensitive) noexcept;

/// The lookup key of a name: every byte through fold_name_byte() (letters
/// and brackets; Q-LVS-3 fix), with the escaped-name backslash and its one
/// trailing space removed, so that names that compare equal share a key.
[[nodiscard]] std::string name_key(std::string_view name);

/// Whether `name` starts with `prefix` under the given case sensitivity.
[[nodiscard]] bool name_starts_with(std::string_view name, std::string_view prefix, bool case_insensitive) noexcept;

}  // namespace hefesto::lvs
