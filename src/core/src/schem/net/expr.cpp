// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 5.2): evaluate_expr returns the expression
// unchanged.
//
// Owner: NETLIST.

#include "hefesto/schem/net/expr.hpp"

namespace hefesto::schem {

std::string evaluate_expr(std::string_view expression, bool /*xschem_compat*/) {
  return std::string{expression};
}

}  // namespace hefesto::schem
