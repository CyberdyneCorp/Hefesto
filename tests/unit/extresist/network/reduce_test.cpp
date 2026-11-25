// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Node merging, the construction-time reductions and ResDoSimplify
// (openspec/specs/extresist-network "Node merging", "Construction-time
// reductions", "Path resistance and capacitance distribution", "Elmore
// delay and pruning", "Network simplification", "Final scrunch pass";
// tasks.md 13.4-13.6; Q-NETLIST-24, 25, 26, 44, 45, 46, 47) on hand-built
// networks.

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>
#include <vector>

#include "extresist/network/graph.hpp"
#include "extresist/network/heap.hpp"
#include "extresist/network/reduce.hpp"
#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/extresist/network/network.hpp"

namespace db = hefesto::db;
namespace extresist = hefesto::extresist;
namespace detail = hefesto::extresist::detail;
using detail::Id;

namespace {

constexpr std::uint32_t kDone = extresist::kStatusFinished | extresist::kStatusMarked | extresist::kStatusTrue;

/// A working network with every node on the done list.
struct G {
  detail::Graph g;

  Id node(std::uint32_t why = extresist::kNodeJunction, std::uint32_t status = kDone) {
    const Id n = g.new_node(db::Point{static_cast<db::Coord>(g.nodes.size()), 0}, why);
    g.nodes[n].status = status;
    g.add_to_queue(n, g.done);
    return n;
  }
  Id res(Id a, Id b, float value) {
    const Id r = g.new_resistor(a, b);
    g.res[r].value = value;
    return r;
  }
  /// A device whose gate is `n` (a device link keeps `n` from the series
  /// check).
  void device(Id n) {
    detail::Device d;
    d.terms = {n, detail::kNil, detail::kNil, detail::kNil};
    g.devices.push_back(d);
    g.push(g.nodes[n].te, static_cast<Id>(g.devices.size() - 1));
  }
  [[nodiscard]] std::vector<Id> list() const {
    std::vector<Id> out;
    for (Id r = g.res_list; r != detail::kNil; r = g.res[r].next) {
      out.push_back(r);
    }
    return out;
  }
  [[nodiscard]] std::set<Id> ends_of_resistors(Id n) const {
    std::set<Id> out;
    for (Id c = g.nodes[n].re; c != detail::kNil; c = g.next(c)) {
      out.insert(g.other_end(g.item(c), n));
    }
    return out;
  }
};

/// A network for reduce(): nodes and resistors with Magic's prepend order.
struct Net {
  extresist::Network n;

  std::size_t node(std::uint32_t kind = extresist::kNodeJunction, double area = 0.0,
                   double path = extresist::kResInfinity) {
    extresist::NetNode x;
    x.location = db::Point{static_cast<db::Coord>(n.nodes.size()) * 10, 0};
    x.kind = kind;
    x.status = kDone;
    x.cap = area;
    x.path_resistance = path;
    n.nodes.push_back(x);
    return n.nodes.size() - 1;
  }
  std::size_t origin(double area = 0.0) {
    const std::size_t o = node(extresist::kNodeOrigin, area, 0.0);
    n.origin = o;
    return o;
  }
  std::size_t res(std::size_t a, std::size_t b, double value) {
    extresist::NetResistor r;
    r.a = a;
    r.b = b;
    r.resistance = value;
    n.resistors.insert(n.resistors.begin(), r);
    for (auto& node : n.nodes) {
      for (auto& i : node.resistors) {
        ++i;
      }
    }
    n.nodes[a].resistors.insert(n.nodes[a].resistors.begin(), 0);
    n.nodes[b].resistors.insert(n.nodes[b].resistors.begin(), 0);
    return 0;
  }
};

const extresist::NetNode& node_at(const extresist::Network& n, db::Coord x) {
  for (const auto& node : n.nodes) {
    if (node.location.x == x) {
      return node;
    }
  }
  FAIL("no node at x = " << x);
  return n.nodes.front();
}

hefesto::compat::CompatSettings mode(bool on) {
  hefesto::compat::CompatSettings c;
  c.magic_compat = on;
  return c;
}

}  // namespace

// --- Node merging ---

