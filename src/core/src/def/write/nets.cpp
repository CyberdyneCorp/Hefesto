// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The nets of DEF write: classification, naming, record headers and
// connections, and the geometry discovery that feeds the route writer
// (capability def-write "Net classification", "DEF net naming", "Net record
// header and connections", "Net geometry discovery"; Q-LEFDEF-102, 108,
// 112, 113).
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defWrite.c defnodeCount,
// defHNsprintf, defHNsprintfPrefix, nodeDefName, defWriteNets,
// defnodeVisit and defPortTileFunc, Copyright (C) 1985, 1990 Regents of
// the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: DEFW (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "def/write/route.hpp"
#include "hefesto/connect/search.hpp"
#include "hefesto/connect/tree.hpp"
#include "hefesto/drc/rules/query.hpp"

namespace hefesto::def::write_detail {

namespace {

/// The substrate sentinel of EFbuild (MINFINITY + 2).
constexpr db::Coord kSubstrateSentinel = db::kMinusInfinity + 2;

bool is_space(const extflat::FlatNode& node) {
  return node.layer == "space";
}

char last_char(const std::string& s) {
  return s.empty() ? '\0' : s.back();
}

/// The Tcl-build rules of defnodeCount for a top-level name, with Magic's
/// notes (Q-LEFDEF-102).
bool top_level_special(const Context& c, const extflat::FlatNode& node, const std::string& leaf,
                       const GlobalNames& names) {
  bool special = last_char(leaf) == '!';
  if (std::find(names.globals.begin(), names.globals.end(), leaf) != names.globals.end()) {
    c.say(kProgress, "Node " + leaf + " is defined in the \"globals\" array");
    special = true;
  }
  if (names.vdd && leaf == *names.vdd) {
    c.say(kProgress, "Node " + leaf + " matches VDD variable definition!");
    special = true;
  }
  if (names.gnd && leaf == *names.gnd) {
    c.say(kProgress, "Node " + leaf + " matches GND variable definition!");
    special = true;
  }
  return special || (!node.port && last_char(leaf) != '#');
}

bool wanted(const NetPlan& plan, const extflat::FlatNode& node, std::size_t i, NetMode mode) {
  switch (mode) {
    case NetMode::regular:
      return node.port && !plan.special[i];
    case NetMode::special:
      return plan.special[i];
    case NetMode::all_special:
      break;
  }
  return node.port || plan.special[i];
}

/// Passes every tile of a connectivity search to the route writer.
class GeometryVisitor final : public connect::NetVisitor {
 public:
  explicit GeometryVisitor(RouteWriter& writer) : writer_(writer) {}
  bool on_tile(const connect::NetTile& tile) override {
    writer_.tile(tile.tile, tile.plane);
    return true;
  }

