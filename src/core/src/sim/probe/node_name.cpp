// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Hierarchical node names brought forward from irsim-interface (Q-SIM-24
// (a); design D4 "Two namings, one node", D13; task 14.2 naming portion):
// "Hierarchical node names", "Node name preference order" and
// simulation-ui-and-crossprobe "Probe names without length limits".
//
// The selected node is followed through the hierarchy with
// connect::search_hierarchical (every cell expanded, as getnode's
// SimTreeCopyConnect with xMask 0). In each cell the node reaches, each of
// its regions there is named as the extractor names it (extract/node), with
// the best name label of the region by the preference order (SimGetNodeName,
// sim/SimExtract.c:723-846), and prefixed with the instance path. The best
// candidate wins; with globals on, a name ending in `!` loses its path and
// the `!` (SimSelectNode, sim/SimSelect.c:155-176). Magic builds these names
// in fixed 256-byte buffers that overflow on long names; every name here is
// a std::string, in both magic-compat modes.
//
// Owner: SIMPROBE.

#include <algorithm>
#include <charconv>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/connect/search.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/extract/node/labels.hpp"
#include "hefesto/extract/node/names.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/sim/probe/probe.hpp"
#include "hefesto/tech/technology.hpp"
#include "sim/probe/node_search.hpp"

namespace hefesto::sim {

namespace {

std::size_t slashes(std::string_view name) noexcept {
  return static_cast<std::size_t>(std::count(name.begin(), name.end(), '/'));
}

/// Rules 2-4 between two names of the same kind (both global or both
/// local): fewer `/`, then shorter, then the greater string.
bool prefer_by_shape(std::string_view a, std::string_view b) noexcept {
  const std::size_t sa = slashes(a);
  const std::size_t sb = slashes(b);
  if (sa != sb) {
    return sa < sb;
  }
  if (a.size() != b.size()) {
    return a.size() < b.size();
  }
  return a.compare(b) > 0;
}

/// Parses "[n]" or "[n,m]" after a use id.
bool parse_subscripts(std::string_view text, std::vector<std::int32_t>& out) {
  if (text.size() < 3 || text.front() != '[' || text.back() != ']') {
    return false;
  }
  text = text.substr(1, text.size() - 2);
  while (!text.empty()) {
    std::int32_t value = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{}) {
      return false;
    }
    out.push_back(value);
    text.remove_prefix(static_cast<std::size_t>(end - text.data()));
    if (!text.empty() && text.front() == ',') {
      text.remove_prefix(1);
    }
  }
  return out.size() == 1 || out.size() == 2;
}

/// The (x, y) of `element` in `use`, when `element` names one of its
/// elements (db::array_element_name: "id", "id[y]", "id[x]" or "id[y,x]").
std::optional<std::pair<std::int32_t, std::int32_t>> element_of(const db::CellUse& use, std::string_view element) {
  if (!element.starts_with(use.id)) {
    return std::nullopt;
  }
  const db::ArrayInfo& a = use.array;
  if (!a.is_array()) {
    return element.size() == use.id.size() ? std::optional{std::pair{a.xlo, a.ylo}} : std::nullopt;
  }
  std::vector<std::int32_t> n;
  if (!parse_subscripts(element.substr(use.id.size()), n)) {
    return std::nullopt;
  }
  std::pair<std::int32_t, std::int32_t> xy{a.xlo, a.ylo};
  if (n.size() == 2) {
    xy = {n[1], n[0]};
  } else if (a.xlo == a.xhi) {
    xy.second = n[0];
  } else {
    xy.first = n[0];
  }
  const auto within = [](std::int32_t v, std::int32_t lo, std::int32_t hi) {
    return std::min(lo, hi) <= v && v <= std::max(lo, hi);
  };
  if (!within(xy.first, a.xlo, a.xhi) || !within(xy.second, a.ylo, a.yhi) ||
      db::array_element_name(use, xy.first, xy.second) != element) {
    return std::nullopt;
  }
  return xy;
}

std::vector<std::string> split_path(std::string_view path) {
  std::vector<std::string> out;
  while (!path.empty()) {
    const std::size_t slash = path.find('/');
    out.emplace_back(path.substr(0, slash));
    path = slash == std::string_view::npos ? std::string_view{} : path.substr(slash + 1);
  }
  return out;
}

class CollectNet final : public connect::NetVisitor {
 public:
  explicit CollectNet(std::vector<connect::NetTile>& out) : out_(out) {}
  bool on_tile(const connect::NetTile& tile) override {
    out_.push_back(tile);
    return true;
  }

