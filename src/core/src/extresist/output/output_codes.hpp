// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: the EXTRESIST-OUT codes of extresist/output/codes.inc.
//
// Owner: RESCMD.

#pragma once

#include <string_view>

namespace hefesto::extresist::output_codes {

inline constexpr std::string_view kCannotOpen = "EXTRESIST-OUT-001";  // the cell's .ext cannot be opened
inline constexpr std::string_view kMessage = "EXTRESIST-OUT-002";     // TxError while reading
inline constexpr std::string_view kReport = "EXTRESIST-OUT-003";      // TxPrintf while reading (debug)

}  // namespace hefesto::extresist::output_codes
