// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Device parameters: the values written after a device's fields, model
// selection by parameter ranges, the `parameters` records and the
// terminal output values. Capability extract-devices "Device parameter
// values", "Model selection by parameter ranges", "Parameters records",
// "Terminal output and ordering"; openspec/changes/m7-extraction/design.md
// D10.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtBasic.c:1101-1180
// (ExtSortTerminals), :1697-2200 (extOutputParameters, extOutputDevParams,
// extDevFindParamMatch) and :4589-4653 (extTransOutTerminal), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: EXTDEV.

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "device_internal.hpp"
#include "hefesto/db/property.hpp"

namespace hefesto::extract {

namespace device_detail {

std::string format_g(double value) {
  if (std::isnan(value)) {
    return std::signbit(value) ? "-nan" : "nan";
  }
  if (std::isinf(value)) {
    return value < 0 ? "-inf" : "inf";
  }
  std::array<char, 64> buf{};
  const auto res = std::to_chars(buf.data(), buf.data() + buf.size(), value, std::chars_format::general, 6);
  return std::string(buf.data(), res.ptr);
}

std::int64_t mul_trunc(std::int64_t value, float by) noexcept {
  const double v = static_cast<double>(static_cast<float>(value) * by);
  if (std::isnan(v)) {
    return 0;
  }
  return static_cast<std::int64_t>(std::clamp(v, -2147483648.0, 2147483647.0));
}

namespace {

// (int) of a double, saturated, NaN as 0 (Q-EXTRACT-30).
std::int64_t to_int(double v) noexcept {
  if (std::isnan(v)) {
    return 0;
  }
  return static_cast<std::int64_t>(std::clamp(v, -2147483648.0, 2147483647.0));
}

std::optional<std::int32_t> resist_value(const DevicePattern& rec, tech::TileType type, std::string_view region) {
  for (const DeviceResist& r : rec.resist) {
    if (r.type == type && r.region == region) {
      return r.value;
    }
  }
  return std::nullopt;
}

// The `r` parameter (Q-EXTRACT-15: 0 for a zero width, area or perimeter
// instead of dividing by zero, in both modes).
std::int64_t r_value(const Context& ctx, const DeviceRegionTiles& reg, const TransRec& tr, std::int64_t length,
                     std::int64_t width) {
  const tech::TileType t = reg.region.type;
  if (const auto area_res = resist_value(*tr.rec, t, "area")) {
    if (reg.region.area == 0) {
      return 0;
    }
    std::int64_t res = *area_res / reg.region.area;
    if (const auto perim_res = resist_value(*tr.rec, t, "perimeter")) {
      if (tr.perim == 0) {
        return 0;
      }
      const std::int64_t perimr = *perim_res / tr.perim;
      res = to_int(1.0 / ((1.0 / static_cast<double>(perimr)) + (1.0 / static_cast<double>(res))));
    }
    return res;
  }
  if (width == 0) {
    return 0;
  }
  std::int64_t res = to_int(static_cast<double>(ctx.style.sheet_resistance[t]) * static_cast<double>(length) /
                            static_cast<double>(width));
  if (const auto term_res = resist_value(*tr.rec, t, "terminal")) {
    res += *term_res * width;
  }
  return res;
}

std::string key_text(const DeviceParam& p, std::size_t chars) {
  return p.code.substr(0, std::min(chars, p.code.size()));
}

bool plain_key(const DeviceParam& p) {
  return p.code.size() < 2 || p.code[1] == '0';
}

std::optional<std::size_t> indexed_key(const DeviceParam& p) {
  if (p.code.size() >= 2 && p.code[1] > '0' && p.code[1] <= '9') {
    return static_cast<std::size_t>(p.code[1] - '1');
  }
  return std::nullopt;
}

struct ParamWriter {
  Context& ctx;
  const DeviceRegionTiles& reg;
  const TransRec& tr;
  std::int64_t length;
  std::int64_t width;
  std::vector<std::string> out;

  void value(const DeviceParam& p, std::size_t chars, const std::string& v) {
    out.push_back(key_text(p, chars) + "=" + v);
  }

  void lw(const DeviceParam& p, bool is_length) {
    if (plain_key(p)) {
      value(p, 1, std::to_string(is_length ? length : width));
      return;
    }
    const std::optional<std::size_t> idx = indexed_key(p);
    if (!idx || *idx >= kMaxTerms) {
      return;
    }
    const Terminal& term = tr.terms[*idx];
    if (is_length) {
      value(p, 2, std::to_string(width == 0 ? 0 : term.area / width));
    } else {
      width = terminal_width(term.area, term.perim);  // replaces W for the later parameters
      value(p, 2, std::to_string(width));
    }
  }

