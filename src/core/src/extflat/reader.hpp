// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EFReadFile and efReadDef (capability ext-flattening-read "Locating and
// recursively reading .ext files", "Keyword recognition", "Scale record",
// "Technology, style, version and resistance classes",
// "Resistance-extraction overlay"), with EFReadFile's dosubckt, resist,
// noscale and isspice taken from ReadOptions (`def write` reads with
// dosubckt TRUE, resist FALSE, noscale TRUE and isspice FALSE,
// defWrite.c:251-293).
//
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13; M6 EXTSUB).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
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

/// efReadDef's search for `<name><suffix>`: the `-p` path, the loaded
/// cell's directory, then the cell search path when no `-p` path was given.
/// A candidate is a file whose size the file system reports (fopen of
/// PaOpen; one query per candidate); the found file's size goes to `size`.
[[nodiscard]] std::optional<std::string> find_ext_file(const ReadOptions& options, cell::LookupContext& ctx,
                                                       std::string_view name, std::string_view suffix,
                                                       std::uint64_t* size = nullptr);

}  // namespace hefesto::extflat::detail
