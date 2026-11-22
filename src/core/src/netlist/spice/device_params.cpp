// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Device parameters, terminal areas and perimeters, and the substrate text
// of the flat SPICE writer (capability netlist-spice-devices "Device
// parameter output" with Q-NETLIST-8, Q-NETLIST-19, Q-NETLIST-32 per mode,
// "Terminal area and perimeter" with Q-NETLIST-7 per mode, "Substrate
// terminal text").
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2spice.c:2113-2519
// (spcWriteValue, spcWriteSubParam, spcWriteParams), 3424-3483
// (spcdevSubstrate) and 3658-3885 (spcnAP, spcnAPHier), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: SPICE.

#include <cctype>
#include <cstdlib>
#include <strings.h>

#include "netlist/spice/flat_writer.hpp"
#include "netlist/spice/numbers.hpp"

namespace hefesto::netlist::spice::detail {

namespace {

const DeviceTypeInfo kNoInfo{};

// sscanf(text, "%d", &value): leading blanks, a sign and at least one digit.
bool scan_int(std::string_view text, std::size_t& pos, int& value) {
  while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
    ++pos;
  }
  const std::size_t start = pos;
  if (pos < text.size() && (text[pos] == '-' || text[pos] == '+')) {
    ++pos;
  }
  const std::size_t digits = pos;
  while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
    ++pos;
  }
  if (pos == digits) {
    return false;
  }
  value = static_cast<int>(std::strtol(std::string{text.substr(start, pos - start)}.c_str(), nullptr, 10));
  return true;
}

// sscanf(text, "%d,%d", &a, &p) == 2.
bool scan_pair(std::string_view text, int& area, int& perim) {
  std::size_t pos = 0;
  if (!scan_int(text, pos, area) || pos >= text.size() || text[pos] != ',') {
    return false;
  }
  ++pos;
  return scan_int(text, pos, perim);
}

// The terminal attribute scan of spcnAP: `<area>,<perim>` from the start or
// after a comma; the attributes are cut where the pair starts.
bool attribute_ap(InputTerminal& term, int& area, int& perim) {
  if (!term.attributes) {
    return false;
  }
  std::string& text = *term.attributes;
  std::size_t pos = 0;
  while (pos != std::string::npos) {
    if (pos < text.size() && text[pos] == ',') {
      ++pos;
    }
    if (scan_pair(std::string_view{text}.substr(std::min(pos, text.size())), area, perim)) {
      text.resize(pos);
      return true;
    }
    pos = text.find(',', pos);
  }
  return false;
}

bool visited(NodeClient& client, std::int32_t cls) {
  const auto index = static_cast<std::size_t>(cls);
  if (client.visited.size() <= index) {
    client.visited.resize(index + 1, false);
  }
  const bool was = client.visited[index];
  client.visited[index] = true;
  return was;
}

AreaPerim node_ap(const InputNode& node, std::int32_t cls) {
  if (cls < 0 || static_cast<std::size_t>(cls) >= node.pa.size()) {
    return {};
  }
  return node.pa[static_cast<std::size_t>(cls)];
}

}  // namespace

const DeviceTypeInfo& FlatWriter::type_info(const std::string& type) const {
  if (!in.style_matches) {
    return kNoInfo;
  }
  const auto it = in.types.find(type);
  return it == in.types.end() ? kNoInfo : it->second;
}

const std::vector<ParamDecl>* FlatWriter::declared(std::string_view key) const {
  const auto it = in.params.find(key);
  return it == in.params.end() ? nullptr : &it->second;
}

std::optional<std::size_t> FlatWriter::substrate(const InputDevice& dev, bool print) {
  const DeviceTypeInfo& info = type_info(dev.type);
  if (info.default_substrate && strcasecmp(dev.substrate_local.c_str(), info.default_substrate->c_str()) == 0) {
    if (print) {
      out += format_subs(dev.substrate_local, names);
    }
    return std::nullopt;
  }
  const auto node = dev.substrate ? find_node(*dev.substrate) : std::nullopt;
  if (!node) {
    if (print) {
      out += "errGnd!";
    }
    return std::nullopt;
  }
  if (print) {
    out += spice_name(*node);
  }
  mark_used(*node);
  return node;
}

void FlatWriter::write_value(const InputDevice& dev) {
  const auto* params = declared(dev.type);
  if (params == nullptr) {
    return;
  }
  for (const ParamDecl& p : *params) {
    if (!p.name.empty() || p.key.empty()) {
      continue;
    }
    if (p.key[0] == 'r') {
      out += " " + si(dev.resistance);
    } else if (p.key[0] == 'c') {
      // Q-NETLIST-4: Magic's flat writer prints the resistance here.
      out += " " + si(compat.magic_compat ? dev.resistance : dev.capacitance);
    }
  }
}