 private:
  std::vector<connect::NetTile>& out_;
};

/// The tile view at `p` on `plane`, on the side that carries `type`.
db::TileView side_at(const db::Plane& plane, db::Point p, tech::TileType type) {
  db::TileView view = plane.tile_at(p);
  if (view.split) {
    view.right_side = view.right_type == type;
    view.type = type;
  }
  return view;
}

/// The best name of `node` in its cell: the extractor's name, replaced by
/// any later name label that is preferred (SimGetNodeName's loop).
std::string local_name(const extract::NodeRegion& node, const NodeNaming& naming,
                       const compat::CompatSettings& compat) {
  std::string best = extract::node_name(node, *naming.technology, compat);
  for (const extract::LabelRegion& label : node.labels) {
    if (extract::label_role(label.text) == extract::LabelRole::name &&
        prefer_node_name(label.text, best, naming.globals)) {
      best = label.text;
    }
  }
  return best;
}

}  // namespace

bool prefer_node_name(std::string_view a, std::string_view b, bool globals) noexcept {
  if (a.starts_with("@=")) {
    return true;
  }
  if (b.starts_with("@=")) {
    return false;
  }
  const bool global_a = !a.empty() && a.back() == '!';
  const bool global_b = !b.empty() && b.back() == '!';
  if (globals && (global_a || global_b)) {
    return global_a && global_b ? prefer_by_shape(a, b) : global_a;
  }
  const bool generated_a = !a.empty() && a.back() == '#';
  const bool generated_b = !b.empty() && b.back() == '#';
  if (generated_a != generated_b) {
    return generated_b;
  }
  return prefer_by_shape(a, b);
}

diag::Outcome<PlacedCell> place(const db::CellDef& root, const std::vector<std::string>& use_path) {
  PlacedCell placed{&root, db::kIdentity};
  for (const std::string& element : use_path) {
    const db::CellUse* found = nullptr;
    std::pair<std::int32_t, std::int32_t> xy{};
    for (const auto& use : placed.def->uses()) {
      if (const auto e = element_of(*use, element); e && use->def != nullptr) {
        found = use.get();
        xy = *e;
        break;
      }
    }
    if (found == nullptr) {
      return diag::make("SIM-PROBE-003", "no instance " + element + " in cell " + placed.def->name());
    }
    placed.to_root = db::compose(placed.to_root, db::element_transform(*found, xy.first, xy.second));
    placed.def = found->def;
  }
  return placed;
}

diag::Outcome<SelectedNet> find_selected_net(const LayoutSelection& selection) {
  const NodeNaming& naming = selection.naming;
  if (selection.root == nullptr || naming.technology == nullptr || naming.style == nullptr) {
    return diag::make("SIM-PROBE-002", "node naming needs a root cell, a technology and an extraction style");
  }
  auto placed = place(*selection.root, selection.point.use_path);
  if (!placed.ok()) {
    return placed.error();
  }
  const db::CellDef& def = *placed.value().def;
  const db::Point p = selection.point.point;
  for (std::size_t plane = tech::kFirstUserPlane; plane < naming.technology->plane_count(); ++plane) {
    const db::TileView view = def.plane(static_cast<tech::PlaneId>(plane)).tile_at(p);
    if (view.type == tech::kSpace) {
      continue;
    }
    connect::SearchStart start;
    start.area = db::apply(placed.value().to_root, db::Rect{p.x, p.y, p.x + 1, p.y + 1});
    start.types = tech::TypeMask::of(view.type);
    connect::SearchOptions options;
    options.magic_compat = naming.magic_compat;
    SelectedNet net;
    CollectNet collect(net.tiles);
    if (auto status = connect::search_hierarchical(*selection.root, *naming.technology, start, options, collect);
        !status.ok()) {
      return status.error();
    }
    return net;
  }
  return diag::make("SIM-PROBE-004", "no paint at (" + std::to_string(p.x) + ", " + std::to_string(p.y) + ") in cell " +
                                         def.name());
}

