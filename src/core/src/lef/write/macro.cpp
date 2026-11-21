// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF macro construction (capability lef-write "Macro source geometry and
// layer masks", "Macro boundary determination", "Macro header lines",
// "Degenerate setback", "Macro PROPERTY lines", "Names written verbatim";
// Q-LEFDEF-35, 40, 47, 53): the flat copy, the type sets, the boundary, the
// MACRO header lines and the PROPERTY lines of lefWriteMacro; the pins and
// obstructions are in pins.cpp and obstruction.cpp.
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/lefWrite.c lefWriteMacro (the
// flattening, type masks, boundary, header and PROPERTY parts) and
// lefGetBound, Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: LEFW (design D2).

#include "hefesto/lef/write/macro.hpp"


#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/extract/tech/style_view.hpp"
#include "lef/write/macro_impl.hpp"
#include "lef/write/properties.hpp"

namespace hefesto::lef {

namespace wd = write_detail;

namespace {

// lefWriteMacro's class test: route, cut and (with domaster) masterslice.
bool routing_class(LayerClass c, bool do_master) noexcept {
  return c == LayerClass::route || c == LayerClass::cut || (do_master && c == LayerClass::masterslice);
}

void add_routing_record(const MacroContext& ctx, const LayerRecord& r, bool do_master, TypeSets& sets) {
  const tech::Technology& tech = *ctx.tech;
  if (r.type != tech::kSpace) {
    sets.route.set(r.type);
    if (tech.is_contact(r.type)) {
      sets.route |= wd::residue_mask(tech, r.type);
    }
    if (r.layer_class == LayerClass::route && r.obstruction_type != tech::kSpace) {
      sets.hide.set(r.type);
    } else if (r.layer_class == LayerClass::masterslice && ctx.style != nullptr && ctx.style->has_style() &&
               !ctx.style->substrate_types().value_or(tech::TypeMask{}).test(r.type)) {
      sets.well.set(r.type);
    }
  }
  if (r.obstruction_type != tech::kSpace) {
    sets.route.set(r.obstruction_type);
  }
  if (do_master && r.layer_class == LayerClass::masterslice && r.type != tech::kSpace) {
    sets.master_planes |= tech.type_planes(r.type);
  }
}

std::string format_pair(float a, float b, LefUnits units, std::string_view separator) {
  return format_distance(a, units) + std::string{separator} + format_distance(b, units);
}

}  // namespace

MacroBuilder::MacroBuilder(const MacroContext& context, const db::CellDef& cell, const MacroOptions& options)
    : state_(std::make_unique<wd::MacroState>(context, cell, options)) {}

MacroBuilder::~MacroBuilder() = default;

const TypeSets& MacroBuilder::sets() const noexcept {
  return state_->sets;
}

// DBCellCopyAllPaint(CU_DESCEND_ALL) into __lefFlat__: every level's paint,
// no labels.
diag::Status MacroBuilder::flatten() {
  wd::MacroState& m = *state_;
  db::FlattenOptions options;
  options.descend = db::Descend::all;
  options.labels = db::FlattenLabels::none;
  options.magic_compat = m.ctx.magic_compat;
  auto flat = db::flatten_to_new(m.scratch, m.cell, "__lefFlat__", m.tech(), options);
  if (!flat.ok()) {
    return flat.error();
  }
  m.flat = flat.value();
  m.flat->flags().internal = true;
  return diag::Done{};
}

void MacroBuilder::type_sets() {
  wd::MacroState& m = *state_;
  TypeSets sets;
  for (const db::Label& lab : m.cell.labels()) {
    if (lab.is_port()) {
      sets.route.set(lab.type);
    }
  }
  m.ctx.layers->enumerate([&](std::string_view, const LayerRecord& r) {
    if (routing_class(r.layer_class, m.options.do_master)) {
      add_routing_record(m.ctx, r, m.options.do_master, sets);
    }
    if (r.layer_class == LayerClass::bound && r.type != tech::kSpace) {
      sets.boundary.set(r.type);
    }
  });
  // Without an extraction style G and D are empty (Q-LEFDEF-40).
  if (m.ctx.style != nullptr) {
    sets.gate = m.ctx.style->gate_types().value_or(tech::TypeMask{});
    sets.diffusion = m.ctx.style->diff_types().value_or(tech::TypeMask{});
  }
  // Labels on space and tech-file oddities never make space a routing type.
  sets.route.clear_type(tech::kSpace);
  m.sets = sets;
  m.sets_ready = true;
}

namespace {

// The boundary B: the extent of the boundary layers (the cell's bounding
// box when they hold no paint, Q-LEFDEF-53), else the cell's bounding box;
// a FIXED_BBOX of four integers overrides both.
db::Rect macro_boundary(const wd::MacroState& m) {
  db::Rect b;
  if (!m.sets.boundary.empty()) {
    for (std::size_t p = tech::kFirstUserPlane; p < m.flat->plane_count(); ++p) {
      m.flat->plane(static_cast<tech::PlaneId>(p)).enumerate(db::whole_plane(), m.sets.boundary,
                                                              [&](const db::TileView& t) {
                                                                b = db::include(b, t.area);
                                                                return true;
                                                              });
    }
  }
  if (b.is_null()) {
    b = m.cell.bbox();
  }
  if (m.cell.flags().fixed_bbox) {
    if (const auto fixed = wd::fixed_bbox(m.cell)) {
      b = *fixed;
    }
  }
  return b;
}

// A -hide distance larger than half the boundary hides nothing
// (Q-LEFDEF-47: Magic drops it silently; Hefesto warns).
void check_setback(wd::MacroState& m) {
  const db::Coord d = m.options.setback;
  if (d <= 0) {
    return;
  }
  const db::Rect& b = m.boundary;
  if (b.width() >= 2 * static_cast<std::int64_t>(d) && b.height() >= 2 * static_cast<std::int64_t>(d)) {
    return;
  }
  m.message("LEF-WRITE-006",
            "Macro \"" + m.cell.name() + "\": the -hide setback " + std::to_string(d) + " is more than half the " +
                std::to_string(b.width()) + " x " + std::to_string(b.height()) +
                " boundary; nothing is hidden (Q-LEFDEF-47)",
            true);
  m.options.setback = -1;
}

// CLASS, FOREIGN, ORIGIN, SIZE, SYMMETRY and SITE.
void header_lines(const wd::MacroState& m) {
  std::string& out = *m.out;
  const std::string& name = m.cell.name();
  out += "MACRO " + name + "\n";
  out += std::string{wd::kIn0} + "CLASS " + wd::string_property(m.cell, "LEFclass").value_or("BLOCK") + " ;\n";
  out += std::string{wd::kIn0} + "FOREIGN " + name + " ;\n";
  const float s = m.ctx.oscale;
  const db::Rect& b = m.boundary;
  // "0.0 - X" is computed in double, then narrowed to lefPrint's float.
  const auto x = static_cast<float>(0.0 - static_cast<double>(s * static_cast<float>(b.x0)));
  const auto y = static_cast<float>(0.0 - static_cast<double>(s * static_cast<float>(b.y0)));
  out += std::string{wd::kIn0} + "ORIGIN " + format_pair(x, y, m.ctx.units, " ") + " ;\n";
  out += std::string{wd::kIn0} + "SIZE " +
         format_pair(s * static_cast<float>(b.x1 - b.x0), s * static_cast<float>(b.y1 - b.y0), m.ctx.units, " BY ") +
         " ;\n";
  if (const auto symmetry = wd::string_property(m.cell, "LEFsymmetry")) {
    out += std::string{wd::kIn0} + "SYMMETRY " + *symmetry + " ;\n";
  }
  if (const auto site = wd::string_property(m.cell, "LEFsite")) {
    out += std::string{wd::kIn0} + "SITE " + *site + " ;\n";
  }
}

std::string scale_text(float oscale) {
  return format_real(static_cast<double>(oscale));
}

}  // namespace

diag::Status MacroBuilder::write(std::string& out, std::vector<diag::Diagnostic>& messages) {
  wd::MacroState& m = *state_;
  m.out = &out;
  m.messages = &messages;
  m.message("LEF-WRITE-001", "Diagnostic:  Writing LEF output for cell " + m.cell.name());
  if (m.flat == nullptr) {
    if (auto st = flatten(); !st.ok()) {
      return st;
    }
  }
  m.message("LEF-WRITE-001", "Diagnostic:  Scale value is " + scale_text(m.ctx.oscale));
  if (!m.sets_ready) {
    type_sets();
  }
  m.names = wd::make_name_map(m.tech(), *m.ctx.layers, m.ctx.vias);
  wd::warn_name(m, "Macro", m.cell.name());
  m.boundary = macro_boundary(m);
  check_setback(m);
  header_lines(m);
  if (auto st = wd::write_pins(m); !st.ok()) {
    return st;
  }
  if (auto st = wd::write_obstructions(m); !st.ok()) {
    return st;
  }
  if (const auto properties = wd::string_property(m.cell, "LEFproperties")) {
    for (const std::string& pair : wd::property_pairs(*properties)) {
      out += std::string{wd::kIn0} + "PROPERTY " + pair + " ;\n";
    }
  }
  out += "END " + m.cell.name() + "\n";
  return diag::Done{};
}

}  // namespace hefesto::lef