TEST_CASE("a port's name and kind survive a merge", "[cap:extresist-network][req:Node merging]") {
  G t;
  const std::set<std::string, std::less<>> ports{"out"};
  t.g.port_nodes = &ports;
  const Id k = t.node();
  const Id d = t.node(extresist::kNodeOrigin);
  t.g.nodes[d].name = "out";
  t.g.nodes[d].named = true;
  t.g.nodes[d].area = 5.0F;
  t.g.merge_nodes(k, d);
  CHECK(t.g.nodes[k].why == extresist::kNodeOrigin);
  CHECK(t.g.nodes[k].name == "out");
  CHECK(t.g.nodes[k].area == 5.0F);
  CHECK_FALSE(t.g.nodes[d].alive);
}

TEST_CASE("a named node keeps its name unless the other is a port node", "[cap:extresist-network][req:Node merging]") {
  G t;
  const std::set<std::string, std::less<>> ports{"p"};
  t.g.port_nodes = &ports;
  const Id k = t.node();
  const Id d = t.node();
  const Id e = t.node();
  t.g.nodes[k].name = "k";
  t.g.nodes[k].named = true;
  t.g.nodes[d].name = "d";
  t.g.nodes[d].named = true;
  t.g.nodes[e].name = "p";
  t.g.nodes[e].named = true;
  t.g.merge_nodes(k, d);
  CHECK(t.g.nodes[k].name == "k");
  t.g.merge_nodes(k, e);
  CHECK(t.g.nodes[k].name == "p");
}

// --- Construction-time reductions ---

TEST_CASE("a parallel pair becomes one resistor", "[cap:extresist-network][req:Construction-time reductions]") {
  G t;
  const Id n = t.node();
  const Id m = t.node(extresist::kNodeJunction, 0);  // not ready: no dangling-node pass
  t.device(n);                                       // keeps n from the series check
  t.res(n, m, 1000.0F);
  t.res(n, m, 3000.0F);
  t.g.done_with_node(n);
  REQUIRE(t.list().size() == 1);
  CHECK(t.g.res[t.list().front()].value == 750.0F);
}

namespace {

/// Three nodes pairwise joined by 300 mOhm; the last one has a device link.
float star_arm(bool magic_compat) {
  G t;
  t.g.magic_compat = magic_compat;
  const Id a = t.node(extresist::kNodeJunction, 0);  // b and c not ready
  const Id b = t.node(extresist::kNodeJunction, 0);
  const Id c = t.node(extresist::kNodeJunction, 0);
  t.res(b, c, 300.0F);
  t.res(a, b, 300.0F);
  t.res(a, c, 300.0F);
  t.device(a);
  t.g.done_with_node(a);
  REQUIRE(t.list().size() == 3);
  const Id x = static_cast<Id>(t.g.nodes.size() - 1);
  CHECK(t.g.nodes[x].why == extresist::kNodeTriangle);
  CHECK(t.ends_of_resistors(x) == std::set<Id>{a, b, c});
  for (const Id r : t.list()) {
    CHECK(t.g.res[r].value == t.g.res[t.list().front()].value);
  }
  return t.g.res[t.list().front()].value;
}

}  // namespace

TEST_CASE("a delta becomes a star with the 0.5 mOhm offset with magic-compat ON",
          "[cap:extresist-network][req:Construction-time reductions]") {
  // Q-NETLIST-44 (compat): Ra*Rb/S + 0.5, single precision as Magic.
  const float denom = static_cast<float>(1.0 / 900.0);
  CHECK(star_arm(true) == static_cast<float>(static_cast<double>((300.0F * 300.0F) * denom) + 0.5));
}

TEST_CASE("a delta becomes a star without the offset with magic-compat OFF",
          "[cap:extresist-network][req:Construction-time reductions]") {
  // Q-NETLIST-44 (compat): Ra*Rb/S.
  const float denom = static_cast<float>(1.0 / 900.0);
  CHECK(star_arm(false) == (300.0F * 300.0F) * denom);
}

