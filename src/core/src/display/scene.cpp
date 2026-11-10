// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The scene builder (capabilities hefesto-rendering, ui-windows-and-views).
// The style loop, the contact/stacking visibility rules, the unique-tile
// search and the pale-style choice follow Magic 8.3.694
// dbwind/DBWdisplay.c (DBWredisplay, dbwPaintFunc, dbwLabelFunc,
// dbwBBoxFunc) and database/DBcellsrch.c (DBTreeSrUniqueTiles), Copyright
// (C) 1985, 1990 Regents of the University of California, used under
// Magic's permissive license (see THIRD_PARTY_NOTICES.md).

#include "hefesto/display/scene.hpp"

#include <algorithm>
#include <map>

namespace hefesto::display {

ResolvedStyle resolve_style(const StyleSet& set, std::size_t index) {
  ResolvedStyle style;
  style.index = index;
  if (index >= set.table.styles.size()) {
    return style;
  }
  const DisplayStyle& s = set.table.styles[index];
  if (s.color >= 0 && static_cast<std::size_t>(s.color) < set.colors.entries.size()) {
    const ColorEntry& c = set.colors.entries[static_cast<std::size_t>(s.color)];
    style.r = static_cast<float>(c.r) / 255.0F;
    style.g = static_cast<float>(c.g) / 255.0F;
    style.b = static_cast<float>(c.b) / 255.0F;
  }
  const bool opengl = set.display_class == DisplayClass::opengl;
  style.a = opengl && s.mask != 127 && s.mask >= 0 && s.mask < 127 ? static_cast<float>(s.mask) / 127.0F : 1.0F;
  style.fill = s.fill;
  style.outline = s.outline & 0xFF;
  if (s.fill == FillKind::stipple) {
    style.stipple = set.table.stipple(s.stipple);
  }
  return style;
}

void resolve_highlight(HighlightItem& item, const StyleSet* styles) {
  if (styles != nullptr) {
    item.resolved = resolve_style(*styles, item.style);
    return;
  }
  ResolvedStyle white;
  white.index = item.style;
  white.outline = 0xFF;
  item.resolved = white;
}

ResolvedStyle opengl_supercolor(const ResolvedStyle& style) noexcept {
  if (style.a >= 1.0F) {
    return style;
  }
  ResolvedStyle out = style;
  out.r = style.r * 2.0F - 0.8F;
  out.g = style.g * 2.0F - 0.8F;
  out.b = style.b * 2.0F - 0.8F;
  return out;
}

// --- boundary --------------------------------------------------------------------

namespace {

using Interval = std::pair<db::Coord, db::Coord>;

std::vector<Interval> merged(std::vector<Interval> in) {
  std::sort(in.begin(), in.end());
  std::vector<Interval> out;
  for (const auto& iv : in) {
    if (!out.empty() && iv.first <= out.back().second) {
      out.back().second = std::max(out.back().second, iv.second);
    } else {
      out.push_back(iv);
    }
  }
  return out;
}

bool covered(const std::vector<Interval>& set, db::Coord lo, db::Coord hi) {
  const auto it = std::upper_bound(set.begin(), set.end(), Interval{lo, std::numeric_limits<db::Coord>::max()});
  if (it == set.begin()) {
    return false;
  }
  const auto& iv = *std::prev(it);
  return iv.first <= lo && hi <= iv.second;
}

/// The parts of [lo, hi) covered by exactly one of a and b, merged.
std::vector<Interval> symmetric_difference(const std::vector<Interval>& a, const std::vector<Interval>& b) {
  std::vector<db::Coord> cuts;
  for (const auto* set : {&a, &b}) {
    for (const auto& iv : *set) {
      cuts.push_back(iv.first);
      cuts.push_back(iv.second);
    }
  }
  std::sort(cuts.begin(), cuts.end());
  cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
  std::vector<Interval> out;
  for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
    if (covered(a, cuts[i], cuts[i + 1]) == covered(b, cuts[i], cuts[i + 1])) {
      continue;
    }
    if (!out.empty() && out.back().second == cuts[i]) {
      out.back().second = cuts[i + 1];
    } else {
      out.emplace_back(cuts[i], cuts[i + 1]);
    }
  }
  return out;
}

struct EdgeSides {
  std::vector<Interval> low;   // material on the low side of the line
  std::vector<Interval> high;  // material on the high side
};

template <typename MakeSegment>
void emit_edges(std::map<db::Coord, EdgeSides>& lines, std::vector<SceneSegment>& out, MakeSegment make) {
  for (auto& [at, sides] : lines) {
    for (const auto& iv : symmetric_difference(merged(sides.low), merged(sides.high))) {
      out.push_back(make(at, iv));
    }
  }
}

}  // namespace

