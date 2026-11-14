// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// TEXT elements, pin shapes and label adjustment (capability gds-read
// "TEXT element parsing", "Label layer assignment", "Pin shapes and port
// labels", "End-of-structure processing" step 4). Ported from Magic 8.3.694
// calma/CalmaRdpt.c (calmaElementText, the pin-label part of
// calmaElementBoundary), database/DBlabel.c (DBAdjustLabelsNew,
// DBPickLabelLayer with doCalma TRUE) and database/DBcellcopy.c
// (DBCellCopyAllPaint, for temporary polygon cells); Magic is (c) the
// Regents of the University of California, distributed under its permissive
// license (THIRD_PARTY_NOTICES.md).
//
// Compat decisions: Q-MASK-120 (a WIDTH in the PATHTYPE/WIDTH run is
// honored only right after PRESENTATION ON, anywhere OFF), Q-MASK-121 (the
// label point and pin rectangle divide by S truncating ON, with floor OFF),
// Q-MASK-122 (the three size formulas, replicated), Q-MASK-123 (a text that
// consumes a pin placeholder becomes a port ON, only when the placeholder
// was one OFF), Q-MASK-133 (an unmapped pair is never a cell-id layer).
//
// Deviations: the label layer choice of DBPickLabelLayer(doCalma) treats no
// type as a device (Hefesto has no extraction style yet, Magic's
// ExtCurStyle device mask), so covering types are ranked by plane only.
//
// Owner: GDSR (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <algorithm>
#include <cmath>

#include "gds/read/reader.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/display/font.hpp"
#include "hefesto/drc/rules/query.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gds/stream/values.hpp"

namespace hefesto::gds::read_detail {

namespace {


constexpr std::uint8_t kAllDirections = db::kPortNorth | db::kPortSouth | db::kPortEast | db::kPortWest;

/// The first technology-dependent plane (PL_TECHDEPBASE).
constexpr tech::PlaneId kTechPlaneBase = tech::kPlaneRHint + 1;

std::uint8_t type_of(RecordType t) {
  return static_cast<std::uint8_t>(t);
}

/// C's truncating division (Q-MASK-121 ON) or floor division (OFF).
std::int64_t divide(std::int64_t v, std::int64_t s, bool magic_compat) noexcept {
  if (magic_compat || v >= 0 || v % s == 0) {
    return v / s;
  }
  return v / s - 1;
}

db::Coord clamp_coord(std::int64_t v) noexcept {
  return static_cast<db::Coord>(std::clamp<std::int64_t>(v, db::kPlaneMin, db::kPlaneMax));
}

/// Magic's port index search for label `self` with text `text`: the
/// largest index seen (from -1), or the index of another label with that
/// text and a positive index, plus one.
std::uint32_t magic_port_index(const db::CellDef& def, const db::Label* self, const std::string& text) {
  std::int64_t i = -1;
  for (const db::Label& l : def.labels()) {
    const auto idx = static_cast<std::int64_t>(l.port_index);
    i = std::max(i, idx);
    if (idx > 0 && &l != self && l.text == text) {
      i = idx - 1;
      break;
    }
  }
  return static_cast<std::uint32_t>(i + 1);
}

bool surrounds(const db::Rect& outer, const db::Rect& inner) noexcept {
  return outer.x0 <= inner.x0 && outer.y0 <= inner.y0 && outer.x1 >= inner.x1 && outer.y1 >= inner.y1;
}

/// size * a * b / (c * d) in 64 bits; 0 (the 1 um default) on overflow.
std::int64_t scaled_size(std::int64_t size, std::int64_t a, std::int64_t b, std::int64_t c, std::int64_t d) noexcept {
  std::int64_t numerator = 0;
  std::int64_t denominator = 0;
  if (__builtin_mul_overflow(size, a, &numerator) || __builtin_mul_overflow(numerator, b, &numerator) ||
      __builtin_mul_overflow(c, d, &denominator) || denominator <= 0) {
    return 0;
  }
  return numerator / denominator;
}

tech::TileType side_type(const db::TileView& t) noexcept {
  return t.split ? (t.right_side ? t.right_type : t.left_type) : t.type;
}

}  // namespace

// --- pin shapes -----------------------------------------------------------------

void Reader::pin_shape(std::size_t cif_layer, const db::Rect& last_strip) {
  const mask::CifLayerLabels labels = engine_->style().label_of(cif_layer);
  if (labels.kind == mask::LabelKind::none || current_ == nullptr || labels.type < 0) {
    return;
  }
  const std::int64_t s = scale_factor();
  const bool compat = compat_.magic_compat;
  const db::Rect rpc{clamp_coord(divide(last_strip.x0, s, compat)), clamp_coord(divide(last_strip.y0, s, compat)),
                     clamp_coord(divide(last_strip.x1, s, compat)), clamp_coord(divide(last_strip.y1, s, compat))};
  const auto type = static_cast<tech::TileType>(labels.type);
  db::CellDef& def = *current_;
  db::Label* lab = nullptr;
  for (db::Label& l : def.labels()) {
    if (surrounds(rpc, l.rect) && l.type == type) {
      l.rect = rpc;  // the larger rectangle replaces the label's
      lab = &l;
      break;
    }
  }
  if (lab == nullptr) {
    db::Label placeholder;  // DBPutLabel with empty text: found by a later TEXT
    placeholder.rect = rpc;
    placeholder.justification = db::kCenter;
    placeholder.type = type;
    def.labels().push_back(std::move(placeholder));
    db::mark_modified(def);
    lab = &def.labels().back();
  }
  if (labels.kind == mask::LabelKind::port && lab->port_directions == 0) {
    lab->port_index = magic_port_index(def, lab, lab->text);
    lab->port_directions = kAllDirections;
  }
}

// --- TEXT -------------------------------------------------------------------------

std::int32_t Reader::default_text_size(std::int32_t type) const {
  const mask::InputStyle& style = engine_->style();
  const std::int64_t s = scale_factor();
  std::int64_t size = 0;
  if (type > 0 && editor_.drc.rules != nullptr) {
    size = drc::default_width(*editor_.drc.rules, *editor_.technology, static_cast<tech::TileType>(type));
    if (size > 0) {
      const ReadScale rs = scale_->scale();
      size = scaled_size(size, rs.den * 8, style.multiplier, rs.num * 10, s);
    }
  }
  const std::int64_t micron = (800 * style.multiplier) / s;
  if (size == 0 || size > micron) {
    size = micron;
  }
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(size, 0, INT32_MAX));
}

