// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS decoder, encoder and repetition expansion, scaffold stubs
// (openspec/changes/m6-lef-def-oasis/design.md D4, D11).
//
// Owner: OASFMT (design D2).

#include "hefesto/cmd/stubs.hpp"
#include "hefesto/oasis/codec/decoder.hpp"
#include "hefesto/oasis/codec/elements.hpp"
#include "hefesto/oasis/codec/encoder.hpp"

namespace hefesto::oasis {

std::vector<Point64> expand(const Repetition& /*repetition*/) {
  return {Point64{}};
}

diag::Outcome<std::optional<Record>> Decoder::next() {
  return cmd::not_implemented_diagnostic("oasis decoder");
}

diag::Status Encoder::begin() {
  return cmd::not_implemented_status("oasis encoder");
}

diag::Status Encoder::begin_cell(std::string_view /*name*/) {
  return cmd::not_implemented_status("oasis encoder");
}

diag::Status Encoder::element(const Element& /*element*/) {
  return cmd::not_implemented_status("oasis encoder");
}

diag::Status Encoder::finish() {
  return cmd::not_implemented_status("oasis encoder");
}

}  // namespace hefesto::oasis
