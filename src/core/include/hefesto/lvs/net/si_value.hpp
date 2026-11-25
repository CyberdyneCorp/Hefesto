// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SI value parsing of the LVS readers (capability lvs-netlist-input "SI
// value parsing"; design D6, D20). The decimal mantissa and the scale
// suffix are combined in the decimal exponent before one correctly rounded
// conversion, so the result does not depend on the width of long double.
//
// Owner: LVSREAD.

#pragma once

#include <optional>
#include <string_view>

namespace hefesto::lvs {

/// The value of `text` (mantissa, optional spaces, at most one scale
/// suffix, optional unit word); nullopt when the conversion fails, in which
/// case the value stays a string.
[[nodiscard]] std::optional<double> parse_si_value(std::string_view text) noexcept;

/// The number test of the readers: the mantissa parses and the next
/// non-space character is a scale letter (g k c m u n p f a, any case) or the
/// end of the text.
[[nodiscard]] bool is_si_number(std::string_view text) noexcept;

}  // namespace hefesto::lvs
