// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extOutputDevices: one device record per kept device region, with the
// class-specific dimensions and values. Capability extract-devices
// "Missing terminals and rejected devices", "Device record header and
// substrate field", "MOSFET-family length and width", "Resistor dimensions
// and value", "Capacitor dimensions and value", "Diode records", "Terminal
// output and ordering"; openspec/changes/m7-extraction/design.md D10.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtBasic.c:2252-3300,
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Fixed in both modes (bugs, X-2 tier 1): a zero-size capacitor with a
// model writes ` 0 0` once (Q-EXTRACT-8); diodes compute L and W before
// range matching (Q-EXTRACT-13); the stale L and W Magic carries from the
// previous device into a resistor whose boundary gives no estimate, a
// capacitor without terminals and a diode without `l`/`w` parameters
// start at 0; `r` is 0 for a zero width, area or perimeter (Q-EXTRACT-15).
// A terminal without node is written `(none)`, as extNodeName does.
//
// Owner: EXTDEV.

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

#include "device_internal.hpp"
#include "hefesto/extract/node/names.hpp"

namespace hefesto::extract {

using device_detail::Context;
using device_detail::kGateAttr;
using device_detail::kMaxTerms;
using device_detail::NodeRef;
using device_detail::PlaneTile;
using device_detail::TransRec;

namespace {

enum class SubsWrite : std::uint8_t { never, if_named, always };

// What a device class writes besides the header and terminals.
struct ClassOut {
  const DevicePattern* devptr = nullptr;
  std::int64_t length = 0;
  std::int64_t width = 0;
  std::vector<std::string> fields;
  bool params = true;
  SubsWrite subs = SubsWrite::always;
};

// One device in progress.
struct Device {
  Context& ctx;
  const DeviceRegionTiles& reg;
  const std::vector<PlaneTile>& tiles;
  TransRec& tr;

  [[nodiscard]] tech::TileType type() const { return reg.region.type; }
  [[nodiscard]] float chop() const { return ctx.style.corner_chop[type()]; }

  [[nodiscard]] TerminalValues terminal_values() const {
    TerminalValues v;
    for (const auto& t : tr.terms) {
      v.length.push_back(t.len);
      v.area.push_back(t.area);
      v.perimeter.push_back(t.perim);
    }
    return v;
  }

  const DevicePattern* match_params(const DevicePattern* devptr, std::int64_t length, std::int64_t width) const {
    bool mismatch = false;
    const std::size_t index = select_pattern(ctx.style, type(), ctx.pattern_index(devptr), {length, width},
                                             terminal_values(), ctx.in.compat, &mismatch);
    if (mismatch) {
      ctx.message("EXTRACT-DEV-007", diag::Severity::warning, "Device parameters do not match any extraction model.");
    }
    return &ctx.style.devices[index];
  }

  void effective(BoundaryLists& lists, std::size_t n, std::int64_t& length, std::int64_t& width) const {
    separate_bounds(lists, n);
    bool multiple = false;
    if (const auto lw = effective_lw(lists, n, chop(), &multiple)) {
      length = lw->length;
      width = lw->width;
      if (multiple) {
        ctx.message("EXTRACT-DEV-008", diag::Severity::warning,
                    "Device has multiple lengths:  scaling all widths to length " + std::to_string(length));
      }
    }
  }

  // extComputeCapLW, or the equivalent square of terminal 0.
  [[nodiscard]] Dimensions cap_dimensions(std::size_t n) const {
    const BoundaryLists lists = device_detail::special_bounds(ctx, tiles, tr, n, true);
    if (const auto lw = cap_lw(lists)) {
      return *lw;
    }
    ctx.message("EXTRACT-DEV-009", diag::Severity::error, "extract:  Can't get capacitor L and W");
    const std::int64_t side = equivalent_square(tr.terms[0].area);
    return {side, side};
  }

  [[nodiscard]] std::size_t leading_terminals() const {
    std::size_t n = 0;
    while (n < tr.nterm && n < kMaxTerms && tr.terms[n].node) {
      ++n;
    }
    return n;
  }