void Reader::skip_width_records() {
  skip_set({RecordType::pathtype, RecordType::width});
}

std::optional<std::int64_t> Reader::text_width(std::int32_t& size) {
  const std::optional<std::int32_t> width = read_i4(RecordType::width);
  if (!width) {
    error("Error in reading WIDTH in calmaElementText()");
    return std::nullopt;
  }
  const ReadScale rs = scale_->scale();
  std::int64_t w = std::int64_t{*width} * rs.num;
  if (w % rs.den != 0) {
    error("Text width snapped to nearest integer boundary.");
  }
  w /= rs.den;
  size = engine_->scale_coord(w, mask::Snap::any);  // dimension of the label, in internal units
  return w;
}

bool Reader::text_presentation(TextFields& f) {
  std::optional<Record> rec = peek();
  const bool has_presentation = rec && rec->type == type_of(RecordType::presentation);
  if (has_presentation) {
    const std::int32_t p = read_i2(RecordType::presentation).value_or(0);
    f.font = (p >> 4) & 0x03;
    f.position = presentation_position(p);
  }
  if (!compat_.magic_compat) {
    return text_width_run(f);  // Q-MASK-120 OFF
  }
  if (!has_presentation && rec && rec->type != type_of(RecordType::strans)) {
    skip_width_records();
  }
  rec = peek();
  if (rec && rec->type == type_of(RecordType::width)) {
    return text_width(f.size).has_value();
  }
  if (rec && rec->type != type_of(RecordType::strans)) {
    skip_width_records();
  }
  return true;
}

bool Reader::text_width_run(TextFields& f) {
  // Every WIDTH of the PATHTYPE/WIDTH run sets the size (the last one wins).
  for (;;) {
    const std::optional<Record> rec = peek();
    if (rec && rec->type == type_of(RecordType::pathtype)) {
      (void)next();
    } else if (rec && rec->type == type_of(RecordType::width)) {
      if (!text_width(f.size)) {
        return false;
      }
    } else {
      return true;
    }
  }
}

