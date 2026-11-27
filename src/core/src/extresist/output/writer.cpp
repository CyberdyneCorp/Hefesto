// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `.res.ext` overlay and `.res.lump` text (extresist/output/output.hpp;
// capability extresist-output "Overlay file grammar and order", "Naming of
// network nodes"; extresist-command "Lumped resistance file"): the scale
// line, the net blocks, the node names and the device records with
// Q-NETLIST-39 per mode, and the geometry centerlines (the FastHenry
// writer is in fasthenry.cpp).
//
// Ported from Magic 8.3.694 (fc5daa9e) resis/ResPrint.c (ResPrintExtRes,
// ResPrintExtNode, ResPrintExtDev) and resis/ResRex.c (ResWriteExtFile,
// ResWriteLumpFile), Copyright (C) 1985, 1990 Regents of the University
// of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: RESCMD.

#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <string>

#include "hefesto/extresist/output/output.hpp"

namespace hefesto::extresist {

namespace {

// extDevTable.
constexpr std::array<std::string_view, 17> kClasses{
    "fet",   "mosfet", "asymmetric", "bjt",    "devres",  "devcap",  "devcaprev", "vsource",  "diode",
    "pdiode", "ndiode", "subckt",     "rsubckt", "msubckt", "csubckt", "dsubckt",   "veriloga",
};

std::string printf_g(double value) {
  std::array<char, 64> buffer{};
  (void)std::snprintf(buffer.data(), buffer.size(), "%g", value);
  return buffer.data();
}

std::string q(std::string_view s) {
  return "\"" + std::string{s} + "\"";
}

std::string attr(const std::string& a) {
  return a.empty() ? "0" : a;
}

// net': one trailing `!` or `#` removed.
std::string trimmed(std::string_view net) {
  if (!net.empty() && (net.back() == '!' || net.back() == '#')) {
    net.remove_suffix(1);
  }
  return std::string{net};
}

const extract::DevicePattern* style_line(const extract::ExtStyle* style, const std::string& model) {
  if (style == nullptr) {
    return nullptr;
  }
  for (const extract::DevicePattern& line : style->devices) {
    if (line.model == model) {
      return &line;
    }
  }
  return nullptr;
}

bool two_terminal(extract::DeviceClass c) {
  return c == extract::DeviceClass::devres || c == extract::DeviceClass::devcap ||
         c == extract::DeviceClass::devcaprev;
}

// The layout record the overlay prints (the input record's values when
// the device was never matched to a layout device).
LayoutRecord record_of(const InputDevice& d) {
  return d.layout.value_or(LayoutRecord{d.location, d.length, d.width, 0, 0});
}

// The geometry pair (or, with magic-compat OFF, a resistor's or
// capacitor's leading fields; Q-NETLIST-39).
std::string geometry_fields(const InputDevice& d, extract::DeviceClass c, bool magic_compat) {
  const LayoutRecord r = record_of(d);
  switch (c) {
    case extract::DeviceClass::fet:
      if (!d.layout && d.leading.size() >= 2) {
        return " " + d.leading[0] + " " + d.leading[1];
      }
      return " " + std::to_string(static_cast<std::int32_t>(r.area)) + " " +
             std::to_string(static_cast<std::int32_t>(r.perimeter));
    case extract::DeviceClass::mosfet:
    case extract::DeviceClass::asymmetric:
    case extract::DeviceClass::bjt:
      return " " + std::to_string(r.length) + " " + std::to_string(r.width);
    default:
      break;
  }
  std::string out;
  if (!magic_compat && two_terminal(c)) {
    for (const std::string& f : d.leading) {
      out += " " + f;
    }
  }
  return out;
}

std::string net_name(const InputModel& input, const std::optional<std::size_t>& net) {
  return net && *net < input.nodes.size() ? input.nodes[*net].name : std::string{};
}

}  // namespace

std::string res_ext_scale(const extract::ExtStyle& style) {
  return "scale " + std::to_string(static_cast<std::int32_t>(style.rscale)) + " " +
         std::to_string(static_cast<std::int32_t>(style.cscale)) + " " +
         printf_g(static_cast<double>(style.units_per_lambda)) + "\n";
}

std::string lump_line(std::string_view net, std::int32_t ohms) {
  return "R " + std::string{net} + " " + std::to_string(ohms) + "\n";
}

std::string format_device(const InputModel& input, const InputDevice& device, std::string_view default_substrate,
                          const compat::CompatSettings& compat) {
  const extract::DeviceClass c = device.style_class.value_or(extract::DeviceClass::mosfet);
  const LayoutRecord r = record_of(device);
  std::string out = c == extract::DeviceClass::fet ? "" : "device ";
  out += std::string{kClasses.at(static_cast<std::size_t>(c))} + " " + device.model + " " +
         std::to_string(r.location.x) + " " + std::to_string(r.location.y) + " " + std::to_string(r.location.x + 1) +
         " " + std::to_string(r.location.y + 1) + " ";
  out += geometry_fields(device, c, compat.magic_compat);
  const std::string subs = net_name(input, device.subs);
  out += " " + q(!subs.empty() ? subs : (!default_substrate.empty() ? std::string{default_substrate} : "None"));
  if (device.gate) {
    out += " " + q(net_name(input, device.gate)) + " " + std::to_string(r.length * 2) + " " + attr(device.gate_attr);
  }
  if (device.source) {
    out += " " + q(net_name(input, device.source)) + " " + std::to_string(r.width) + " " + attr(device.source_attr);
  }
  // Two-terminal devices (exts_deviceSDCount 1) write no drain.
  if (device.sd_count > 1 && device.drain) {
    out += " " + q(net_name(input, device.drain)) + " " + std::to_string(r.width) + " " + attr(device.drain_attr);
  }
  return out + "\n";
}

namespace {

// ResPrintExtNode's killnode decision: no node keeps the net's exact name.
bool kills(std::string_view net, const Network& network) {
  return std::none_of(network.nodes.begin(), network.nodes.end(),
                      [&](const NetNode& n) { return n.named && n.name == net; });
}

// The node that has the net's own capacitance subtracted: the one named
// as the net, else (with the orphan patch) the first node.
std::size_t kept_node(std::string_view net, const Network& network, bool kill, bool patch) {
  if (kill) {
    return kNoNode;
  }
  for (std::size_t i = 0; i < network.nodes.size(); ++i) {
    if (network.nodes[i].named && network.nodes[i].name == net) {
      return i;
    }
  }
  return patch && !network.nodes.empty() ? 0 : kNoNode;
}

// `%g` of the float snodecap / exts_capScale.
std::string rnode_line(const NetNode& n, float cap, float cap_scale) {
  return "rnode " + q(n.name) + " 0 " + printf_g(static_cast<double>(cap / cap_scale)) + " " +
         std::to_string(n.location.x) + " " + std::to_string(n.location.y) + " 0\n";
}

// ResPrintExtNode's orphan patch.
std::string orphan_patch(const InputNode& node, const Network& network, std::vector<std::string>& messages) {
  if (network.nodes.empty()) {
    messages.push_back("Error:  Orphaned node \"" + node.name + "\" not output.");
    return {};
  }
  messages.push_back("Warning:  Orphaned node \"" + node.name + "\" arbitrarily attached to \"" +
                     network.nodes.front().name + "\"");
  return "resist " + q(node.name) + " " + q(network.nodes.front().name) + " 0.0\n";
}

}  // namespace

std::string net_block(const InputNode& node, Network& network, bool dont_kill, const extract::ExtStyle& style,
                      std::vector<std::string>& messages) {
  const auto cap_scale = static_cast<float>(static_cast<std::int32_t>(style.cscale));
  const auto resist_scale = static_cast<float>(static_cast<std::int32_t>(style.rscale));
  const bool unnamed_net = kills(node.name, network);
  const bool patch = unnamed_net && dont_kill;
  const bool kill = unnamed_net && !dont_kill;
  std::string out = kill ? "killnode " + q(node.name) + "\n" : std::string{};
  const std::size_t keep = kept_node(node.name, network, kill, patch);
  const std::string base = trimmed(node.name);
  int k = 0;
  for (std::size_t i = 0; i < network.nodes.size(); ++i) {
    NetNode& n = network.nodes[i];
    if (!n.named) {
      n.name = base + ".n" + std::to_string(k++);
      n.named = true;
    }
    const auto cap = static_cast<float>(n.cap);
    out += rnode_line(n, i == keep ? cap - static_cast<float>(node.node_cap) : cap, cap_scale);
  }
  if (patch) {
    out += orphan_patch(node, network, messages);
  }
  for (const NetResistor& r : network.resistors) {
    if (r.a < network.nodes.size() && r.b < network.nodes.size()) {
      const float value = static_cast<float>(r.resistance) / resist_scale;
      out += "resist " + q(network.nodes[r.a].name) + " " + q(network.nodes[r.b].name) + " " +
             printf_g(static_cast<double>(value)) + "\n";
    }
  }
  return out;
}

diag::Outcome<std::string> write_res_ext(const InputModel& input, const std::vector<OutputNet>& nets,
                                         const extract::ExtStyle& style, const compat::CompatSettings& compat) {
  std::string out = res_ext_scale(style);
  for (const OutputNet& on : nets) {
    out += on.block;
  }
  for (std::size_t i = input.devices.size(); i-- > 0;) {
    const InputDevice& d = input.devices[i];
    if (!d.touched) {
      continue;
    }
    const extract::DevicePattern* line = style_line(&style, d.model);
    out += format_device(input, d, line != nullptr ? line->substrate_node : std::string_view{}, compat);
  }
  return out;
}

std::string write_res_lump(const InputModel& input, const std::vector<OutputNet>& nets) {
  std::string out;
  for (const OutputNet& on : nets) {
    out += lump_line(input.nodes.at(on.net).name, on.lump_ohms);
  }
  return out;
}

std::vector<GeometryElement> geometry_elements(const Network& net, std::int32_t& next_id,
                                               const tech::Technology* technology) {
  std::vector<GeometryElement> out;
  const auto end_name = [&](const NetNode& n) {
    return n.name.empty() ? std::to_string(next_id++) : n.name;
  };
  for (const NetResistor& r : net.resistors) {
    if (r.a >= net.nodes.size() || r.b >= net.nodes.size() ||
        (technology != nullptr && technology->is_contact(r.type))) {
      continue;  // vias are ignored
    }
    const NetNode& a = net.nodes[r.a];
    const NetNode& b = net.nodes[r.b];
    GeometryElement e;
    e.name = "N" + end_name(a) + "_";
    e.name += end_name(b);
    const db::Rect canon = db::canonical(db::Rect{a.location.x, a.location.y, b.location.x, b.location.y});
    e.from = db::Point{canon.x0, canon.y0};
    e.to = db::Point{canon.x1, canon.y1};
    out.push_back(std::move(e));
  }
  return out;
}

namespace {

std::string fixed2(double value) {
  std::array<char, 64> buffer{};
  (void)std::snprintf(buffer.data(), buffer.size(), "%1.2f", value);
  return buffer.data();
}

}  // namespace

std::string fasthenry_reference(std::string_view cell, const db::Rect& bbox, float oscale) {
  const auto um = [oscale](db::Coord v) { return fixed2(static_cast<double>(static_cast<float>(v) * oscale)); };
  const std::int64_t segs_x = std::max<std::int64_t>(bbox.width() / 20, 4);
  const std::int64_t segs_y = std::max<std::int64_t>(bbox.height() / 20, 4);
  std::string out = "* FastHenry output for magic cell " + std::string{cell} + "\n\n";
  out += ".Units um\n.Default rho=0.02 nhinc=3 nwinc=3 rh=2 rw=2\n\n* Reference plane (substrate, ground)\n";
  out += "Gsub x1=" + um(bbox.x0) + " y1=" + um(bbox.y0) + " z1=0 x2=" + um(bbox.x1) + " y2=" + um(bbox.y0) +
         " z2=0\n";
  out += "+ x3=" + um(bbox.x1) + " y3=" + um(bbox.y1) + " z3=0\n";
  out += "+ thick=0.1 seg1=" + std::to_string(segs_x) + " seg2=" + std::to_string(segs_y) + "\n";
  out += "+ Ngp (" + um(bbox.x0) + "," + um(bbox.y0) + ",0)\n";
  out += "\nNsub x=" + um(bbox.x0) + " y=" + um(bbox.y0) + " z=0\n.Equiv Nsub Ngp\n";
  return out;
}

}  // namespace hefesto::extresist
