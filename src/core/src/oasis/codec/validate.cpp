// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Validation and CBLOCK compression, scaffold stubs
// (openspec/changes/m6-lef-def-oasis/design.md D11).
//
// Owner: OASFMT (design D2).

#include "hefesto/oasis/codec/cblock.hpp"
#include "hefesto/oasis/codec/validate.hpp"

#include "hefesto/cmd/stubs.hpp"
#include "hefesto/gds/stream/io.hpp"

namespace hefesto::oasis {

diag::Outcome<ValidationResult> validate(gds::ByteSource& /*source*/) {
  return cmd::not_implemented_diagnostic("oasis validation");
}

diag::Outcome<std::vector<std::byte>> inflate_cblock(std::span<const std::byte> /*compressed*/,
                                                     std::uint64_t /*uncompressed_size*/, std::uint64_t /*cap*/) {
  return cmd::not_implemented_diagnostic("oasis read");
}

diag::Outcome<std::vector<std::byte>> deflate_cblock(std::span<const std::byte> /*raw*/, int /*level*/) {
  return cmd::not_implemented_diagnostic("oasis write");
}

}  // namespace hefesto::oasis
