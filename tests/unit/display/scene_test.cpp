// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The scene builder and the type-to-style mapping (capabilities
// hefesto-rendering, ui-windows-and-views, tech-styles-section).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

#include "hefesto/db/cell.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/display/diag_codes.hpp"
#include "hefesto/display/scene.hpp"
#include "hefesto/tech/technology.hpp"

namespace display = hefesto::display;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace diag = hefesto::diag;

namespace {

tech::StyleRefLine ref_line(tech::TileType type, std::vector<std::string> refs) {
  tech::StyleRefLine line;
  line.type = type;
  line.refs = std::move(refs);
  return line;
}

/// planes active, metal1, metal2; types poly, ndiff (active), metal1,
/// metal2; contacts pc (active: poly, metal1) and m2c (metal1: metal1, metal2),
/// both stackable.
struct Fixture {
  std::shared_ptr<const tech::Technology> tech;
  tech::TileType poly = 0;
  tech::TileType ndiff = 0;
  tech::TileType metal1 = 0;
  tech::TileType metal2 = 0;
  tech::TileType pc = 0;
  tech::TileType m2c = 0;
  tech::PlaneId active = 0;
  tech::PlaneId m1 = 0;

  Fixture() {
    tech::TechnologyBuilder b;
    b.set_name("test");
    active = b.add_plane("active").value();
    m1 = b.add_plane("metal1").value();
    REQUIRE(b.add_plane("metal2").ok());
    poly = b.add_type("active", "polysilicon,poly").value();
    ndiff = b.add_type("active", "ndiff").value();
    metal1 = b.add_type("metal1", "metal1,m1").value();
    metal2 = b.add_type("metal2", "metal2,m2").value();
    pc = b.add_type("active", "pcontact,pc").value();
    m2c = b.add_type("metal1", "m2contact,m2c").value();
    const std::array<std::string_view, 2> pc_res{"poly", "metal1"};
    const std::array<std::string_view, 2> m2c_res{"metal1", "metal2"};
    REQUIRE(b.add_contact("pc", pc_res).ok());
    REQUIRE(b.add_contact("m2c", m2c_res).ok());
    REQUIRE(b.add_stackable({}).ok());
    auto built = b.finalize();
    REQUIRE(built.ok());
    tech = built.value();
  }

  [[nodiscard]] tech::TileType stacked() const {
    for (const auto& info : tech->contacts()) {
      if (info.stacked) {
        return info.type;
      }
    }
    return tech::kSpace;
  }
};

/// N layout styles; layout style k (index 52+k) is solid, color k+1,
/// long name "s<k>", ordinal 10+k; style 52+N+k is its pale version.
display::StyleSet style_set(std::size_t n) {
  display::StyleSet set;
  set.colors.entries.resize(256);
  for (std::size_t i = 0; i < 256; ++i) {
    const auto v = static_cast<std::uint8_t>(i);
    set.colors.entries[i] = {v, 0, 0, ""};
  }
  set.table.layout_count = n;
  set.table.styles.resize(display::kFixedStyleCount + 2 * n);
  for (std::size_t k = 0; k < n; ++k) {
    auto& s = set.table.styles[display::kFixedStyleCount + k];
    s.color = static_cast<std::int32_t>(k + 1);
    s.mask = 127;
    s.long_name = "s" + std::to_string(k);
    s.ordinal = static_cast<std::int32_t>(10 + k);
    auto& pale = set.table.styles[display::kFixedStyleCount + n + k];
    pale = s;
    pale.mask = 40;
    pale.long_name = "p" + std::to_string(k);
  }
  return set;
}

void bind(display::StyleSet& set, const tech::Technology& t, tech::TileType type, std::size_t layout_k) {
  set.types.styles_of_type.resize(t.type_count());
  set.types.styles_of_type[type].push_back(display::kFixedStyleCount + layout_k);
}

db::PaintRow fill_with(tech::TileType type) {
  db::PaintRow row{};
  row.fill(type);
  return row;
}

void paint(db::CellDef& def, tech::PlaneId plane, const db::Rect& r, tech::TileType type) {
  REQUIRE(def.plane(plane).paint(r, fill_with(type)).ok());
}

const display::SceneLayer* layer_with(const display::LayoutScene& scene, std::size_t style, bool pale) {
  for (const auto& layer : scene.layers) {
    if (layer.style.index == style && layer.pale == pale) {
      return &layer;
    }
  }
  return nullptr;
}

bool contains(const std::vector<db::Rect>& rects, const db::Rect& r) {
  return std::find(rects.begin(), rects.end(), r) != rects.end();
}

}  // namespace

