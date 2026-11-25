// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 4.2): node_name, resolve_probe and probe fail
// with CORE-NOTIMPL-009 until SIMPROBE lands them (task 14.2).
//
// Owner: SIMPROBE.

#include "hefesto/sim/probe/probe.hpp"

#include "hefesto/sim/not_implemented.hpp"

namespace hefesto::sim {

diag::Outcome<std::string> node_name(const LayoutSelection& /*selection*/) {
  return not_implemented("sim::node_name");
}

diag::Outcome<NetNameEntry> resolve_probe(const NetNameMap& /*map*/, const LayoutSelection& /*selection*/,
                                          const OnSelectedNode& /*on_selected_node*/) {
  return not_implemented("sim::resolve_probe");
}

diag::Outcome<TraceId> probe(const Job& /*job*/, const ProbeTarget& /*target*/) {
  return not_implemented("sim::probe");
}

}  // namespace hefesto::sim
