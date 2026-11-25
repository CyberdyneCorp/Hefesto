// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The header, VIAS, COMPONENTS and PINS sections of DEF write (capability
// def-write "DEF output file grammar and section order", "DIEAREA", "VIAS
// section content", "Component orientation", "COMPONENTS section", "PINS
// section"; Q-LEFDEF-91, 93, 94, 103, 114).
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defWrite.c defWriteHeader,
// defTransPos, defWriteVias, defCountComponents, defWriteComponents,
// arrayDefFunc, defCountPins and defWritePins, Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: DEFW (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <algorithm>
#include <array>
#include <cstdlib>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "def/write/internal.hpp"
#include "hefesto/db/array_overlap.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/db/use_index.hpp"

namespace hefesto::def::write_detail {

namespace {

/// A FIXED_BBOX property of four integers.
std::optional<db::Rect> fixed_bbox_property(const db::CellDef& cell) {
  const db::PropertyValue* value = cell.properties().find("FIXED_BBOX");
  const auto* dims = value != nullptr ? std::get_if<db::DimensionList>(value) : nullptr;
  if (dims == nullptr || dims->values.size() != 4) {
    return std::nullopt;
  }
  return db::Rect{dims->values[0], dims->values[1], dims->values[2], dims->values[3]};
}

/// R: the child's FIXED_BBOX when it is marked fixed-bbox, else its bbox.
db::Rect placement_box(const db::CellDef& child) {
  if (child.flags().fixed_bbox) {
    if (const auto fixed = fixed_bbox_property(child)) {
      return *fixed;
    }
  }
  return child.bbox();
}

std::string rect_text(float x1, float y1, float x2, float y2) {
  return "( " + g10(x1) + " " + g10(y1) + " ) ( " + g10(x2) + " " + g10(y2) + " )";
}

std::string placed(const std::string& name, const std::string& macro, float x, float y, std::string_view orient) {
  return "   - " + name + " " + macro + "\n      + PLACED ( " + g10(x) + " " + g10(y) + " ) " +
         std::string{orient} + " ;\n";
}

/// One VIAS record's name and RECT lines.
void via_record(const InfoEntry& e, const lef::DefViaContext& via_context, const mask::OutputStyle* style,
                std::string& out) {
  lef::ViaDefinition record;
  if (e.via != nullptr) {
    record = *e.via;
  } else {
    // A cut layer of the layer table: its contact area, no extra shapes.
    record.type = e.type();
    record.area = e.layer->contact_area.value_or(db::Rect{0, 0, 0, 0});
    record.canonical_name = e.layer->canonical_name;
  }
  out += "   - " + e.canonical_name();
  for (const lef::ViaRect& r : lef::via_rects(record, e.canonical_name(), style, via_context)) {
    out += "\n      + RECT " + r.layer + " " + rect_text(r.x1, r.y1, r.x2, r.y2);
  }
  out += " ;\n";
}

/// The underscore filter of defWriteVias: two `_` or more, the first not
/// the last.
bool generated_name(const std::string& name) {
  const std::size_t first = name.find('_');
  return first != std::string::npos && name.rfind('_') != first;
}

/// The records the VIAS section writes (Q-LEFDEF-91).
std::vector<const InfoEntry*> via_records(const Context& c, const std::vector<InfoEntry>& order,
                                          const std::vector<std::string>& created) {
  const std::set<std::string> mine(created.begin(), created.end());
  std::vector<const InfoEntry*> out;
  for (const InfoEntry& e : order) {
    if (!e.live() || e.layer_class() != lef::LayerClass::cut) {
      continue;
    }
    const bool selected = c.compat ? generated_name(e.canonical_name()) : mine.contains(e.key);
    if (selected) {
      out.push_back(&e);
    }
  }
  return out;
}

// defCountCompFunc: Magic's signed product with magic-compat ON
// (Q-LEFDEF-94), the true element count with it OFF.
std::int64_t use_count(const db::CellUse& use, bool compat) {
  const std::int64_t dx = static_cast<std::int64_t>(use.array.xhi) - use.array.xlo;
  const std::int64_t dy = static_cast<std::int64_t>(use.array.yhi) - use.array.ylo;
  if (compat) {
    return (dx + 1) * (dy + 1);
  }
  return (std::llabs(dx) + 1) * (std::llabs(dy) + 1);
}

/// arrayDefFunc's index text: `y,x`, `y` or `x`.
std::string element_index(const db::CellUse& use, std::int32_t x, std::int32_t y) {
  const bool sx = use.array.xhi != use.array.xlo;
  const bool sy = use.array.yhi != use.array.ylo;
  std::string idx;
  if (sy) {
    idx = std::to_string(y) + (sx ? "," : "");
  }
  if (sx) {
    idx += std::to_string(x);
  }
  return idx;
}

void array_components(const Context& c, const db::CellUse& use, std::string& out) {
  const db::ElementRange range = db::array_overlap(use, use.bbox);
  const db::Rect r = placement_box(*use.def);
  const std::string_view orient = def_orientation(use.transform);
  for (std::int32_t y = range.ylo; y <= range.yhi; ++y) {
    for (std::int32_t x = range.xlo; x <= range.xhi; ++x) {
      const db::Rect box = db::apply(db::element_transform(use, x, y), r);
      out += placed(use.id + "[" + element_index(use, x, y) + "]", use.def->name(), scaled(box.x0, c.scale),
                    scaled(box.y0, c.scale), orient);
    }
  }
}

// defComponentFunc (Q-LEFDEF-93).
void single_component(const Context& c, const db::CellUse& use, std::string& out) {
  const std::string& name = use.def->name();
  const std::size_t slash = name.rfind('/');
  const std::string macro = slash == std::string::npos ? name : name.substr(slash + 1);
  const db::Rect r = placement_box(*use.def);
  const db::Rect tr = db::apply(use.transform, r);
  db::Point at{tr.x0, tr.y0};
  if (c.compat) {
    const db::Rect tb = db::apply(use.transform, use.def->bbox());
    at = db::Point{use.bbox.x0 - r.x0 + (tr.x0 - tb.x0), use.bbox.y0 - r.y0 + (tr.y0 - tb.y0)};
  }
  out += placed(use.id, macro, scaled(at.x, c.scale), scaled(at.y, c.scale), def_orientation(use.transform));
}

std::string_view direction(db::PortClass cls) {
  switch (cls) {
    case db::PortClass::input:
      return "INPUT";
    case db::PortClass::output:
      return "OUTPUT";
    case db::PortClass::tristate:
    case db::PortClass::bidirectional:
      return "INOUT";
    case db::PortClass::feedthrough:
      return "FEEDTHRU";
    case db::PortClass::default_class:
      break;
  }
  return "";
}

// Q-LEFDEF-103: RESET, SCAN and TIEOFF print empty with magic-compat ON.
std::string_view use_keyword(db::PortUse use, bool compat) {
  constexpr std::array<std::string_view, 9> kNames{"",      "SIGNAL", "ANALOG", "POWER", "GROUND",
                                                   "CLOCK", "RESET",  "SCAN",   "TIEOFF"};
  const auto index = static_cast<std::size_t>(use);
  if (index >= kNames.size() || (compat && use >= db::PortUse::reset)) {
    return "";
  }
  return kNames[index];
}

void pin_layer(const Context& c, const db::Label& lab, std::string& out) {
  const auto& name = c.map.name(lab.type);
  const std::string layer{c.tech.type_name(lab.type)};
  if (!name) {
    // Q-LEFDEF-114: Magic passes the label text where the layer belongs.
    c.say(kPinLayer, c.compat ? "No LEF layer corresponding to layer " + lab.text + " of pin \"" + layer + "\"."
                              : "No LEF layer corresponding to layer " + layer + " of pin \"" + lab.text + "\".");
    return;
  }
  const db::Coord w = lab.rect.x1 - lab.rect.x0;
  const db::Coord h = lab.rect.y1 - lab.rect.y0;
  const auto half = [&](db::Coord v) { return static_cast<double>(c.scale * static_cast<float>(v)) / 2.0; };
  out += "        + LAYER " + *name + " ( " + g10(half(-w)) + " " + g10(half(-h)) + " ) ( " + g10(half(w)) + " " +
         g10(half(h)) + " )";
  if (!c.compat) {
    out += "\n";
  }
}

void pin(const Context& c, const db::Label& lab, std::string& out) {
  out += "   - " + lab.text + " + NET " + lab.text + "\n";
  if (lab.port_class != db::PortClass::default_class) {
    out += "     + DIRECTION " + std::string{direction(lab.port_class)} + "\n";
  }
  if (lab.port_use != db::PortUse::default_use) {
    out += "     + USE " + std::string{use_keyword(lab.port_use, c.compat)} + "\n";
  }
  out += "     + PORT\n";
  pin_layer(c, lab, out);
  const auto center = [&](db::Coord v) { return static_cast<double>(c.scale * static_cast<float>(v)) / 2.0; };
  out += "        + PLACED ( " + g10(center(lab.rect.x0 + lab.rect.x1)) + " " +
         g10(center(lab.rect.y0 + lab.rect.y1)) + " ) N ;\n";
}

}  // namespace

std::string_view def_orientation(const db::Transform& t) noexcept {
  constexpr std::array<std::string_view, 8> kOrient{"N", "S", "E", "W", "FN", "FS", "FE", "FW"};
  const bool ew = t.a == 0 && t.e == 0;
  const bool flip = ew ? static_cast<std::int64_t>(t.b) * t.d > 0 : static_cast<std::int64_t>(t.a) * t.e < 0;
  const bool sw = ew ? t.d > 0 : t.e <= 0;
  return kOrient[(flip ? 4U : 0U) + (ew ? 2U : 0U) + (sw ? 1U : 0U)];
}

void write_header(const Context& c, std::string_view tech_name, std::int32_t units, std::string& out) {
  c.say(kProgress, "Diagnostic:  Write DEF header for cell " + c.cell.name());
  out += "VERSION 5.7 ;\n";
  out += "   NAMESCASESENSITIVE ON ;\n";
  out += "   DIVIDERCHAR \"/\" ;\n";
  out += "   BUSBITCHARS \"()\" ;\n";
  out += "   DESIGN " + c.cell.name() + " ;\n";
  out += "   TECHNOLOGY " + std::string{tech_name} + " ;\n";
  out += "   UNITS DISTANCE MICRONS " + std::to_string(units) + " ;\n";
  const db::Rect die = fixed_bbox_property(c.cell).value_or(c.cell.bbox());
  out += "   DIEAREA " + rect_text(scaled(die.x0, c.scale), scaled(die.y0, c.scale), scaled(die.x1, c.scale),
                                   scaled(die.y1, c.scale)) +
         " ;\n\n";
}

void write_vias(const Context& c, const std::vector<InfoEntry>& order, const lef::DefViaContext& via_context,
                const std::vector<std::string>& created, std::string& out) {
  const std::vector<const InfoEntry*> records = via_records(c, order, created);
  const std::size_t count = c.compat ? created.size() : records.size();
  out += "VIAS " + std::to_string(count) + " ;\n";
  if (count > 0) {
    for (const InfoEntry* e : records) {
      via_record(*e, via_context, c.style, out);
    }
  }
  out += "END VIAS\n\n";
}

void write_components(const Context& c, std::string& out) {
  c.say(kProgress, "Diagnostic:  Finding all components of cell " + c.cell.name());
  std::vector<const db::CellUse*> uses;
  (void)c.cell.use_index().enumerate_all([&](db::CellUse& use) {
    if (!use.id.empty() && use.def != nullptr) {
      uses.push_back(&use);
    }
    return true;
  });
  std::int64_t total = 0;
  for (const db::CellUse* use : uses) {
    total += use_count(*use, c.compat);
  }
  out += "COMPONENTS " + std::to_string(static_cast<std::int32_t>(total)) + " ;\n";
  for (const db::CellUse* use : uses) {
    if (use->array.is_array()) {
      array_components(c, *use, out);
    } else {
      single_component(c, *use, out);
    }
  }
  out += "END COMPONENTS\n\n";
}

void write_pins(const Context& c, std::string& out) {
  c.say(kProgress, "Diagnostic:  Finding all pins of cell " + c.cell.name());
  const auto count = std::count_if(c.cell.labels().begin(), c.cell.labels().end(),
                                   [](const db::Label& l) { return l.is_port(); });
  out += "PINS " + std::to_string(count) + " ;\n";
  for (const db::Label& lab : c.cell.labels()) {
    if (lab.is_port()) {
      pin(c, lab, out);
    }
  }
  out += "END PINS\n\n";
}

}  // namespace hefesto::def::write_detail