std::vector<SceneSegment> boundary_segments(const std::vector<db::Rect>& rects) {
  std::map<db::Coord, EdgeSides> vertical;
  std::map<db::Coord, EdgeSides> horizontal;
  for (const db::Rect& r : rects) {
    if (r.is_null()) {
      continue;
    }
    vertical[r.x0].high.emplace_back(r.y0, r.y1);
    vertical[r.x1].low.emplace_back(r.y0, r.y1);
    horizontal[r.y0].high.emplace_back(r.x0, r.x1);
    horizontal[r.y1].low.emplace_back(r.x0, r.x1);
  }
  std::vector<SceneSegment> out;
  emit_edges(horizontal, out, [](db::Coord y, const Interval& iv) {
    return SceneSegment{db::Point{iv.first, y}, db::Point{iv.second, y}};
  });
  emit_edges(vertical, out, [](db::Coord x, const Interval& iv) {
    return SceneSegment{db::Point{x, iv.first}, db::Point{x, iv.second}};
  });
  return out;
}

// --- scene builder ---------------------------------------------------------------

namespace {

/// The types drawn with layout style `i` in this window (DBWredisplay's
/// `layers` after the contact and stacking rules).
tech::TypeMask drawn_types(const tech::Technology& tech, tech::TypeMask layers) {
  const auto end = static_cast<tech::TileType>(tech.type_count());
  // A contact that is not visible is drawn in place of its visible residues.
  for (tech::TileType t = tech::kFirstUserType; t < end; ++t) {
    const auto* contact = tech.contact(t);
    if (contact != nullptr && !layers.test(t) && !(contact->residues & layers).empty()) {
      layers.set(t);
    }
  }
  // A stacking type is drawn only through the residue sharing its home plane.
  for (const auto& info : tech.contacts()) {
    if (!info.stacked || !layers.test(info.type)) {
      continue;
    }
    for (tech::TileType t = tech::kFirstUserType; t < end; ++t) {
      // DBResidueMask of a stacked type: its two component contacts.
      const bool component = t == info.component_a || t == info.component_b;
      if (layers.test(t) && component && tech.home_plane(info.type) != tech.home_plane(t)) {
        layers.set(info.type, false);
        break;
      }
    }
  }
  return layers;
}

db::Rect clip_to_plane(const db::Rect& r) {
  const db::Rect limit = db::whole_plane();
  return db::Rect{std::max(r.x0, limit.x0), std::max(r.y0, limit.y0), std::min(r.x1, limit.x1), std::min(r.y1, limit.y1)};
}

struct StyleSlot {
  std::size_t normal_layer = 0;  // index into LayoutScene::layers
  std::size_t pale_layer = 0;
  tech::TypeMask types;
};

class SceneBuilder {
 public:
  SceneBuilder(const tech::Technology& tech, const StyleSet& styles, const FontRegistry& fonts,
               const SceneOptions& options)
      : tech_(tech), styles_(styles), fonts_(fonts), options_(options) {}

  LayoutScene build(const db::CellDef& root);

 private:
  void prepare_styles();
  void visit(const db::CellDef& def, const db::Transform& to_root, int depth);
  void visit_plane(const db::CellDef& def, tech::PlaneId plane, const db::Transform& to_root, bool normal,
                   const db::Rect& area);
  void visit_use(const db::CellUse& use, const db::Transform& to_root, int depth);
  void add_label(const db::Label& label, const db::Transform& to_root, bool normal);
  [[nodiscard]] bool is_normal(const db::CellDef& def, const db::Transform& to_root) const;
  [[nodiscard]] bool paint(const db::CellDef& def, tech::PlaneId plane, const db::Rect& area,
                           const tech::TypeMask& types, const db::TileVisitor& visit) const;
  void include(const db::Rect& r) { scene_.bounds = db::include(scene_.bounds, r); }

