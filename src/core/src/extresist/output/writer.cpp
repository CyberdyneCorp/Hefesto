// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `.res.ext` overlay and `.res.lump` text (extresist/output/output.hpp;
// capability extresist-output "Overlay file grammar and order", "Naming of
// network nodes"; extresist-command "Lumped resistance file"): the scale
// line, the net blocks, the node names and the device records with
// Q-NETLIST-39 per mode. The FastHenry and geometry writers need the
// network's widths, heights and contact geometry, which the RESNET
// network contract does not carry yet; they answer CORE-NOTIMPL-008.
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

#include "hefesto/cmd/m7_stubs.hpp"
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

// The geometry pair (or, with magic-compat OFF, a resistor's or
// capacitor's leading fields; Q-NETLIST-39).
std::string geometry_fields(const InputDevice& d, extract::DeviceClass c, bool magic_compat) {
  std::string out;
  switch (c) {
    case extract::DeviceClass::fet:
    case extract::DeviceClass::mosfet:
    case extract::DeviceClass::asymmetric:
    case extract::DeviceClass::bjt:
      if (c == extract::DeviceClass::fet && d.leading.size() >= 2) {
        return " " + d.leading[0] + " " + d.leading[1];
      }
      return " " + std::to_string(d.length) + " " + std::to_string(d.width);
    default:
      break;
  }
  if (!magic_compat && two_terminal(c)) {
    for (const std::string& f : d.leading) {
      out += " " + f;
    }
  }
  return out;
}

std::string term_name(const InputModel& input, const std::vector<std::string>& names, std::size_t slot,
                      const std::optional<std::size_t>& net) {
  if (slot < names.size() && !names[slot].empty()) {
    return names[slot];
  }
  return net ? input.nodes[*net].name : std::string{};
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

// names: [gate, source, drain, subs] renamed nets (empty: unchanged).
std::string format_device(const InputModel& input, const InputDevice& device, const std::vector<std::string>& names,
                          std::string_view default_substrate, const compat::CompatSettings& compat) {
  const extract::DeviceClass c = device.style_class.value_or(extract::DeviceClass::mosfet);
  std::string out = c == extract::DeviceClass::fet ? "" : "device ";
  out += std::string{kClasses.at(static_cast<std::size_t>(c))} + " " + device.model + " " +
         std::to_string(device.location.x) + " " + std::to_string(device.location.y) + " " +
         std::to_string(device.location.x + 1) + " " + std::to_string(device.location.y + 1) + " ";
  out += geometry_fields(device, c, compat.magic_compat);
  const std::string subs = term_name(input, names, 3, device.subs);
  out += " " + q(!subs.empty() ? subs : (!default_substrate.empty() ? std::string{default_substrate} : "None"));
  if (device.gate) {
    out += " " + q(term_name(input, names, 0, device.gate)) + " " + std::to_string(device.length * 2) + " " +
           attr(device.gate_attr);
  }
  if (device.source) {
    out += " " + q(term_name(input, names, 1, device.source)) + " " + std::to_string(device.width) + " " +
           attr(device.source_attr);
  }
  // Two-terminal devices (exts_deviceSDCount 1) write no drain.
  if (device.sd_count > 1 && device.drain) {
    out += " " + q(term_name(input, names, 2, device.drain)) + " " + std::to_string(device.width) + " " +
           attr(device.drain_attr);
  }
  return out + "\n";
}

bool name_nodes(std::string_view net, Network& network, const std::vector<TerminalNode>& terminals) {
  const std::string base = trimmed(net);
  std::size_t k = 0;
  for (const TerminalNode& t : terminals) {
    if (t.node < network.nodes.size() && network.nodes[t.node].name.empty()) {
      network.nodes[t.node].name = base + ".t" + std::to_string(k++);
    }
  }
  k = 0;
  for (NetNode& n : network.nodes) {
    if (n.name.empty()) {
      n.name = base + ".n" + std::to_string(k++);
    }
  }
  for (const NetNode& n : network.nodes) {
    if (n.name == net) {
      return false;
    }
  }
  return true;
}

std::string net_block(std::string_view net, const Network& network, bool kill, double node_cap,
                      const extract::ExtStyle& style) {
  const double cap_scale = static_cast<double>(static_cast<std::int32_t>(style.cscale));
  const double resist_scale = static_cast<double>(static_cast<std::int32_t>(style.rscale));
  std::string out = kill ? "killnode " + q(net) + "\n" : std::string{};
  for (const NetNode& n : network.nodes) {
    const double cap = n.name == net ? n.cap - node_cap : n.cap;
    out += "rnode " + q(n.name) + " 0 " + printf_g(cap / cap_scale) + " " + std::to_string(n.location.x) + " " +
           std::to_string(n.location.y) + " 0\n";
  }
  for (const NetResistor& r : network.resistors) {
    if (r.a < network.nodes.size() && r.b < network.nodes.size()) {
      out += "resist " + q(network.nodes[r.a].name) + " " + q(network.nodes[r.b].name) + " " +
             printf_g(r.resistance / resist_scale) + "\n";
    }
  }
  return out;
}

diag::Outcome<std::string> write_res_ext(const InputModel& input, std::vector<OutputNet>& nets,
                                         const extract::ExtStyle& style, const compat::CompatSettings& compat) {
  std::string out = res_ext_scale(style);
  // device index -> renamed [gate, source, drain, subs]
  std::map<std::size_t, std::vector<std::string>> renamed;
  for (OutputNet& on : nets) {
    const InputNode& node = input.nodes.at(on.net);
    const bool kill = name_nodes(node.name, on.network, on.terminals);
    out += net_block(node.name, on.network, kill, node.node_cap, style);
    for (const TerminalNode& t : on.terminals) {
      std::vector<std::string>& names = renamed[t.device];
      names.resize(4);
      names[static_cast<std::size_t>(t.terminal)] = on.network.nodes.at(t.node).name;
    }
  }
  for (std::size_t i = input.devices.size(); i-- > 0;) {
    const auto found = renamed.find(i);
    if (found == renamed.end()) {
      continue;
    }
    const InputDevice& d = input.devices[i];
    const extract::DevicePattern* line = style_line(&style, d.model);
    out += format_device(input, d, found->second, line != nullptr ? line->substrate_node : std::string_view{}, compat);
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

diag::Outcome<std::string> write_fasthenry(const InputModel& /*input*/, const std::vector<OutputNet>& /*nets*/,
                                           double /*frequency*/, const compat::CompatSettings& /*compat*/) {
  return cmd::m7_not_implemented_diagnostic("extresist fasthenry");
}

std::vector<GeometryElement> geometry_elements(const Network& net, std::int32_t& next_id) {
  std::vector<GeometryElement> out;
  const auto end_name = [&](const NetNode& n) {
    return n.name.empty() ? std::to_string(next_id++) : n.name;
  };
  for (const NetResistor& r : net.resistors) {
    if (r.a >= net.nodes.size() || r.b >= net.nodes.size()) {
      continue;
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
