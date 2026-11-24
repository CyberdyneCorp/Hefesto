// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The coupling pass and its output (ExtCouple.c extFindCoupling,
// extRelocateSubstrateCoupling, extOutputCoupling :160-322; capability
// extract-capacitance "Capacitance option gating", "Coupling to the
// substrate node", "Coupling record output" with Q-EXTRACT-7), and the
// fringe fraction of Open Question 3.
//
// Owner: EXTCAP.

#include <algorithm>
#include <tuple>

#include "cap_context.hpp"
#include "util/cr_atan.hpp"

namespace hefesto::extract {

namespace {

using cap::Context;
using cap::Kinds;

/// extFindCoupling: per plane in plane-number order, overlap then edges.
diag::Status run(const Context& ctx, const Kinds& kinds) {
  for (std::size_t p = tech::kFirstUserPlane; p < ctx.plane_end(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    if (kinds.overlap) {
      cap::overlap_plane(ctx, plane);
    }
    if (kinds.sidewall || kinds.sideoverlap || kinds.corners) {
      cap::couple_plane(ctx, plane, kinds);
    }
  }
  return diag::Done{};
}

Kinds only(bool overlap, bool sidewall, bool sideoverlap, bool corners) {
  Kinds k;
  k.overlap = overlap;
  k.sidewall = sidewall;
  k.sideoverlap = sideoverlap;
  k.corners = corners;
  return k;
}

}  // namespace

std::optional<std::size_t> client_node(const db::Plane& plane, const db::TileView& tile, std::size_t node_count) {
  const std::uintptr_t c = plane.client(tile);
  if (c == db::kClientDefault || c >= node_count) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(c);
}

diag::Status find_coupling(NodeRegionList& nodes, const CapInputs& in, CouplingTable& out) {
  Context ctx(in, &nodes, &out);
  if (!ctx.valid()) {
    return cap::missing_inputs("coupling capacitance");
  }
  if (!ctx.options().coupling) {
    return diag::Done{};
  }
  return run(ctx, Kinds{});
}

diag::Status overlap_caps(NodeRegionList& nodes, const CapInputs& in, CouplingTable& out) {
  Context ctx(in, &nodes, &out);
  if (!ctx.valid()) {
    return cap::missing_inputs("overlap capacitance");
  }
  return run(ctx, only(true, false, false, false));
}

diag::Status sidewall_caps(const NodeRegionList& nodes, const CapInputs& in, CouplingTable& out) {
  // Sidewall coupling never changes a node; a copy keeps the context's
  // node list writable.
  NodeRegionList scratch = nodes;
  Context ctx(in, &scratch, &out);
  if (!ctx.valid()) {
    return cap::missing_inputs("sidewall capacitance");
  }
  return run(ctx, only(false, true, false, false));
}

diag::Status sideoverlap_caps(NodeRegionList& nodes, const CapInputs& in, FringeModel model, CouplingTable& out) {
  Context ctx(in, &nodes, &out);
  if (!ctx.valid()) {
    return cap::missing_inputs("sideoverlap capacitance");
  }
  ctx.options().fringehalo = model == FringeModel::fringe_halo;
  return run(ctx, only(false, false, true, false));
}

diag::Status corner_caps(NodeRegionList& nodes, const CapInputs& in, double factor, CouplingTable& out) {
  Context ctx(in, &nodes, &out);
  if (!ctx.valid()) {
    return cap::missing_inputs("corner capacitance");
  }
  ctx.options().corners = true;
  ctx.options().corner_factor = factor;
  return run(ctx, only(false, false, false, true));
}

void relocate_substrate_coupling(NodeRegionList& nodes, std::optional<std::size_t> substrate, CouplingTable& table) {
  if (!substrate) {
    return;
  }
  for (const auto& [key, value] : table.pairs()) {
    if (value == 0) {
      continue;
    }
    const auto [a, b] = key;
    const std::size_t other = a == *substrate ? b : (b == *substrate ? a : nodes.size());
    if (other < nodes.size()) {
      nodes[other].substrate_cap += value;
      table.set(a, b, 0.0);
    }
  }
}

std::vector<CouplingRecord> coupling_records(const CouplingTable& table, std::span<const std::string> names,
                                             double cscale) {
  std::vector<CouplingRecord> out;
  for (const auto& [key, value] : table.pairs()) {
    const double cap = value / cscale;
    if (cap == 0 || key.first >= names.size() || key.second >= names.size()) {
      continue;
    }
    const std::string& a = names[key.first];
    const std::string& b = names[key.second];
    out.push_back(a <= b ? CouplingRecord{a, b, cap} : CouplingRecord{b, a, cap});
  }
  std::stable_sort(out.begin(), out.end(), [](const CouplingRecord& x, const CouplingRecord& y) {
    return std::tie(x.first, x.second) < std::tie(y.first, y.second);
  });
  return out;
}

double node_cap_output(double cap, double cscale) noexcept {
  const double v = cap / cscale;
  return v < 0 ? 0.0 : v;
}

double fringe_fraction(double mult, double d) {
  return 0.6366 * util::cr_atan(mult * d);
}

}  // namespace hefesto::extract
