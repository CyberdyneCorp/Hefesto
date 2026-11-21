// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 5.2): pin_order returns file order and ignores
// `sim_pinnumber`.
//
// Owner: SYMLIB.

#include "hefesto/schem/sym/symbol.hpp"

#include <numeric>

namespace hefesto::schem {

std::vector<PinIndex> pin_order(const Symbol& symbol) {
  std::vector<PinIndex> order(symbol.pins.size());
  std::iota(order.begin(), order.end(), PinIndex{0});
  return order;
}

}  // namespace hefesto::schem
