// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Ports of a cell for the netlisters (capabilities netlist-spice-options
// "Subcircuit header line", netlist-spice-hierarchy "Port promotion"):
// - port_entries: topVisit walks the definition's node-name hash table
//   and, for each name whose node is a port, reads the port index of the
//   names from that one to the end of the node's list and of the node's
//   canonical name; the netlisters number the ports from these (Magic
//   ext2spice/ext2spice.c:1897-2060).
// - make_ports: esMakePorts (ext2spice/ext2hier.c:1995-2298), which makes
//   every child net a parent's `merge` or coupling reaches a port.
//
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13).

#include "ports.hpp"

#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <unordered_map>

#include "build.hpp"
#include "hier_name.hpp"

namespace hefesto::extflat::detail {

namespace {

// The entry a hash entry designates and its live node, without the path
// compression of NodeGraph::resolve/find (the model is shared and const).
std::uint32_t resolved(const NodeGraph& graph, std::uint32_t entry) {
  while (graph.name(entry).redirect != kNil) {
    entry = graph.name(entry).redirect;
  }
  return entry;
}

std::uint32_t live_node(const NodeGraph& graph, std::uint32_t entry) {
  std::uint32_t id = graph.name(entry).node;
  while (!graph.live(id)) {
    id = graph.node(id).parent;
  }
  return id;
}

PortName port_name(const NodeGraph& graph, std::uint32_t entry) {
  return PortName{name_to_string(graph.name(entry).key), graph.name(entry).port};
}

// cn_name: the connection name with each subscript range as `%d`
// (efConnBuildName), as esMakePorts reads it.
std::string conn_text(const ConnName& name) {
  if (name.nsubs == 0) {
    return name.before;
  }
  return name.before + (name.nsubs == 2 ? "[%d,%d]" : "[%d]") + name.after;
}

// HashLookOnly(&def->def_uses, id).
const Use* find_use(const Def& def, const std::string& id) {
  const auto* entry = def.use_table.find(id);
  return entry != nullptr && entry->value ? &def.uses[*entry->value] : nullptr;
}

// The live node a def's name designates, creating the node when the name
// is new (esMakePorts' HashFind and efBuildNode).
std::uint32_t node_of_name(ReadState& st, Def& def, const std::string& name) {
  def.name_order.find_or_insert(name);
  if (def.names.find(name) == def.names.end()) {
    build_node(st, def, NodeRecord{name, 0, 0, std::nullopt});
  }
  return def.graph.node_of(def.names.at(name));
}

// EF_PORT on the node, the name's index left to be numbered later.
void mark_port(Def& def, std::uint32_t node, const std::string& name) {
  def.graph.node(node).flags |= kFlagPort;
  def.graph.name(def.graph.resolve(def.names.at(name))).port = -1;
}

// sscanf("[%d:%d:%d][%d:%d:%d]") == 6.
bool is_array_ranges(const std::string& text) {
  int v[6];
  return std::sscanf(text.c_str(), "[%d:%d:%d][%d:%d:%d]", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6;
}

// esMakePorts' flagDefRecord: the parent node and the EF_SUBS_PORT flag a
// chain of connected names shares.
struct FlagRecord {
  std::uint32_t node = kNil;
  std::uint32_t flags = 0;
};

class PortMaker {
 public:
  PortMaker(ReadState& st, std::uint32_t def, bool cap_infinite, double cap_threshold)
      : st_(st), def_(def), cap_infinite_(cap_infinite), cap_threshold_(cap_threshold) {}

  void run() {
    const Def& def = st_.defs.at(def_);
    if (def.uses.empty()) {
      return;
    }
    // def_conns and def_caps are prepended lists: walk the records backwards.
    for (std::size_t i = def.conns.size(); i-- > 0;) {
      const std::string name1 = conn_text(def.conns[i].name1);
      const std::string name2 = conn_text(def.conns[i].name2);
      merge_side(name1, name2);
      merge_side(name2, name1);
    }
    for (std::size_t i = def.caps.size(); i-- > 0;) {
      const Conn& cap = def.caps[i];
      if (std::fabs(static_cast<double>(cap.cap) / 1000) < cap_threshold_) {
        continue;
      }
      cap_port(conn_text(cap.name1));
      cap_port(conn_text(cap.name2));
    }
  }

 private:
  // One side of a `merge`: trace `name` down through the uses, making the
  // net a port in each cell reached, and share the EF_SUBS_PORT flag with
  // the parent net `locname`.
  void merge_side(const std::string& name, const std::string& locname) {
    if (name.find('/') == std::string::npos) {
      return;
    }
    const std::size_t rec = record_for(name, locname);
    std::string portname = name;
    std::uint32_t updef = def_;
    for (std::size_t slash = portname.find('/'); slash != std::string::npos; slash = portname.find('/')) {
      const std::optional<std::uint32_t> portdef = merge_child(updef, portname.substr(0, slash));
      portname = portname.substr(slash + 1);
      if (portdef) {
        promote(*portdef, portname, rec);
      }
      if (!portdef || portname.find('/') == std::string::npos) {
        break;
      }
      updef = *portdef;
    }
    if (locname.find('/') == std::string::npos) {
      flag_parent(locname, rec);
    }
  }

  // The use's cell: the whole component when it is a use id, else the
  // part before its last `[`.
  std::optional<std::uint32_t> merge_child(std::uint32_t updef, std::string component) const {
    const Def& parent = st_.defs.at(updef);
    const std::size_t bracket = component.rfind('[');
    if (bracket != std::string::npos && find_use(parent, component) == nullptr) {
      component.resize(bracket);
    }
    const Use* use = find_use(parent, component);
    return use != nullptr ? std::optional<std::uint32_t>{use->def} : std::nullopt;
  }

  // A substrate net that reaches no device is not made a port while
  // parasitics are off (cthresh infinite).
  void promote(std::uint32_t portdef, const std::string& portname, std::size_t rec) {
    Def& def = st_.defs.at(portdef);
    const std::uint32_t node = node_of_name(st_, def, portname);
    const std::uint32_t flags = def.graph.node(node).flags;
    const bool substrate = (flags & (kFlagSubsNode | kFlagGlobSubs)) != 0;
    if ((flags & kFlagPort) == 0 && (!cap_infinite_ || !substrate || (flags & kFlagSubsPort) != 0)) {
      mark_port(def, node, portname);
    }
    if ((def.graph.node(node).flags & kFlagSubsPort) != 0) {
      records_[rec].flags = kFlagSubsPort;
      if (records_[rec].node != kNil) {
        st_.defs.at(def_).graph.node(records_[rec].node).flags |= kFlagSubsPort;
      }
    }
  }

  void flag_parent(const std::string& locname, std::size_t rec) {
    Def& def = st_.defs.at(def_);
    def.name_order.find_or_insert(locname);
    const auto it = def.names.find(locname);
    if (it == def.names.end()) {
      st_.message(codes::kNoSuchNode, "Error:  Node " + locname + " not found in cell " + def.name + "!");
      return;
    }
    records_[rec].node = def.graph.node_of(it->second);
    def.graph.node(records_[rec].node).flags |= records_[rec].flags;
  }

  // flagHashTable: both names of a connection share one record.
  std::size_t record_for(const std::string& name, const std::string& locname) {
    const auto a = by_name_.find(name);
    const auto b = by_name_.find(locname);
    if (a == by_name_.end() && b == by_name_.end()) {
      records_.push_back(FlagRecord{});
      by_name_[name] = records_.size() - 1;
      by_name_[locname] = records_.size() - 1;
    } else if (a == by_name_.end()) {
      by_name_[name] = b->second;
    } else if (b == by_name_.end()) {
      by_name_[locname] = a->second;
    }
    return by_name_.at(name);
  }

  // A coupling into a child: the net becomes a port in each cell reached.
  // `[a:b:c][d:e:f]` before the first `/` is an array use.
  void cap_port(const std::string& name) {
    std::string portname = name;
    std::uint32_t updef = def_;
    for (std::size_t slash = portname.find('/'); slash != std::string::npos; slash = portname.find('/')) {
      std::string component = portname.substr(0, slash);
      const std::size_t bracket = portname.find('[');
      if (bracket != std::string::npos && bracket < slash && is_array_ranges(portname.substr(bracket))) {
        component = portname.substr(0, bracket);
      }
      const Use* use = find_use(st_.defs.at(updef), component);
      portname = portname.substr(slash + 1);
      if (use != nullptr) {
        Def& def = st_.defs.at(use->def);
        const std::uint32_t node = node_of_name(st_, def, portname);
        if ((def.graph.node(node).flags & kFlagPort) == 0) {
          mark_port(def, node, portname);
        }
      }
      if (use == nullptr || portname.find('/') == std::string::npos) {
        break;
      }
      updef = use->def;
    }
  }

  ReadState& st_;
  std::uint32_t def_;
  bool cap_infinite_;
  double cap_threshold_;
  std::vector<FlagRecord> records_;
  std::unordered_map<std::string, std::size_t> by_name_;
};

}  // namespace

std::vector<PortEntry> port_entries(const Def& def) {
  std::vector<PortEntry> out;
  def.name_order.for_each([&](const auto& hashed) {
    const auto it = def.names.find(hashed.key);
    if (it == def.names.end()) {
      return;
    }
    const std::uint32_t entry = resolved(def.graph, it->second);
    const std::uint32_t node = live_node(def.graph, entry);
    const GraphNode& n = def.graph.node(node);
    if ((n.flags & kFlagPort) == 0 || n.head == kNil) {
      return;
    }
    PortEntry port;
    for (std::uint32_t e = entry; e != kNil; e = def.graph.name(e).next) {
      port.chain.push_back(port_name(def.graph, e));
    }
    port.canonical = port_name(def.graph, n.head);
    out.push_back(std::move(port));
  });
  return out;
}

void make_ports(ReadState& st, std::uint32_t def, bool cap_infinite, double cap_threshold) {
  PortMaker{st, def, cap_infinite, cap_threshold}.run();
}

DefPorts def_ports(const Def& def) {
  DefPorts out;
  out.entries = port_entries(def);
  for (std::uint32_t i = 0; i < def.graph.node_count(); ++i) {
    const GraphNode& n = def.graph.node(i);
    if (!def.graph.live(i) || n.head == kNil) {
      continue;
    }
    out.any_port = out.any_port || (n.flags & (kFlagPort | kFlagSubsPort)) != 0;
    out.substrate_port = out.substrate_port || ((n.flags & kFlagPort) == 0 && (n.flags & kFlagSubsPort) != 0);
  }
  return out;
}

void clear_ports(Def& def) {
  for (std::uint32_t i = 0; i < def.graph.node_count(); ++i) {
    def.graph.node(i).flags &= ~(kFlagPort | kFlagSubsPort);
  }
}

}  // namespace hefesto::extflat::detail
