// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extresist network construction and reduction (capability
// extresist-network; openspec/changes/m7-extraction/design.md D5, D16):
// a net's geometry is yanked into a scratch cell, fractured with the
// restricted stitch editor (db/plane_stitch.hpp, design D6), turned into a
// resistor network with devices and breakpoints, and reduced (delta-star,
// series and parallel merges, Elmore pruning, simplification). HeapState
// is the path-resistance heap kept across nets with magic-compat ON
// (Q-NETLIST-46).
//
// Scaffold state (tasks.md 3.3): every function reports CORE-NOTIMPL-008
// until RESNET lands it. db::StitchEditor is declared here only; it lands
// with task 3.4 (INTEGRATION, MODEL review).
//
// Owner: INTEGRATION (this header); RESNET (the implementation).

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/tech/style.hpp"

namespace hefesto::db {
class StitchEditor;  // db/plane_stitch.hpp (tasks.md 3.4)
}

namespace hefesto::extresist {

/// A net's geometry in a scratch cell (never registered in the cell table).
struct NetGeometry {
  std::string net;
  std::unique_ptr<db::CellDef> scratch;
  db::Point origin;
};

/// A node of the network (resNode).
struct NetNode {
  db::Point location;
  std::string name;
  double cap = 0.0;  // attofarads
};

/// A resistor between two nodes (resResistor), in milliohms.
struct NetResistor {
  std::size_t a = 0;
  std::size_t b = 0;
  double resistance = 0.0;
};

/// A device terminal attached to a node (resDevice).
struct NetDevice {
  std::string name;
  std::vector<std::size_t> terminals;
};

/// The R-C network of one net.
struct Network {
  std::vector<NetNode> nodes;
  std::vector<NetResistor> resistors;
  std::vector<NetDevice> devices;
  std::vector<db::Point> breakpoints;
};

/// The devices on the net, from the `.ext` input model.
struct DeviceAnnotations {
  std::vector<NetDevice> devices;
};

/// Reduction tolerances (`minresist`, `mindelay`, `simplify`).
struct Tolerances {
  double min_resistance = 1000.0;  // milliohms
  double min_delay = 1.0e9;        // zeptoseconds (1 ps)
  bool simplify = true;
};

/// The path-resistance heap state (Q-NETLIST-25, Q-NETLIST-46).
struct HeapState {
  std::vector<std::int64_t> keys;
};

/// Yanks `net` (the net through `at` on `type`) into a scratch cell, minus
/// the `skip` types.
[[nodiscard]] diag::Outcome<NetGeometry> yank_net(const db::CellDef& cell, db::Point at, tech::TileType type,
                                                  const tech::TypeMask& skip);
/// ResFracture over `area` of the scratch plane the editor wraps.
[[nodiscard]] diag::Status fracture(db::StitchEditor& editor, const db::Rect& area);
[[nodiscard]] diag::Outcome<Network> build_network(const NetGeometry& geometry, const extract::ExtStyle& style,
                                                   const DeviceAnnotations& devices);
[[nodiscard]] diag::Status reduce(Network& network, const Tolerances& tolerances, HeapState& heap,
                                  const compat::CompatSettings& compat);

}  // namespace hefesto::extresist
