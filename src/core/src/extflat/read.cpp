// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `.ext` read-and-flatten subset, scaffold stub
// (openspec/changes/m6-lef-def-oasis/design.md D4, D10).
//
// Owner: EXTSUB (design D2).

#include "hefesto/extflat/read.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::extflat {

diag::Outcome<Netlist> read_flat(cell::LookupContext& /*ctx*/, std::string_view /*root*/,
                                 const ReadOptions& /*options*/) {
  return cmd::not_implemented_diagnostic("extflat read_flat");
}

}  // namespace hefesto::extflat
