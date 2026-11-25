// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The construction-time reductions (capability extresist-network
// "Construction-time reductions"): ResDoneWithNode with its loop and
// zero-resistor eliminations, ResSeriesCheck, ResParallelCheck and
// ResTriangleCheck. Q-NETLIST-44 (compat): a delta-to-star arm is
// Ra*Rb/S + 0.5 with magic-compat ON and Ra*Rb/S with it OFF. Q-NETLIST-45
// (fix, both modes): a node with 10 or more resistors visits its
// neighbours in resistor-list order, not in hash-table order. Arithmetic is
// single precision as in Magic (the area builds with -ffp-contract=off).
// (Magic ref: resis/ResMerge.c:45-876)
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#include <unordered_map>
#include <utility>
#include <vector>

#include "extresist/network/graph.hpp"

namespace hefesto::extresist::detail {

namespace {

constexpr int kHashThreshold = 10;

int list_length_capped(const Graph& g, Id head) {
  int count = 0;
  for (Id c = head; c != kNil && count < kHashThreshold; c = g.next(c)) {
    ++count;
  }
  return count;
}

}  // namespace

bool Graph::drop_trivial_resistor(Id node) {
  for (Id c = nodes[node].re; c != kNil; c = next(c)) {
    const Id r = item(c);
    if (res[r].node[0] == res[r].node[1]) {  // a loop
      delete_res_pointer(node, r);
      delete_res_pointer(node, r);
      nodes[node].area += res[r].area;
      eliminate_resistor(r);
      done_with_node(node);
      return true;
    }
    if (res[r].value == 0.0F) {  // a short: the nodes merge
      delete_res_pointer(res[r].node[0], r);
      delete_res_pointer(res[r].node[1], r);
      const Id other = other_end(r, node);
      merge_nodes(other, node);
      nodes[other].area += res[r].area;
      eliminate_resistor(r);
      if ((nodes[other].status & kStatusTrue) != 0) {
        nodes[other].status &= ~kStatusTrue;
        done_with_node(other);
      }
      return true;
    }
  }
  return false;
}

void Graph::done_with_node(Id node) {
  nodes[node].status |= kStatusTrue;
  if (nodes[node].re == kNil || geometry || drop_trivial_resistor(node)) {
    return;
  }
  int status = kUntouched;
  if (nodes[node].te == kNil && !protected_node(node)) {
    status = series_check(node);
  }
  if (status == kUntouched && !protected_node(node)) {
    status = parallel_check(node);
  }
  if (status == kUntouched && !protected_node(node)) {
    triangle_check(node);
  }
}

void Graph::fix_series(Id node, Id far, Id other, Id elim, Id keep) {
  const float sum = res[keep].value + res[elim].value;
  nodes[other].area += res[keep].value * nodes[node].area / sum;
  nodes[far].area += res[elim].value * nodes[node].area / sum;
  res[keep].value += res[elim].value;
  res[keep].area += res[elim].area;
  bool found = false;
  for (Id c = nodes[other].re; c != kNil; c = next(c)) {
    if (cells[c].item == elim) {
      cells[c].item = keep;
      found = true;
      break;
    }
  }
  if (!found) {
    message("Resistor not found in duo");
  }
  delete_res_pointer(node, elim);
  delete_res_pointer(node, keep);
  eliminate_resistor(elim);
  clean_node(node, true);
}

int Graph::series_check(Id node) {
  const Id first = nodes[node].re;
  const Id r1 = item(first);
  const Id second = next(first);
  if (second == kNil) {
    // A dangling node: its resistor and area go to the other end.
    const Id other = other_end(r1, node);
    delete_res_pointer(res[r1].node[0], r1);
    delete_res_pointer(res[r1].node[1], r1);
    nodes[other].area += res[r1].area + nodes[node].area;
    eliminate_resistor(r1);
    clean_node(node, true);
    if ((nodes[other].status & kStatusTrue) != 0) {
      nodes[other].status &= ~kStatusTrue;
      done_with_node(other);
    }
    return kSingle;
  }
  if (next(second) != kNil) {
    return kUntouched;
  }
  const Id r2 = item(second);
  const Id far = other_end(r1, node);
  const Id far2 = res[r2].node[0] == node ? res[r2].node[1] : res[r2].node[0];
  int status = kSeries;
  if (r1 == r2 && res[r1].node[0] == node && res[r2].node[0] == node) {
    message("Internal error:  Resistor linked to itself!");
    status = kLoop;
    delete_res_pointer(res[r2].node[0], r2);
    delete_res_pointer(res[r2].node[1], r2);
    nodes[far].area += res[r2].area + res[r2].area + nodes[node].area;
    eliminate_resistor(r2);
    clean_node(node, true);
  } else if (far == far2) {
    status = kLoop;
    delete_res_pointer(res[r1].node[0], r1);
    delete_res_pointer(res[r1].node[1], r1);
    delete_res_pointer(res[r2].node[0], r2);
    delete_res_pointer(res[r2].node[1], r2);
    nodes[far].area += res[r1].area + res[r2].area + nodes[node].area;
    eliminate_resistor(r1);
    eliminate_resistor(r2);
    clean_node(node, true);
  } else {
    // r1's end at `node` moves to r2's far end; r2 and `node` go.
    (res[r1].node[0] == node ? res[r1].node[0] : res[r1].node[1]) = far2;
    fix_series(node, far, far2, r2, r1);
  }
  if ((nodes[far].status & kStatusTrue) != 0) {
    nodes[far].status &= ~kStatusTrue;
    done_with_node(far);
  }
  return status;
}

void Graph::fix_parallel(Id elim, Id keep) {
  const float sum = res[keep].value + res[elim].value;
  if (sum != 0.0F) {
    res[keep].value = (res[keep].value * res[elim].value) / sum;
  } else {
    res[keep].value = 0.0F;
  }
  res[keep].area += res[elim].area;
  delete_res_pointer(res[elim].node[0], elim);
  delete_res_pointer(res[elim].node[1], elim);
  eliminate_resistor(elim);
}

namespace {

/// The parallel pair of a node with 10 or more resistors: for each r2 in
/// list order, the first earlier r1 to the same neighbour.
std::pair<Id, Id> parallel_hashed(const Graph& g, Id node) {
  std::unordered_map<Id, Id> first_to;
  for (Id c = g.nodes[node].re; c != kNil; c = g.next(c)) {
    const auto [it, fresh] = first_to.emplace(g.other_end(g.item(c), node), g.item(c));
    if (!fresh) {
      return {it->second, g.item(c)};
    }
  }
  return {kNil, kNil};
}

bool same_ends(const Resistor& a, const Resistor& b) {
  return (a.node[0] == b.node[0] && a.node[1] == b.node[1]) || (a.node[0] == b.node[1] && a.node[1] == b.node[0]);
}

/// The parallel pair of a shorter list: for each r1, the first later r2.
std::pair<Id, Id> parallel_short(const Graph& g, Id node) {
  for (Id c1 = g.nodes[node].re; g.next(c1) != kNil; c1 = g.next(c1)) {
    for (Id c2 = g.next(c1); c2 != kNil; c2 = g.next(c2)) {
      if (same_ends(g.res[g.item(c1)], g.res[g.item(c2)])) {
        return {g.item(c1), g.item(c2)};
      }
    }
  }
  return {kNil, kNil};
}

}  // namespace

int Graph::parallel_check(Id node) {
  const auto [r1, r2] = list_length_capped(*this, nodes[node].re) >= kHashThreshold ? parallel_hashed(*this, node)
                                                                                    : parallel_short(*this, node);
  if (r1 == kNil) {
    return kUntouched;
  }
  const Id other = other_end(r1, node);
  fix_parallel(r1, r2);
  const bool other_ready = (nodes[other].status & kStatusTrue) != 0;
  if (other_ready) {
    nodes[other].status &= ~kStatusTrue;
  }
  done_with_node(node);
  if (other_ready) {
    done_with_node(other);
  }
  return kParallel;
}

void Graph::star(Id node, Id r1, Id r2, Id r3, Id n1, Id n2) {
  float denom = res[r1].value + res[r2].value + res[r3].value;
  if (denom != 0.0F) {
    denom = static_cast<float>(1.0 / static_cast<double>(denom));
    const float v1 = (res[r1].value * res[r2].value) * denom;
    const float v2 = (res[r2].value * res[r3].value) * denom;
    const float v3 = (res[r1].value * res[r3].value) * denom;
    // Q-NETLIST-44: Magic's leftover rounding term, kept with magic-compat ON.
    const double offset = magic_compat ? 0.5 : 0.0;
    res[r1].value = static_cast<float>(static_cast<double>(v1) + offset);
    res[r2].value = static_cast<float>(static_cast<double>(v2) + offset);
    res[r3].value = static_cast<float>(static_cast<double>(v3) + offset);
  } else {
    res[r1].value = 0.0F;
    res[r2].value = 0.0F;
    res[r3].value = 0.0F;
  }
  const Id x = new_node(nodes[node].loc, kNodeTriangle);
  nodes[x].status = kStatusFinished | kStatusTrue | kStatusMarked;
  add_to_queue(x, done);
  const auto rewire = [&](Id r, Id keep_end) {
    if (res[r].node[0] == keep_end) {
      delete_res_pointer(res[r].node[1], r);
      res[r].node[1] = x;
    } else {
      delete_res_pointer(res[r].node[0], r);
      res[r].node[0] = x;
    }
  };
  rewire(r1, node);
  rewire(r2, n2);
  rewire(r3, n1);
  push(nodes[x].re, r1);
  push(nodes[x].re, r2);
  push(nodes[x].re, r3);
  const auto take_ready = [&](Id n) {
    if ((nodes[n].status & kStatusTrue) == 0) {
      return kNil;
    }
    nodes[n].status &= ~kStatusTrue;
    return n;
  };
  const Id ready1 = take_ready(n1);
  const Id ready2 = take_ready(n2);
  done_with_node(node);
  if (ready1 != kNil) {
    done_with_node(ready1);
  }
  if (ready2 != kNil) {
    done_with_node(ready2);
  }
}

namespace {

/// A delta at a node: N's resistors r1 (to n1) and r2 (to n2), and n1's
/// resistor r3 to n2.
struct Delta {
  Id r1 = kNil;
  Id r2 = kNil;
  Id r3 = kNil;
  Id n1 = kNil;
  Id n2 = kNil;
};

/// The first side n1-n2 in n1's list whose other end is a neighbour of N
/// in `first_to` (skipping r1 and loops on n1).
Delta side_of(const Graph& g, Id n1, Id r1, const std::unordered_map<Id, Id>& first_to) {
  for (Id c3 = g.nodes[n1].re; c3 != kNil; c3 = g.next(c3)) {
    const Id r3 = g.item(c3);
    const Id n2 = g.other_end(r3, n1);
    if (r3 == r1 || n2 == n1) {
      continue;  // a loop on n1 is not a triangle side
    }
    const auto hit = first_to.find(n2);
    if (hit != first_to.end()) {
      return Delta{r1, hit->second, r3, n1, n2};
    }
  }
  return {};
}

/// Q-NETLIST-45: for 10 or more resistors the neighbours are taken in
/// resistor-list order (each through its first resistor), not in Magic's
/// hash-table order.
Delta delta_hashed(const Graph& g, Id node) {
  std::unordered_map<Id, Id> first_to;
  std::vector<Id> neighbours;
  for (Id c = g.nodes[node].re; c != kNil; c = g.next(c)) {
    const Id other = g.other_end(g.item(c), node);
    if (first_to.emplace(other, g.item(c)).second) {
      neighbours.push_back(other);
    }
  }
  for (const Id n1 : neighbours) {
    const Delta d = side_of(g, n1, first_to.at(n1), first_to);
    if (d.r1 != kNil) {
      return d;
    }
  }
  return {};
}

Id joining(const Graph& g, Id n1, Id n2) {
  for (Id c3 = g.nodes[n1].re; c3 != kNil; c3 = g.next(c3)) {
    const auto& e = g.res[g.item(c3)].node;
    if ((e[0] == n1 && e[1] == n2) || (e[1] == n1 && e[0] == n2)) {
      return g.item(c3);
    }
  }
  return kNil;
}

Delta delta_short(const Graph& g, Id node) {
  for (Id c1 = g.nodes[node].re; g.next(c1) != kNil; c1 = g.next(c1)) {
    const Id n1 = g.other_end(g.item(c1), node);
    for (Id c2 = g.next(c1); c2 != kNil; c2 = g.next(c2)) {
      const Id n2 = g.other_end(g.item(c2), node);
      const Id r3 = joining(g, n1, n2);
      if (r3 != kNil) {
        return Delta{g.item(c1), g.item(c2), r3, n1, n2};
      }
    }
  }
  return {};
}

}  // namespace

int Graph::triangle_check(Id node) {
  const Delta d = list_length_capped(*this, nodes[node].re) >= kHashThreshold ? delta_hashed(*this, node)
                                                                              : delta_short(*this, node);
  if (d.r1 == kNil) {
    return kUntouched;
  }
  star(node, d.r1, d.r2, d.r3, d.n1, d.n2);
  return kTriangle;
}

}  // namespace hefesto::extresist::detail
