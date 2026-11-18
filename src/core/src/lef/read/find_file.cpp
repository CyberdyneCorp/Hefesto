// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF file lookup, scaffold stub (openspec/changes/m6-lef-def-oasis/design.md
// D6).
//
// Owner: LEFR (design D2).

#include "hefesto/lef/read/find_file.hpp"

namespace hefesto::lef {

std::optional<cell::FoundFile> find_lef_file(cell::LookupContext& /*ctx*/, std::string_view /*name*/) {
  return std::nullopt;
}

}  // namespace hefesto::lef
