// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// spcWriteParams: the declared parameters of a device type, then its
// verbatim parameters (capability netlist-spice-devices "Device parameter
// output"; Q-NETLIST-8, Q-NETLIST-19 and Q-NETLIST-32 per mode).
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2spice.c:2200-2519,
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: SPICE.

#include <cstdlib>

#include "netlist/spice/flat_writer.hpp"
#include "netlist/spice/numbers.hpp"

namespace hefesto::netlist::spice::detail {

namespace {

// One declared parameter and the device it is written for.
struct ParamCall {
  FlatWriter& w;
  InputDevice& dev;
  std::int64_t l;
  std::int64_t width;
  float m;
  bool subdone;
};

// Q-NETLIST-8: with magic-compat OFF the offset is added before scaling in
// every column; with it ON only in the SI column.
double raw(const ParamCall& c, std::int64_t value, const ParamDecl& p) {
  return static_cast<double>(c.w.compat.magic_compat ? value : value + p.offset);
}

// A length-like value (l, w, x, y, l<n>, w<n>): `%g` in scale mode, `%g`
// with the parameter scale, otherwise SI.
std::string length_value(const ParamCall& c, std::int64_t value, const ParamDecl& p) {
  const float scale = c.dev.scale;
  const float es = c.w.es_scale;
  if (es < 0) {
    return format_g(dbl(static_cast<float>(raw(c, value, p)) * scale));
  }
  if (p.scale != 1.0) {
    return format_g(raw(c, value, p) * dbl(scale) * dbl(es) * p.scale * 1E-6);
  }
  return si(static_cast<float>(1.0E-6 * static_cast<double>(value + p.offset) * dbl(scale) * dbl(es)));
}

std::string area_value(const ParamCall& c, const ParamDecl& p) {
  const float scale = c.dev.scale;
  const float es = c.w.es_scale;
  if (es < 0) {
    return format_g(dbl(static_cast<float>(raw(c, c.dev.area, p)) * scale * scale));
  }
  if (p.scale != 1.0) {
    return format_g(raw(c, c.dev.area, p) * dbl(scale) * dbl(scale) * dbl(es) * dbl(es) * p.scale * 1E-12);
  }
  return si(static_cast<float>(1.0E-12 * static_cast<double>(c.dev.area + p.offset) * dbl(scale) * dbl(scale) *
                               dbl(es) * dbl(es)));
}

std::string perim_value(const ParamCall& c, const ParamDecl& p) {
  const float scale = c.dev.scale;
  const float es = c.w.es_scale;
  if (es < 0) {
    return format_g(dbl(static_cast<float>(raw(c, c.dev.perim, p)) * scale));
  }
  if (p.scale != 1.0) {
    return format_g(raw(c, c.dev.perim, p) * dbl(scale) * dbl(es) * p.scale * 1E-6);
  }
  // Q-NETLIST-19 (a): Magic's flat writer uses 1e-12 for the perimeter.
  const double unit = c.w.compat.magic_compat ? 1.0E-12 : 1.0E-6;
  return si(static_cast<float>(unit * static_cast<double>(c.dev.perim + p.offset) * dbl(scale) * dbl(es)));
}

std::size_t terminal_index(const InputDevice& dev, char digit) {
  const int pn = digit - '0';
  const auto last = dev.terminals.size() - 1;
  return pn < 0 ? 0 : std::min(static_cast<std::size_t>(pn), last);
}

// a<n>/p<n>: the area and perimeter of terminal n; `pair` is the matching
// p<n>/a<n> that follows, written with it.
void terminal_param(const ParamCall& c, const ParamDecl& p, const ParamDecl* pair) {
  const std::size_t pn = terminal_index(c.dev, p.key[1]);
  const DeviceTypeInfo& info = c.w.type_info(c.dev.type);
  const std::int32_t cls = pn > 1 ? info.drain_class : info.source_class;
  InputTerminal& term = c.dev.terminals[pn];
  const bool area_first = p.key[0] == 'a';
  const ParamDecl* area = area_first ? &p : pair;
  const ParamDecl* perim = area_first ? pair : &p;
  // spcnAPHier names the values by their keys (magic-compat ON) and skips
  // a missing one (Magic prints "(null)=" there).
  const bool hier = hier_sd_attr(term, c.w.settings.hier_ap);
  const auto label = [&](const ParamDecl* d) -> std::string {
    if (d == nullptr) {
      return {};
    }
    return hier && c.w.compat.magic_compat ? d->key : d->name;
  };
  c.w.terminal_ap(term, c.w.find_node(term.node), cls, c.dev.scale, label(area), label(perim), c.m,
                  hier ? -1 : c.width);
}

// l<n>/w<n>: the verbatim `l<n>=<int>` parameter, consumed.
void terminal_length(const ParamCall& c, const ParamDecl& p) {
  for (std::string& verbatim : c.dev.params) {
    if (verbatim.size() > 2 && verbatim[0] == p.key[0] && verbatim[1] == p.key[1] && verbatim[2] == '=') {
      std::size_t used = 0;
      const std::string digits = verbatim.substr(3);
      char* end = nullptr;
      const long value = std::strtol(digits.c_str(), &end, 10);
      used = static_cast<std::size_t>(end - digits.c_str());
      if (used == 0) {
        continue;
      }
      c.w.out += " " + p.name + "=" + length_value(c, value, p);
      verbatim.clear();
      break;
    }
  }
}

void substrate_param(const ParamCall& c, const ParamDecl& p) {
  if (c.subdone) {
    return;
  }
  if (!c.dev.substrate) {
    c.w.warn("Error:  No substrate definition for device " + c.dev.type);
    return;
  }
  c.w.out += " " + p.name + "=";
  (void)c.w.substrate(c.dev, true);
}

void named_value(const ParamCall& c, const ParamDecl& p, float value) {
  if (!p.name.empty()) {
    c.w.out += " " + p.name + "=" + si(value);
  }
}

// A device-wide value (key without a terminal digit, or digit 0).
bool whole_device(const ParamDecl& p) {
  return p.key.size() < 2 || p.key[1] == '0';
}

void length_param(const ParamCall& c, const ParamDecl& p, std::int64_t value) {
  if (whole_device(p)) {
    c.w.out += " " + p.name + "=" + length_value(c, value, p);
  } else {
    terminal_length(c, p);
  }
}

// Writes `params[i]`; returns the number of declarations consumed.
std::size_t write_one(const ParamCall& c, const std::vector<ParamDecl>& params, std::size_t i) {
  const ParamDecl& p = params[i];
  const char key = p.key.empty() ? '\0' : p.key[0];
  if ((key == 'a' || key == 'p') && !whole_device(p)) {
    const char other = key == 'a' ? 'p' : 'a';
    const ParamDecl* next = i + 1 < params.size() ? &params[i + 1] : nullptr;
    const bool paired = next != nullptr && next->key.size() > 1 && next->key[0] == other && next->key[1] == p.key[1];
    terminal_param(c, p, paired ? next : nullptr);
    return paired ? 2 : 1;
  }
  switch (key) {
    case 'a':
      c.w.out += " " + p.name + "=" + area_value(c, p);
      break;
    case 'p':
      c.w.out += " " + p.name + "=" + perim_value(c, p);
      break;
    case 'l':
      length_param(c, p, c.l);
      break;
    case 'w':
      length_param(c, p, c.width);
      break;
    case 's':
      substrate_param(c, p);
      break;
    case 'x':
      c.w.out += " " + p.name + "=" + length_value(c, c.dev.rect.x0, p);
      break;
    case 'y':
      c.w.out += " " + p.name + "=" + length_value(c, c.dev.rect.y0, p);
      break;
    case 'r':
      named_value(c, p, c.dev.resistance);
      break;
    case 'c':
      named_value(c, p, c.dev.capacitance);
      break;
    default:
      break;
  }
  return 1;
}

}  // namespace

void FlatWriter::write_params(InputDevice& dev, std::int64_t l, std::int64_t w, float m, bool subdone) {
  const ParamCall call{*this, dev, l, w, m, subdone};
  if (const auto* params = declared(dev.type)) {
    for (std::size_t i = 0; i < params->size();) {
      i += write_one(call, *params, i);
    }
  }
  // Q-NETLIST-32: a consumed l<n>/w<n> entry writes a lone space with
  // magic-compat ON and nothing with it OFF.
  for (const std::string& verbatim : dev.params) {
    if (!verbatim.empty() || compat.magic_compat) {
      out += " " + verbatim;
    }
  }
}

}  // namespace hefesto::netlist::spice::detail
