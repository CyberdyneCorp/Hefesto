// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Label attachment, the per-node records and the duplicate-label warning
// (capability extract-nodes-and-labels "Label attachment", "Port
// records", "Node and attribute records", "Alias output", "Duplicate
// label warning"; openspec/changes/m7-extraction design.md D8).
// (Magic ref: extract/ExtRegion.c:180-360, extract/ExtBasic.c:681-860,
// extract/ExtBasic.c:1029-1105)
//
// Owner: EXTNODE (design D3).

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/file/writer.hpp"
#include "hefesto/extract/node/labels.hpp"
#include "hefesto/extract/node/names.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extract {

namespace {

constexpr std::string_view kCodeShorted = "EXTRACT-NODE-001";

LabelRegion region_label(const db::Label& label, std::size_t index) {
  LabelRegion out;
  out.text = label.text;
  out.position = {label.rect.x0, label.rect.y0};
  out.type = label.type;
  out.port = label.is_port();
  out.rect = label.rect;
  out.port_index = label.port_index;
  out.label = index;
  return out;
}

/// The probe of one label: the node index it attaches to, whether it
/// found a tile with a region, and whether a connecting tile was seen.
struct Probe {
  std::optional<std::size_t> node;
  bool found = false;
  bool connected = false;
};

Probe probe_label(const db::CellDef& cell, const RegionFinder& finder, std::size_t list_size,
                  const ExtStyle& style, const db::Label& label, tech::PlaneId plane) {
  static constexpr std::array<db::Point, 4> kOffsets{{{0, 0}, {0, -1}, {-1, -1}, {-1, 0}}};
  const db::Plane& p = cell.plane(plane);
  const auto cx = static_cast<std::int32_t>((std::int64_t{label.rect.x0} + label.rect.x1) >> 1);
  const auto cy = static_cast<std::int32_t>((std::int64_t{label.rect.y0} + label.rect.y1) >> 1);
  Probe out;
  for (const db::Point& off : kOffsets) {
    db::TileView tile = p.tile_at({cx + off.x, cy + off.y});
    const tech::TileType type = tile.split ? tile.left_type : tile.type;  // TiGetType
    const bool connects = type < style.node_conn.size() && style.node_conn[type].test(label.type);
    if (connects && p.client(tile) != db::kClientDefault) {
      tile.right_side = false;  // ExtGetRegion(tp, 0): the left side
      out.found = true;
      out.node = finder.node_of(p, tile, list_size);
      return out;
    }
    out.connected = out.connected || connects;
  }
  return out;
}

void insert_label(NodeRegion& node, LabelRegion label, const compat::CompatSettings& compat) {
  const std::size_t at = label_insert_position(node.labels, label.port, compat);
  node.labels.insert(node.labels.begin() + static_cast<std::ptrdiff_t>(at), std::move(label));
}

/// A label over no node: the substrate node (prepended) for a substrate
/// label over space on the substrate plane, otherwise a sticky-label node.
void attach_unconnected(NodeRegionList& nodes, const RegionFinder& finder, const ExtStyle& style,
                        const LabelPassOptions& options, const Probe& probe, LabelRegion label,
                        tech::PlaneId plane) {
  if (!options.sticky) {
    return;
  }
  const bool substrate_label = style.substrate.plane && *style.substrate.plane == plane &&
                               style.substrate.types.test(label.type) && !probe.connected;
  if (substrate_label && finder.substrate_region()) {
    const std::size_t d = *finder.substrate_region();
    if (d >= finder.first_region() && d - finder.first_region() < nodes.size()) {
      NodeRegion& subs = nodes[node_index(nodes.size(), d - finder.first_region())];
      subs.labels.insert(subs.labels.begin(), std::move(label));
    }
    return;
  }
  const db::Rect clip = options.clip.value_or(db::whole_plane());
  if (!db::touch(label.rect, clip) || label.type == tech::kSpace || !style.active.test(label.type)) {
    return;
  }
  NodeRegion sticky;
  sticky.plane = plane;
  sticky.type = label.type;
  sticky.reference = label.position;
  sticky.classes.resize(style.resist_classes.size());
  sticky.labels.push_back(std::move(label));
  nodes.insert(nodes.begin(), std::move(sticky));
}

std::string short_type(const tech::Technology& technology, tech::TileType type) {
  return std::string{technology.type_name(type, tech::NameForm::short_name)};
}

std::vector<std::int64_t> pairs_of(const NodeRegion& node) {
  std::vector<std::int64_t> out;
  out.reserve(node.classes.size() * 2);
  for (const ClassArea& c : node.classes) {
    out.push_back(c.area);
    out.push_back(c.perimeter);
  }
  return out;
}

/// The node's name for its records: a name label, else extMakeNodeNumPrint.
std::string record_name(const NodeRegionList& nodes, std::size_t i, const tech::Technology& technology,
                        const SubstrateName& substrate) {
  if (const LabelRegion* label = name_label(nodes[i])) {
    return label->text;
  }
  return number_name(nodes, i, technology, substrate);
}

void write_ports(ExtWriter& writer, NodeRegionList& nodes, const tech::Technology& technology,
                 const SubstrateName& substrate) {
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    for (std::size_t k = 0; k < nodes[i].labels.size(); ++k) {
      const LabelRegion label = nodes[i].labels[k];
      if (!label.port) {
        continue;
      }
      writer.port(label.text, static_cast<std::int32_t>(label.port_index), label.rect,
                  short_type(technology, label.type));
      if (record_name(nodes, i, technology, substrate) == label.text) {
        NodeRegion& node = nodes[i];
        node.reference = label.position;
        node.type = label.type;
        node.plane = technology.home_plane(label.type).value_or(node.plane);
        node.right_side = false;
      }
    }
  }
}

