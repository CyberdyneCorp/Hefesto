// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF library header (capability lef-write "Library header", "SITE
// blocks", "Technology layers"; Q-LEFDEF-30, 31, 32, 33): the fixed lines,
// UNITS under -tech, PROPERTYDEFINITIONS and the SITE blocks in Magic's
// string hash order (tables of 4 initial buckets), and the technology
// LAYER blocks in layer-table enumeration order, each record once.
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/lefWrite.c lefWriteHeader,
// lefGetProperties and lefGetSites, Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: LEFW (design D2).

#include <set>

#include "hefesto/util/magic_hash_table.hpp"
#include "lef/write/library.hpp"
#include "lef/write/macro_impl.hpp"
#include "lef/write/properties.hpp"

namespace hefesto::lef::write_detail {

namespace {

// HashInit(&table, 4, HT_STRINGKEYS).
using KeyTable = util::MagicHashTable<bool>;
constexpr std::size_t kHeaderTableSize = 4;

std::string line(std::string_view indent, std::string_view text) {
  return std::string{indent} + std::string{text} + "\n";
}

std::string property_definitions(const HeaderInput& in) {
  KeyTable keys{kHeaderTableSize};
  for (const db::CellDef* cell : in.cells) {
    if (const auto value = string_property(*cell, "LEFproperties")) {
      for (const std::string& key : property_keys(*value, in.magic_compat)) {
        (void)keys.find_or_insert(key);
      }
    }
  }
  if (keys.size() == 0) {
    return {};
  }
  std::string out = "PROPERTYDEFINITIONS\n";
  keys.for_each([&](const KeyTable::Entry& e) { out += line(kIn0, "MACRO " + e.key + " STRING ;"); });
  return out + "END PROPERTYDEFINITIONS\n\n";
}

// The SITE's size: the site cell's bounding box; when it has the fixed-bbox
// flag, the FIXED_BBOX of the cell being written with magic-compat ON (as
// Magic reads it), of the site cell itself with it OFF (Q-LEFDEF-33).
db::Rect site_box(const HeaderInput& in, const db::CellDef& site) {
  db::Rect box = site.bbox();
  if (site.flags().fixed_bbox) {
    const db::CellDef& source = in.magic_compat ? *in.def : site;
    if (const auto fixed = fixed_bbox(source)) {
      box = *fixed;
    }
  }
  return box;
}

std::string site_block(const HeaderInput& in, const db::CellDef& site) {
  std::string out = "SITE " + site.name() + "\n";
  out += line(kIn0, "SYMMETRY " + string_property(site, "LEFsymmetry").value_or("Y") + " ;");
  out += line(kIn0, "CLASS " + string_property(site, "LEFclass").value_or("CORE") + " ;");
  const db::Rect box = site_box(in, site);
  out += line(kIn0, "SIZE " + format_distance(in.oscale * static_cast<float>(box.x1 - box.x0), in.units) + " BY " +
                        format_distance(in.oscale * static_cast<float>(box.y1 - box.y0), in.units) + " ;");
  return out + "END " + site.name() + "\n\n";
}

std::string sites(const HeaderInput& in) {
  KeyTable names{kHeaderTableSize};
  for (const db::CellDef* cell : in.cells) {
    if (const auto site = string_property(*cell, "LEFsite")) {
      (void)names.find_or_insert(*site);
    }
  }
  std::string out;
  names.for_each([&](const KeyTable::Entry& e) {
    const db::CellDef* site = in.library != nullptr ? in.library->find(e.key) : nullptr;
    if (site != nullptr) {
      out += site_block(in, *site);
    }
  });
  return out;
}

std::string real_line(std::string_view keyword, float value) {
  return line(kIn0, std::string{keyword} + " " + format_real(static_cast<double>(value)) + " ;");
}

// A cut record: CUT AREA, 4x the cut area in half units, with magic-compat
// ON; the side as WIDTH for a square area with it OFF (Q-LEFDEF-32).
std::string cut_body(const HeaderInput& in, const LayerRecord& r) {
  std::string out = line(kIn0, "TYPE CUT ;");
  if (!r.contact_area) {
    return out;
  }
  const db::Rect& a = *r.contact_area;
  if (in.magic_compat) {
    const std::int64_t area = a.width() * a.height();
    if (area > 0) {
      out += real_line("CUT AREA", static_cast<float>(area) * in.oscale * in.oscale);
    }
  } else if (!a.is_null() && a.width() == a.height()) {
    out += real_line("WIDTH", static_cast<float>(a.width()) / 2.0F * in.oscale);
  }
  return out;
}

// A route record: what the record holds (Q-LEFDEF-31: the fill rules of
// lef-tech-section decide it per mode).
std::string route_body(const HeaderInput& in, const LayerRecord& r) {
  std::string out = line(kIn0, "TYPE ROUTING ;");
  const auto positive = [](const std::optional<db::Coord>& v) { return v && *v > 0; };
  if (positive(r.pitch)) {
    out += real_line("PITCH", static_cast<float>(*r.pitch) * in.oscale);
  }
  if (positive(r.width)) {
    out += real_line("WIDTH", static_cast<float>(*r.width) * in.oscale);
  }
  if (positive(r.spacing)) {
    out += real_line("SPACING", static_cast<float>(*r.spacing) * in.oscale);
  }
  if (positive(r.width)) {
    out += line(kIn0, r.direction == RouteDirection::vertical ? "DIRECTION VERTICAL ;" : "DIRECTION HORIZONTAL ;");
  }
  return out;
}

std::string layer_body(const HeaderInput& in, const LayerRecord& r) {
  switch (r.layer_class) {
    case LayerClass::cut:
      return cut_body(in, r);
    case LayerClass::route:
      return route_body(in, r);
    case LayerClass::masterslice:
      return line(kIn0, "TYPE MASTERSLICE ;");
    case LayerClass::overlap:
      return line(kIn0, "TYPE OVERLAP ;");
    default:
      return {};
  }
}

// Records with no magic type (obstruction-only, ignore) and boundary
// records are skipped. Magic's test for vias holding a geometry cell never
// fires (the cell is never set), so with magic-compat ON a via record of a
// LEF or DEF read is written; with it OFF it is skipped.
bool layer_written(const HeaderInput& in, std::string_view name, const LayerRecord& r) {
  if (r.type == tech::kSpace || r.layer_class == LayerClass::bound) {
    return false;
  }
  return in.magic_compat || in.vias == nullptr || in.vias->find(std::string{name}) == nullptr;
}

std::string technology_layers(const HeaderInput& in) {
  std::string out;
  std::set<const LayerRecord*> written;
  in.layers->enumerate([&](std::string_view name, const LayerRecord& r) {
    if (!written.insert(&r).second || !layer_written(in, name, r)) {
      return;
    }
    out += "LAYER " + r.canonical_name + "\n" + layer_body(in, r) + "END " + r.canonical_name + "\n\n";
  });
  return out;
}

}  // namespace

std::string library_header(const HeaderInput& in, std::vector<diag::Diagnostic>& messages) {
  messages.push_back(diag::make("LEF-WRITE-001", "Diagnostic:  Write LEF header for cell " + in.def->name()));
  std::string out = "VERSION 5.7 ;\n";
  out += line(kIn0, "NOWIREEXTENSIONATPIN ON ;");
  out += line(kIn0, "DIVIDERCHAR \"/\" ;");
  out += line(kIn0, "BUSBITCHARS \"[]\" ;");
  if (in.tech) {
    out += "UNITS\n" + line(kIn0, "DATABASE MICRONS " + std::to_string(in.units.database) + " ;") + "END UNITS\n\n";
  }
  out += property_definitions(in);
  out += sites(in);
  if (in.tech && in.layers != nullptr) {
    out += technology_layers(in);
  }
  return out;
}

}  // namespace hefesto::lef::write_detail
