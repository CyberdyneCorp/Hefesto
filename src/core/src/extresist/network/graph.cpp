// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The list and node primitives of the working network (capability
// extresist-network "Node merging"): InitializeResNode, ResNewBreak,
// ResAddToQueue and ResRemoveFromQueue, ResMergeNodes, ResDeleteResPointer,
// ResEliminateResistor, ResCleanNode, ResFixBreakPoint and
// ResAddResistorToList, and the conversion to and from extresist::Network.
// (Magic ref: resis/ResMerge.c:878-1267, resis/ResUtils.c:1088-1150,
// resis/ResRex.c:2230-2334, resis/ResSimple.c:660-714)
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

#include "extresist/network/graph.hpp"

namespace hefesto::extresist::detail {

Id Graph::new_node(db::Point loc, std::uint32_t why) {
  Node n;
  n.loc = loc;
  n.why = why;
  nodes.push_back(std::move(n));
  return static_cast<Id>(nodes.size() - 1);
}

Id Graph::new_resistor(Id a, Id b) {
  Resistor r;
  r.node = {a, b};
  r.next = res_list;
  res.push_back(r);
  const Id id = static_cast<Id>(res.size() - 1);
  if (res_list != kNil) {
    res[res_list].last = id;
  }
  res_list = id;
  push(nodes[a].re, id);
  push(nodes[b].re, id);
  return id;
}

void Graph::new_break(Id tile, Id node, db::Point loc, Id crect) {
  auto& breaks = tiles[tile].breaks;
  breaks.insert(breaks.begin(), Breakpoint{node, loc, crect});
}

void Graph::push(Id& head, Id item_id) {
  cells.push_back(Cell{head, item_id});
  head = static_cast<Id>(cells.size() - 1);
}

bool Graph::erase(Id& head, Id item_id) {
  Id prev = kNil;
  for (Id c = head; c != kNil; prev = c, c = cells[c].next) {
    if (cells[c].item != item_id) {
      continue;
    }
    (prev == kNil ? head : cells[prev].next) = cells[c].next;
    cells[c] = Cell{};
    return true;
  }
  return false;
}

void Graph::add_to_queue(Id node, Id& list) {
  nodes[node].more = list;
  nodes[node].less = kNil;
  if (list != kNil) {
    nodes[list].less = node;
  }
  list = node;
}

void Graph::remove_from_queue(Id node, Id& list) {
  Node& n = nodes[node];
  if (n.less != kNil) {
    nodes[n.less].more = n.more;
  } else if (node != list) {
    message("Error: Attempt to remove node from wrong list");
  } else {
    list = n.more;
  }
  if (n.more != kNil) {
    nodes[n.more].less = n.less;
  }
  n.more = kNil;
  n.less = kNil;
}

namespace {

/// Moves every cell of `from` to the head of `to`, one by one (so their
/// order reverses), calling `visit` on each item first.
template <typename Visit>
void move_cells(Graph& g, Id& from, Id& to, Visit visit) {
  Id c = from;
  while (c != kNil) {
    visit(g.cells[c].item);
    const Id following = g.cells[c].next;
    g.cells[c].next = to;
    to = c;
    c = following;
  }
  from = kNil;
}

}  // namespace

void Graph::merge_values(Id keep, Id gone) {
  Node& k = nodes[keep];
  const Node& d = nodes[gone];
  if ((d.why & kNodeOrigin) != 0) {
    k.why = kNodeOrigin;
  }
  if ((d.why & kNodeSink) != 0) {
    k.why = kNodeSink;
  }
  if (k.noderes > d.noderes) {
    k.noderes = d.noderes;
    if ((k.status & kStatusFinished) == 0) {
      remove_from_queue(keep, pending);
      add_to_queue(keep, pending);
    }
  }
  k.area += d.area;
  k.status |= d.status & kStatusMaxTdi;
}

void Graph::merge_name(Id keep, Id gone) {
  Node& k = nodes[keep];
  const Node& d = nodes[gone];
  if (!k.named) {
    k.name = d.name;
    k.named = d.named;
  } else if (d.named && d.name != k.name && port_nodes != nullptr && port_nodes->contains(d.name)) {
    k.name = d.name;
  }
}

void Graph::fix_contact(Id cp, Id keep, Id gone) {
  Contact& c = contacts[cp];
  for (std::size_t i = 0; i < static_cast<std::size_t>(c.current); ++i) {
    if (c.cnode[i] != gone) {
      continue;
    }
    c.cnode[i] = keep;
    if ((tiles[c.tile[i]].status & kTileDone) == 0) {
      fix_breakpoint(c.tile[i], gone, keep);
    }
  }
}

void Graph::fix_junction(Id j, Id keep, Id gone) {
  for (const Id tile : junctions[j].tile) {
    if ((tiles[tile].status & kTileDone) == 0) {
      fix_breakpoint(tile, gone, keep);
    }
  }
  junctions[j].node = keep;
}

void Graph::merge_nodes(Id keep, Id gone) {
  if (keep == gone) {
    return;
  }
  if (keep == kNil || gone == kNil) {
    message("Attempt to merge NULL node");
    return;
  }
  merge_values(keep, gone);
  move_cells(*this, nodes[gone].te, nodes[keep].te,
             [&](Id dev) { std::replace(devices[dev].terms.begin(), devices[dev].terms.end(), gone, keep); });
  move_cells(*this, nodes[gone].je, nodes[keep].je, [&](Id j) { fix_junction(j, keep, gone); });
  move_cells(*this, nodes[gone].ce, nodes[keep].ce, [&](Id cp) { fix_contact(cp, keep, gone); });
  merge_name(keep, gone);
  move_cells(*this, nodes[gone].re, nodes[keep].re, [&](Id r) {
    auto& ends = res[r].node;
    if (ends[0] == gone || ends[1] == gone) {
      (ends[0] == gone ? ends[0] : ends[1]) = keep;
    } else {
      message("Resistor not found.");
    }
  });
  Node& d = nodes[gone];
  remove_from_queue(gone, (d.status & kStatusFinished) != 0 ? done : pending);
  d.delay = kNil;
  if (origin == gone) {
    origin = keep;
  }
  d.alive = false;
}

void Graph::delete_res_pointer(Id node, Id r) {
  if (!erase(nodes[node].re, r)) {
    message("Missing rptr at (" + std::to_string(nodes[node].loc.x) + " " + std::to_string(nodes[node].loc.y) + ").");
  }
}

void Graph::unlink_resistor(Id r, Id& list) {
  Resistor& x = res[r];
  if (r == list) {
    list = x.next;
  } else if (x.last != kNil) {
    res[x.last].next = x.next;
  }
  if (x.next != kNil) {
    res[x.next].last = x.last;
  }
}

void Graph::eliminate_resistor(Id r) {
  Resistor& x = res[r];
  if (x.last == kNil) {
    res_list = x.next;
  } else {
    res[x.last].next = x.next;
  }
  if (x.next != kNil) {
    res[x.next].last = x.last;
  }
  x.next = kNil;
  x.last = kNil;
  x.node = {kNil, kNil};
  x.alive = false;
}

void Graph::clean_node(Id node, bool info) {
  Node& n = nodes[node];
  n.ce = kNil;
  n.je = kNil;
  if (!info) {
    return;
  }
  n.delay = kNil;
  n.te = kNil;
  n.re = kNil;
  if (n.less != kNil) {
    nodes[n.less].more = n.more;
  } else if (done == node) {
    done = n.more;
  } else if (pending == node) {
    pending = n.more;
  } else {
    message("Error: Attempted to eliminate node from wrong list.");
  }
  if (n.more != kNil) {
    nodes[n.more].less = n.less;
  }
  n.more = kNil;
  n.less = kNil;
  n.alive = false;
}

namespace {

/// bp4 (the first breakpoint of `replacement`) inherits the first deleted
/// contact rectangle when it has none.
void inherit_crect(std::vector<Breakpoint>& kept, Id replacement, Id inherited) {
  for (Breakpoint& b : kept) {
    if (b.node == replacement) {
      if (b.crect == kNil) {
        b.crect = inherited;
      }
      return;
    }
  }
}

}  // namespace

void Graph::fix_breakpoint(Id tile, Id orig, Id replacement) {
  auto& breaks = tiles[tile].breaks;
  const bool present =
      std::any_of(breaks.begin(), breaks.end(), [&](const Breakpoint& b) { return b.node == replacement; });
  std::vector<Breakpoint> kept;
  kept.reserve(breaks.size());
  Id inherited = kNil;
  for (const Breakpoint& b : breaks) {
    if (b.node != orig) {
      kept.push_back(b);
    } else if (!present) {
      kept.push_back(Breakpoint{replacement, b.loc, b.crect});
    } else if (b.crect != kNil && inherited == kNil) {
      inherited = b.crect;
    }
  }
  if (present && inherited != kNil) {
    inherit_crect(kept, replacement, inherited);
  }
  breaks = std::move(kept);
}

void Graph::add_resistor_to_list(Id r, Id& list) {
  Id last = kNil;
  Id local = list;
  for (; local != kNil; local = res[local].next) {
    if (res[local].value >= res[r].value) {
      break;
    }
    last = local;
  }
  Resistor& x = res[r];
  if (local != kNil) {
    x.next = local;
    x.last = res[local].last;
    if (res[local].last == kNil) {
      list = r;
    } else {
      res[res[local].last].next = r;
    }
    res[local].last = r;
  } else if (last != kNil) {
    res[last].next = r;
    x.last = last;
    x.next = kNil;
  } else {
    x.next = kNil;
    x.last = kNil;
    list = r;
  }
}

namespace {

using IndexMap = std::unordered_map<Id, std::size_t>;

std::size_t index_or_none(const IndexMap& index, Id id) {
  const auto it = index.find(id);
  return it == index.end() ? kNoNode : it->second;
}

std::int32_t clamp32(std::int64_t v) {
  return static_cast<std::int32_t>(
      std::clamp<std::int64_t>(v, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
}

NetResistor net_resistor(const Resistor& x, const IndexMap& node_index) {
  NetResistor nr;
  nr.a = index_or_none(node_index, x.node[0]);
  nr.b = index_or_none(node_index, x.node[1]);
  nr.resistance = static_cast<double>(x.value);
  nr.status = x.status;
  nr.area = static_cast<double>(x.area);
  nr.centerline = x.cl;
  nr.width = x.width;
  nr.type = x.tt;
  return nr;
}

NetDevice net_device(const Device& d, const IndexMap& node_index) {
  NetDevice nd;
  for (const Id t : d.terms) {
    nd.terminals.push_back(index_or_none(node_index, t));
  }
  nd.type = d.devtype;
  nd.inside = d.inside;
  nd.location = db::Point{d.inside.x0, d.inside.y0};
  nd.length = clamp32(d.length);
  nd.width = clamp32(d.width);
  nd.perimeter = d.perim;
  nd.area = d.area;
  nd.tiles = d.tiles;
  nd.gate_cap = d.gate_cap;
  nd.sd_cap = d.sd_cap;
  return nd;
}

}  // namespace

NetNode Graph::net_node(Id n, const std::unordered_map<Id, std::size_t>& res_index,
                        const std::unordered_map<Id, std::size_t>& dev_index) const {
  const Node& x = nodes[n];
  NetNode nn;
  nn.location = x.loc;
  nn.name = x.name;
  nn.named = x.named;
  nn.cap = static_cast<double>(x.area);
  nn.kind = x.why;
  nn.status = x.status;
  nn.path_resistance = x.noderes;
  for (Id c = x.re; c != kNil; c = cells[c].next) {
    if (res_index.contains(cells[c].item)) {
      nn.resistors.push_back(res_index.at(cells[c].item));
    }
  }
  for (Id c = x.te; c != kNil; c = cells[c].next) {
    nn.devices.push_back(dev_index.at(cells[c].item));
  }
  return nn;
}

Network Graph::to_network() const {
  Network out;
  IndexMap node_index;
  std::vector<Id> order;
  for (const Id head : {done, pending}) {
    for (Id n = head; n != kNil; n = nodes[n].more) {
      node_index.emplace(n, order.size());
      order.push_back(n);
    }
  }
  IndexMap res_index;
  for (Id r = res_list; r != kNil; r = res[r].next) {
    res_index.emplace(r, out.resistors.size());
    out.resistors.push_back(net_resistor(res[r], node_index));
  }
  IndexMap dev_index;
  for (std::size_t i = devices.size(); i-- > 0;) {
    dev_index.emplace(static_cast<Id>(i), out.devices.size());
    out.devices.push_back(net_device(devices[i], node_index));
  }
  for (const Id n : order) {
    out.nodes.push_back(net_node(n, res_index, dev_index));
  }
  if (origin != kNil && node_index.contains(origin)) {
    out.origin = node_index.at(origin);
  }
  out.messages = messages;
  return out;
}

namespace {

Id to_id(std::size_t index) {
  return index == kNoNode ? kNil : static_cast<Id>(index);
}

/// Appends the cells of `items` to the list `head` in the given order.
void append_cells(Graph& g, Id& head, const std::vector<std::size_t>& items, bool reverse_ids, std::size_t count) {
  Id last = kNil;  // an index: push_back may move the cells
  for (const std::size_t i : items) {
    const Id id = reverse_ids ? static_cast<Id>(count - 1 - i) : static_cast<Id>(i);
    g.cells.push_back(Cell{kNil, id});
    const auto cell = static_cast<Id>(g.cells.size() - 1);
    (last == kNil ? head : g.cells[last].next) = cell;
    last = cell;
  }
}

Resistor working_resistor(const NetResistor& nr) {
  Resistor r;
  r.node = {to_id(nr.a), to_id(nr.b)};
  r.value = static_cast<float>(nr.resistance);
  r.status = nr.status;
  r.area = static_cast<float>(nr.area);
  r.cl = nr.centerline;
  r.width = nr.width;
  r.tt = nr.type;
  return r;
}

Device working_device(const NetDevice& nd) {
  Device d;
  for (const std::size_t t : nd.terminals) {
    d.terms.push_back(to_id(t));
  }
  d.devtype = nd.type;
  d.inside = nd.inside;
  d.length = nd.length;
  d.width = nd.width;
  d.perim = nd.perimeter;
  d.area = nd.area;
  d.tiles = nd.tiles;
  d.gate_cap = nd.gate_cap;
  d.sd_cap = nd.sd_cap;
  return d;
}

Node working_node(const NetNode& nn) {
  Node n;
  n.loc = nn.location;
  n.name = nn.name;
  n.named = nn.named;
  n.area = static_cast<float>(nn.cap);
  n.why = nn.kind;
  n.status = nn.status;
  n.noderes = nn.path_resistance;
  return n;
}

/// The neighbours of entry i in a list linked in index order.
Id prev_id(std::size_t i) {
  return i == 0 ? kNil : static_cast<Id>(i - 1);
}
Id next_id(std::size_t i, std::size_t size) {
  return i + 1 == size ? kNil : static_cast<Id>(i + 1);
}

}  // namespace

Graph Graph::from_network(const Network& network) {
  Graph g;
  for (const NetResistor& nr : network.resistors) {
    g.res.push_back(working_resistor(nr));
  }
  for (std::size_t i = 0; i < g.res.size(); ++i) {
    g.res[i].last = prev_id(i);
    g.res[i].next = next_id(i, g.res.size());
  }
  g.res_list = g.res.empty() ? kNil : 0;
  // Network::devices is ResDevList order; the working list keeps creation
  // order (the reverse).
  for (std::size_t i = network.devices.size(); i-- > 0;) {
    g.devices.push_back(working_device(network.devices[i]));
  }
  for (std::size_t i = 0; i < network.nodes.size(); ++i) {
    Node n = working_node(network.nodes[i]);
    n.less = prev_id(i);
    n.more = next_id(i, network.nodes.size());
    g.nodes.push_back(std::move(n));
  }
  for (std::size_t i = 0; i < network.nodes.size(); ++i) {
    append_cells(g, g.nodes[i].re, network.nodes[i].resistors, false, 0);
    append_cells(g, g.nodes[i].te, network.nodes[i].devices, true, network.devices.size());
  }
  g.done = g.nodes.empty() ? kNil : 0;
  g.origin = network.origin ? static_cast<Id>(*network.origin) : kNil;
  g.messages = network.messages;
  return g;
}

}  // namespace hefesto::extresist::detail
