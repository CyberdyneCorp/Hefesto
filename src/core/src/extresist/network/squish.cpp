// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Network simplification and the final scrunch (capability
// extresist-network "Network simplification", "Final scrunch pass"):
// ResSimplifyNet with its dead-end, series and propagation steps,
// ResMoveDevices, and ResScrunchNet. Q-NETLIST-26 (fix, both modes): a
// list of 10 or more resistors is sorted with a stable sort (Magic's qsort
// order is the C library's). Q-NETLIST-47 (fix, both modes): the final
// pass keeps Magic's insertion order for short lists, and skips the
// resistor when the adjacent resistor to fold into is missing or is the
// list head (Magic dereferences a null pointer in both cases).
// (Magic ref: resis/ResSimple.c:66-714)
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#include <algorithm>
#include <string>
#include <vector>

#include "extresist/network/reduce.hpp"

namespace hefesto::extresist::detail {

void Reducer::reschedule(Id node) {
  g_.nodes[node].status &= ~kStatusMarked;
  g_.remove_from_queue(node, g_.done);
  g_.add_to_queue(node, g_.pending);
}

void Reducer::simplify_step(float tol) {
  const Id node = g_.pending;
  if (node == kNil) {
    return;
  }
  Node& n = g_.nodes[node];
  n.status |= kStatusMarked | kStatusFinished;
  g_.pending = n.more;
  if (n.more != kNil) {
    g_.nodes[n.more].less = kNil;
  }
  g_.add_to_queue(node, g_.done);
  const Directions d = orient(node);
  const bool protected_node = g_.protected_node(node);
  if (d.receive == 0 && d.drive == 1 && !protected_node) {
    dead_end(node, d.r1, tol);
  } else if (d.drive + d.receive == 2 && g_.res[d.r1].value < tol && g_.res[d.r2].value < tol && !protected_node) {
    series(node, d.r1, d.r2, tol);
  } else if (d.receive > 0 && (g_.nodes[node].status & kStatusDoneOnce) == 0) {
    propagate(node, tol);
  }
}

Reducer::Directions Reducer::orient(Id node) {
  Directions d;
  for (Id c = g_.nodes[node].re; c != kNil; c = g_.next(c)) {
    Resistor& r = g_.res[g_.item(c)];
    if ((r.status & kStatusMarked) != 0 && r.node[1] == node) {
      ++d.drive;
    } else {
      if ((r.status & kStatusMarked) == 0 && r.node[1] == node) {
        std::swap(r.node[0], r.node[1]);
      }
      r.status |= kStatusMarked;
      ++d.receive;
    }
    (d.r1 == kNil ? d.r1 : d.r2) = g_.item(c);
  }
  return d;
}

Reducer::FarEnd Reducer::far_end(Id other, Id r1, float tol) const {
  FarEnd f;
  f.big = r1;
  for (Id c = g_.nodes[other].re; c != kNil; c = g_.next(c)) {
    const Id r = g_.item(c);
    const Resistor& x = g_.res[r];
    if (x.node[0] != other) {
      ++f.drivers;
      continue;
    }
    if ((g_.nodes[x.node[1]].status & kStatusMarked) == 0) {
      ++f.pending;
    }
    if ((x.status & kStatusDeadEnd) != 0 || x.value > tol) {
      ++f.marked;
      f.big = x.value >= g_.res[f.big].value ? r : f.big;
    } else {
      ++f.unmarked;
    }
  }
  return f;
}

void Reducer::dead_end(Id node, Id r1, float tol) {
  g_.res[r1].status |= kStatusDeadEnd;
  if (!(g_.res[r1].value < tol)) {
    return;
  }
  const Id other = g_.other_end(r1, node);
  const FarEnd f = far_end(other, r1, tol);
  const int marked = f.marked;
  const int unmarked = f.unmarked;
  const int drivers = f.drivers;
  const int pending = f.pending;
  const Id big = f.big;
  if (marked + unmarked + drivers == 2 || (unmarked == 0 && marked > 1 && big == r1 && pending == 0)) {
    if ((g_.nodes[other].status & kStatusMarked) != 0) {
      reschedule(other);
    }
    return;
  }
  if (unmarked == 0 && ((marked == 1 && drivers > 1) || big != r1)) {
    g_.nodes[other].area += g_.res[r1].area;
    g_.nodes[other].status &= ~kStatusDoneOnce;
    g_.delete_res_pointer(g_.res[r1].node[0], r1);
    g_.delete_res_pointer(g_.res[r1].node[1], r1);
    g_.eliminate_resistor(r1);
    g_.merge_nodes(other, node);
    if ((g_.nodes[other].status & kStatusMarked) != 0) {
      reschedule(other);
    }
    g_.done_with_node(other);
  }
}

void Reducer::move_devices(Id from, Id to) {
  Id c = g_.nodes[from].te;
  while (c != kNil) {
    std::vector<Id>& t = g_.devices[g_.item(c)].terms;
    const std::size_t nterms = t.size();
    if (nterms > 0 && t[0] == from) {
      t[0] = to;
    } else if (nterms > 1 && t[1] == from) {
      t[1] = to;
    } else if (nterms > 2 && t[2] == from) {
      t[2] = to;
    } else if (nterms > 3 && t[3] == from) {
      t[3] = to;
    } else {
      g_.message("Missing Device connection in squish routines at " + std::to_string(g_.nodes[from].loc.x) + ", " +
                 std::to_string(g_.nodes[from].loc.y));
    }
    const Id following = g_.next(c);
    g_.cells[c].next = g_.nodes[to].te;
    g_.nodes[to].te = c;
    c = following;
  }
  g_.nodes[from].te = kNil;
}

void Reducer::series(Id node, Id r1, Id r2, float tol) {
  for (const Id r : {r1, r2}) {
    Resistor& x = g_.res[r];
    if ((x.status & kStatusMarked) == 0 && x.node[1] == node) {
      std::swap(x.node[0], x.node[1]);
    }
    x.status |= kStatusMarked;
  }
  const Id n1 = g_.other_end(r1, node);
  const Id n2 = g_.other_end(r2, node);
  const Resistor& a = g_.res[r1];
  const Resistor& b = g_.res[r2];
  const bool to_n1 = ((a.status & kStatusDeadEnd) != 0 && a.value < tol / 2) ||
                     ((b.status & kStatusDeadEnd) == 0 && a.value < b.value);
  const Id other = to_n1 ? n1 : n2;
  move_devices(node, other);
  g_.nodes[other].noderes = std::min(g_.nodes[node].noderes, g_.nodes[other].noderes);
  const float sum = g_.res[r1].value + g_.res[r2].value;
  g_.nodes[n2].area += g_.res[r1].value * g_.nodes[node].area / sum;
  g_.nodes[n1].area += g_.res[r2].value * g_.nodes[node].area / sum;
  g_.res[r1].value += g_.res[r2].value;
  g_.res[r1].area += g_.res[r2].area;
  g_.unlink_resistor(r1, g_.res_list);
  g_.add_resistor_to_list(r1, g_.res_list);
  g_.delete_res_pointer(node, r1);
  g_.delete_res_pointer(node, r2);
  g_.delete_res_pointer(n2, r2);
  (g_.res[r1].node[0] == node ? g_.res[r1].node[0] : g_.res[r1].node[1]) = n2;
  g_.push(g_.nodes[n2].re, r1);
  g_.eliminate_resistor(r2);
  g_.nodes[other].status |= g_.nodes[node].status & kStatusMaxTdi;
  g_.clean_node(node, true);
  for (const Id n : {n1, n2}) {
    g_.nodes[n].status &= ~kStatusDoneOnce;
    if ((g_.nodes[n].status & kStatusMarked) != 0) {
      reschedule(n);
    }
  }
  g_.done_with_node(n1);
}

void Reducer::propagate(Id node, float tol) {
  g_.nodes[node].status |= kStatusDoneOnce;
  for (Id c = g_.nodes[node].re; c != kNil; c = g_.next(c)) {
    Resistor& r = g_.res[g_.item(c)];
    if (r.node[0] != node || (r.status & kStatusDoneOnce) != 0) {
      continue;
    }
    if ((g_.nodes[r.node[1]].status & kStatusMarked) != 0) {
      if (r.value > tol) {
        r.status |= kStatusDoneOnce;
      }
      reschedule(r.node[1]);
    }
  }
}

void Reducer::sort_resistors() {
  int count = 0;
  for (Id r = g_.res_list; r != kNil && count < 10; r = g_.res[r].next) {
    ++count;
  }
  if (count >= 10) {
    // Q-NETLIST-26: a stable sort, one order on every platform.
    std::vector<Id> list;
    for (Id r = g_.res_list; r != kNil; r = g_.res[r].next) {
      list.push_back(r);
    }
    std::stable_sort(list.begin(), list.end(), [&](Id a, Id b) { return g_.res[a].value < g_.res[b].value; });
    for (std::size_t i = 0; i < list.size(); ++i) {
      g_.res[list[i]].next = i + 1 == list.size() ? kNil : list[i + 1];
      g_.res[list[i]].last = i == 0 ? kNil : list[i - 1];
    }
    g_.res_list = list.front();
    return;
  }
  // Insertion before the first value >= its own: equal values reverse
  // (Q-NETLIST-47 keeps this order).
  Id local = kNil;
  Id current = g_.res_list;
  while (current != kNil) {
    const Id working = current;
    current = g_.res[current].next;
    g_.unlink_resistor(working, g_.res_list);
    g_.add_resistor_to_list(working, local);
  }
  g_.res_list = local;
}

Reducer::FoldTarget Reducer::fold_target(Id current) const {
  FoldTarget f;
  for (std::size_t end = 0; end < 2; ++end) {
    const Id n = g_.res[current].node[end];
    for (Id c = g_.nodes[n].re; c != kNil; c = g_.next(c)) {
      const Id r = g_.item(c);
      if (r == current) {
        continue;
      }
      ++(end == 0 ? f.c1 : f.c2);
      if (f.working == kNil || g_.res[f.working].value > g_.res[r].value) {
        f.working = r;
        f.node1 = n;
      }
    }
  }
  return f;
}

bool Reducer::scrunch_one(Id current) {
  const FoldTarget f = fold_target(current);
  const Id working = f.working;
  Id node1 = f.node1;
  const int c1 = f.c1;
  const int c2 = f.c2;
  // Q-NETLIST-47: no adjacent resistor, or the adjacent resistor is the list
  // head (Magic unlinks it through its null predecessor): skip the resistor.
  if (working == kNil || g_.res[working].last == kNil) {
    return false;
  }
  if (c1 != 0 && c2 != 0) {
    g_.res[working].value += g_.res[current].value;
    g_.res[working].area += g_.res[current].area;
  } else {
    g_.nodes[node1].area += g_.res[current].area;
  }
  const Id node2 = g_.other_end(current, node1);
  g_.delete_res_pointer(g_.res[current].node[0], current);
  g_.delete_res_pointer(g_.res[current].node[1], current);
  Resistor& w = g_.res[working];
  g_.res[w.last].next = w.next;
  if (w.next != kNil) {
    g_.res[w.next].last = w.last;
  }
  g_.eliminate_resistor(current);
  g_.add_resistor_to_list(working, g_.res_list);
  if (g_.protected_node(node2)) {
    g_.merge_nodes(node2, node1);
    node1 = node2;
  } else {
    g_.merge_nodes(node1, node2);
  }
  g_.remove_from_queue(node1, g_.done);
  g_.add_to_queue(node1, g_.pending);
  g_.nodes[node1].status &= ~(kStatusDoneOnce | kStatusFinished);
  g_.done_with_node(node1);
  return true;
}

void Reducer::scrunch(float tol) {
  if (g_.res_list == kNil) {
    return;
  }
  sort_resistors();
  Id current = g_.res_list;
  while (current != kNil && g_.res[current].value < tol) {
    if (g_.res[g_.res_list].next == kNil) {
      break;
    }
    const bool both_protected =
        g_.protected_node(g_.res[current].node[0]) && g_.protected_node(g_.res[current].node[1]);
    if (both_protected || !scrunch_one(current)) {
      current = g_.res[current].next;
      continue;
    }
    while (g_.pending != kNil) {
      simplify_step(tol);
    }
    current = g_.res_list;
  }
}

}  // namespace hefesto::extresist::detail
