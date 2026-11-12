// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Deterministic generated-cell names (openspec/changes/m4-scripting/
// design.md D9; capability pcell-toolkit "Parameter value normalization for
// naming", "Deterministic generated-cell naming", "Legacy random cell
// names"; Q-SCRIPT-21, Q-SCRIPT-22): each value is normalized (Inf/NaN
// verbatim), the normalized values are hashed with MurmurHash3 (32-bit)
// over the characters' low bytes as Tcl 8.6 sees them, and the name is
// `<device>_<hash in Magic's alphabet>`. Computed in C++, never through the
// interpreter, so names do not depend on the Tcl version.
//
// (Magic ref: tcltk/toolkit.tcl:1133-1198,1778-1869)
//
// Owner: PCELL (design D2).

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/pcell/framework/parameters.hpp"

namespace hefesto::pcell {

/// Tcl 8.6 `string is double -strict` followed by `expr {double($v)}`
/// (TclParseNumber with surrounding white space, legacy octal `010`, `0x`,
/// `0b`, `0o`, Inf and NaN forms). nullopt when `text` is not a strict
/// double.
[[nodiscard]] std::optional<double> tcl_strict_double(std::string_view text);

/// magic::normalize_value (Q-SCRIPT-21: an infinite or NaN value is used
/// verbatim, in both magic-compat settings).
[[nodiscard]] std::string normalize_value(std::string_view value);

/// magic::murmur3_32 over `text` (seed 0): the characters of the UTF-8
/// `text` as Tcl 8.6 holds them (UTF-16 units; a stray byte is one
/// character), each contributing its low byte (Q-SCRIPT-22).
[[nodiscard]] std::uint32_t gencell_hash(std::string_view text, std::uint32_t seed = 0);

/// The six-character suffix of magic::get_gencell_hash for `parameters`.
[[nodiscard]] std::string gencell_suffix(const ParameterDict& parameters);

/// `<device>_<gencell_suffix(parameters)>`.
[[nodiscard]] std::string generated_cell_name(std::string_view device, const ParameterDict& parameters);

/// magic::get_gencell_name: `<device>_` plus six characters drawn from
/// 0-9a-z by `random_index(36)`, redrawn while `exists(name)`.
[[nodiscard]] std::string random_gencell_name(std::string_view device,
                                              const std::function<int(int)>& random_index,
                                              const std::function<bool(const std::string&)>& exists);

}  // namespace hefesto::pcell