  const tech::Technology& tech_;
  const StyleSet& styles_;
  const FontRegistry& fonts_;
  const SceneOptions& options_;
  db::Rect label_area_ = db::whole_plane();  // options_.area grown by the label extent
  const db::CellDef* root_ = nullptr;
  LayoutScene scene_;
  std::vector<StyleSlot> slots_;
  std::vector<std::vector<std::size_t>> slots_of_type_;  // type -> indices into slots_
  tech::TypeMask all_drawn_;
};

void SceneBuilder::prepare_styles() {
  const StyleTable& table = styles_.table;
  tech::TypeMask visible = options_.visible;
  visible.set(tech::kSpace);
  slots_of_type_.assign(tech::kMaxTileTypes, {});
  for (std::size_t i = 0; i < table.layout_count; ++i) {
    const std::size_t index = kFixedStyleCount + i;
    const tech::TypeMask layers = styles_.types.types_of(index) & visible;
    if (layers.empty()) {
      continue;
    }
    StyleSlot slot;
    slot.types = drawn_types(tech_, layers);
    slot.normal_layer = scene_.layers.size();
    scene_.layers.push_back(SceneLayer{resolve_style(styles_, index), false, {}, {}, {}, 2 * i});
    slot.pale_layer = scene_.layers.size();
    scene_.layers.push_back(SceneLayer{resolve_style(styles_, table.pale_index(index)), true, {}, {}, {}, 2 * i + 1});
    for (std::size_t t = 0; t < tech::kMaxTileTypes; ++t) {
      if (slot.types.test(static_cast<tech::TileType>(t))) {
        slots_of_type_[t].push_back(slots_.size());
      }
    }
    all_drawn_ |= slot.types;
    slots_.push_back(slot);
  }
}

bool SceneBuilder::paint(const db::CellDef& def, tech::PlaneId plane, const db::Rect& area,
                         const tech::TypeMask& types, const db::TileVisitor& visit) const {
  if (options_.paint_source) {
    return options_.paint_source(def, plane, area, types, visit);
  }
  return def.plane(plane).enumerate(area, types, visit);
}

bool SceneBuilder::is_normal(const db::CellDef& def, const db::Transform& to_root) const {
  if (options_.all_same) {
    return true;
  }
  if (!options_.edit_root) {
    return false;
  }
  const db::CellDef* edit = options_.edit_cell != nullptr ? options_.edit_cell : root_;
  return &def == edit && to_root == options_.edit_to_root;
}

void SceneBuilder::visit_plane(const db::CellDef& def, tech::PlaneId plane, const db::Transform& to_root,
                               bool normal, const db::Rect& area) {
  // Unique tiles: only the types whose home plane is this plane.
  const tech::TypeMask mask = all_drawn_ & tech_.plane_types(plane);
  if (mask.empty()) {
    return;
  }
  std::map<std::size_t, std::vector<db::Rect>> by_layer;
  (void)paint(def, plane, area, mask, [&](const db::TileView& tile) {
    const auto home = tech_.home_plane(tile.type);
    if (!home || *home != plane || tile.type >= slots_of_type_.size()) {
      return true;
    }
    const db::Rect r = db::apply(to_root, tile.area);
    for (const std::size_t s : slots_of_type_[tile.type]) {
      const std::size_t layer = normal ? slots_[s].normal_layer : slots_[s].pale_layer;
      if (tile.split) {
        // Split tiles keep their diagonal; the transform may mirror it.
        scene_.layers[layer].triangles.push_back(SceneTriangle{r, tile.diagonal, tile.right_side});
      } else {
        scene_.layers[layer].rects.push_back(r);
        by_layer[layer].push_back(r);
      }
      include(r);
    }
    return true;
  });
  for (auto& [layer, rects] : by_layer) {
    if (scene_.layers[layer].style.outline != 0) {
      auto edges = boundary_segments(rects);
      auto& out = scene_.layers[layer].outline;
      out.insert(out.end(), edges.begin(), edges.end());
    }
  }
}

void SceneBuilder::visit(const db::CellDef& def, const db::Transform& to_root, int depth) {
  const bool normal = is_normal(def, to_root);
  // The redisplay area grown by one unit, in this cell's coordinates, kept
  // inside the plane (the whole-plane area must not overflow when moved).
  const db::Rect area = clip_to_plane(db::apply(db::invert(to_root), clip_to_plane(db::expand(options_.area, 1))));
  const std::size_t planes = std::min(def.plane_count(), tech_.plane_count());
  // DRC error tiles are shown only for the root ("DRC error display").
  if (depth == 0 && tech::kPlaneDrcError < planes) {
    visit_plane(def, tech::kPlaneDrcError, to_root, normal, area);
  }
  for (std::size_t p = tech::kFirstUserPlane; p < planes; ++p) {
    visit_plane(def, static_cast<tech::PlaneId>(p), to_root, normal, area);
  }
  if (options_.show_labels) {
    for (const db::Label& label : def.labels()) {
      add_label(label, to_root, normal);
    }
  }
  for (const auto& use : def.uses()) {
    if (use && use->def != nullptr) {
      visit_use(*use, to_root, depth);
    }
  }
}

void SceneBuilder::visit_use(const db::CellUse& use, const db::Transform& to_root, int depth) {
  const db::Rect bbox = db::apply(to_root, use.bbox);
  // The extended box holds the rendered text of font labels, which the
  // label search finds by that text (display search).
  const db::Rect reach = db::include(bbox, db::apply(to_root, use.extended));
  if (!db::touch(reach, label_area_)) {
    return;
  }
  const bool expanded = options_.expanded && options_.expanded(use);
  if (!expanded) {
    if (options_.show_cells && db::touch(bbox, options_.area)) {
      scene_.cells.push_back(SceneCell{bbox, use.def->name(), use.id, depth + 1, resolve_style(styles_, kStyleCellBox)});
      include(bbox);
    }
    return;
  }
  const db::ArrayInfo& a = use.array;
  const db::Coord xsep = a.xlo > a.xhi ? -a.xsep : a.xsep;
  const db::Coord ysep = a.ylo > a.yhi ? -a.ysep : a.ysep;
  for (std::int32_t y = std::min(a.ylo, a.yhi); y <= std::max(a.ylo, a.yhi); ++y) {
    for (std::int32_t x = std::min(a.xlo, a.xhi); x <= std::max(a.xlo, a.xhi); ++x) {
      // GeoTransTranslate: the element offset is applied before the use transform.
      const db::Transform element = db::compose(use.transform, db::translation(xsep * (x - a.xlo), ysep * (y - a.ylo)));
      visit(*use.def, db::compose(to_root, element), depth + 1);
    }
  }
}

std::vector<SceneSegment> port_lines(const db::Label& label, const db::Transform& to_root) {
  const db::Rect& r = label.rect;
  std::vector<SceneSegment> out;
  auto add = [&](db::Point a, db::Point b) { out.push_back(SceneSegment{db::apply(to_root, a), db::apply(to_root, b)}); };
  if ((label.port_directions & db::kPortNorth) != 0) {
    add(db::Point{r.x0, r.y1}, db::Point{r.x1, r.y1});
  }
  if ((label.port_directions & db::kPortSouth) != 0) {
    add(db::Point{r.x0, r.y0}, db::Point{r.x1, r.y0});
  }
  if ((label.port_directions & db::kPortEast) != 0) {
    add(db::Point{r.x1, r.y0}, db::Point{r.x1, r.y1});
  }
  if ((label.port_directions & db::kPortWest) != 0) {
    add(db::Point{r.x0, r.y0}, db::Point{r.x0, r.y1});
  }
  return out;
}

/// The label as it appears in root coordinates (Magic copies font labels
/// through the instance transform the same way).
db::Label to_root_label(const db::Label& label, const db::Transform& t, bool magic_compat) {
  db::Label out = label;
  out.rect = db::apply(t, label.rect);
  out.justification = db::transform_position(t, label.justification);
  out.offset = db::Point{t.a * label.offset.x + t.b * label.offset.y, t.d * label.offset.x + t.e * label.offset.y};
  out.rotation = db::transform_angle(t, label.rotation, magic_compat);
  return out;
}

void SceneBuilder::add_label(const db::Label& label, const db::Transform& to_root, bool normal) {
  if (label.type != tech::kSpace && !options_.visible.test(label.type)) {
    return;  // "A label SHALL be drawn only if its attached type is visible"
  }
  SceneLabel out;
  const db::Label root_label = to_root_label(label, to_root, options_.magic_compat);
  out.text = label.text;
  out.type = label.type;
  out.rect = root_label.rect;
  out.justification = root_label.justification;
  out.port = label.is_port();
  out.pale = !normal;
  const std::size_t style = out.port ? (normal ? kStylePorts : kStylePalePorts)
                                     : (normal ? kStyleLabels : kStylePaleLabels);
  out.style = resolve_style(styles_, style);
  if (out.port) {
    out.port_lines = port_lines(label, to_root);
    out.port_line_style = resolve_style(styles_, kStylePortConnection);
  }
  if (const OutlineFont* font = fonts_.font_of(label)) {
    out.font_label = true;
    out.size = label.size;
    out.font_bbox = font_label_geometry(root_label, *font, options_.magic_compat).bbox;
    out.outline_polygons = font_label_polygons(root_label, *font);
  }
  if (!label_in_display_area(out, label_area_)) {
    return;
  }
  if (out.font_label) {
    include(out.font_bbox);
  }
  include(db::Rect{out.rect.x0, out.rect.y0, std::max(out.rect.x1, out.rect.x0 + 1),
                   std::max(out.rect.y1, out.rect.y0 + 1)});
  scene_.labels.push_back(std::move(out));
}

LayoutScene SceneBuilder::build(const db::CellDef& root) {
  root_ = &root;
  label_area_ = clip_to_plane(db::expand(options_.area, std::max<db::Coord>(options_.label_extent, 0)));
  scene_.background = resolve_style(styles_, kStyleBackground);
  prepare_styles();
  visit(root, db::kIdentity, 0);
  // Drop empty layers but keep their relative order.
  std::erase_if(scene_.layers, [](const SceneLayer& l) { return l.rects.empty() && l.triangles.empty(); });
  return std::move(scene_);
}

}  // namespace