void Reader::text_transform(TextFields& f) {
  std::optional<Record> rec = next();
  if (!rec || rec->type != type_of(RecordType::strans)) {
    if (rec) {
      unread(std::move(*rec));
    }
    return;
  }
  rec = next();
  if (rec && rec->type == type_of(RecordType::mag)) {
    const double mag = r8_at(rec->data, 0).value_or(0.0);
    if (!(mag > 0) || mag > 10000) {
      char text[64];
      std::snprintf(text, sizeof text, "Invalid text magnification %g.", mag);
      error(text);
    } else {
      const mask::InputStyle& style = engine_->style();
      const double v = 0.5 + ((mag * 1000 * static_cast<double>(static_cast<float>(settings().magscale)) *
                              static_cast<double>(style.multiplier)) /
                             static_cast<double>(scale_factor()));
      f.size = static_cast<std::int32_t>(std::clamp(v, 0.0, 2147483647.0));
    }
  } else if (rec) {
    unread(std::move(*rec));
  }
  rec = next();
  if (rec && rec->type == type_of(RecordType::angle)) {
    const double angle = r8_at(rec->data, 0).value_or(0.0);
    f.rotation = std::isfinite(angle) ? static_cast<std::int32_t>(std::clamp(angle, -2147483647.0, 2147483647.0)) : 0;
  } else if (rec) {
    unread(std::move(*rec));
  }
}

std::optional<db::Point> Reader::text_point() {
  std::optional<Record> xy = next();
  if (!xy) {
    return std::nullopt;
  }
  if (xy->type != type_of(RecordType::xy)) {
    unexpected(RecordType::xy, xy->type);
    return std::nullopt;
  }
  std::int64_t x = 0;
  std::int64_t y = 0;
  if (xy->data.size() < 8) {
    error("Not enough bytes in point record.");
  } else {
    std::int64_t r = 1;
    x = convert(*i4_at(xy->data, 0), 1, r);
    y = convert(*i4_at(xy->data, 1), 1, r);
    x *= r;
  }
  const std::int64_t s = scale_factor();
  return db::Point{clamp_coord(divide(x, s, compat_.magic_compat)), clamp_coord(divide(y, s, compat_.magic_compat))};
}

void Reader::element_text() {
  const auto pair = read_layer_pair(RecordType::texttype, {});
  if (!pair) {
    return;
  }
  const mask::InputStyle& style = engine_->style();
  TextFields f;
  f.cif = style.calma_layer(pair->layer, pair->datatype);
  if (!f.cif) {
    if (style.has_option(mask::input_option::ignore_unknown_layer_labels)) {
      f.type = -1;
    } else {
      layer_error("Label on unknown layer/datatype", pair->layer, pair->datatype);
      f.type = tech::kSpace;
    }
  } else {
    f.type = style.label_of(*f.cif).type;
  }
  f.size = default_text_size(f.type);
  if (!text_presentation(f)) {
    return;
  }
  text_transform(f);
  const std::optional<db::Point> point = text_point();
  if (!point) {
    return;
  }
  const std::optional<std::string> text = read_string(RecordType::string);
  if (!text) {
    return;
  }
  place_text(f, *pair, *point, *text);
}

void Reader::place_text(const TextFields& f, const LayerPair& pair, const db::Point& point, const std::string& text) {
  const std::int64_t s = scale_factor();
  const mask::CifLayerLabels labels = f.cif ? engine_->style().label_of(*f.cif) : mask::CifLayerLabels{};
  if (text.empty()) {
    error("Warning:  Ignoring empty string label at (" + std::to_string(point.x * s) + ", " +
          std::to_string(point.y * s) + ")");
    return;
  }
  if (current_ == nullptr) {
    return;
  }
  if (labels.kind == mask::LabelKind::cell_id) {
    (void)editor_.cells.rename_definition(*current_, text);  // the cell is named by the text
    return;
  }
  if (f.type < 0) {
    if (!engine_->style().has_option(mask::input_option::ignore_unknown_layer_labels)) {
      error("Warning:  label \"" + text + "\" at (" + std::to_string(point.x * s) + ", " +
            std::to_string(point.y * s) + ") is on unhandled layer:purpose pair " + std::to_string(pair.layer) +
            ":" + std::to_string(pair.datatype) + " and will be discarded.");
    }
    return;
  }
  put_text_label(f, labels, point, text);
}

