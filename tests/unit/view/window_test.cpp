// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Window model and view arithmetic (capability ui-windows-and-views).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/view/diag_codes.hpp"
#include "hefesto/view/window.hpp"

namespace view = hefesto::view;
namespace db = hefesto::db;

namespace {
struct CountingObserver final : view::ViewObserver {
  int calls = 0;
  void on_window_changed(view::WindowId /*id*/, view::Change /*change*/) override { ++calls; }
};
}  // namespace

TEST_CASE("window ids reuse the smallest free id", "[cap:ui-windows-and-views][req:Window identity and limits]") {
  view::WindowManager windows;
  CountingObserver observer;
  windows.add_observer(observer);
  for (int i = 0; i < 3; ++i) {
    REQUIRE(windows.open(nullptr, {0, 0, 499, 399}).ok());
  }
  REQUIRE(windows.close(1).ok());
  const auto reopened = windows.open(nullptr, {0, 0, 499, 399});
  REQUIRE(reopened.ok());
  CHECK(reopened.value()->id() == 1);
  CHECK(windows.current() == reopened.value());
  CHECK(observer.calls == 5);
  CHECK(windows.close(7).error().code == view::codes::kWindowUnknown);
  windows.remove_observer(observer);
}

TEST_CASE("at most 31 windows are open", "[cap:ui-windows-and-views][req:Window identity and limits]") {
  view::WindowManager windows;
  for (std::size_t i = 0; i < view::kMaxWindows; ++i) {
    REQUIRE(windows.open(nullptr, {0, 0, 99, 99}).ok());
  }
  const auto extra = windows.open(nullptr, {0, 0, 99, 99});
  REQUIRE_FALSE(extra.ok());
  CHECK(extra.error().code == view::codes::kWindowLimit);
  CHECK(extra.error().text == "Can't have more than 31 windows.");
  CHECK(windows.size() == view::kMaxWindows);
}

TEST_CASE("new windows go to the front; over and underneath restack",
          "[cap:ui-windows-and-views][req:specialopen, over, underneath and grow]") {
  view::WindowManager windows;
  REQUIRE(windows.open(nullptr, {0, 0, 9, 9}).ok());
  REQUIRE(windows.open(nullptr, {0, 0, 9, 9}).ok());
  std::string order;
  auto record = [&order](view::LayoutWindow& w) { order += std::to_string(w.id()); };
  windows.for_each(record);
  CHECK(order == "10");
  windows.lower(1);
  order.clear();
  windows.for_each(record);
  CHECK(order == "01");
  windows.raise(1);
  order.clear();
  windows.for_each(record);
  CHECK(order == "10");
}

TEST_CASE("view mapping integer arithmetic", "[cap:ui-windows-and-views][req:View mapping]") {
  const auto m = view::ViewMapping::fit({0, 0, 499, 399}, {-10, -10, 110, 60});
  CHECK(m.scale == 270809);
  CHECK(m.surface == db::Rect{-11, -24, 112, 75});
  CHECK_FALSE(m.at_minimum);
  // A layout point maps to a pixel and back to itself at this scale.
  for (const db::Coord x : {-11, 0, 50, 100}) {
    CHECK(m.to_layout(m.screen_x(x), m.screen_y(0)).x == x);
  }
  // A huge area clamps the scale to 1.
  const auto tiny = view::ViewMapping::fit({0, 0, 9, 9}, {-2000000000, -2000000000, 2000000000, 2000000000});
  CHECK(tiny.at_minimum);
  CHECK(tiny.scale == 1);
}

TEST_CASE("windows framed by Magic's chrome", "[cap:ui-windows-and-views][req:Window chrome flags]") {
  CHECK(view::inner_area(view::kNullScreen, {}) == view::ScreenRect{11, 11, 507, 479});
  CHECK(view::inner_area(view::kNullScreen, {false, false, false}) == view::ScreenRect{0, 0, 511, 483});
  view::WindowManager windows;
  windows.chrome_defaults().caption = false;
  auto w = windows.open_frame(nullptr);
  REQUIRE(w.ok());
  CHECK_FALSE(w.value()->chrome().caption);
  CHECK(w.value()->mapping().screen == view::ScreenRect{11, 11, 507, 479});
}