TEST_CASE("a high-fanout node takes the first triangle in resistor-list order",
          "[cap:extresist-network][req:Construction-time reductions]") {
  // Q-NETLIST-45 (fix, both modes): N has 10 neighbours n1..n10 (its list,
  // head first, is n10..n1) and two triangle sides, n2-n5 and n9-n4. The
  // list order reaches n9 before n2, so the first star joins N, n9 and n4.
  G t;
  const Id n = t.node();
  std::vector<Id> nb;
  for (int i = 0; i < 10; ++i) {
    nb.push_back(t.node(extresist::kNodeJunction, 0));
  }
  t.res(nb[1], nb[4], 500.0F);  // n2-n5
  t.res(nb[8], nb[3], 700.0F);  // n9-n4
  for (const Id m : nb) {
    t.res(n, m, 100.0F);
  }
  const auto first_star = static_cast<Id>(t.g.nodes.size());
  t.g.done_with_node(n);
  REQUIRE(t.g.nodes.size() > first_star);
  CHECK(t.ends_of_resistors(first_star) == std::set<Id>{n, nb[8], nb[3]});
}

TEST_CASE("a zero resistor merges its ends and a loop is dropped",
          "[cap:extresist-network][req:Construction-time reductions]") {
  G t;
  const Id a = t.node(extresist::kNodeOrigin);
  const Id b = t.node();
  const Id r = t.res(a, b, 0.0F);
  t.g.res[r].area = 3.0F;
  t.g.done_with_node(b);
  CHECK_FALSE(t.g.nodes[b].alive);
  CHECK(t.list().empty());
  CHECK(t.g.nodes[a].area == 3.0F);
  const Id loop = t.res(a, a, 10.0F);
  t.g.res[loop].area = 2.0F;
  t.g.done_with_node(a);
  CHECK(t.list().empty());
  CHECK(t.g.nodes[a].area == 5.0F);
}

// --- Path resistance and capacitance ---

TEST_CASE("path resistance follows the shortest path",
          "[cap:extresist-network][req:Path resistance and capacitance "
          "distribution]") {
  Net t;
  const auto o = t.origin();
  const auto a = t.node();
  const auto b = t.node();
  t.res(o, a, 2000.0);
  t.res(a, b, 3000.0);
  extresist::HeapState heap;
  extresist::Tolerances tol;
  tol.simplify = false;
  REQUIRE(extresist::reduce(t.n, tol, heap, mode(true)).ok());
  CHECK(node_at(t.n, 10).path_resistance == 2000.0);
  CHECK(node_at(t.n, 20).path_resistance == 5000.0);
  CHECK(t.n.max_resistance == 5000.0);
  // No area at all: Magic's message, and the capacitance stays undistributed.
  CHECK(t.n.messages.back() == "Error: Node with no area.");
}

TEST_CASE("a loop resistor is ignored for delay",
          "[cap:extresist-network][req:Path resistance and capacitance "
          "distribution]") {
  Net t;
  const auto o = t.origin();
  const auto a = t.node();
  const auto b = t.node();
  t.res(o, a, 100.0);
  t.res(o, b, 200.0);
  t.res(a, b, 50.0);
  extresist::HeapState heap;
  extresist::Tolerances tol;
  tol.simplify = false;
  REQUIRE(extresist::reduce(t.n, tol, heap, mode(true)).ok());
  CHECK(node_at(t.n, 10).path_resistance == 100.0);
  CHECK(node_at(t.n, 20).path_resistance == 150.0);
  for (const auto& r : t.n.resistors) {
    const bool ob = r.resistance == 200.0;
    CHECK(((r.status & extresist::kStatusTdiIgnore) != 0) == ob);
  }
}

TEST_CASE("path resistances are whole milliohms with magic-compat ON",
          "[cap:extresist-network][req:Path resistance and capacitance "
          "distribution]") {
  // Q-NETLIST-25 (compat): int keys and int path resistances.
  Net t;
  const auto o = t.origin();
  const auto a = t.node(extresist::kNodeJunction, 1.0);
  t.res(o, a, 1000.75);
  extresist::HeapState heap;
  extresist::Tolerances tol;
  tol.simplify = false;
  REQUIRE(extresist::reduce(t.n, tol, heap, mode(true)).ok());
  CHECK(node_at(t.n, 10).path_resistance == 1000.0);
  CHECK(t.n.max_resistance == 1000.0);
}