void Reader::put_text_label(const TextFields& f, const mask::CifLayerLabels& labels, const db::Point& point,
                            const std::string& text) {
  db::CellDef& def = *current_;
  const auto type = static_cast<tech::TileType>(f.type);
  db::Label lab;
  lab.text = text;
  lab.type = type;
  lab.rect = db::Rect{point.x, point.y, point.x, point.y};
  lab.justification = f.position;
  lab.sticky = type != tech::kSpace && f.cif && labels.kind != mask::LabelKind::none;
  consume_placeholder(def, lab);
  if (f.font >= 0) {
    lab.font = f.font;
    lab.size = f.size;
    lab.rotation = f.rotation;
    const display::OutlineFont* font = editor_.fonts.font_of(lab);
    if (font == nullptr) {
      lab.font = -1;  // DBPutFontLabel with an unknown font: a default-font label
      lab.size = 0;
      lab.rotation = 0;
    } else {
      const display::FontLabelGeometry geo = display::font_label_geometry(lab, *font, compat_.magic_compat);
      lab.text_bbox = geo.bbox;
      lab.rotation = geo.rotation;
    }
  }
  def.labels().push_back(std::move(lab));
  db::mark_modified(def);
  db::Label& placed = def.labels().back();
  if (f.cif && labels.kind == mask::LabelKind::port) {
    placed.port_index = magic_port_index(def, &placed, text);
    placed.port_directions = kAllDirections;
  }
}

void Reader::consume_placeholder(db::CellDef& def, db::Label& lab) {
  auto& labels = def.labels();
  for (auto it = labels.begin(); it != labels.end(); ++it) {
    if (!it->text.empty() || !surrounds(it->rect, lab.rect) || it->type != lab.type) {
      continue;
    }
    lab.rect = it->rect;
    const bool was_port = it->port_directions != 0;
    labels.erase(it);
    if (compat_.magic_compat || was_port) {  // Q-MASK-123
      lab.port_index = magic_port_index(def, nullptr, lab.text);
      lab.port_directions = kAllDirections;
    }
    return;
  }
}

// --- label adjustment (DBAdjustLabelsNew) -------------------------------------------

std::optional<tech::TileType> Reader::pick_label_layer(const db::CellDef& def, const db::Label& lab) const {
  const tech::Technology& technology = *editor_.technology;
  // types[0]: covers the label; types[1]: a component of covering material.
  tech::TypeMask covers;
  tech::TypeMask components;
  label_cover(def, lab, covers, components);
  if (covers.test(lab.type)) {
    return lab.type;
  }
  const auto plane = technology.home_plane(lab.type);
  if (!plane) {
    return std::nullopt;
  }
  const tech::TypeMask on_plane = technology.plane_types(*plane);
  std::optional<tech::TileType> choice1;
  std::optional<tech::TileType> choice3;
  for (std::size_t i = tech::kMagnet; i < technology.user_type_end(); ++i) {
    const auto t = static_cast<tech::TileType>(i);
    if (!on_plane.test(t)) {
      continue;
    }
    if (covers.test(t)) {
      choice1 = t;  // no extraction style: no type is a device
    } else if (components.test(t)) {
      choice3 = t;
    }
  }
  return choice1 ? choice1 : choice3;
}

void Reader::label_cover(const db::CellDef& def, const db::Label& lab, tech::TypeMask& covers,
                         tech::TypeMask& components) const {
  const db::Rect& r = lab.rect;
  if (r.x0 == r.x1 && r.y0 == r.y1) {
    point_cover(def, r, covers);
    components = covers;
    return;
  }
  if (r.x0 == r.x1) {
    two_area_cover(def, db::Rect{r.x0 - 1, r.y0, r.x1, r.y1}, db::Rect{r.x0, r.y0, r.x1 + 1, r.y1}, covers,
                   components);
    return;
  }
  if (r.y0 == r.y1) {
    two_area_cover(def, db::Rect{r.x0, r.y0 - 1, r.x1, r.y1}, db::Rect{r.x0, r.y0, r.x1, r.y1 + 1}, covers,
                   components);
    return;
  }
  area_cover(def, r, covers, components);
}

