// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The highlight layer and its built-in clients (capability
// ui-feedback-and-highlights "Highlight layer and clients"; cmd-selection
// "Selection model" display scenarios; ui-wizard-and-debug "Tile structure
// overlay" full-redraw rule).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "hefesto/db/plane.hpp"
#include "hefesto/view/highlights.hpp"
#include "view_session.hpp"

namespace db = hefesto::db;
namespace view = hefesto::view;
namespace display = hefesto::display;
namespace tech = hefesto::tech;
using hefesto::test::ViewSession;

namespace {

constexpr tech::TileType kMetal = 9;  // any type id: the selection cell needs no technology

db::PaintRow paint_with(tech::TileType type) {
  db::PaintRow row{};
  row.fill(type);
  return row;
}

struct HighlightSession : ViewSession {
  db::CellDef* top = nullptr;
  view::LayoutWindow* win = nullptr;

  HighlightSession() {
    top = cell("top", {0, 0, 100, 100});
    win = &window(top);
    editor.edit_cell = top;
  }

  /// Selects a rectangle of paint (in the selection cell, root coordinates).
  void select_paint(const db::Rect& r, tech::PlaneId plane = 1) {
    editor.selection.set_root(top, editor.technology->plane_count());
    REQUIRE(editor.selection.def()->plane(plane).paint(r, paint_with(kMetal)).ok());
  }

  display::HighlightScene build() { return editor.highlights.build(editor, *win); }
};

struct Recorder final : view::ViewObserver {
  std::vector<std::pair<view::WindowId, view::Change>> seen;
  void on_window_changed(view::WindowId id, view::Change change) override { seen.emplace_back(id, change); }
};

std::size_t first_index(const display::HighlightScene& s, std::size_t style) {
  for (std::size_t i = 0; i < s.items.size(); ++i) {
    if (s.items[i].style == style) return i;
  }
  return s.items.size();
}

}  // namespace

TEST_CASE("highlight clients run in Magic's registration order, at most 10",
          "[cap:ui-feedback-and-highlights][req:Highlight layer and clients]") {
  view::HighlightLayer layer;
  CHECK(layer.client_names() == std::vector<std::string>{"box", "feedback", "elements", "crosshair"});
  for (int i = 0; i < 6; ++i) {
    CHECK(layer.add_client("extra" + std::to_string(i), [](const view::HighlightContext&, display::HighlightScene&) {}));
  }
  CHECK(layer.client_count() == view::kMaxHighlightClients);
  CHECK_FALSE(layer.add_client("eleventh", [](const view::HighlightContext&, display::HighlightScene&) {}));
  view::HighlightLayer other;
  CHECK_FALSE(other.add_client("box", [](const view::HighlightContext&, display::HighlightScene&) {}));  // name taken
}

TEST_CASE("the selection client registers on first display, after the others",
          "[cap:ui-feedback-and-highlights][req:Highlight layer and clients]") {
  HighlightSession s;
  CHECK(s.build().empty());  // no box, nothing selected
  CHECK(s.editor.highlights.client_count() == 4);
  s.select_paint({10, 10, 20, 20});
  (void)s.build();
  CHECK(s.editor.highlights.client_names().back() == "selection");
}

TEST_CASE("the selection is drawn above the box", "[cap:ui-feedback-and-highlights][req:Highlight layer and clients]") {
  // Scenario "Selection drawn above box".
  HighlightSession s;
  s.editor.box = db::Rect{0, 0, 30, 30};
  s.select_paint({10, 10, 40, 40});
  const display::HighlightScene scene = s.build();
  const std::size_t box = first_index(scene, display::kStyleBox);
  const std::size_t selection = first_index(scene, view::kStyleOutlineHighlights);
  REQUIRE(box < scene.items.size());
  REQUIRE(selection < scene.items.size());
  CHECK(box < selection);
}

