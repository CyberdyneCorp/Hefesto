// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ResDoSimplify (capability extresist-network "Path resistance and
// capacitance distribution", "Elmore delay and pruning", "Network
// simplification", "Final scrunch pass"):
// - ResSetPathRes with Magic's heap and int keys (magic-compat ON) or a
//   per-net stable queue with double keys (OFF) (Q-NETLIST-25, 46);
// - ResDistributeCapacitance;
// - Cdown with L*W*gateCap + 2*W*sdCap in both modes (Q-NETLIST-24), Tdi
//   and ResPruneTree;
// - ResSimplifyNet;
// - ResScrunchNet with a stable sort (Q-NETLIST-26) and its two null cases
//   skipped in both modes (Q-NETLIST-47).
// (Magic ref: resis/ResSimple.c:66-1211)
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#include <algorithm>
#include <string>
#include <vector>

#include "extresist/network/heap.hpp"
#include "extresist/network/reduce.hpp"

namespace hefesto::extresist::detail {

namespace {

constexpr float kNoCap = -1.0F;

}  // namespace

// --- path resistance ---

void Reducer::path_node(Id node) {
  g_.nodes[node].status |= kStatusFinished;
  for (Id c = g_.nodes[node].re; c != kNil; c = g_.next(c)) {
    const Id r = g_.item(c);
    if ((g_.res[r].status & kStatusHeap) != 0) {
      continue;
    }
    const Id other = g_.res[r].node[0] == node ? g_.res[r].node[1] : g_.res[r].node[0];
    if ((g_.nodes[other].status & kStatusFinished) != 0) {
      continue;
    }
    if (compat_) {
      // int + float in Magic, truncated by HeapAddInt.
      const float key = static_cast<float>(static_cast<std::int32_t>(g_.nodes[node].noderes)) + g_.res[r].value;
      magic_->add(static_cast<std::int32_t>(key), r);
    } else {
      stable_.add(g_.nodes[node].noderes + static_cast<double>(g_.res[r].value), r);
    }
  }
}

void Reducer::path_res(Id r) {
  Resistor& x = g_.res[r];
  x.status |= kStatusHeap;
  x.status &= ~kStatusMarked;
  const bool f0 = (g_.nodes[x.node[0]].status & kStatusFinished) != 0;
  const bool f1 = (g_.nodes[x.node[1]].status & kStatusFinished) != 0;
  if (f0 && f1) {
    x.status |= kStatusTdiIgnore;
    return;
  }
  if (!f0) {
    std::swap(x.node[0], x.node[1]);
  }
  const double base = g_.nodes[x.node[0]].noderes;
  double value = base + static_cast<double>(x.value);
  if (compat_) {
    value =
        static_cast<double>(static_cast<std::int32_t>(static_cast<float>(static_cast<std::int32_t>(base)) + x.value));
  }
  g_.nodes[x.node[1]].noderes = value;
  path_node(x.node[1]);
}

bool Reducer::set_path_res() {
  for (Id n = g_.done; n != kNil; n = g_.nodes[n].more) {
    Node& node = g_.nodes[n];
    if (node.noderes == 0) {
      g_.origin = n;
      node.status |= kStatusFinished;
    } else {
      node.noderes = kResInfinity;
      node.status &= ~kStatusFinished;
    }
  }
  if (g_.origin == kNil) {
    if (driver_source_ == kNil) {
      g_.message("Error:  No device type found at the driver location");
      return false;
    }
    g_.origin = driver_source_;
    g_.nodes[g_.origin].why = kNodeOrigin;
    g_.nodes[g_.origin].noderes = 0;
  }
  path_node(g_.origin);
  while (true) {
    const std::optional<std::uint32_t> r = compat_ ? magic_->remove_top() : stable_.remove_top();
    if (!r) {
      break;
    }
    path_res(*r);
  }
  return true;
}

// --- capacitance and delay ---

void Reducer::distribute_capacitance(float total) {
  float area = 0.0F;
  for (Id n = g_.done; n != kNil; n = g_.nodes[n].more) {
    for (Id c = g_.nodes[n].re; c != kNil; c = g_.next(c)) {
      if (g_.res[g_.item(c)].area != 0.0F) {
        g_.message("Nonnull resistor area");
      }
    }
    area += g_.nodes[n].area;
  }
  if (area == 0.0F) {
    g_.message("Error: Node with no area.");
    return;
  }
  const float per_area = total / area;
  for (Id n = g_.done; n != kNil; n = g_.nodes[n].more) {
    g_.nodes[n].area *= per_area;
  }
}

float Reducer::child_capacitance(Id me) {
  if (g_.nodes[me].delay != kNil) {
    return kNoCap;  // a loop
  }
  g_.delays.push_back(Delay{g_.nodes[me].area, 0.0F});
  const Id d = static_cast<Id>(g_.delays.size() - 1);
  g_.nodes[me].delay = d;
  for (Id c = g_.nodes[me].te; c != kNil; c = g_.next(c)) {
    const Device& dev = g_.devices[g_.item(c)];
    if (!dev.terms.empty() && dev.terms[0] == me) {
      // Q-NETLIST-24: two gate-diffusion edges of length W.
      const double load = static_cast<double>(dev.length) * static_cast<double>(dev.width) * dev.gate_cap +
                          2.0 * static_cast<double>(dev.width) * dev.sd_cap;
      g_.delays[d].cdown = static_cast<float>(static_cast<double>(g_.delays[d].cdown) + load);
    }
  }
  for (Id c = g_.nodes[me].re; c != kNil; c = g_.next(c)) {
    const Resistor& r = g_.res[g_.item(c)];
    if (r.node[0] != me || (r.status & kStatusTdiIgnore) != 0) {
      continue;
    }
    const float child = child_capacitance(r.node[1]);
    if (child == kNoCap) {
      return kNoCap;
    }
    g_.delays[d].cdown += child;
  }
  return g_.delays[d].cdown;
}

void Reducer::calc_tdi(Id node, Id via) {
  const Id d = g_.nodes[node].delay;
  if (d == kNil) {
    return;
  }
  if (via == kNil) {
    g_.delays[d].tdi = g_.delays[d].cdown * 0.0F;
  } else {
    const Id up = g_.nodes[g_.res[via].node[0]].delay;
    g_.delays[d].tdi = g_.delays[d].cdown * g_.res[via].value + (up == kNil ? 0.0F : g_.delays[up].tdi);
  }
  for (Id c = g_.nodes[node].re; c != kNil; c = g_.next(c)) {
    const Id r = g_.item(c);
    if (g_.res[r].node[0] == node && (g_.res[r].status & kStatusTdiIgnore) == 0) {
      calc_tdi(g_.res[r].node[1], r);
    }
  }
}

void Reducer::prune(Id node, float min_tdi) {
  Id c = g_.nodes[node].re;
  while (c != kNil) {
    const Id r = g_.item(c);
    c = g_.next(c);
    if ((g_.res[r].status & kStatusTdiIgnore) == 0 && g_.res[r].node[0] == node) {
      prune(g_.res[r].node[1], min_tdi);
    }
  }
  const Id head = g_.nodes[node].re;
  if (head == kNil || g_.next(head) != kNil || g_.res[g_.item(head)].node[1] != node || g_.protected_node(node)) {
    return;
  }
  const Id d = g_.nodes[node].delay;
  if (d == kNil) {
    g_.message("Internal Error in Tree Pruning: Missing TDi value.");
    return;
  }
  if (g_.delays[d].tdi < min_tdi) {
    const Id r = g_.item(head);
    const Id up = g_.res[r].node[0];
    g_.delete_res_pointer(up, r);
    g_.delete_res_pointer(node, r);
    g_.merge_nodes(up, node);
    g_.eliminate_resistor(r);
  }
}

void Reducer::delay(float min_delay) {
  if (g_.origin == kNil) {
    g_.message("Error:  Network simplification:  Failed to to get origin node.");
    tdi_ = 0.0F;
    return;
  }
  if (min_delay <= 0.0F) {
    tdi_ = 0.0F;
    return;
  }
  const float total = node_cap_ == kNoCap ? kNoCap : child_capacitance(g_.origin);
  if (total == kNoCap) {
    tdi_ = -1.0F;
    return;
  }
  node_cap_ = total;
  calc_tdi(g_.origin, kNil);
  const Id head_delay = g_.done == kNil ? kNil : g_.nodes[g_.done].delay;
  tdi_ = head_delay == kNil ? 0.0F : g_.delays[head_delay].tdi;
  Id slow = g_.done;
  for (Id n = g_.done; n != kNil; n = g_.nodes[n].more) {
    const Id d = g_.nodes[n].delay;
    if (d != kNil && tdi_ < g_.delays[d].tdi) {
      slow = n;
      tdi_ = g_.delays[d].tdi;
    }
  }
  if (slow != kNil) {
    g_.nodes[slow].status |= kStatusMaxTdi;
  }
}

// --- the run ---

void Reducer::run(const Tolerances& tol) {
  if (!set_path_res()) {
    return;
  }
  float big = 0.0F;
  for (Id n = g_.done; n != kNil; n = g_.nodes[n].more) {
    big = std::max(big, static_cast<float>(g_.nodes[n].noderes));
  }
  max_res_ = big;
  distribute_capacitance(node_cap_);
  if (!tol.simplify && !tol.lumped) {
    return;
  }
  for (Id r = g_.res_list; r != kNil; r = g_.res[r].next) {
    g_.res[r].status &= ~kStatusHeap;
  }
  const auto min_delay = static_cast<float>(tol.min_delay);
  delay(min_delay);
  if (!tol.simplify) {
    return;
  }
  for (Id n = g_.done; n != kNil; n = g_.nodes[n].more) {
    if (g_.nodes[n].noderes == 0) {
      g_.origin = n;
    }
    g_.nodes[n].status |= kStatusFinished;
  }
  if (g_.origin == kNil) {
    return;
  }
  if (tdi_ != -1.0F && min_delay > 0.0F) {
    prune(g_.origin, min_delay);
  }
  const Id o = g_.origin;
  g_.nodes[o].status &= ~kStatusMarked;
  g_.remove_from_queue(o, g_.done);
  g_.pending = o;
  const auto minres = static_cast<float>(tol.min_resistance);
  while (g_.pending != kNil) {
    simplify_step(minres);
  }
  scrunch(minres);
}

}  // namespace hefesto::extresist::detail