std::string truncate_uq(std::string_view text) {
  const std::size_t at = text.find("_uq");
  return std::string{at == std::string_view::npos ? text : text.substr(0, at)};
}

/// The state of extOutputNodes' alias loop for one node.
struct AliasState {
  explicit AliasState(const std::string& n) : name(n) {}
  const std::string& name;
  std::string portname;  // empty = none
  bool have_port = false;
  bool name_is_port = false;
  std::string lastname;
  bool have_last = false;
};

void alias_one(ExtWriter& writer, AliasState& st, const LabelRegion& label, bool warn,
               diag::DiagnosticSink& sink, NodeRecordResult& result) {
  if (!st.have_port || label.text != st.portname) {
    if (!st.have_last || label.text != st.lastname) {
      writer.equiv(st.name, label.text);
    }
    st.lastname = label.text;
    st.have_last = true;
  }
  if (warn && st.have_port && label.port && label.text != st.portname) {
    const std::string a = truncate_uq(st.name);
    const std::string b = truncate_uq(label.text);
    if (a != b) {
      sink.emit(diag::make(kCodeShorted, "Warning:  Ports \"" + a + "\" and \"" + b + "\" are electrically shorted."));
      ++result.warnings;
    }
  }
  if (!st.name_is_port && label.port) {
    st.portname = label.text;
    st.have_port = true;
  }
}

void write_aliases(ExtWriter& writer, const NodeRegion& node, const std::string& name,
                   const NodeRecordOptions& options, bool warn, diag::DiagnosticSink& sink,
                   NodeRecordResult& result) {
  const LabelRegion* chosen = name_label(node);
  if (chosen == nullptr) {
    return;
  }
  AliasState st(name);
  st.name_is_port = chosen->port;
  st.have_port = chosen->port;
  st.portname = chosen->port ? name : std::string{};
  for (auto it = node.labels.begin() + (chosen - node.labels.data()) + 1; it != node.labels.end(); ++it) {
    if (label_role(it->text) != LabelRole::name || it->text == name) {
      continue;
    }
    if (it->port || options.aliases) {
      alias_one(writer, st, *it, warn, sink, result);
    } else if (it->label < result.non_exported.size()) {
      result.non_exported[it->label] = true;  // lab_port = INFINITY
    }
  }
}

void write_one_node(ExtWriter& writer, const NodeRegionList& nodes, std::size_t i, const std::string& name,
                    const tech::Technology& technology, const NodeRecordOptions& options) {
  const NodeRegion& node = nodes[i];
  const std::int64_t r = i < options.resistance.size() ? options.resistance[i] : 0;
  const std::vector<std::int64_t> pairs = pairs_of(node);
  const std::string type = short_type(technology, node.type);
  if (options.substrate.node == i && options.substrate.record) {
    writer.substrate(name, r, node.reference, type, pairs);
  } else {
    double c = i < options.capacitance.size() ? options.capacitance[i] : node.substrate_cap / options.cscale;
    if (c < 0.0) {
      c = 0.0;  // round-off near zero
    }
    writer.node(name, r, c, node.reference, type, pairs);
  }
  for (const LabelRegion& label : node.labels) {
    if (label_role(label.text) == LabelRole::node_attribute) {
      writer.attr(name, label.rect, short_type(technology, label.type),
                  std::string_view{label.text}.substr(0, label.text.size() - 1));
    }
  }
}

}  // namespace