TEST_CASE("highlight redraw requests apply to every window on the root, widened and grown",
          "[cap:ui-feedback-and-highlights][req:Highlight layer and clients]") {
  HighlightSession s;
  view::LayoutWindow& second = s.window(s.top);
  view::LayoutWindow& elsewhere = s.window(s.cell("other", {0, 0, 10, 10}));
  s.editor.highlights.request(s.top, {10, 10, 10, 20}, true);  // zero width
  REQUIRE(s.editor.highlights.pending(s.top) != nullptr);
  CHECK(s.editor.highlights.pending(s.top)->front() == db::Rect{9, 10, 11, 20});  // widened
  s.editor.highlights.redraw(s.editor);
  CHECK(s.editor.highlights.frame(s.win->id()) != nullptr);
  CHECK(s.editor.highlights.frame(second.id()) != nullptr);
  CHECK(s.editor.highlights.frame(elsewhere.id()) == nullptr);
  CHECK_FALSE(s.editor.highlights.has_pending());

  // Erase: the widened area (9,10)-(11,20) on screen, grown by the cross
  // size (no label extent yet); redraw: grown once more, back to layout
  // units, plus one unit.
  const view::ViewMapping& m = s.win->mapping();
  const view::ScreenRect screen{m.screen_x(9), m.screen_y(10), m.screen_x(11), m.screen_y(20)};
  const view::HighlightAreas a = view::highlight_areas(*s.win, {10, 10, 10, 20}, true);
  REQUIRE(a.erase.has_value());
  const view::ScreenRect grown{screen.x0 - 5, screen.y0 - 5, screen.x1 + 5, screen.y1 + 5};
  CHECK(*a.erase == grown);
  const db::Rect back = m.screen_to_surface({grown.x0 - 5, grown.y0 - 5, grown.x1 + 5, grown.y1 + 5});
  CHECK(a.redraw == db::Rect{back.x0 - 1, back.y0 - 1, back.x1 + 1, back.y1 + 1});
  CHECK(s.editor.highlights.frame(s.win->id())->erase.front() == grown);

  // Without erase nothing is erased and the redraw area grows once.
  const view::HighlightAreas plain = view::highlight_areas(*s.win, {50, 50, 60, 60}, false);
  CHECK_FALSE(plain.erase.has_value());
  const view::ScreenRect p{m.screen_x(50), m.screen_y(50), m.screen_x(60), m.screen_y(60)};
  const db::Rect pb = m.screen_to_surface({p.x0 - 5, p.y0 - 5, p.x1 + 5, p.y1 + 5});
  CHECK(plain.redraw == db::Rect{pb.x0 - 1, pb.y0 - 1, pb.x1 + 1, pb.y1 + 1});
}

TEST_CASE("the label extent grows highlight areas", "[cap:ui-feedback-and-highlights][req:Highlight layer and clients]") {
  HighlightSession s;
  s.win->include_label_extent(view::ScreenRect{-30, -2, 40, 3});
  const view::ViewMapping& m = s.win->mapping();
  const view::HighlightAreas a = view::highlight_areas(*s.win, {10, 10, 20, 20}, true);
  REQUIRE(a.erase.has_value());
  CHECK(*a.erase == view::ScreenRect{m.screen_x(10) - 30, m.screen_y(10) - 5, m.screen_x(20) + 40, m.screen_y(20) + 5});
}

TEST_CASE("the end-of-command pass runs the clients and notifies the window",
          "[cap:ui-feedback-and-highlights][req:Highlight layer and clients]") {
  HighlightSession s;
  Recorder rec;
  s.editor.windows.add_observer(rec);
  s.editor.box = db::Rect{10, 10, 20, 20};
  s.editor.highlights.request(s.top, {10, 10, 20, 20}, true);
  s.editor.highlights.redraw(s.editor);
  CHECK(s.editor.highlights.pending(s.top) == nullptr);
  REQUIRE(s.editor.highlights.frame(s.win->id()) != nullptr);
  const view::HighlightFrame frame = s.editor.highlights.take_frame(s.win->id());
  CHECK(frame.erase.size() == 1);
  CHECK(frame.redraw.size() == 1);
  REQUIRE_FALSE(frame.scene.items.empty());
  CHECK(frame.scene.items.front().style == display::kStyleBox);
  CHECK(frame.scene.items.front().rect == db::Rect{10, 10, 20, 20});
  CHECK(std::count(rec.seen.begin(), rec.seen.end(), std::make_pair(s.win->id(), view::Change::highlights)) == 1);
  s.editor.windows.remove_observer(rec);
}

TEST_CASE("the box client follows the box display rules", "[cap:ui-feedback-and-highlights][req:Highlight layer and clients]") {
  HighlightSession s;
  s.editor.box = db::Rect{5, 5, 5, 5};  // a point
  display::HighlightScene scene = s.build();
  REQUIRE(scene.items.size() == 2);
  CHECK(scene.items[0].shape == display::HighlightShape::cross);
  s.editor.box = db::Rect{102, 102, 110, 110};  // outside the edit cell's bbox: medium highlights
  scene = s.build();
  REQUIRE_FALSE(scene.items.empty());
  CHECK(scene.items.back().style == display::kStyleMediumHighlights);
}

