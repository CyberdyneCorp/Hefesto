// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 4.2): both datasets fail with CORE-NOTIMPL-009.
//
// Owner: SIMPROBE.

#include "hefesto/sim/probe/overlay_feed.hpp"

#include "hefesto/sim/not_implemented.hpp"

namespace hefesto::sim {

diag::Outcome<std::vector<NetVoltage>> OverlayFeed::voltages(const Job& /*job*/, double /*time*/) const {
  return not_implemented("sim::OverlayFeed::voltages");
}

diag::Outcome<std::vector<EdgeCurrent>> OverlayFeed::currents(const Job& /*job*/, double /*time*/) const {
  return not_implemented("sim::OverlayFeed::currents");
}

}  // namespace hefesto::sim
