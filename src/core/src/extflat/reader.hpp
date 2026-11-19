// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EFReadFile and efReadDef (capability ext-flattening-read "Locating and
// recursively reading .ext files", "Keyword recognition", "Scale record",
// "Technology, style, version and resistance classes"), reading with
// dosubckt TRUE, resist FALSE, noscale TRUE and isspice FALSE as
// `def write` does (defWrite.c:251-293).
//
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "hefesto/cell/search_path.hpp"
#include "state.hpp"

namespace hefesto::extflat::detail {

/// Reads `<root>.ext` and, depth first in use-table order, every cell it
/// uses that has not been read. Returns the root def, or the diagnostic
/// that makes EFReadFile return FALSE (the first one); a cap or a cycle
/// is in `st.fatal` and stops the read.
[[nodiscard]] diag::Outcome<std::uint32_t> read_tree(ReadState& st, cell::LookupContext& ctx,
                                                     std::string_view root);

}  // namespace hefesto::extflat::detail