TEST_CASE("crosshair and elements are highlight clients", "[cap:ui-feedback-and-highlights][req:Highlight layer and clients]") {
  HighlightSession s;
  s.editor.windows.set_crosshair(s.top, db::Point{40, 50});
  db::Element el;
  el.kind = db::Element::Kind::line;
  el.name = "e1";
  el.area = db::Rect{0, 0, 30, 30};
  s.top->elements().push_back(el);
  const display::HighlightScene scene = s.build();
  REQUIRE(scene.items.size() == 3);
  CHECK(scene.items[0].shape == display::HighlightShape::line);  // the element (elements before crosshair)
  CHECK(scene.items[0].line == display::SceneSegment{{0, 0}, {30, 30}});
  CHECK(scene.items[1].style == display::kStyleCrosshair);
  CHECK(scene.items[1].line.a.y == 50);
  CHECK(scene.items[2].line.a.x == 40);
}

TEST_CASE("the selection is drawn in white solid outlines in windows on its root",
          "[cap:cmd-selection][req:Selection model]") {
  HighlightSession s;
  view::LayoutWindow& elsewhere = s.window(s.cell("other", {0, 0, 10, 10}));
  s.select_paint({10, 10, 20, 20});
  s.select_paint({20, 10, 30, 20});  // abutting, same type: one outline
  const display::HighlightScene scene = s.build();
  std::vector<display::SceneSegment> lines;
  for (const auto& it : scene.items) {
    if (it.shape == display::HighlightShape::line && it.style == view::kStyleOutlineHighlights) {
      lines.push_back(it.line);
      // No display styles: highlights resolve to opaque white, solid outline.
      CHECK(it.resolved.r == 1.0F);
      CHECK(it.resolved.g == 1.0F);
      CHECK(it.resolved.b == 1.0F);
      CHECK(it.resolved.outline == 0xFF);
    }
  }
  REQUIRE_FALSE(lines.empty());
  const auto has = [&](display::SceneSegment seg) {
    return std::find(lines.begin(), lines.end(), seg) != lines.end();
  };
  CHECK(has({{10, 10}, {10, 20}}));
  CHECK(has({{30, 10}, {30, 20}}));
  CHECK_FALSE(has({{20, 10}, {20, 20}}));  // internal edge between equal material
  // Not shown in a window on another root.
  CHECK(s.editor.highlights.build(s.editor, elsewhere).empty());
}

TEST_CASE("a floating selection is drawn at its offset", "[cap:cmd-selection][req:Selection model]") {
  HighlightSession s;
  s.select_paint({10, 10, 20, 20});
  s.editor.selection.set_floating_offset(db::Point{5, 0});
  const display::HighlightScene scene = s.build();
  bool moved = false;
  for (const auto& it : scene.items) {
    moved = moved || (it.shape == display::HighlightShape::line && it.line == display::SceneSegment{{15, 10}, {15, 20}});
  }
  CHECK(moved);
}

TEST_CASE("selected labels and cells are outlined with their names", "[cap:cmd-selection][req:Selection model]") {
  HighlightSession s;
  s.editor.selection.set_root(s.top, s.editor.technology->plane_count());
  db::Label label;
  label.text = "VDD";
  label.rect = db::Rect{5, 5, 5, 5};
  s.editor.selection.def()->labels().push_back(label);
  const display::HighlightScene scene = s.build();
  REQUIRE(scene.items.size() == 2);
  CHECK(scene.items[0].shape == display::HighlightShape::cross);
  CHECK(scene.items[1].shape == display::HighlightShape::text);
  CHECK(scene.items[1].text == "VDD");
}

TEST_CASE("a watched window is redrawn whole after any change",
          "[cap:ui-wizard-and-debug][req:Tile structure overlay]") {
  HighlightSession s;
  view::LayoutWindow& plain = s.window(s.top);
  s.editor.tile_overlays[s.win->id()] = view::TileOverlay{1, false, false, s.top, db::kIdentity};
  s.editor.windows.area_changed(*s.top, {10, 10, 20, 20}, std::nullopt);
  s.editor.highlights.redraw(s.editor);
  CHECK(s.win->pending_redisplay().all);
  CHECK_FALSE(plain.pending_redisplay().all);
}
