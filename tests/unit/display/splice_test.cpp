// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Partial scene rebuilds and the label display search (capabilities
// ui-windows-and-views "Change-driven redisplay", font-labels "Font labels
// in bounding boxes and searches").

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/display/scene.hpp"
#include "hefesto/tech/technology.hpp"

namespace display = hefesto::display;
namespace db = hefesto::db;
namespace tech = hefesto::tech;

namespace {

struct Fixture {
  std::shared_ptr<const tech::Technology> tech;
  tech::PlaneId m1 = 0;
  tech::PlaneId m2 = 0;
  tech::TileType metal1 = 0;
  tech::TileType metal2 = 0;
  display::StyleSet styles;
  display::FontRegistry fonts;

  explicit Fixture(display::FillKind metal2_fill = display::FillKind::solid) {
    tech::TechnologyBuilder b;
    b.set_name("splice");
    m1 = b.add_plane("metal1").value();
    m2 = b.add_plane("metal2").value();
    metal1 = b.add_type("metal1", "metal1,m1").value();
    metal2 = b.add_type("metal2", "metal2,m2").value();
    tech = b.finalize().value();
    styles.colors.entries.resize(256);
    styles.table.layout_count = 2;
    styles.table.styles.resize(display::kFixedStyleCount + 4);
    for (std::size_t k = 0; k < 2; ++k) {
      auto& s = styles.table.styles[display::kFixedStyleCount + k];
      s.color = static_cast<std::int32_t>(k + 1);
      s.mask = 127;
      s.outline = 0xff;  // outlines drawn: their cut is checked too
      s.fill = k == 1 ? metal2_fill : display::FillKind::solid;
      styles.table.styles[display::kFixedStyleCount + 2 + k] = s;
    }
    styles.types.styles_of_type.resize(tech->type_count());
    styles.types.styles_of_type[metal1].push_back(display::kFixedStyleCount);
    styles.types.styles_of_type[metal2].push_back(display::kFixedStyleCount + 1);
  }

  void paint(db::CellDef& def, tech::PlaneId plane, const db::Rect& r, tech::TileType type) const {
    db::PaintRow row{};
    row.fill(type);
    REQUIRE(def.plane(plane).paint(r, row).ok());
  }

  [[nodiscard]] display::LayoutScene build(const db::CellDef& root, const db::Rect& area = db::whole_plane()) const {
    display::SceneOptions options;
    options.area = area;
    auto built = display::build_scene(root, *tech, styles, fonts, options);
    REQUIRE(built.ok());
    return std::move(built).value();
  }
};

/// What a scene draws, independent of how rectangles and segments are cut:
/// covered unit cells and unit outline pieces per layer, triangles, labels
/// and instance boxes.
struct Drawn {
  std::set<std::tuple<std::size_t, db::Coord, db::Coord>> cells;
  std::set<std::tuple<std::size_t, db::Coord, db::Coord, bool>> edges;
  std::multiset<std::tuple<std::size_t, db::Coord, db::Coord, db::Coord, db::Coord>> triangles;
  std::multiset<std::tuple<std::string, db::Coord, db::Coord, db::Coord, db::Coord>> labels;
  std::multiset<std::tuple<std::string, db::Coord, db::Coord, db::Coord, db::Coord>> instances;
  db::Rect bounds;
  friend bool operator==(const Drawn&, const Drawn&) = default;
};

Drawn drawn(const display::LayoutScene& scene) {
  Drawn d;
  for (const display::SceneLayer& l : scene.layers) {
    for (const db::Rect& r : l.rects) {
      for (db::Coord x = r.x0; x < r.x1; ++x) {
        for (db::Coord y = r.y0; y < r.y1; ++y) {
          d.cells.emplace(l.order, x, y);
        }
      }
    }
    for (const display::SceneSegment& s : l.outline) {
      const bool horizontal = s.a.y == s.b.y;
      const db::Coord lo = horizontal ? std::min(s.a.x, s.b.x) : std::min(s.a.y, s.b.y);
      const db::Coord hi = horizontal ? std::max(s.a.x, s.b.x) : std::max(s.a.y, s.b.y);
      for (db::Coord v = lo; v < hi; ++v) {
        d.edges.emplace(l.order, horizontal ? v : s.a.x, horizontal ? s.a.y : v, horizontal);
      }
    }
    for (const display::SceneTriangle& t : l.triangles) {
      d.triangles.emplace(l.order, t.bounds.x0, t.bounds.y0, t.bounds.x1, t.bounds.y1);
    }
  }
  for (const display::SceneLabel& l : scene.labels) {
    d.labels.emplace(l.text, l.rect.x0, l.rect.y0, l.rect.x1, l.rect.y1);
  }
  for (const display::SceneCell& c : scene.cells) {
    d.instances.emplace(c.id, c.bbox.x0, c.bbox.y0, c.bbox.x1, c.bbox.y1);
  }
  d.bounds = scene.bounds;
  return d;
}

db::Label label(std::string text, const db::Rect& r, tech::TileType type) {
  db::Label l;
  l.text = std::move(text);
  l.rect = r;
  l.type = type;
  return l;
}

}  // namespace