 private:
  RouteWriter& writer_;
};

void search(const Context& c, RouteWriter& writer, const db::Rect& area, const tech::TypeMask& types) {
  writer.begin_search();
  GeometryVisitor visit(writer);
  connect::SearchOptions options;
  options.magic_compat = c.compat;
  (void)connect::search_flat(c.cell, c.tech, connect::SearchStart{area, types}, options, visit);
}

/// defPortTileFunc over DBTreeSrUniqueTiles: the first tile of `types` at
/// `area` in the cell or its subcells (a contact only on its home plane),
/// in root coordinates.
std::optional<db::Rect> port_tile(const Context& c, const db::Rect& area, const tech::TypeMask& types) {
  if (c.read_area) {
    c.read_area(area);  // the search reads the subcells it reaches (DBCellRead)
  }
  std::optional<db::Rect> found;
  (void)connect::search_tree_tiles(c.cell, area, types, {}, c.compat, [&](const connect::TreeTile& t) {
    if (c.tech.is_contact(t.tile.type) && c.tech.home_plane(t.tile.type) != t.plane) {
      return true;
    }
    found = db::apply(t.to_root, t.tile.area);
    return false;
  });
  return found;
}

/// Nothing of the node's layer was written from a seed: look for material
/// at the node's location in the hierarchy and search from around it.
void hierarchy_seed(const Context& c, RouteWriter& writer, const extflat::FlatNode& node, tech::TileType type,
                    tech::TypeMask types, const std::string& name) {
  const db::Rect at{node.location.x, node.location.y, node.location.x + 1, node.location.y + 1};
  const auto port = port_tile(c, at, types);
  if (const tech::ContactInfo* info = c.tech.contact(type)) {
    types |= info->residues;
    types.set(type);
  }
  if (!port) {
    if (c.active(type)) {
      c.say(kFloating, "Nothing of type " + std::string{c.tech.type_name(type)} + " found at node " + name +
                           " location (floating label?)!");
    }
    return;
  }
  search(c, writer, db::Rect{port->x0 - 1, port->y0 - 1, port->x1 + 1, port->y1 + 1}, types);
}

void geometry(const Context& c, RouteWriter& writer, const extflat::FlatNode& node, const std::string& name) {
  for (const extflat::NodeLocation& loc : node.locations) {
    if (loc.location.x <= kSubstrateSentinel || loc.location.y <= kSubstrateSentinel) {
      continue;
    }
    const auto type = node_type(c, loc.layer);
    if (!type) {
      c.say_native(kMagicCrash, "Node " + name + " has layer \"" + loc.layer +
                                    "\", which is not a type of this technology; its geometry is not written.");
      continue;
    }
    const tech::TypeMask& types = c.tech.connects_to(*type);
    search(c, writer, db::Rect{loc.location.x, loc.location.y, loc.location.x + 1, loc.location.y + 1}, types);
    if (!writer.wrote()) {
      hierarchy_seed(c, writer, node, *type, types, name);
    }
  }
}

void net_record(const Context& c, RouteWriter& writer, const extflat::FlatNode& node, std::string& out) {
  const std::string name = def_name(name_components(node), '/');
  out += "   - " + name;
  writer.begin_net(name.size());
  if (node.top_port) {
    out += " ( PIN " + name + " )";  // the column is not updated (Q-LEFDEF-97)
  }
  for (const std::vector<std::string>& components : node.names) {
    if (components.size() == 2 && last_char(components[1]) != '#') {
      const std::string connection = def_name(components, ' ');
      writer.check_break(5 + connection.size());
      out += " ( " + connection + " )";
    }
  }
  geometry(c, writer, node, name);
  writer.end_net();
  out += " ;\n";
}

}  // namespace

std::vector<std::string> name_components(const extflat::FlatNode& node) {
  if (!node.names.empty() && !node.names.front().empty()) {
    return node.names.front();
  }
  std::vector<std::string> out;
  std::size_t start = 0;
  for (std::size_t slash = node.name.find('/'); slash != std::string::npos; slash = node.name.find('/', start)) {
    out.push_back(node.name.substr(start, slash - start));
    start = slash + 1;
  }
  out.push_back(node.name.substr(start));
  return out;
}

std::string def_name(const std::vector<std::string>& components, char divider) {
  std::string out;
  for (std::size_t i = 0; i + 1 < components.size(); ++i) {
    out += components[i];
    out += divider;
  }
  if (components.empty()) {
    return out;
  }
  for (const char ch : components.back()) {
    if (ch == '#') {
      continue;
    }
    const bool wildcard = ch == ';' || ch == '-' || ch == '*' || ch == '%';
    out += wildcard ? '_' : ch;
  }
  return out;
}

NetPlan classify_nets(const Context& c, const extflat::Netlist& netlist, const GlobalNames& names,
                      bool all_special) {
  NetPlan plan;
  plan.special.assign(netlist.nodes().size(), false);
  plan.skip.assign(netlist.nodes().size(), false);
  for (std::size_t i = 0; i < netlist.nodes().size(); ++i) {
    const extflat::FlatNode& node = netlist.nodes()[i];
    if (is_space(node)) {
      plan.skip[i] = true;
      continue;
    }
    const std::vector<std::string> components = name_components(node);
    if (components.size() == 1) {
      plan.special[i] = top_level_special(c, node, components.front(), names);
    }
    if (all_special) {
      plan.special_count += (plan.special[i] || node.port) ? 1 : 0;
    } else if (plan.special[i]) {
      ++plan.special_count;
    } else if (node.port) {
      ++plan.regular_count;
    }
  }
  return plan;
}

RuleBook::Selected RuleBook::select(const Context& c, tech::TileType type, const std::string& lef_name,
                                    db::Coord width) {
  const std::string name = lef_name + "_width_" + std::to_string(static_cast<std::int32_t>(scaled(width, c.scale)));
  auto& entry = table_.find_or_insert(name);
  if (!entry.value) {
    lef::NonDefaultRule rule;
    rule.name = name;
    lef::NonDefaultRuleLayer layer;
    layer.name = lef_name;
    layer.target = c.map.at(type).via_class ? lef::RuleTarget::via : lef::RuleTarget::layer;
    layer.width = width;
    layer.spacing = c.rules != nullptr ? drc::default_spacing(*c.rules, c.tech, type, type) : 0;
    layer.extension = 0;
    rule.layers.push_back(std::move(layer));
    entry.value = std::move(rule);
  }
  const db::Coord first = entry.value->layers.empty() ? 0 : entry.value->layers.front().width;
  return Selected{name, first};
}

void write_nets(const Context& c, const extflat::Netlist& netlist, const NetPlan& plan, NetMode mode,
                const ViaPositions& vias, RuleBook& rules, std::string& out) {
  RouteWriter writer(c, mode, vias, rules, out);
  for (std::size_t i = 0; i < netlist.nodes().size(); ++i) {
    const extflat::FlatNode& node = netlist.nodes()[i];
    if (wanted(plan, node, i, mode) && !plan.skip[i]) {
      net_record(c, writer, node, out);
    }
  }
}

}  // namespace hefesto::def::write_detail
