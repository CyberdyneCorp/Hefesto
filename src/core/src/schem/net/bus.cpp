// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 5.2): expand_bus treats every name as a plain
// name.
//
// Owner: NETLIST.

#include "hefesto/schem/net/bus.hpp"

namespace hefesto::schem {

std::vector<std::string> expand_bus(std::string_view name) {
  return {std::string{name}};
}

}  // namespace hefesto::schem
