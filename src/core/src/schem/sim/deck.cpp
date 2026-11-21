// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 5.2): build_deck fails with CORE-NOTIMPL-007;
// vector_name returns the net name unchanged.
//
// Owner: SIMINT.

#include "hefesto/schem/sim/deck.hpp"

#include "hefesto/schem/not_implemented.hpp"

namespace hefesto::schem {

diag::Outcome<Deck> build_deck(const Session& /*session*/, const Document& /*testbench*/) {
  return not_implemented("schem::build_deck");
}

std::string vector_name(const Session& /*session*/, const HierPath& /*path*/, std::string_view net) {
  return std::string{net};
}

}  // namespace hefesto::schem