  [[nodiscard]] Dimensions bounding_box() const {
    db::Rect box = tiles.front().second.area;
    for (const auto& [p, t] : tiles) {
      box = db::include(box, t.area);
    }
    return {box.width(), box.height()};
  }

  ClassOut mos(const DevicePattern* devptr);
  ClassOut diode(const DevicePattern* devptr);
  ClassOut resistor(const DevicePattern* devptr);
  std::size_t resistor_lw(ClassOut& o);
  double resistor_value(const ClassOut& o, std::size_t n, bool has_model);
  ClassOut capacitor(const DevicePattern* devptr);
  ClassOut fet(const DevicePattern* devptr) const;
};

// The initial estimate of a MOSFET-family device (ExtBasic.c:2708-2790).
std::size_t mos_estimate(const Device& d, std::int64_t& length, std::int64_t& width) {
  std::size_t n = d.tr.nterm;
  for (std::size_t i = 0; i < n; ++i) {
    if (d.tr.terms[i].node) {
      width += d.tr.terms[i].len;
    }
  }
  while (n > 0 && d.tr.terms[n - 1].node && d.tr.terms[n - 1].len == 0 && d.tr.terms[n - 1].area == 0 &&
         d.tr.terms[n - 1].perim == 0) {
    --n;
  }
  if (n == 0) {
    return 0;
  }
  const auto ni = static_cast<std::int64_t>(n);
  width /= ni;
  if (n > 1) {
    length = d.tr.gatelen / ni;
  } else if (d.tr.gatelen < width) {
    length = d.tr.gatelen;
  } else {
    length = (d.tr.gatelen - width) / 2;
  }
  if (n == 1 && length == 0 && d.tr.gatelen == 0) {
    const Dimensions box = d.bounding_box();  // L, W not distinguishable: the bounding box
    length = box.length;
    width = box.width;
  }
  return n;
}

ClassOut Device::mos(const DevicePattern* devptr) {
  ClassOut o;
  const std::size_t n = mos_estimate(*this, o.length, o.width);
  if (n == 0) {
    if (ctx.in.warn_any && devptr->terminal_count > 0) {
      ctx.bad(reg.first, "Could not determine device boundary");
    }
    o.length = o.width = 0;
  } else {
    BoundaryLists lists = device_detail::special_bounds(ctx, tiles, tr, tr.nterm, true);
    effective(lists, n, o.length, o.width);
  }
  o.devptr = match_params(devptr, o.length, o.width);
  const DeviceClass c = o.devptr->device_class;
  if (c == DeviceClass::mosfet || c == DeviceClass::asymmetric || c == DeviceClass::bjt) {
    o.fields = {std::to_string(o.length), std::to_string(o.width)};
  }
  return o;
}

bool needs_lw(const DevicePattern& d) {
  return std::any_of(d.params.begin(), d.params.end(), [](const DeviceParam& p) {
    const char c = p.code.empty() ? ' ' : static_cast<char>(std::tolower(static_cast<unsigned char>(p.code[0])));
    return !p.name.empty() && (c == 'l' || c == 'w');
  });
}

ClassOut Device::diode(const DevicePattern* devptr) {
  ClassOut o;
  o.subs = SubsWrite::if_named;
  // Q-EXTRACT-13 (fix, both modes): L and W before the range matching.
  const std::size_t n = leading_terminals();
  bool computed = false;
  if (needs_lw(*devptr) && n > 0) {
    const Dimensions dims = cap_dimensions(n);
    o.length = dims.length;
    o.width = dims.width;
    computed = true;
  }
  o.devptr = match_params(devptr, o.length, o.width);
  if (!computed && needs_lw(*o.devptr) && n > 0) {
    const Dimensions dims = cap_dimensions(n);
    o.length = dims.length;
    o.width = dims.width;
  }
  return o;
}

// The single-tile resistor estimate; returns Magic's reused `n` (the
// leading terminals with a length).
std::size_t single_tile_resistor(const Device& d, std::int64_t& length, std::int64_t& width) {
  width = 0;
  std::size_t n = 0;
  for (; n < kMaxTerms && d.tr.terms[n].len != 0; ++n) {
    width += d.tr.terms[n].len;
    length -= d.tr.terms[n].len;
  }
  width >>= 1;
  length >>= 1;
  return n;
}

bool annular_resistor(const TransRec& tr) {
  bool annular = false;
  for (std::size_t n = 0; n < tr.nterm && n < kMaxTerms; ++n) {
    const auto& t = tr.terms[n];
    if (t.node && t.vector.x == 0 && t.vector.y == 0 && (n == 0 || t.node != tr.terms[n - 1].node)) {
      annular = true;
    }
  }
  return annular;
}

// The resistor L and W; returns Magic's `n` for the terminal-count warning.
std::size_t Device::resistor_lw(ClassOut& o) {
  o.length = tr.perim;
  o.width = 0;  // Magic leaves W stale when the estimate fails: 0 in both modes
  const bool annular = annular_resistor(tr);
  std::size_t n = std::min(tr.nterm, kMaxTerms);
  if (n == 0) {
    o.length = o.width = 0;
  } else if (tiles.size() > 1) {
    BoundaryLists lists = device_detail::special_bounds(ctx, tiles, tr, n, annular);
    if (!lists[0].empty()) {
      // Non-annular: the side lists give the length as the estimate's width.
      effective(lists, n, annular ? o.length : o.width, annular ? o.width : o.length);
    } else {
      if (ctx.in.warn_any) {
        ctx.bad(reg.first, "Could not determine resistor boundary");
      }
      o.length = o.width = 0;
    }
  } else {
    n = single_tile_resistor(*this, o.length, o.width);
  }
  return n;
}

// R = sheet * L / W (mohm), with the zero-width and terminal-count feedback.
double Device::resistor_value(const ClassOut& o, std::size_t n, bool has_model) {
  double dres = 0.0;
  if (o.width != 0) {
    dres = static_cast<double>(ctx.style.sheet_resistance[type()]) * static_cast<double>(o.length) /
           static_cast<double>(o.width);
    if (ctx.in.warn_any && n > 2) {
      ctx.bad(reg.first, "Resistor has " + std::to_string(n) + " terminals: extracted " +
                             (has_model ? "L/W will be wrong" : "value will be wrong"));
    }
  } else if (ctx.in.warn_any) {
    ctx.bad(reg.first, "Resistor has zero width");
  }
  return dres;
}

ClassOut Device::resistor(const DevicePattern* devptr) {
  ClassOut o;
  const bool has_model = devptr->model != "None";
  const std::size_t n = resistor_lw(o);
  const double dres = resistor_value(o, n, has_model);
  o.devptr = match_params(devptr, o.length, o.width);
  if (o.devptr->device_class == DeviceClass::rsubckt) {
    o.subs = SubsWrite::always;
  } else if (has_model) {
    o.fields = {std::to_string(o.length), std::to_string(o.width)};
    o.subs = SubsWrite::if_named;
  } else {
    o.fields = {device_detail::format_g(dres / 1000.0)};
    o.subs = SubsWrite::never;
  }
  return o;
}

ClassOut Device::capacitor(const DevicePattern* devptr) {
  ClassOut o;
  o.devptr = devptr;
  if (devptr->model == "None") {
    const double dcap = (devptr->area_cap * static_cast<double>(reg.region.area)) +
                        (devptr->perimeter_cap * static_cast<double>(tr.perim));
    o.fields = {device_detail::format_g(dcap / 1000.0)};
    o.params = false;
    o.subs = SubsWrite::never;
    return o;
  }
  o.subs = devptr->device_class == DeviceClass::csubckt ? SubsWrite::always : SubsWrite::if_named;
  const std::size_t n = leading_terminals();
  if (n == 0) {
    // Q-EXTRACT-8 (fix, both modes): ` 0 0` once, no uninitialised L and W.
    ctx.bad(reg.first, "Capacitor has zero size");
    o.fields = {"0", "0"};
    return o;
  }
  const Dimensions dims = cap_dimensions(n);
  o.length = dims.length;
  o.width = dims.width;
  if (devptr->device_class == DeviceClass::csubckt) {
    return o;
  }
  if (o.length * o.width > reg.region.area) {
    if (ctx.in.warn_any) {
      ctx.bad(reg.first, "L,W estimated for non-rectangular capacitor.");
    }
    o.fields = {std::to_string(o.width), std::to_string(o.width == 0 ? 0 : reg.region.area / o.width)};
  } else {
    o.fields = {std::to_string(o.length), std::to_string(o.width)};
  }
  return o;
}

ClassOut Device::fet(const DevicePattern* devptr) const {
  ClassOut o;
  o.devptr = devptr;
  o.fields = {std::to_string(reg.region.area), std::to_string(tr.perim)};
  o.params = false;
  return o;
}

ClassOut class_values(Device& d, const DevicePattern* devptr) {
  switch (devptr->device_class) {
    case DeviceClass::fet:
      return d.fet(devptr);
    case DeviceClass::mosfet:
    case DeviceClass::bjt:
    case DeviceClass::subckt:
    case DeviceClass::veriloga:
    case DeviceClass::msubckt:
    case DeviceClass::asymmetric:
      return d.mos(devptr);
    case DeviceClass::diode:
    case DeviceClass::ndiode:
    case DeviceClass::pdiode:
    case DeviceClass::dsubckt:
      return d.diode(devptr);
    case DeviceClass::devres:
    case DeviceClass::rsubckt:
      return d.resistor(devptr);
    case DeviceClass::devcap:
    case DeviceClass::devcaprev:
    case DeviceClass::csubckt:
      return d.capacitor(devptr);
    case DeviceClass::vsource:
      break;
  }
  ClassOut o;  // vsource: Magic writes nothing past the record start
  o.devptr = devptr;
  o.params = false;
  o.subs = SubsWrite::never;
  return o;
}

// The substrate field (ExtBasic.c:2612-2630).
void substrate_field(const Context& ctx, const TransRec& tr, const DevicePattern& devptr, SubsWrite mode,
                     DeviceRecord& rec) {
  if (mode == SubsWrite::never) {
    return;
  }
  if (!devptr.substrate_types.empty() && tr.subs_set && tr.subs_node) {
    rec.substrate = tr.subs_node;
    rec.substrate_field = SubstrateField::node;
    return;
  }
  std::string name = devptr.substrate_node;
  if (!ctx.style.substrate.plane && name.size() > 1 && name[0] == '$' && name[1] != '$' && ctx.in.variable) {
    if (const auto value = ctx.in.variable(std::string_view(name).substr(1))) {
      name = *value;  // backward-compatible substrate mode: the variable's value (Q-EXTRACT-9)
    }
  }
  if (name.empty()) {
    if (mode == SubsWrite::always) {
      rec.substrate_name = "None";
      rec.substrate_field = SubstrateField::name;
    }
    return;
  }
  rec.substrate_name = std::move(name);
  rec.substrate_field = SubstrateField::name;
}

std::string node_text(const Context& ctx, NodeRef node) {
  if (!node || *node >= ctx.in.nodes->size()) {
    return {};
  }
  return node_name((*ctx.in.nodes)[*node], ctx.tech, ctx.in.compat);
}

// Missing terminals are copies of the last one; false when the device is
// not written.
bool fill_missing(Context& ctx, const DeviceRegionTiles& reg, TransRec& tr, std::size_t nsd) {
  if (tr.nterm >= nsd) {
    return true;
  }
  const std::size_t missing = nsd - tr.nterm;
  std::string mesg = "device missing " + std::to_string(missing) + " terminal" + (missing == 1 ? "" : "s");
  NodeRef node;
  if (tr.nterm > 0) {
    node = tr.terms[tr.nterm - 1].node;
    mesg += ";\n connecting remainder to node " + node_text(ctx, node);
    while (tr.nterm < nsd && tr.nterm < kMaxTerms) {
      device_detail::Terminal& t = tr.terms[tr.nterm++];
      t = device_detail::Terminal{};
      t.node = node;
    }
  }
  if (ctx.in.warn_fets) {
    ctx.bad(reg.first, mesg);
  }
  return tr.nterm != 0 && node.has_value();
}

bool sorted_class(DeviceClass c) {
  return c == DeviceClass::fet || c == DeviceClass::mosfet || c == DeviceClass::msubckt;
}

// The gate and terminals, in output order (a NULL node is written `(none)`).
void terminals_out(Context& ctx, TransRec& tr, const DevicePattern& devptr, DeviceRecord& rec) {
  std::vector<device_detail::GateLabel>& labels = ctx.gate_labels(tr.gate_node);
  rec.gate = device_detail::out_terminal(labels, tr.gate_node, kGateAttr, tr.gatelen, 0, 0, 0);
  if (sorted_class(devptr.device_class) && device_detail::sd_types(devptr, 1).empty()) {
    device_detail::sort_terminals(ctx, tr, labels);
  }
  for (std::size_t i = 0; i < tr.nterm && i < kMaxTerms; ++i) {
    const device_detail::Terminal& t = tr.terms[i];
    rec.terminals.push_back(
        device_detail::out_terminal(labels, t.node, static_cast<int>(i), t.len, t.area, t.perim, t.shared));
  }
}

void one_device(Context& ctx, const DeviceRegionTiles& reg) {
  TransRec tr;
  tr.plane = reg.region.plane;
  tr.gate_node = ctx.node(reg.first_plane, reg.first);
  device_detail::TileSet visited;
  const std::vector<PlaneTile> tiles = device_detail::walk_region(
      ctx.cell, ctx.tech, ctx.style.device_conn, {reg.first_plane, reg.first}, visited,
      [&](tech::PlaneId p, const db::TileView& t) { return ctx.node(p, t) == tr.gate_node; });
  device_detail::scan_tiles(ctx, reg, tiles, tr);
  device_detail::terminal_areas(ctx, tiles, tr);
  if (tr.rec == nullptr) {
    return;  // no matching record: not a device
  }
  std::size_t nsd = 0;
  const DevicePattern* devptr = device_detail::off_plane_terminals(ctx, reg, tiles, tr, nsd);
  if (!fill_missing(ctx, reg, tr, nsd)) {
    return;
  }
  if (devptr == nullptr) {
    ctx.message("EXTRACT-DEV-001", diag::Severity::warning,
                "Warning:  No matching extraction type for device at (" + std::to_string(reg.first.area.x0) + " " +
                    std::to_string(reg.first.area.y0) + ")");
    return;
  }
  tr.rec = devptr;
  if (devptr->model == "Ignore") {
    return;
  }
  DeviceRecord rec;
  const db::Point ll = reg.region.reference;
  rec.area = {ll.x, ll.y, ll.x + 1, ll.y + 1};
  Device d{ctx, reg, tiles, tr};
  ClassOut o = class_values(d, devptr);
  rec.pattern = ctx.pattern_index(o.devptr);
  rec.length = o.length;
  rec.width = o.width;
  rec.fields = std::move(o.fields);
  if (o.params) {
    rec.params = device_detail::device_params(ctx, reg, tr, *o.devptr, o.length, o.width);
  }
  substrate_field(ctx, tr, *devptr, o.subs, rec);
  terminals_out(ctx, tr, *o.devptr, rec);
  ctx.out.devices.push_back(std::move(rec));
}

}  // namespace

DeviceResult find_devices(const DeviceInputs& inputs, std::vector<DeviceRegionTiles>& regions) {
  DeviceResult out;
  Context ctx{inputs,
              *inputs.cell,
              *inputs.technology,
              *inputs.style,
              inputs.node_of ? inputs.node_of : client_slot_lookup(*inputs.cell),
              out,
              {}};
  for (const DeviceRegionTiles& reg : regions) {
    if (!reg.disabled) {
      one_device(ctx, reg);
    }
  }
  return out;
}

diag::Outcome<DeviceResult> find_devices(const DeviceInputs& inputs) {
  if (inputs.cell == nullptr || inputs.technology == nullptr || inputs.style == nullptr || inputs.nodes == nullptr) {
    return diag::make("EXTRACT-DEV-012", "find_devices: missing cell, technology, style or node list");
  }
  std::vector<DeviceRegionTiles> regions = find_device_regions(*inputs.cell, *inputs.technology, *inputs.style);
  return find_devices(inputs, regions);
}

}  // namespace hefesto::extract