diag::Outcome<LayoutScene> build_scene(const db::CellDef& root, const tech::Technology& tech, const StyleSet& styles,
                                       const FontRegistry& fonts, const SceneOptions& options) {
  SceneBuilder builder(tech, styles, fonts, options);
  return builder.build(root);
}

BoxDrawing box_drawing(const db::Rect& box, std::int64_t box_px_width, std::int64_t box_px_height,
                       const std::optional<db::Rect>& edit_bbox) {
  // DBWDrawBox (dbwind/DBWtools.c).
  if (box_px_width == 0 && box_px_height == 0) {
    return BoxDrawing{BoxShape::point, kStyleOutlineHighlights};
  }
  const bool thin_x = box_px_width != 0 && box_px_width < 4;
  const bool thin_y = box_px_height != 0 && box_px_height < 4;
  if (thin_x || thin_y) {
    return BoxDrawing{BoxShape::outline, kStyleOutlineHighlights};
  }
  const bool outside = edit_bbox && !db::overlap(box, *edit_bbox);
  return BoxDrawing{BoxShape::thick, outside ? kStyleMediumHighlights : kStyleBox};
}

std::vector<LayerSwatch> layer_palette(const tech::Technology& tech, const StyleSet& styles) {
  std::vector<LayerSwatch> out;
  for (std::size_t t = tech::kFirstUserType; t < tech.type_count(); ++t) {
    const auto type = static_cast<tech::TileType>(t);
    const auto& indices = styles.types.of(type);
    if (indices.empty()) {
      continue;
    }
    LayerSwatch swatch;
    swatch.type = type;
    swatch.name = std::string(tech.type_name(type));
    if (const auto home = tech.home_plane(type)) {
      swatch.plane = std::string(tech.plane_name(*home));
    }
    swatch.contact = tech.is_contact(type);
    for (const std::size_t index : indices) {
      swatch.styles.push_back(resolve_style(styles, index));
    }
    out.push_back(std::move(swatch));
  }
  return out;
}

}  // namespace hefesto::display
