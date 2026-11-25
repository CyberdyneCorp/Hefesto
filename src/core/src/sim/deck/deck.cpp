// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 4.2): both functions fail with CORE-NOTIMPL-009.
//
// Owner: SIMDECK.

#include "hefesto/sim/deck/deck.hpp"

#include "hefesto/sim/not_implemented.hpp"

namespace hefesto::sim {

diag::Outcome<ModelDescriptor> ModelDescriptor::load(std::string_view /*pdk*/) {
  return not_implemented("sim::ModelDescriptor::load");
}

diag::Outcome<PreparedDeck> prepare(const Testbench& /*testbench*/, const DeckContext& /*context*/) {
  return not_implemented("sim::prepare");
}

}  // namespace hefesto::sim