void Reader::point_cover(const db::CellDef& def, const db::Rect& r, tech::TypeMask& touching) const {
  const db::Rect check{r.x0 - 1, r.y0 - 1, r.x1 + 1, r.y1 + 1};
  for (std::size_t p = tech::kPlaneMHint; p < def.plane_count(); ++p) {
    def.plane(static_cast<tech::PlaneId>(p)).enumerate(check, tech::TypeMask::all(), [&](const db::TileView& tile) {
      const tech::TileType type = side_type(tile);
      if (type != tech::kSpace) {
        touching.set(type);
      }
      return true;
    });
  }
}

void Reader::two_area_cover(const db::CellDef& def, const db::Rect& a, const db::Rect& b, tech::TypeMask& covers,
                            tech::TypeMask& components) const {
  tech::TypeMask covers2;
  tech::TypeMask components2;
  area_cover(def, a, covers, components);
  area_cover(def, b, covers2, components2);
  covers |= covers2;
  components |= components2;
}

void Reader::area_cover(const db::CellDef& def, const db::Rect& area, tech::TypeMask& covers,
                        tech::TypeMask& components) const {
  const tech::Technology& technology = *editor_.technology;
  for (std::size_t t = tech::kMagnet; t < tech::kMaxTileTypes; ++t) {
    covers.set(static_cast<tech::TileType>(t));
  }
  components = covers;
  for (std::size_t p = tech::kPlaneMHint; p < def.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    const tech::TypeMask plane_types = technology.plane_types(plane);
    def.plane(plane).enumerate(area, tech::TypeMask::all(), [&](const db::TileView& tile) {
      const tech::TileType type = side_type(tile);
      if (type == tech::kSpace) {
        covers &= ~plane_types;
        components &= ~plane_types;
        return false;
      }
      tech::TypeMask others = plane_types;
      others.set(type, false);
      covers &= ~others;
      const tech::ContactInfo* contact = technology.contact(type);
      if (contact != nullptr) {
        others &= ~contact->residues;
      }
      components &= ~others;
      return true;
    });
  }
}

void Reader::adjust_labels(db::CellDef& def) {
  bool modified = false;
  for (db::Label& lab : def.labels()) {
    const std::optional<tech::TileType> new_type =
        lab.type == tech::kSpace ? db::choose_label_layer(def, *editor_.technology, lab) : pick_label_layer(def, lab);
    if (new_type == lab.type || lab.sticky) {
      continue;
    }
    if (!new_type || *new_type == tech::kSpace) {
      // A space label never reaches this branch (doCalma FALSE returns space for it).
      info("Making label \"" + lab.text + "\" on type " + std::string{editor_.technology->type_name(lab.type)} +
           " in cell " + def.name() + " sticky.");
      lab.sticky = true;
      continue;
    }
    if (!def.flags().internal) {  // DBVerbose is DB_VERBOSE_ALL by default
      info("Moving label \"" + lab.text + "\" from " + std::string{editor_.technology->type_name(lab.type)} + " to " +
           std::string{editor_.technology->type_name(*new_type)} + " in cell " + def.name() + ".");
    }
    lab.type = *new_type;
    modified = true;
  }
  if (modified) {
    db::mark_modified(def);
  }
}

// --- temporary polygon cells --------------------------------------------------------

void copy_paint(const db::CellDef& from, db::CellDef& to, const tech::Technology& technology) {
  for (std::size_t p = kTechPlaneBase; p < from.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    std::vector<db::TileView> tiles;
    from.plane(plane).enumerate(db::whole_plane(), tech::TypeMask::all(), [&](const db::TileView& tile) {
      tiles.push_back(tile);
      return true;
    });
    for (const db::TileView& tile : tiles) {
      const tech::TileType type = side_type(tile);
      if (type < tech::kMagnet || technology.home_plane(type) != plane) {
        continue;  // space, DRC types and contact images on their other planes
      }
      std::optional<db::TriangleSpec> triangle;
      if (tile.split) {
        triangle = db::TriangleSpec{tile.diagonal, tile.right_side};
      }
      (void)db::paint_type(to, technology, tile.area, type, triangle);
    }
  }
}

}  // namespace hefesto::gds::read_detail
