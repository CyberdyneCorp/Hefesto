// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASIS content stamps and file lookup, scaffold stubs
// (openspec/changes/m6-lef-def-oasis/design.md D12).
//
// Owner: OASR (design D2).

#include "hefesto/oasis/read/find_file.hpp"
#include "hefesto/oasis/read/stamps.hpp"

#include "hefesto/gds/stream/io.hpp"

namespace hefesto::oasis {

gds::StampTable content_stamps(gds::ByteSource& /*source*/) {
  return {};
}

std::optional<cell::FoundFile> find_oasis_file(cell::LookupContext& /*ctx*/, std::string_view /*name*/) {
  return std::nullopt;
}

}  // namespace hefesto::oasis