void attach_labels(const db::CellDef& cell, const RegionFinder& finder, NodeRegionList& nodes,
                   const ExtStyle& style, const tech::Technology& technology, const LabelPassOptions& options,
                   const compat::CompatSettings& compat) {
  const auto& labels = cell.labels();
  for (std::size_t i = 0; i < labels.size(); ++i) {
    const db::Label& label = labels[i];
    if (options.skip != nullptr && i < options.skip->size() && (*options.skip)[i]) {
      continue;
    }
    const std::optional<tech::PlaneId> plane = technology.home_plane(label.type);
    if (label.type == tech::kSpace || !plane || *plane < tech::kFirstUserPlane || *plane >= cell.plane_count()) {
      continue;
    }
    const Probe probe = probe_label(cell, finder, nodes.size(), style, label, *plane);
    if (probe.node) {
      insert_label(nodes[*probe.node], region_label(label, i), compat);
    } else if (!probe.found) {
      attach_unconnected(nodes, finder, style, options, probe, region_label(label, i), *plane);
    }
  }
}

NodeRecordResult write_node_records(ExtWriter& writer, NodeRegionList& nodes, const db::CellDef& cell,
                                    const tech::Technology& technology, const NodeRecordOptions& options,
                                    const compat::CompatSettings& compat, diag::DiagnosticSink& sink) {
  NodeRecordResult result;
  result.non_exported.assign(cell.labels().size(), false);
  write_ports(writer, nodes, technology, options.substrate);
  // Q-EXTRACT-22: ON suppresses the warning with lumped resistance on (as
  // Magic), OFF only when full R-C extraction runs in the same pass.
  const bool warn = compat.magic_compat ? !options.resistance_on : !options.extresist;
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    const std::string name = record_name(nodes, i, technology, options.substrate);
    write_one_node(writer, nodes, i, name, technology, options);
    write_aliases(writer, nodes[i], name, options, warn, sink, result);
  }
  return result;
}

std::string number_name(const NodeRegionList& nodes, std::size_t i, const tech::Technology& technology,
                        const SubstrateName& substrate) {
  if (substrate.node == i) {
    return substrate.name;
  }
  return generated_name(nodes[i], technology);
}

namespace {

/// One feedback entry per occurrence of `text` in `nodes`.
void report_occurrences(std::vector<NodeFeedback>& out, const NodeRegionList& nodes, const std::string& text,
                        const tech::Technology& technology, const SubstrateName& substrate) {
  for (std::size_t j = 0; j < nodes.size(); ++j) {
    for (const LabelRegion& other : nodes[j].labels) {
      if (other.text != text) {
        continue;
      }
      const db::Point p = other.position;
      out.push_back({db::Rect{p.x - 1, p.y - 1, p.x + 1, p.y + 1},
                     "Label \"" + text + "\" attached to more than one unconnected node: " +
                         number_name(nodes, j, technology, substrate),
                     false});
    }
  }
}

}  // namespace

std::vector<NodeFeedback> duplicate_label_feedback(const db::CellDef& cell, const NodeRegionList& nodes,
                                                   const tech::Technology& technology,
                                                   const SubstrateName& substrate) {
  std::vector<NodeFeedback> out;
  if (cell.properties().find("LEFview") != nullptr) {
    return out;
  }
  constexpr std::size_t kReported = static_cast<std::size_t>(-1);
  std::unordered_map<std::string, std::size_t> first;
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    for (const LabelRegion& label : nodes[i].labels) {
      if (label_role(label.text) != LabelRole::name) {
        continue;
      }
      const auto [it, inserted] = first.try_emplace(label.text, i);
      if (!inserted && it->second != i && it->second != kReported) {
        report_occurrences(out, nodes, label.text, technology, substrate);
        it->second = kReported;
      }
    }
  }
  return out;
}

}  // namespace hefesto::extract