TEST_CASE("instance paint is pale, edit-cell paint is normal",
          "[cap:ui-windows-and-views][req:Redisplay order for layout windows]") {
  Fixture f;
  auto set = style_set(4);
  bind(set, *f.tech, f.poly, 1);
  db::CellLibrary lib;
  db::CellDef* top = lib.create("top", f.tech->plane_count()).value();
  db::CellDef* inv = lib.create("inv", f.tech->plane_count()).value();
  paint(*inv, f.active, {0, 0, 4, 10}, f.poly);
  inv->set_bboxes({0, 0, 4, 10}, {0, 0, 4, 10});
  paint(*top, f.active, {-10, 0, -6, 10}, f.poly);
  auto use = std::make_unique<db::CellUse>();
  use->id = "u1";
  use->def = inv;
  use->parent = top;
  use->transform = db::translation(20, 0);
  use->bbox = db::apply(use->transform, inv->bbox());
  const db::CellUse* u1 = use.get();
  top->uses().push_back(std::move(use));

  display::SceneOptions options;
  options.expanded = [u1](const db::CellUse& u) { return &u == u1; };
  const auto scene = display::build_scene(*top, *f.tech, set, {}, options);
  REQUIRE(scene.ok());
  const auto* normal = layer_with(scene.value(), 53, false);
  const auto* pale = layer_with(scene.value(), 53 + 4, true);
  REQUIRE(normal != nullptr);
  REQUIRE(pale != nullptr);
  CHECK(contains(normal->rects, db::Rect{-10, 0, -6, 10}));
  CHECK(contains(pale->rects, db::Rect{20, 0, 24, 10}));
  CHECK(pale->style.a < 1.0F);  // OpenGL opacity 40/127 (Q-UI-35)
  CHECK(scene.value().cells.empty());

  // allSame: everything normal.
  options.all_same = true;
  const auto same = display::build_scene(*top, *f.tech, set, {}, options);
  REQUIRE(same.ok());
  CHECK(layer_with(same.value(), 57, true) == nullptr);
}

TEST_CASE("layers are drawn in style order", "[cap:ui-windows-and-views][req:Redisplay order for layout windows]") {
  Fixture f;
  auto set = style_set(4);
  bind(set, *f.tech, f.metal1, 3);
  bind(set, *f.tech, f.ndiff, 0);
  db::CellLibrary lib;
  db::CellDef* top = lib.create("top", f.tech->plane_count()).value();
  paint(*top, f.m1, {0, 0, 10, 10}, f.metal1);
  paint(*top, f.active, {0, 0, 10, 10}, f.ndiff);
  const auto scene = display::build_scene(*top, *f.tech, set, {}, {});
  REQUIRE(scene.ok());
  REQUIRE(scene.value().layers.size() == 2);
  CHECK(scene.value().layers[0].style.index == 52);
  CHECK(scene.value().layers[1].style.index == 55);
}

TEST_CASE("outlines skip internal edges and crosses keep each rectangle",
          "[cap:display-style-files][req:Outline and fill rendering semantics]") {
  const auto edges = display::boundary_segments({{0, 0, 4, 4}, {4, 0, 8, 4}});
  for (const auto& e : edges) {
    CHECK_FALSE((e.a.x == 4 && e.b.x == 4));
  }
  CHECK(edges.size() == 4);

  Fixture f;
  auto set = style_set(2);
  set.table.styles[52].fill = display::FillKind::cross;
  set.table.styles[52].outline = 0xFF;
  bind(set, *f.tech, f.ndiff, 0);
  db::CellLibrary lib;
  db::CellDef* top = lib.create("top", f.tech->plane_count()).value();
  paint(*top, f.active, {0, 0, 4, 4}, f.ndiff);
  paint(*top, f.active, {4, 0, 8, 4}, f.ndiff);
  const auto scene = display::build_scene(*top, *f.tech, set, {}, {});
  REQUIRE(scene.ok());
  const auto& layer = scene.value().layers.front();
  CHECK(layer.style.fill == display::FillKind::cross);
  for (const auto& e : layer.outline) {
    CHECK_FALSE((e.a.x == 4 && e.b.x == 4));
  }
}

