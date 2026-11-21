// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (tasks.md 5.2): netlist fails with CORE-NOTIMPL-007 and an
// empty text.
//
// Owner: NETLIST.

#include "hefesto/schem/net/netlist.hpp"

#include "hefesto/schem/not_implemented.hpp"

namespace hefesto::schem {

NetlistResult netlist(const Session& /*session*/, const Document& /*top*/, const NetlistOptions& /*options*/,
                      const compat::CompatSettings& /*compat*/) {
  NetlistResult result;
  result.failed = true;
  result.diagnostics.push_back(not_implemented("schem::netlist"));
  return result;
}

}  // namespace hefesto::schem
