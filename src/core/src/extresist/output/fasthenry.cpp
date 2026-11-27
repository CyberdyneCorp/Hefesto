// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The FastHenry geometry file's per-net blocks (extresist/output/output.hpp;
// capability extresist-output "FastHenry geometry file", "Geometry
// centerlines"): ResAlignNodes, the node list with heights and contact
// cuts, the segments and the file's end. Q-NETLIST-27 (a contact-only node
// takes the contact's upper residue's height) and the cut of a contact the
// CIF style does not define (edge 0) apply in both modes.
//
// Ported from Magic 8.3.694 (fc5daa9e) resis/ResPrint.c (resWriteNodeName,
// ResPrintFHNodes, ResPrintFHRects), resis/ResRex.c (ResAlignNodes and the
// end of ResCheckExtNodes), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: RESCMD.

#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include "hefesto/extresist/output/output.hpp"
#include "hefesto/lef/write/number.hpp"
#include "hefesto/mask/style/style.hpp"

namespace hefesto::extresist {

namespace {

constexpr std::uint32_t kXAdjusted = 1;  // RES_NODE_XADJ
constexpr std::uint32_t kYAdjusted = 2;  // RES_NODE_YADJ

// C's "%1.2f" of a float.
std::string f2(float value) {
  std::array<char, 64> buffer{};
  (void)std::snprintf(buffer.data(), buffer.size(), "%1.2f", static_cast<double>(value));
  return buffer.data();
}

// resWriteNodeName without the leading N.
std::string node_name(const Network& net, std::size_t n, const std::vector<std::int32_t>& ids) {
  const NetNode& node = net.nodes[n];
  return node.named ? node.name : std::to_string(n < ids.size() ? ids[n] : 0);
}

// One end of an aligned resistor.
void align_end(NetNode& node, std::uint32_t& adjusted, const NetResistor& r, std::vector<std::string>& messages) {
  if ((r.status & kStatusEW) != 0) {
    if (node.location.y != r.centerline) {
      if ((adjusted & kYAdjusted) != 0) {
        messages.emplace_back("Warning: contention over node Y position");
      }
      node.location.y = r.centerline;
      adjusted |= kYAdjusted;
    }
  } else if ((r.status & kStatusNS) != 0 && node.location.x != r.centerline) {
    if ((adjusted & kXAdjusted) != 0) {
      messages.emplace_back("Warning: contention over node X position");
    }
    node.location.x = r.centerline;
    adjusted |= kXAdjusted;
  }
}

bool is_contact(const GeometryContext& ctx, tech::TileType type) {
  return ctx.technology != nullptr && ctx.technology->is_contact(type);
}

// The height of a non-contact type: exts_height, else 0.1 times the plane
// order of its plane.
float type_height(const GeometryContext& ctx, tech::TileType type) {
  if (ctx.style == nullptr || type >= ctx.style->heights.size()) {
    return 0.0F;
  }
  const float height = ctx.style->heights[type].height;
  if (height != 0.0F || ctx.technology == nullptr) {
    return height;
  }
  const std::optional<tech::PlaneId> plane = ctx.technology->home_plane(type);
  if (!plane || *plane >= ctx.style->plane_order.size()) {
    return 0.0F;
  }
  return static_cast<float>(0.1 * static_cast<double>(ctx.style->plane_order[*plane]));
}

// Q-NETLIST-27: the height of the contact's residue on its highest plane
// in the plane order.
float contact_height(const GeometryContext& ctx, tech::TileType contact) {
  if (ctx.technology == nullptr || ctx.style == nullptr) {
    return 0.0F;
  }
  std::int32_t best_order = -1;
  float height = 0.0F;
  const tech::TypeMask residues = ctx.technology->full_residues(contact);
  for (tech::TileType t = 0; t < tech::kMaxTileTypes; ++t) {
    const std::optional<tech::PlaneId> plane = residues.test(t) ? ctx.technology->home_plane(t) : std::nullopt;
    if (plane && *plane < ctx.style->plane_order.size() && ctx.style->plane_order[*plane] > best_order) {
      best_order = ctx.style->plane_order[*plane];
      height = type_height(ctx, t);
    }
  }
  return height;
}

struct NodeLayer {
  float height = 0.0F;
  const NetResistor* contact = nullptr;
};

// The height of a node and its (last) contact resistor.
NodeLayer node_layer(const Network& net, const NetNode& node, const GeometryContext& ctx) {
  NodeLayer out;
  bool found = false;
  for (const std::size_t ri : node.resistors) {
    const NetResistor& r = net.resistors[ri];
    if (is_contact(ctx, r.type)) {
      out.contact = &r;
    } else if (!found || ctx.magic_compat) {
      out.height = type_height(ctx, r.type);
      found = true;
    }
  }
  if (!found && out.contact != nullptr) {
    out.height = contact_height(ctx, out.contact->type);
  }
  return out;
}

mask::OutputStyle::ContactCut cut_of(const GeometryContext& ctx, tech::TileType type) {
  if (ctx.cif == nullptr) {
    return {};
  }
  return ctx.cif->contact_cut(type).value_or(mask::OutputStyle::ContactCut{});
}

// The cut nodes of a multi-cut contact node and their `.equiv` line.
std::string cut_nodes(const std::string& name, const NetNode& node, const NetResistor& contact, float height,
                      float oscale, const GeometryContext& ctx) {
  const mask::OutputStyle::ContactCut cut = cut_of(ctx, contact.type);
  const float del = static_cast<float>(cut.spacing + cut.edge) / (oscale * 100.0F);
  const float cxb = static_cast<float>(contact.centerline - 1) / 2.0F;
  const float cyb = static_cast<float>(contact.width - 1) / 2.0F;
  std::string out;
  std::string equiv = ".equiv N" + name;
  for (std::int32_t i = 0; i < contact.centerline; ++i) {
    const float cx = del * (static_cast<float>(i) - cxb);
    for (std::int32_t j = 0; j < contact.width; ++j) {
      const float cy = del * (static_cast<float>(j) - cyb);
      const std::string cut_name = "N" + name + "_" + std::to_string(i) + "_" + std::to_string(j);
      out += cut_name + " x=" + f2((static_cast<float>(node.location.x) + cx) * oscale) +
             " y=" + f2((static_cast<float>(node.location.y) + cy) * oscale) + " z=" + f2(height) + "\n";
      equiv += " " + cut_name;
    }
  }
  return out + equiv + "\n";
}

bool multi_cut(const NetResistor& r) {
  return r.centerline > 1 || r.width > 1;
}

}  // namespace

void align_nodes(Network& net, bool magic_compat, std::vector<std::string>& messages) {
  std::vector<std::uint32_t> adjusted(net.nodes.size(), 0);
  const std::size_t count = magic_compat && !net.resistors.empty() ? net.resistors.size() - 1 : net.resistors.size();
  for (std::size_t i = 0; i < count; ++i) {
    const NetResistor& r = net.resistors[i];
    if ((r.status & kStatusDiagonal) != 0) {
      continue;
    }
    for (const std::size_t n : {r.a, r.b}) {
      if (n < net.nodes.size()) {
        align_end(net.nodes[n], adjusted[n], r, messages);
      }
    }
  }
}

std::string fasthenry_nodes(const Network& net, const GeometryContext& ctx, FastHenryIds& ids,
                            std::vector<std::int32_t>& node_ids) {
  const float oscale = lef::output_scale(ctx.cif);
  node_ids.assign(net.nodes.size(), -1);
  std::string out = "\n* List of nodes in network\n";
  for (std::size_t n = 0; n < net.nodes.size(); ++n) {
    const NetNode& node = net.nodes[n];
    if (!node.named) {
      node_ids[n] = ids.node++;
    }
    const std::string name = node_name(net, n, node_ids);
    const NodeLayer layer = node_layer(net, node, ctx);
    const float height = layer.height * oscale;
    out += "N" + name + " x=" + f2(static_cast<float>(node.location.x) * oscale) +
           " y=" + f2(static_cast<float>(node.location.y) * oscale) + " z=" + f2(height) + "\n";
    if (layer.contact != nullptr && multi_cut(*layer.contact)) {
      out += cut_nodes(name, node, *layer.contact, height, oscale, ctx);
    }
  }
  return out;
}

namespace {

// A wire segment: the type's thickness (0.05 when it has none).
std::string wire_segment(const NetResistor& r, const GeometryContext& ctx, FastHenryIds& ids, const std::string& a,
                         const std::string& b, float oscale) {
  float thick =
      ctx.style != nullptr && r.type < ctx.style->heights.size() ? ctx.style->heights[r.type].thickness : 0.0F;
  if (thick == 0.0F) {
    thick = 0.05F;
  }
  return "E" + std::to_string(ids.segment++) + " N" + a + " N" + b + " w=" + f2(static_cast<float>(r.width) * oscale) +
         " h=" + f2(thick * oscale) + "\n";
}

// A contact segment, or one per cut of a multi-cut contact, square in the
// cut width.
std::string contact_segments(const NetResistor& r, const GeometryContext& ctx, FastHenryIds& ids,
                             const std::string& a, const std::string& b) {
  const float cwidth = static_cast<float>(cut_of(ctx, r.type).edge) / 100.0F;
  const std::string wh = "w=" + f2(cwidth) + " h=" + f2(cwidth) + "\n";
  if (!multi_cut(r)) {
    return "E" + std::to_string(ids.segment++) + " N" + a + " N" + b + " " + wh;
  }
  std::string out;
  for (std::int32_t i = 0; i < r.centerline; ++i) {
    for (std::int32_t j = 0; j < r.width; ++j) {
      const std::string suffix = "_" + std::to_string(i) + "_" + std::to_string(j) + " ";
      out += "E" + std::to_string(ids.segment++) + " N" + a + suffix + "N" + b + suffix + wh;
    }
  }
  return out;
}

}  // namespace

std::string fasthenry_segments(const Network& net, const GeometryContext& ctx, FastHenryIds& ids,
                               const std::vector<std::int32_t>& node_ids) {
  const float oscale = lef::output_scale(ctx.cif);
  std::string out = "* Segments connecting nodes in network\n";
  for (const NetResistor& r : net.resistors) {
    if (r.a >= net.nodes.size() || r.b >= net.nodes.size()) {
      continue;
    }
    const std::string a = node_name(net, r.a, node_ids);
    const std::string b = node_name(net, r.b, node_ids);
    out += is_contact(ctx, r.type) ? contact_segments(r, ctx, ids, a, b) : wire_segment(r, ctx, ids, a, b, oscale);
  }
  return out;
}

std::string fasthenry_trailer(double frequency, const std::vector<std::pair<std::uint32_t, std::string>>& ports) {
  std::array<char, 64> f{};
  (void)std::snprintf(f.data(), f.size(), "%2.1g", frequency);
  std::string out = std::string{"\n.freq fmin="} + f.data() + " fmax=" + f.data() + "\n";
  out += "\n* Order of arguments to SPICE subcircuit call:\n";
  for (const auto& [index, text] : ports) {
    out += "* " + std::to_string(index) + " " + text + "\n";
  }
  return out + "\n.end\n";
}

}  // namespace hefesto::extresist
