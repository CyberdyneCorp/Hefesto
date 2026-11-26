// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The extresist-network entry points (network.hpp): yank_net
// (DBTreeCopyConnect of the net with the `skip` copy masks of
// Q-NETLIST-23), build_network (ResExtractNet after the copy) and reduce
// (ResDoSimplify). The scaffold overloads without a technology fail with
// EXTRESIST-NET-001.
// (Magic ref: resis/ResMain.c:1243-1420, resis/ResRex.c:200-220, :572-588)
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#include <memory>
#include <string>
#include <utility>

#include "extresist/network/builder.hpp"
#include "extresist/network/reduce.hpp"
#include "hefesto/connect/search.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/extresist/network/network.hpp"

namespace hefesto::extresist {

namespace {

/// The name of every scratch cell yank_net makes (Magic's __RESIS__); no
/// cell of a library may have it, so no cell plane is ever fractured.
constexpr const char* kScratchName = "__RESIS__";

diag::Diagnostic missing_technology(const char* what) {
  return diag::make_native("EXTRESIST-NET-001",
                           std::string("extresist ") + what + " needs the technology (use the M7 R4 overload)");
}

class Copier final : public connect::NetVisitor {
 public:
  Copier(NetGeometry& out, const tech::Technology& tech, bool magic_compat)
      : out_(out), tech_(tech), compat_(magic_compat) {}

  bool on_tile(const connect::NetTile& t) override {
    const db::PaintRow row = db::paint_row(tech_, t.plane, t.tile.type);
    db::Plane& plane = out_.scratch->plane(t.plane);
    const diag::Status st = t.tile.split
                                ? plane.paint_triangle(t.tile.area, t.tile.diagonal, t.tile.right_side, row, compat_)
                                : plane.paint(t.tile.area, row);
    return st.ok();
  }

  bool on_label(const connect::NetLabel& l) override {
    out_.labels.push_back(l.copy);
    return true;
  }

 private:
  NetGeometry& out_;
  const tech::Technology& tech_;
  bool compat_;
};

/// Design D6 / R6: only a scratch plane of yank_net may be fractured.
bool scratch_only(const NetGeometry& geometry) {
  return geometry.scratch != nullptr && geometry.scratch->name() == kScratchName &&
         geometry.scratch.get() != geometry.source;
}

}  // namespace

diag::Outcome<NetGeometry> yank_net(const db::CellDef& /*cell*/, db::Point /*at*/, tech::TileType /*type*/,
                                    const tech::TypeMask& /*skip*/) {
  return missing_technology("yank");
}

diag::Outcome<NetGeometry> yank_net(const db::CellDef& cell, const tech::Technology& technology,
                                    const YankRequest& request) {
  NetGeometry out;
  out.scratch = std::make_unique<db::CellDef>(kScratchName, technology.plane_count());
  out.source = &cell;
  out.origin = db::Point{(request.area.x0 + request.area.x1) / 2, (request.area.y0 + request.area.y1) / 2};
  connect::SearchStart start;
  start.area = request.area;
  start.types = request.start_types;
  connect::SearchOptions options;
  options.magic_compat = request.magic_compat;
  options.labels = connect::LabelPolicy::full;
  // ResCopyMask: the connectivity with every `skip` type removed, and a
  // skipped type connecting to nothing (Q-NETLIST-23).
  const tech::TypeMask skip = request.skip;
  options.connect = [&technology, skip](tech::TileType t) {
    if (skip.test(t)) {
      return tech::TypeMask{};
    }
    return technology.connects_to(t) & ~skip;
  };
  Copier copier(out, technology, request.magic_compat);
  const diag::Status st = connect::search_hierarchical(cell, technology, start, options, copier);
  if (!st.ok()) {
    return st.error();
  }
  return out;
}

diag::Outcome<Network> build_network(const NetGeometry& /*geometry*/, const extract::ExtStyle& /*style*/,
                                     const DeviceAnnotations& /*devices*/) {
  return missing_technology("network");
}

diag::Outcome<Network> build_network(const NetGeometry& geometry, const extract::ExtStyle& style,
                                     const DeviceAnnotations& devices, const BuildInput& input) {
  if (input.technology == nullptr) {
    return missing_technology("network");
  }
  if (!scratch_only(geometry)) {
    return diag::make_native("EXTRESIST-NET-002",
                             "extresist network: only a scratch cell of yank_net may be fractured");
  }
  detail::Builder builder(geometry, style, devices, input);
  if (!builder.run()) {
    Network failed;
    failed.failed = true;
    failed.messages = builder.g.messages;
    failed.added_sinks = builder.added_sinks;
    return failed;
  }
  return builder.finish();
}

diag::Status reduce(Network& network, const Tolerances& tolerances, HeapState& heap,
                    const compat::CompatSettings& compat) {
  const bool on = compat.magic_compat;
  detail::Graph g = detail::Graph::from_network(network);
  g.magic_compat = on;
  const detail::Id driver = network.driver_source ? static_cast<detail::Id>(*network.driver_source) : detail::kNil;
  // Q-NETLIST-46: the session's heap state with magic-compat ON; a fresh
  // queue per net with it OFF.
  detail::Reducer reducer(g, on, on ? heap.built : 0, driver, static_cast<float>(network.node_cap));
  reducer.run(tolerances);
  if (on) {
    heap.built = reducer.heap_built();
  }
  Network out = g.to_network();
  out.driver_source = std::nullopt;
  out.node_cap = static_cast<double>(reducer.node_cap());
  out.max_resistance = static_cast<double>(reducer.max_res());
  out.tdi = static_cast<double>(reducer.tdi());
  out.input_devices = std::move(network.input_devices);
  out.added_sinks = std::move(network.added_sinks);
  out.breakpoints = std::move(network.breakpoints);
  network = std::move(out);
  return diag::Done{};
}

void merge_nodes(Network& network, std::size_t keep, std::size_t gone, const compat::CompatSettings& compat) {
  if (keep == gone || keep >= network.nodes.size() || gone >= network.nodes.size()) {
    return;
  }
  detail::Graph g = detail::Graph::from_network(network);
  g.magic_compat = compat.magic_compat;
  g.merge_nodes(static_cast<detail::Id>(keep), static_cast<detail::Id>(gone));
  g.done_with_node(static_cast<detail::Id>(keep));
  Network out = g.to_network();
  out.node_cap = network.node_cap;
  out.max_resistance = network.max_resistance;
  out.tdi = network.tdi;
  out.input_devices = std::move(network.input_devices);
  out.added_sinks = std::move(network.added_sinks);
  out.breakpoints = std::move(network.breakpoints);
  network = std::move(out);
}

}  // namespace hefesto::extresist