TEST_CASE("loading a cell frames its bounding box", "[cap:ui-windows-and-views][req:Loading a cell into a window]") {
  CHECK(view::load_frame({0, 0, 100, 50}) == db::Rect{-10, -10, 110, 60});
  db::CellDef def("inv", 8);
  def.set_bboxes({0, 0, 100, 50}, {0, 0, 100, 50});
  view::LayoutWindow window(0, nullptr);
  window.set_mapping(view::ViewMapping::fit({0, 0, 499, 399}, {0, 0, 1, 1}));
  window.set_root(&def);
  CHECK(window.mapping().surface == db::Rect{-11, -24, 112, 75});
  CHECK(window.is_expanded(*window.root_use()));  // the root is always expanded
  def.flags().not_found = true;
  window.set_root(&def);
  CHECK(window.mapping().surface == view::ViewMapping::fit({0, 0, 499, 399}, view::load_frame({0, 0, 1, 1})).surface);
}

TEST_CASE("view and zoom framing", "[cap:ui-windows-and-views][req:view, xview and zoom]") {
  CHECK(view::view_frame({0, 0, 100, 50}) == db::Rect{-10, -5, 111, 55});
  CHECK(view::zoom_area({-11, -24, 112, 75}, 2.0F) == db::Rect{-72, -73, 174, 125});
}

TEST_CASE("findbox and openwindow margins", "[cap:ui-windows-and-views][req:center and findbox]") {
  CHECK(view::box_frame({0, 0, 10, 10}) == db::Rect{-2, -2, 12, 12});
  CHECK(view::box_frame({0, 0, 100, 40}) == db::Rect{-5, -2, 105, 42});
}

TEST_CASE("window captions", "[cap:ui-windows-and-views][req:Window captions]") {
  CHECK(view::window_caption("top", "inv", true) == "top EDITING inv");
  CHECK(view::window_caption("nand2", "", false) == "nand2 [NOT BEING EDITED]");
  const std::string long_name(200, 'x');
  const std::string shortened = view::window_caption(long_name + "y", "", false);
  CHECK(shortened == "..." + std::string(171, 'x') + "y [NOT BEING EDITED]");
  const std::string editing = view::window_caption(long_name, long_name, true);
  CHECK(editing.size() == 90 + 9 + 90);
  CHECK(editing.starts_with("..."));
}

TEST_CASE("scrolling by screen pixels moves the surface", "[cap:ui-windows-and-views][req:scroll]") {
  const auto m = view::ViewMapping::fit({0, 0, 499, 399}, {-10, -10, 110, 60});
  const auto moved = m.scrolled({0, 0}, {-49, 0});  // `scroll r .1 w`: 49.9 -> 49 pixels
  const double units = 49.0 * 65536.0 / static_cast<double>(m.scale);
  CHECK(moved.surface.x0 - m.surface.x0 >= static_cast<db::Coord>(units) - 1);
  CHECK(moved.surface.x0 - m.surface.x0 <= static_cast<db::Coord>(units) + 1);
  CHECK(moved.scale == m.scale);
  const auto shifted = m.scrolled({10, 0}, {0, 0});
  CHECK(shifted.surface.x0 == m.surface.x0 + 10);
}

TEST_CASE("expansion is per window", "[cap:ui-windows-and-views][req:Per-window expansion state]") {
  db::CellDef top("top", 8);
  db::CellUse use;
  view::LayoutWindow a(0, &top);
  view::LayoutWindow b(1, &top);
  a.set_expanded(use, true);
  CHECK(a.is_expanded(use));
  CHECK_FALSE(b.is_expanded(use));
}

TEST_CASE("the crosshair follows the root", "[cap:ui-windows-and-views][req:Crosshair]") {
  db::CellDef top("top", 8);
  db::CellDef inv("inv", 8);
  view::WindowManager windows;
  auto* w0 = windows.open(&top, {0, 0, 99, 99}).value();
  auto* w1 = windows.open(&top, {0, 0, 99, 99}).value();
  auto* w2 = windows.open(&inv, {0, 0, 99, 99}).value();
  windows.set_crosshair(&top, db::Point{100, 100});
  CHECK(w0->crosshair() == db::Point{100, 100});
  CHECK(w1->crosshair() == db::Point{100, 100});
  CHECK_FALSE(w2->crosshair().has_value());
  windows.set_crosshair(nullptr, std::nullopt);
  CHECK_FALSE(w0->crosshair().has_value());
}