namespace {

/// The local names of the node's regions in one cell instance.
diag::Outcome<std::vector<std::string>> region_names(const PlacedCell& placed,
                                                     const std::vector<const connect::NetTile*>& tiles,
                                                     const NodeNaming& naming) {
  compat::CompatSettings compat;
  compat.magic_compat = naming.magic_compat;
  extract::RegionFinder finder(*naming.technology);
  finder.keep_clients = true;
  extract::NodePassOptions pass;
  pass.magic_compat = naming.magic_compat;
  auto nodes = finder.find_nodes(*placed.def, *naming.style, pass);
  if (!nodes.ok()) {
    extract::reset(*placed.def);
    return nodes.error();
  }
  extract::attach_labels(*placed.def, finder, nodes.value(), *naming.style, *naming.technology,
                         extract::LabelPassOptions{}, compat);
  const db::Transform from_root = db::invert(placed.to_root);
  std::set<std::size_t> seen;
  std::vector<std::string> names;
  for (const connect::NetTile* tile : tiles) {
    const db::Rect local = db::apply(from_root, tile->tile.area);
    const db::Plane& plane = placed.def->plane(tile->plane);
    const auto index = finder.node_of(plane, side_at(plane, db::Point{local.x0, local.y0}, tile->tile.type),
                                      nodes.value().size());
    if (index && seen.insert(*index).second) {
      names.push_back(local_name(nodes.value()[*index], naming, compat));
    }
  }
  extract::reset(*placed.def);
  return names;
}

/// Keeps the preferred of `best` and the candidates of one instance path.
void keep_best(std::optional<std::string>& best, const std::string& path, const std::vector<std::string>& names,
               bool globals) {
  for (const std::string& local : names) {
    std::string candidate = path.empty() ? local : path + "/" + local;
    if (!best || prefer_node_name(candidate, *best, globals)) {
      best = std::move(candidate);
    }
  }
}

/// SimSelectNode: a global name loses its path and its `!`.
void strip_global(std::string& name) {
  name.pop_back();
  if (const std::size_t slash = name.rfind('/'); slash != std::string::npos) {
    name.erase(0, slash + 1);
  }
}

}  // namespace

diag::Outcome<std::string> node_name(const LayoutSelection& selection) {
  auto net = find_selected_net(selection);
  if (!net.ok()) {
    return net.error();
  }
  std::map<std::string, std::vector<const connect::NetTile*>> by_path;
  for (const connect::NetTile& tile : net.value().tiles) {
    by_path[tile.use_path].push_back(&tile);
  }
  std::optional<std::string> best;
  for (const auto& [path, tiles] : by_path) {
    auto placed = place(*selection.root, split_path(path));
    if (!placed.ok()) {
      return placed.error();
    }
    auto names = region_names(placed.value(), tiles, selection.naming);
    if (!names.ok()) {
      return names.error();
    }
    keep_best(best, path, names.value(), selection.naming.globals);
  }
  if (!best) {
    return diag::make("SIM-PROBE-004", "the selected node has no paint in an extracted region");
  }
  if (selection.naming.globals && best->ends_with('!')) {
    strip_global(*best);
  }
  return *best;
}

diag::Outcome<OnSelectedNode> selected_node_predicate(const LayoutSelection& selection) {
  auto net = find_selected_net(selection);
  if (!net.ok()) {
    return net.error();
  }
  auto tiles = std::make_shared<const std::vector<connect::NetTile>>(std::move(net.value().tiles));
  const db::CellDef* root = selection.root;
  return OnSelectedNode{[root, tiles](const LayoutPoint& point) {
    auto placed = place(*root, point.use_path);
    if (!placed.ok()) {
      return false;
    }
    // Compared in the cell's own coordinates, where the tiles are half open.
    const db::Transform from_root = db::invert(placed.value().to_root);
    const db::Point p = point.point;
    std::string path;
    for (const std::string& element : point.use_path) {
      path += path.empty() ? element : "/" + element;
    }
    return std::any_of(tiles->begin(), tiles->end(), [&](const connect::NetTile& t) {
      const db::Rect r = db::apply(from_root, t.tile.area);
      return t.use_path == path && r.x0 <= p.x && p.x < r.x1 && r.y0 <= p.y && p.y < r.y1;
    });
  }};
}

}  // namespace hefesto::sim