TEST_CASE("path resistances keep their fraction with magic-compat OFF",
          "[cap:extresist-network][req:Path resistance and capacitance "
          "distribution]") {
  // Q-NETLIST-25 (compat): double keys and path resistances.
  Net t;
  const auto o = t.origin();
  const auto a = t.node(extresist::kNodeJunction, 1.0);
  t.res(o, a, 1000.75);
  extresist::HeapState heap;
  extresist::Tolerances tol;
  tol.simplify = false;
  REQUIRE(extresist::reduce(t.n, tol, heap, mode(false)).ok());
  CHECK(node_at(t.n, 10).path_resistance == 1000.75);
}

TEST_CASE("the session heap is unbuilt for the first net with magic-compat ON",
          "[cap:extresist-network][req:Path resistance and capacitance "
          "distribution]") {
  // Q-NETLIST-46 (compat): Magic's heap appends until the first removal
  // heapifies it; from then on every insert sifts up, for later nets too.
  detail::MagicHeap first(0);
  first.add(3, 0);
  first.add(3, 1);
  first.add(1, 2);
  CHECK(first.keys() == std::vector<std::int32_t>{3, 3, 1});
  CHECK(first.remove_top() == 2U);
  CHECK(first.built() == 3);
  detail::MagicHeap later(3);
  later.add(3, 0);
  later.add(3, 1);
  later.add(1, 2);
  CHECK(later.keys() == std::vector<std::int32_t>{1, 3, 3});
  // reduce() keeps the state in HeapState.
  Net t;
  const auto o = t.origin();
  const auto a = t.node(extresist::kNodeJunction, 1.0);
  t.res(o, a, 10.0);
  extresist::HeapState heap;
  extresist::Tolerances tol;
  tol.simplify = false;
  REQUIRE(extresist::reduce(t.n, tol, heap, mode(true)).ok());
  CHECK(heap.built == 1);
}

TEST_CASE("each net has a fresh queue with magic-compat OFF",
          "[cap:extresist-network][req:Path resistance and capacitance "
          "distribution]") {
  // Q-NETLIST-46 (compat): no session state; ties in insertion order.
  detail::StableQueue q;
  q.add(5.0, 0);
  q.add(5.0, 1);
  q.add(5.0, 2);
  CHECK(q.remove_top() == 0U);
  CHECK(q.remove_top() == 1U);
  CHECK(q.remove_top() == 2U);
  Net t;
  const auto o = t.origin();
  const auto a = t.node(extresist::kNodeJunction, 1.0);
  t.res(o, a, 10.0);
  extresist::HeapState heap;
  extresist::Tolerances tol;
  tol.simplify = false;
  REQUIRE(extresist::reduce(t.n, tol, heap, mode(false)).ok());
  CHECK(heap.built == 0);
}

TEST_CASE("the net capacitance is shared by area",
          "[cap:extresist-network][req:Path resistance and capacitance "
          "distribution]") {
  Net t;
  const auto o = t.origin(1.0);
  const auto a = t.node(extresist::kNodeJunction, 3.0);
  t.res(o, a, 10.0);
  t.n.node_cap = 8.0;
  extresist::HeapState heap;
  extresist::Tolerances tol;
  tol.simplify = false;
  REQUIRE(extresist::reduce(t.n, tol, heap, mode(true)).ok());
  CHECK(node_at(t.n, 0).cap == 2.0);
  CHECK(node_at(t.n, 10).cap == 6.0);
}

// --- Elmore delay and pruning ---

TEST_CASE("the delay of a single RC", "[cap:extresist-network][req:Elmore delay and pruning]") {
  Net t;
  const auto o = t.origin();
  const auto a = t.node(extresist::kNodeJunction, 2000.0);
  t.res(o, a, 1000.0);
  t.n.node_cap = 2000.0;
  extresist::HeapState heap;
  extresist::Tolerances tol;
  tol.simplify = false;
  tol.lumped = true;
  REQUIRE(extresist::reduce(t.n, tol, heap, mode(true)).ok());
  CHECK(t.n.tdi == 2.0e6);
  CHECK((node_at(t.n, 10).status & extresist::kStatusMaxTdi) != 0);
}