bool FlatWriter::write_sub_param(const InputDevice& dev) {
  bool done = false;
  const auto* params = declared(dev.type);
  if (params == nullptr) {
    return false;
  }
  for (const ParamDecl& p : *params) {
    if (p.key.empty() || p.key[0] != 's') {
      continue;
    }
    if (!dev.substrate) {
      warn("Error:  No substrate definition for device " + dev.type);
      continue;
    }
    out += " " + p.name + "=";
    (void)substrate(dev, true);
    done = true;
  }
  return done;
}

void FlatWriter::geometry(std::int64_t l, std::int64_t w, float scale, int dscale) {
  if (es_scale < 0) {
    out += " w=" + format_g(dbl(static_cast<float>(w) * scale)) +
           " l=" + format_g(dbl((static_cast<float>(l) * scale) / static_cast<float>(dscale)));
    return;
  }
  out += " w=" + si(static_cast<float>(1.0E-6 * static_cast<double>(w) * dbl(scale) * dbl(es_scale)));
  out += " l=" + si(static_cast<float>(1.0E-6 * dbl(static_cast<float>(l) * scale * es_scale) / dscale));
}

namespace {

struct ApValues {
  double area = 0;
  double perim = 0;
};

// -d: the node's values scaled by w / (total width on the node and class).
ApValues distributed_ap(FlatWriter& w, NodeClient& client, const AreaPerim& pa, std::int32_t res_class, float scale,
                        std::int64_t width) {
  float dsc = static_cast<float>(width);
  if (!client.widths.empty() && res_class >= 0 && static_cast<std::size_t>(res_class) < client.widths.size()) {
    dsc = static_cast<float>(width) / client.widths[static_cast<std::size_t>(res_class)];
  } else if (client.widths.empty()) {
    w.warn("Device missing records for source/drain area/perim.");
  }
  const float a = static_cast<float>(pa.area);
  const float p = static_cast<float>(pa.perim);
  const double es = dbl(w.es_scale);
  if (w.es_scale < 0) {
    return {1.0E-12 * static_cast<double>(pa.area) * dbl(scale) * dbl(scale) * dbl(dsc),
            1.0E-6 * static_cast<double>(pa.perim) * dbl(scale) * dbl(dsc)};
  }
  return {1.0E-12 * dbl(a * scale * scale) * es * es * dbl(dsc), 1.0E-6 * dbl(p * scale) * es * dbl(dsc)};
}

// The newer form: values from the terminal attributes, else the node's
// once per node and class.
ApValues plain_ap(const FlatWriter& w, int area, int perim, float scale, float m) {
  if (w.es_scale < 0) {
    return {1.0E-12 * area * dbl(scale) * dbl(scale) / dbl(m), 1.0E-6 * perim * dbl(scale) / dbl(m)};
  }
  // Q-NETLIST-7: divided by m outside scale mode only with magic-compat OFF.
  const double div = w.compat.magic_compat ? 1.0 : dbl(m);
  const double es = dbl(w.es_scale);
  return {1.0E-12 * dbl(static_cast<float>(area) * scale * scale) * es * es / div,
          1.0E-6 * dbl(static_cast<float>(perim) * scale) * es / div};
}

void emit_ap(std::string& out, std::string_view aname, std::string_view pname, const ApValues& v) {
  if (!aname.empty()) {
    out += " " + std::string{aname} + "=" + si(static_cast<float>(v.area));
  }
  if (!pname.empty()) {
    out += " " + std::string{pname} + "=" + si(static_cast<float>(v.perim));
  }
}

}  // namespace

void FlatWriter::terminal_ap(InputTerminal& term, std::optional<std::size_t> node, std::int32_t res_class,
                             float scale, std::string_view aname, std::string_view pname, float m,
                             std::int64_t w) {
  if (!node || !clients[*node].exists) {
    warn("spcnAP: major internal inconsistency");
    return;
  }
  const AreaPerim pa = node_ap(in.nodes[*node], res_class);
  if (settings.distributed_junctions && w != -1) {
    emit_ap(out, aname, pname, distributed_ap(*this, clients[*node], pa, res_class, scale, w));
    return;
  }
  int area = 0;
  int perim = 0;
  if (!attribute_ap(term, area, perim)) {
    area = static_cast<int>(pa.area);
    perim = static_cast<int>(pa.perim);
    if (res_class < 0 || visited(clients[*node], res_class)) {
      scale = 0;
    }
  }
  emit_ap(out, aname, pname, plain_ap(*this, area, perim, scale, m));
}

void FlatWriter::substrate_ap(std::size_t node, std::int32_t res_class, float scale, float m) {
  InputTerminal none;
  terminal_ap(none, node, res_class, scale, "asub", "psub", m, -1);
}

}  // namespace hefesto::netlist::spice::detail