  void param(const DeviceParam& p) {
    switch (std::tolower(static_cast<unsigned char>(p.code.empty() ? ' ' : p.code[0]))) {
      case 'a':
        if (plain_key(p)) {
          value(p, 1, std::to_string(reg.region.area));
        }
        break;
      case 'p':
        if (plain_key(p)) {
          value(p, 1, std::to_string(tr.perim));
        }
        break;
      case 'l':
        lw(p, true);
        break;
      case 'w':
        lw(p, false);
        break;
      case 'c':
        value(p, 1, format_g((tr.rec->area_cap * static_cast<double>(reg.region.area)) +
                             (tr.rec->perimeter_cap * static_cast<double>(tr.perim))));
        break;
      case 'r':
        value(p, 1, format_g(static_cast<double>(static_cast<float>(r_value(ctx, reg, tr, length, width)))));
        break;
      case 's':
      case 'x':
      case 'y':
        break;
      default:
        value(p, 1, "");
        break;
    }
  }
};

}  // namespace

std::vector<std::string> device_params(Context& ctx, const DeviceRegionTiles& reg, const TransRec& tr,
                                       const DevicePattern& devptr, std::int64_t length, std::int64_t width) {
  ParamWriter w{ctx, reg, tr, length, width, {}};
  for (const DeviceParam& p : devptr.params) {
    if (!p.name.empty()) {
      w.param(p);
    }
  }
  // Gate attribute labels with '=' are extra parameters, and stop being
  // gate attributes.
  for (GateLabel& l : ctx.gate_labels(tr.gate_node)) {
    if (l.attr == kGateAttr && l.text.find('=') != std::string::npos) {
      w.out.push_back(l.text.substr(0, l.text.size() - 1));
      l.attr = kNoAttr;
    }
  }
  return w.out;
}

DeviceTerminal out_terminal(std::vector<GateLabel>& labels, NodeRef node, int which, std::int64_t len,
                            std::int64_t area, std::int64_t perim, std::int64_t shared) {
  DeviceTerminal t;
  t.node = node;
  t.length = len;
  if (shared == 0) {
    shared = 1;
  }
  for (GateLabel& l : labels) {
    if (l.attr == which) {
      t.attributes.push_back(l.text.empty() ? l.text : l.text.substr(0, l.text.size() - 1));
      l.attr = kNoAttr;
    }
  }
  if (which != kGateAttr && area != 0 && perim != 0) {
    t.area = area / shared;
    t.perimeter = perim / shared;
  }
  return t;
}

namespace {

// p2 sorts before p1 (ExtSortTerminals' swap test); equal positions are
// reported and kept.
bool should_swap(Context& ctx, const TermPos& p1, const TermPos& p2) {
  if (p2.plane > p1.plane) {
    return false;
  }
  if (p2.plane == p1.plane) {
    if (p2.pt.x > p1.pt.x || (p2.pt.x == p1.pt.x && p2.pt.y > p1.pt.y)) {
      return false;
    }
    if (p2.pt.x == p1.pt.x && p2.pt.y == p1.pt.y) {
      ctx.message("EXTRACT-DEV-011", diag::Severity::info, "Extract error:  Duplicate tile position, ignoring");
      return false;
    }
  }
  return true;
}

void swap_attrs(std::vector<GateLabel>& labels, int a) {
  for (GateLabel& l : labels) {
    if (l.attr == a) {
      l.attr = kSortAttr;
    } else if (l.attr == a + 1) {
      l.attr = a;
    }
  }
  for (GateLabel& l : labels) {
    if (l.attr == kSortAttr) {
      l.attr = a + 1;
    }
  }
}

}  // namespace

void sort_terminals(Context& ctx, TransRec& tr, std::vector<GateLabel>& labels) {
  bool changed = true;
  while (changed) {
    changed = false;
    for (std::size_t i = 0; i + 1 < tr.nterm && i + 1 < kMaxTerms; ++i) {
      if (!should_swap(ctx, tr.terms[i].pos, tr.terms[i + 1].pos)) {
        continue;
      }
      changed = true;
      std::swap(tr.terms[i], tr.terms[i + 1]);
      swap_attrs(labels, static_cast<int>(i));
    }
  }
}

}  // namespace device_detail

// --- extDevFindParamMatch ---

namespace {

using device_detail::indexed_key;
using device_detail::kMaxTerms;
using device_detail::plain_key;

// The written range of `p` (Q-EXTRACT-21 OFF: a lower limit alone applies).
bool has_upper(const DevicePattern& d, const DeviceParam& p) {
  for (const ParamRange& r : d.ranges) {
    if (r.name == p.code) {
      return r.max.has_value();
    }
  }
  return p.maximum >= p.minimum;
}

std::int64_t at_or_zero(const std::vector<std::int64_t>& v, std::size_t i) {
  return i < v.size() ? v[i] : 0;
}

// The value a parameter range checks, or nullopt for a key that selects
// nothing.
std::optional<std::int64_t> checked_value(const DeviceParam& p, const Dimensions& dims, const TerminalValues& tv) {
  const std::optional<std::size_t> idx = indexed_key(p);
  switch (std::tolower(static_cast<unsigned char>(p.code.empty() ? ' ' : p.code[0]))) {
    case 'a':
      return plain_key(p) ? dims.length * dims.width : at_or_zero(tv.area, idx.value_or(kMaxTerms));
    case 'p':
      return plain_key(p) ? 2 * (dims.length + dims.width) : at_or_zero(tv.perimeter, idx.value_or(kMaxTerms));
    case 'l':
      if (plain_key(p)) {
        return dims.length;
      }
      return idx ? std::optional<std::int64_t>(at_or_zero(tv.length, *idx)) : std::nullopt;
    case 'w':
      return dims.width;
    default:
      return std::nullopt;
  }
}

bool in_range(const DevicePattern& d, const Dimensions& dims, const TerminalValues& tv, bool magic_compat) {
  for (const DeviceParam& p : d.params) {
    const bool upper = magic_compat ? p.maximum >= p.minimum : has_upper(d, p);
    if (magic_compat && !upper) {
      continue;  // Magic: a minimum above the default maximum (-1) is never checked
    }
    const std::optional<std::int64_t> v = checked_value(p, dims, tv);
    if (!v) {
      continue;
    }
    // Q-EXTRACT-21: internal units with magic-compat ON, milli-units OFF.
    const double value = magic_compat ? static_cast<double>(*v) : static_cast<double>(*v) * 1000.0;
    if (value < p.minimum || (upper && value > p.maximum)) {
      return false;
    }
  }
  return true;
}

bool compatible(const DevicePattern& a, const DevicePattern& b) {
  if (a.device_class != b.device_class || a.terminal_count != b.terminal_count) {
    return false;
  }
  for (std::size_t i = 0; i < static_cast<std::size_t>(std::max(a.terminal_count, 0)); ++i) {
    if (device_detail::sd_types(a, i) != device_detail::sd_types(b, i)) {
      return false;
    }
  }
  return a.substrate_types == b.substrate_types && a.identifiers == b.identifiers;
}

}  // namespace

std::size_t select_pattern(const ExtStyle& style, tech::TileType type, std::size_t pattern, const Dimensions& dims,
                           const TerminalValues& terminals, const compat::CompatSettings& compat, bool* mismatch) {
  if (mismatch != nullptr) {
    *mismatch = false;
  }
  if (pattern >= style.devices.size()) {
    return pattern;
  }
  const DevicePattern* devptr = &style.devices[pattern];
  while (true) {
    if (in_range(*devptr, dims, terminals, compat.magic_compat)) {
      return static_cast<std::size_t>(devptr - style.devices.data());
    }
    // The next record of the type's list (Magic's exts_next).
    const DevicePattern* next = device_detail::next_record(style, devptr, type);
    if (next == nullptr || !compatible(*next, *devptr)) {
      if (mismatch != nullptr) {
        *mismatch = true;
      }
      return compat.magic_compat ? static_cast<std::size_t>(devptr - style.devices.data()) : pattern;
    }
    devptr = next;
  }
}

// --- extOutputParameters ---

namespace {

std::string parameters_line(const DevicePattern& d) {
  std::string line = "parameters " + d.model;
  for (const DeviceParam& p : d.params) {
    if (p.name.empty()) {
      continue;
    }
    line += " " + p.code.substr(0, std::min<std::size_t>(p.code.size(), 2)) + "=" + p.name;
    if (p.scale != 1.0) {
      line += "*" + device_detail::format_g(p.scale);
    } else if (p.offset != 0.0) {
      const auto off = static_cast<std::int64_t>(p.offset);
      line += (off >= 0 ? "+" : "") + std::to_string(off);
    }
  }
  return line;
}

bool has_output(const DevicePattern& d) {
  return std::any_of(d.params.begin(), d.params.end(), [](const DeviceParam& p) { return !p.name.empty(); });
}

// The device model named by a `device` property (its second word).
std::string property_model(const db::CellDef& cell) {
  const db::PropertyValue* v = cell.properties().find("device");
  if (v == nullptr) {
    return {};
  }
  const std::string text = db::property_text(*v);
  std::size_t i = 0;
  while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])) == 0) {
    ++i;
  }
  while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])) != 0) {
    ++i;
  }
  return text.substr(i);
}