TEST_CASE("the gate load counts two diffusion edges", "[cap:extresist-network][req:Elmore delay and pruning]") {
  // Q-NETLIST-24 (replicate): Cdown(A) = 0 + L*W*gateCap + (W+W)*sdCap
  // = 2*3*10 + 6*4 = 84, so Tdi(A) = 84 * 1000.
  Net t;
  const auto o = t.origin();
  const auto a = t.node();
  t.res(o, a, 1000.0);
  extresist::NetDevice d;
  d.terminals = {a, extresist::kNoNode, extresist::kNoNode, extresist::kNoNode};
  d.length = 2;
  d.width = 3;
  d.gate_cap = 10.0;
  d.sd_cap = 4.0;
  t.n.devices.push_back(d);
  t.n.nodes[a].devices.push_back(0);
  t.n.node_cap = 5.0;
  extresist::HeapState heap;
  extresist::Tolerances tol;
  tol.simplify = false;
  tol.lumped = true;
  REQUIRE(extresist::reduce(t.n, tol, heap, mode(true)).ok());
  CHECK(t.n.tdi == 84000.0);
}

TEST_CASE("a chain is pruned bottom-up", "[cap:extresist-network][req:Elmore delay and pruning]") {
  // Cdown(B) = 1, Cdown(A) = 2, Tdi(A) = 2000, Tdi(B) = 3000 < 5000: B goes
  // into A, then A (a leaf with 2000 < 5000) into the origin.
  Net t;
  const auto o = t.origin();
  const auto a = t.node(extresist::kNodeJunction, 1.0);
  const auto b = t.node(extresist::kNodeJunction, 1.0);
  t.res(o, a, 1000.0);
  t.res(a, b, 1000.0);
  t.n.node_cap = 2.0;
  extresist::HeapState heap;
  extresist::Tolerances tol;
  tol.min_delay = 5000.0;
  REQUIRE(extresist::reduce(t.n, tol, heap, mode(true)).ok());
  CHECK(t.n.tdi == 3000.0);
  CHECK(t.n.resistors.empty());
  REQUIRE(t.n.nodes.size() == 1);
  CHECK(t.n.nodes[0].cap == 2.0);
}

// --- Network simplification ---

TEST_CASE("a series node under the tolerance is removed", "[cap:extresist-network][req:Network simplification]") {
  // origin -> X (400) -> port (300), X has a gate: one 700 mOhm resistor, the
  // device moves to the port end (r1 = 400 is not smaller than r2 = 300).
  Net t;
  const auto o = t.origin();
  const auto x = t.node();
  const auto p = t.node(extresist::kNodeOrigin);
  t.res(o, x, 400.0);
  t.res(x, p, 300.0);
  extresist::NetDevice d;
  d.terminals = {x, extresist::kNoNode, extresist::kNoNode, extresist::kNoNode};
  t.n.devices.push_back(d);
  t.n.nodes[x].devices.push_back(0);
  extresist::HeapState heap;
  extresist::Tolerances tol;
  tol.min_delay = 0.0;
  REQUIRE(extresist::reduce(t.n, tol, heap, mode(true)).ok());
  REQUIRE(t.n.resistors.size() == 1);
  CHECK(t.n.resistors[0].resistance == 700.0);
  REQUIRE(t.n.nodes.size() == 2);
  CHECK(t.n.nodes[t.n.devices[0].terminals[0]].location.x == 20);
}

TEST_CASE("a large resistor is not merged in series", "[cap:extresist-network][req:Network simplification]") {
  // 4000 >= tol keeps X in the series step; the final pass then folds the
  // 300 mOhm dead end into the port.
  Net t;
  const auto o = t.origin();
  const auto x = t.node();
  const auto p = t.node(extresist::kNodeOrigin);
  t.res(o, x, 4000.0);
  t.res(x, p, 300.0);
  extresist::HeapState heap;
  extresist::Tolerances tol;
  tol.min_delay = 0.0;
  REQUIRE(extresist::reduce(t.n, tol, heap, mode(true)).ok());
  REQUIRE(t.n.resistors.size() == 1);
  CHECK(t.n.resistors[0].resistance == 4000.0);
}

// --- Final scrunch pass ---

namespace {

detail::Reducer reducer(G& t) {
  return detail::Reducer(t.g, true, 0, detail::kNil, 0.0F);
}

}  // namespace

