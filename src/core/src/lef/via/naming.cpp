// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The generated vias of DEF write: naming with the directional-surround
// suffix, registration in the via table and the position index
// (defCountViaFunc, lef/defWrite.c:1858-2023; capability def-write
// "Generated via definitions", Q-LEFDEF-105, 106). Magic is (c) the Regents
// of the University of California, BSD-style licence (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: LEFVIA (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <array>
#include <cstdio>
#include <string>
#include <utility>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/drc/rules/query.hpp"
#include "hefesto/lef/via/regions.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::lef {

namespace {

/// One residue L or H of the contact: its default surround s and
/// directional surround d, and the orientation letter.
struct Surround {
  tech::TileType type = tech::kSpace;
  std::int32_t base = 0;         // sL / sH
  std::int32_t directional = 0;  // dL / dH
  char orient = 'x';
};

/// The first two residues of `ctype` in type order with their surrounds;
/// the second stays empty (surround 0) for a single residue (Q-LEFDEF-106
/// fix; Magic reads it uninitialised).
std::array<Surround, 2> surrounds(tech::TileType ctype, const DefViaContext& context) {
  std::array<Surround, 2> out{};
  const tech::ContactInfo* info = context.tech.contact(ctype);
  if (info == nullptr) {
    return out;
  }
  std::size_t n = 0;
  for (std::size_t t = tech::kFirstUserType; t < context.tech.user_type_end() && n < 2; ++t) {
    const auto r = static_cast<tech::TileType>(t);
    if (!info->residues.test(r)) {
      continue;
    }
    out[n].type = r;
    if (context.rules != nullptr) {
      out[n].base = drc::default_surround(*context.rules, context.tech, ctype, r);
      out[n].directional = drc::directional_surround(*context.rules, ctype, r);
    }
    ++n;
  }
  return out;
}

/// 'h', or 'v' when a tile not connecting to the residue lies in the probe
/// area around `c` (Q-LEFDEF-105: Magic's probe is asymmetric in x).
char orientation(const db::CellDef& cell, const db::Rect& c, const Surround& s, const DefViaContext& context) {
  const std::int32_t sx = context.compat.magic_compat ? s.base : -s.base;
  const db::Rect probe{c.x0 - s.directional + sx, c.y0 - s.base, c.x1 + s.directional + s.base, c.y1 + s.base};
  const auto plane = context.tech.home_plane(s.type);
  if (!plane || *plane >= cell.plane_count()) {
    return 'h';
  }
  const bool found = !cell.plane(*plane).enumerate(probe, context.tech.not_connected(s.type),
                                                   [](const db::TileView&) { return false; });
  return found ? 'v' : 'h';
}

std::string format_g10(float value) {
  std::array<char, 64> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%.10g", static_cast<double>(value));
  return buffer.data();
}

/// The residue rectangle of `s`: A grown by 2s, then by 2d along the
/// orientation.
db::Rect residue_rect(const db::Rect& a, const Surround& s) {
  db::Rect r{a.x0 - 2 * s.base, a.y0 - 2 * s.base, a.x1 + 2 * s.base, a.y1 + 2 * s.base};
  if (s.directional > 0 && s.orient == 'h') {
    r.x0 -= 2 * s.directional;
    r.x1 += 2 * s.directional;
  } else if (s.directional > 0) {
    r.y0 -= 2 * s.directional;
    r.y1 += 2 * s.directional;
  }
  return r;
}

}  // namespace

std::function<std::optional<std::string>(tech::TileType)> lef_names(const LayerTable& table) {
  return [&table](tech::TileType type) -> std::optional<std::string> {
    const LayerRecord* record = table.magic_to_lef(type);
    if (record == nullptr) {
      return std::nullopt;
    }
    return record->canonical_name;
  };
}

std::optional<RegionVia> region_via(const db::CellDef& cell, const ContactRegion& region,
                                    const DefViaContext& context) {
  const auto lname = context.lef_name ? context.lef_name(region.type) : std::nullopt;
  if (!lname) {
    return std::nullopt;  // undefined LEF layers are not written
  }
  const db::Rect& c = region.area;
  const auto w = static_cast<db::Coord>(c.width());
  const auto h = static_cast<db::Coord>(c.height());
  std::array<Surround, 2> s = surrounds(region.type, context);
  for (Surround& one : s) {
    one.orient = one.directional > 0 ? orientation(cell, c, one, context) : 'x';
  }
  RegionVia out;
  out.name = *lname + "_" + format_g10(static_cast<float>(w) * context.scale) + "_" +
             format_g10(static_cast<float>(h) * context.scale);
  if (s[0].directional > 0 || s[1].directional > 0) {
    out.name += std::string{"_"} + s[0].orient + s[1].orient;
  }
  out.record.type = region.type;
  out.record.area = db::Rect{-w, -h, w, h};
  out.record.canonical_name = out.name;
  out.record.generated = true;
  out.record.ref_count = 0;
  for (const Surround& one : s) {
    if (one.base > 0 || one.directional > 0) {
      out.record.shapes.insert(out.record.shapes.begin(), ViaShape{one.type, residue_rect(out.record.area, one)});
    }
  }
  out.position_key = std::string{context.tech.plane_name(region.plane, tech::NameForm::short_name)} + "_" +
                     std::to_string(c.x0) + "_" + std::to_string(c.y0);
  return out;
}

std::optional<std::string> via_name(const db::CellDef& cell, const ContactRegion& region,
                                    const DefViaContext& context) {
  auto via = region_via(cell, region, context);
  if (!via) {
    return std::nullopt;
  }
  return std::move(via->name);
}

DefVias register_vias(const db::CellDef& cell, ViaTable& vias, const LayerTable& layers,
                      const DefViaContext& context) {
  DefVias out;
  for (const ContactRegion& region : contact_regions(cell, context.tech, context.compat)) {
    auto via = region_via(cell, region, context);
    if (!via) {
      continue;
    }
    const auto* known = vias.find(via->name);
    if ((known == nullptr || !known->value) && layers.find(via->name) == nullptr) {
      vias.insert(via->name, via->record);
      out.created.push_back(via->name);
      ++out.count;
    }
    out.positions[via->position_key] = via->name;
  }
  return out;
}

}  // namespace hefesto::lef