TEST_CASE("contacts are drawn in visible residue styles and stacked types on their home plane",
          "[cap:ui-windows-and-views][req:Redisplay order for layout windows]") {
  Fixture f;
  auto set = style_set(3);
  bind(set, *f.tech, f.metal1, 0);
  bind(set, *f.tech, f.poly, 1);
  db::CellLibrary lib;
  db::CellDef* top = lib.create("top", f.tech->plane_count()).value();
  paint(*top, f.active, {0, 0, 4, 4}, f.pc);
  const auto scene = display::build_scene(*top, *f.tech, set, {}, {});
  REQUIRE(scene.ok());
  // The pc tile (home plane active) appears in both residue styles.
  const auto* m1 = layer_with(scene.value(), 52, false);
  const auto* poly = layer_with(scene.value(), 53, false);
  REQUIRE(m1 != nullptr);
  REQUIRE(poly != nullptr);
  CHECK(contains(m1->rects, db::Rect{0, 0, 4, 4}));
  CHECK(contains(poly->rects, db::Rect{0, 0, 4, 4}));
}

TEST_CASE("labels use label or port styles, port lines and visibility",
          "[cap:ui-windows-and-views][req:Label display]") {
  Fixture f;
  auto set = style_set(2);
  bind(set, *f.tech, f.metal2, 0);
  db::CellLibrary lib;
  db::CellDef* top = lib.create("top", f.tech->plane_count()).value();
  db::Label port;
  port.text = "A";
  port.type = f.metal2;
  port.rect = {0, 0, 10, 4};
  port.port_directions = db::kPortNorth;
  top->labels().push_back(port);
  db::Label plain = port;
  plain.text = "B";
  plain.port_directions = 0;
  top->labels().push_back(plain);

  const auto scene = display::build_scene(*top, *f.tech, set, {}, {});
  REQUIRE(scene.ok());
  REQUIRE(scene.value().labels.size() == 2);
  const auto& a = scene.value().labels[0];
  CHECK(a.style.index == display::kStylePorts);
  REQUIRE(a.port_lines.size() == 1);
  CHECK(a.port_lines[0] == display::SceneSegment{{0, 4}, {10, 4}});
  CHECK(a.port_line_style.index == display::kStylePortConnection);
  CHECK(scene.value().labels[1].style.index == display::kStyleLabels);

  display::SceneOptions hidden;
  hidden.visible = tech::TypeMask::all();
  hidden.visible.set(f.metal2, false);
  const auto none = display::build_scene(*top, *f.tech, set, {}, hidden);
  REQUIRE(none.ok());
  CHECK(none.value().labels.empty());

  display::SceneOptions not_edit;
  not_edit.edit_root = false;
  const auto pale = display::build_scene(*top, *f.tech, set, {}, not_edit);
  REQUIRE(pale.ok());
  CHECK(pale.value().labels[0].style.index == display::kStylePalePorts);
  CHECK(pale.value().labels[1].style.index == display::kStylePaleLabels);
}

TEST_CASE("unexpanded instances show their outline and names", "[cap:ui-windows-and-views][req:Subcell display]") {
  Fixture f;
  auto set = style_set(1);
  db::CellLibrary lib;
  db::CellDef* top = lib.create("top", f.tech->plane_count()).value();
  db::CellDef* inv = lib.create("inv", f.tech->plane_count()).value();
  inv->set_bboxes({0, 0, 200, 100}, {0, 0, 200, 100});
  auto use = std::make_unique<db::CellUse>();
  use->id = "u1";
  use->def = inv;
  use->parent = top;
  use->bbox = inv->bbox();
  top->uses().push_back(std::move(use));
  const auto scene = display::build_scene(*top, *f.tech, set, {}, {});
  REQUIRE(scene.ok());
  REQUIRE(scene.value().cells.size() == 1);
  CHECK(scene.value().cells[0].name == "inv");
  CHECK(scene.value().cells[0].id == "u1");
  CHECK(scene.value().cells[0].bbox == db::Rect{0, 0, 200, 100});
  CHECK(scene.value().cells[0].style.index == display::kStyleCellBox);

  display::SceneOptions no_cells;
  no_cells.show_cells = false;
  CHECK(display::build_scene(*top, *f.tech, set, {}, no_cells).value().cells.empty());
}

TEST_CASE("DRC errors of subcells are not drawn", "[cap:ui-windows-and-views][req:DRC error display]") {
  Fixture f;
  auto set = style_set(1);
  bind(set, *f.tech, tech::kErrorP, 0);
  db::CellLibrary lib;
  db::CellDef* top = lib.create("top", f.tech->plane_count()).value();
  db::CellDef* inv = lib.create("inv", f.tech->plane_count()).value();
  paint(*top, tech::kPlaneDrcError, {0, 0, 2, 2}, tech::kErrorP);
  paint(*inv, tech::kPlaneDrcError, {50, 50, 52, 52}, tech::kErrorP);
  inv->set_bboxes({50, 50, 52, 52}, {50, 50, 52, 52});
  auto use = std::make_unique<db::CellUse>();
  use->id = "u1";
  use->def = inv;
  use->parent = top;
  use->bbox = inv->bbox();
  top->uses().push_back(std::move(use));
  display::SceneOptions options;
  options.expanded = [](const db::CellUse&) { return true; };
  const auto scene = display::build_scene(*top, *f.tech, set, {}, options);
  REQUIRE(scene.ok());
  REQUIRE(scene.value().layers.size() == 1);
  CHECK(scene.value().layers[0].rects == std::vector<db::Rect>{{0, 0, 2, 2}});
}