TEST_CASE("a short list sorts equal values in reverse", "[cap:extresist-network][req:Final scrunch pass]") {
  // Q-NETLIST-47 keeps Magic's insertion order: a = 5, b = 5, c = 3 -> c, b, a.
  G t;
  const Id n1 = t.node();
  const Id n2 = t.node();
  const Id c = t.res(n1, n2, 3.0F);
  const Id b = t.res(n1, n2, 5.0F);
  const Id a = t.res(n1, n2, 5.0F);
  REQUIRE(t.list() == std::vector<Id>{a, b, c});
  reducer(t).scrunch(0.0F);
  CHECK(t.list() == std::vector<Id>{c, b, a});
}

TEST_CASE("a long list sorts equal values stably", "[cap:extresist-network][req:Final scrunch pass]") {
  // Q-NETLIST-26 (fix): one order on every platform.
  G t;
  const Id n1 = t.node();
  const Id n2 = t.node();
  std::vector<Id> made;
  for (int i = 0; i < 12; ++i) {
    made.push_back(t.res(n1, n2, i % 2 == 0 ? 7.0F : 3.0F));
  }
  // The list is newest first; the 3s keep that order, then the 7s.
  std::vector<Id> expected;
  for (int pass = 0; pass < 2; ++pass) {
    for (auto it = made.rbegin(); it != made.rend(); ++it) {
      if ((t.g.res[*it].value == 3.0F) == (pass == 0)) {
        expected.push_back(*it);
      }
    }
  }
  reducer(t).scrunch(0.0F);
  CHECK(t.list() == expected);
}

TEST_CASE("a resistor with no adjacent resistor is skipped", "[cap:extresist-network][req:Final scrunch pass]") {
  // Q-NETLIST-47 (fix): Magic dereferences the null `work`.
  G t;
  const Id a = t.node();
  const Id b = t.node();
  const Id c = t.node();
  const Id d = t.node();
  t.res(a, b, 10.0F);
  t.res(c, d, 20.0F);
  reducer(t).scrunch(1000.0F);
  CHECK(t.list().size() == 2);
}

TEST_CASE("a resistor whose fold target heads the list is skipped", "[cap:extresist-network][req:Final scrunch pass]") {
  // Q-NETLIST-47 (fix): the smallest neighbour is the list head (after the
  // port-to-port resistor is skipped); Magic unlinks it through a null.
  G t;
  const Id p1 = t.node(extresist::kNodeOrigin);
  const Id p2 = t.node(extresist::kNodeOrigin);
  const Id a = t.node();
  t.res(p1, a, 20.0F);
  t.res(p1, p2, 5.0F);
  reducer(t).scrunch(1000.0F);
  CHECK(t.list().size() == 2);
}

TEST_CASE("a port-to-port resistor is kept", "[cap:extresist-network][req:Final scrunch pass]") {
  G t;
  const Id p1 = t.node(extresist::kNodeOrigin);
  const Id p2 = t.node(extresist::kNodeSink);
  const Id a = t.node();
  t.res(p2, a, 2000.0F);
  const Id small = t.res(p1, p2, 10.0F);
  reducer(t).scrunch(1000.0F);
  CHECK(t.list().front() == small);
  CHECK(t.list().size() == 2);
}

TEST_CASE("the smallest resistor folds into its smallest neighbour",
          "[cap:extresist-network][req:Final scrunch pass]") {
  // tol 1000; A-B 100, B-C 500, A-D 2000; A, C, D protected: B-C becomes
  // 600 and B merges into A, so A-C 600 and A-D 2000 remain.
  G t;
  const Id a = t.node(extresist::kNodeOrigin);
  const Id b = t.node();
  const Id c = t.node(extresist::kNodeSink);
  const Id d = t.node(extresist::kNodeSink);
  t.res(a, d, 2000.0F);
  t.res(b, c, 500.0F);
  t.res(a, b, 100.0F);
  reducer(t).scrunch(1000.0F);
  REQUIRE(t.list().size() == 2);
  CHECK(t.g.res[t.list()[0]].value == 600.0F);
  CHECK(t.g.res[t.list()[1]].value == 2000.0F);
  CHECK(t.ends_of_resistors(a) == std::set<Id>{c, d});
  CHECK_FALSE(t.g.nodes[b].alive);
}