#define SPLICE "[cap:ui-windows-and-views][req:Change-driven redisplay]"

TEST_CASE("a spliced partial rebuild draws what a full rebuild draws", SPLICE) {
  const Fixture f;
  db::CellDef top("top", f.tech->plane_count());
  db::CellDef child("child", f.tech->plane_count());
  f.paint(top, f.m1, {0, 0, 20, 10}, f.metal1);
  f.paint(top, f.m1, {30, 0, 40, 10}, f.metal1);
  f.paint(top, f.m2, {0, 5, 40, 8}, f.metal2);
  top.labels().push_back(label("far", {38, 2, 38, 2}, f.metal1));
  top.labels().push_back(label("edge", {15, 0, 15, 0}, f.metal1));  // touches the edited area
  auto use = std::make_unique<db::CellUse>();
  use->id = "u1";
  use->def = &child;
  use->parent = &top;
  use->bbox = {16, 12, 22, 18};
  use->extended = use->bbox;
  top.uses().push_back(std::move(use));
  display::LayoutScene retained = f.build(top);

  // Bridge the two metal1 boxes, erase a hole in metal2, add a label.
  const db::Rect area{15, 0, 35, 20};
  f.paint(top, f.m1, {15, 0, 35, 10}, f.metal1);
  f.paint(top, f.m2, {20, 5, 25, 8}, tech::kSpace);
  top.labels().push_back(label("new", {25, 5, 25, 5}, f.metal1));

  REQUIRE(display::splice_scene(retained, f.build(top, area), area));
  CHECK(drawn(retained) == drawn(f.build(top)));
}

TEST_CASE("a cut through a cross fill asks for a full rebuild", SPLICE) {
  const Fixture f(display::FillKind::cross);
  db::CellDef top("top", f.tech->plane_count());
  f.paint(top, f.m2, {0, 0, 40, 10}, f.metal2);
  display::LayoutScene retained = f.build(top);
  const Drawn before = drawn(retained);
  const db::Rect area{10, 0, 20, 10};
  CHECK_FALSE(display::splice_scene(retained, f.build(top, area), area));
  CHECK(drawn(retained) == before);  // unchanged
}

#define DISPLAY_SEARCH "[cap:font-labels][req:Font labels in bounding boxes and searches]"

TEST_CASE("the display search finds a font label by its rendered text box", DISPLAY_SEARCH) {
  display::SceneLabel plain;
  plain.rect = {0, 0, 0, 0};
  display::SceneLabel font = plain;
  font.font_label = true;
  font.font_bbox = {-20, -6, 24, 6};
  const db::Rect area{20, 4, 30, 10};
  CHECK_FALSE(display::label_in_display_area(plain, area));
  CHECK(display::label_in_display_area(font, area));
  CHECK(display::label_in_display_area(plain, {0, 0, 5, 5}));  // the attachment rectangle still counts

  // The database search: attachment only, unless for display.
  db::CellDef top("top", 4);
  db::Label l;
  l.text = "big";
  l.rect = {0, 0, 0, 0};
  l.font = 0;
  l.text_bbox = db::Rect{-20, -6, 24, 6};
  top.labels().push_back(l);
  int attach = 0;
  int shown = 0;
  db::search_labels(top, area, [&attach](const db::LabelHit&) { return ++attach, true; });
  db::search_labels(top, area, [&shown](const db::LabelHit&) { return ++shown, true; }, db::Descend::all,
                    db::LabelMatch::display);
  CHECK(attach == 0);
  CHECK(shown == 1);
}

TEST_CASE("a partial scene includes labels only by the display search", DISPLAY_SEARCH) {
  const Fixture f;
  db::CellDef top("top", f.tech->plane_count());
  f.paint(top, f.m1, {0, 0, 40, 10}, f.metal1);
  top.labels().push_back(label("in", {5, 5, 5, 5}, f.metal1));
  top.labels().push_back(label("out", {35, 5, 35, 5}, f.metal1));
  const display::LayoutScene partial = f.build(top, {0, 0, 10, 10});
  REQUIRE(partial.labels.size() == 1);
  CHECK(partial.labels[0].text == "in");
  CHECK(f.build(top).labels.size() == 2);
}