TEST_CASE("the layer palette lists styled types with their PDK styles",
          "[cap:hefesto-rendering][req:Style-file-faithful 2D drawing]") {
  Fixture f;
  auto set = style_set(2);
  bind(set, *f.tech, f.metal1, 1);
  const auto palette = display::layer_palette(*f.tech, set);
  REQUIRE(palette.size() == 1);
  CHECK(palette[0].name == "metal1");
  CHECK(palette[0].plane == "metal1");
  REQUIRE(palette[0].styles.size() == 1);
  CHECK(palette[0].styles[0].r == 2.0F / 255.0F);  // the color map, never a theme color
}

TEST_CASE("OpenGL translucent styles are pre-compensated", "[cap:hefesto-rendering][req:Style-file-faithful 2D drawing]") {
  display::ResolvedStyle s;
  s.r = 0.5F;
  s.a = 0.5F;
  CHECK(std::abs(display::opengl_supercolor(s).r - 0.2F) < 1e-6F);
  s.a = 1.0F;
  CHECK(display::opengl_supercolor(s).r == 0.5F);
}

// --- tech-styles-section mapping ----------------------------------------------------------

TEST_CASE("style references resolve by ordinal or long name; invalid ones are skipped",
          "[cap:tech-styles-section][req:Type to style mapping]") {
  Fixture f;
  const auto set = style_set(3);
  tech::StylesSection section;
  section.style_family = "mos";
  section.lines.push_back(ref_line(f.metal1, {"s0"}));
  section.lines.push_back(ref_line(f.poly, {"11", "nosuchstyle", "12"}));
  diag::CollectingSink sink;
  const auto map = display::resolve_type_styles(*f.tech, section, set.table, sink);
  CHECK(map.of(f.metal1) == std::vector<std::size_t>{52});
  CHECK(map.of(f.poly) == std::vector<std::size_t>{53, 54});
  const auto warnings = sink.snapshot();
  REQUIRE(warnings.size() == 1);
  CHECK(warnings[0].code == display::codes::kStyleInvalid);
  CHECK(warnings[0].text == "Invalid style \"nosuchstyle\" for tile type polysilicon");
  CHECK(map.types_of(53).test(f.poly));
}

TEST_CASE("contact styles propagate to stacked types on the same home plane",
          "[cap:tech-styles-section][req:Contact style propagation]") {
  Fixture f;
  const tech::TileType stacked = f.stacked();
  REQUIRE(stacked != tech::kSpace);
  const auto set = style_set(3);
  tech::StylesSection section;
  section.lines.push_back(ref_line(f.m2c, {"10"}));
  section.lines.push_back(ref_line(f.pc, {"11"}));
  diag::CollectingSink sink;
  const auto map = display::resolve_type_styles(*f.tech, section, set.table, sink);
  const auto home = f.tech->home_plane(stacked);
  const auto& styles = map.of(stacked);
  if (home == f.tech->home_plane(f.m2c)) {
    CHECK(styles == std::vector<std::size_t>{52});
  } else {
    CHECK(styles == std::vector<std::size_t>{53});
  }
}

TEST_CASE("the box is drawn by its size on screen and its overlap with the edit cell",
          "[cap:ui-windows-and-views][req:Box display]") {
  const std::optional<db::Rect> edit = db::Rect{0, 0, 100, 100};
  CHECK(display::box_drawing({200, 200, 300, 300}, 50, 50, edit).style == display::kStyleMediumHighlights);
  CHECK(display::box_drawing({10, 10, 20, 20}, 50, 50, edit).style == display::kStyleBox);
  CHECK(display::box_drawing({10, 10, 20, 20}, 50, 50, std::nullopt).style == display::kStyleBox);
  CHECK(display::box_drawing({0, 0, 0, 0}, 0, 0, edit).shape == display::BoxShape::point);
  const auto thin = display::box_drawing({0, 0, 1, 10}, 3, 30, edit);
  CHECK(thin.shape == display::BoxShape::outline);
  CHECK(thin.style == display::kStyleOutlineHighlights);
  CHECK(display::box_drawing({0, 0, 0, 10}, 0, 30, edit).shape == display::BoxShape::thick);
}