// The types with a record named `model`.
void add_model_types(const ExtStyle& style, const std::string& model, std::set<tech::TileType>& types) {
  for (std::size_t t = 0; t < tech::kMaxTileTypes; ++t) {
    const std::vector<const DevicePattern*> list = style.records_for(static_cast<tech::TileType>(t));
    if (std::any_of(list.begin(), list.end(), [&](const DevicePattern* d) { return d->model == model; })) {
      types.insert(static_cast<tech::TileType>(t));
    }
  }
}

}  // namespace

std::vector<std::string> parameter_records(const ExtStyle& style, const std::vector<DeviceRegionTiles>& regions,
                                           const db::CellDef& cell) {
  std::set<tech::TileType> types;
  for (const DeviceRegionTiles& r : regions) {
    if (!r.disabled) {
      types.insert(r.region.type);
    }
  }
  if (const std::string model = property_model(cell); !model.empty()) {
    add_model_types(style, model, types);
  }
  std::vector<std::string> out;
  for (const tech::TileType t : types) {
    if (t < tech::kFirstUserType) {
      continue;
    }
    for (const DevicePattern* d : style.records_for(t)) {
      if (d->model != "Ignore" && has_output(*d)) {
        out.push_back(parameters_line(*d));
      }
    }
  }
  return out;
}

}  // namespace hefesto::extract
