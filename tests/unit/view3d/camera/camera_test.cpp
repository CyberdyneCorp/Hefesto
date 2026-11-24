// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// view3d/camera: ViewState, default_view, rescale, the viewing matrix,
// gestures, presets and the Q-MISC-78 rescale rule
// (openspec/changes/m10-3d-view tasks.md 4.1-4.4, design D7).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>

#include "hefesto/view3d/camera/input.hpp"
#include "hefesto/view3d/camera/view_state.hpp"

namespace view3d = hefesto::view3d;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using hefesto::db::Rect;

namespace {

// Magic's `2.0 / ((float)extent * 1.1)` rounded to float.
float magic_fit(float extent) {
  return static_cast<float>(2.0 / (static_cast<double>(extent) * 1.1));
}

struct Projected {
  float x = 0.0F;  // window pixels, origin lower left
  float y = 0.0F;
  float depth = 0.0F;
};

Projected project(const view3d::Matrix4& m, std::array<float, 3> v, int width, int height) {
  std::array<float, 4> out{};
  for (std::size_t r = 0; r < 4; ++r) {
    out[r] = (m[r] * v[0]) + (m[4 + r] * v[1]) + (m[8 + r] * v[2]) + m[12 + r];
  }
  const float w = out[3];
  return Projected{(out[0] / w + 1.0F) * 0.5F * static_cast<float>(width),
                   (out[1] / w + 1.0F) * 0.5F * static_cast<float>(height), out[2] / w};
}

bool all_finite(const view3d::Matrix4& m) {
  for (const float e : m) {
    if (!std::isfinite(e)) {
      return false;
    }
  }
  return true;
}

view3d::Gesture gesture(view3d::Gesture::Kind kind, float dx = 0.0F, float dy = 0.0F) {
  view3d::Gesture g;
  g.kind = kind;
  g.dx = dx;
  g.dy = dy;
  return g;
}

}  // namespace

// --- 4.1 default_view and rescale --------------------------------------

TEST_CASE("default view of a 100 x 50 cell in layout-layer mode", "[cap:view-3d][req:Default view]") {
  const auto v = view3d::default_view(Rect{0, 0, 100, 50}, view3d::Mode::layout_layer, 10.0F);
  CHECK(v.trans_x == -50.0F);
  CHECK(v.trans_y == -25.0F);
  CHECK(v.trans_z == 0.0F);
  CHECK(v.scale_xy == magic_fit(100.0F));
  CHECK_THAT(static_cast<double>(v.scale_xy), WithinRel(static_cast<double>(0.0181818F), 1e-5));
  CHECK(v.scale_z == 25.0F);
  CHECK(v.prescale_z == static_cast<float>(0.0001));
  CHECK(v.view_x == 0.0F);
  CHECK(v.view_y == 0.0F);
  CHECK(v.view_z == 0.0F);
}

TEST_CASE("same cell in mask-layer mode with scalefactor 10", "[cap:view-3d][req:Default view]") {
  const auto v = view3d::default_view(Rect{0, 0, 100, 50}, view3d::Mode::mask_layer, 10.0F);
  CHECK(v.trans_x == -500.0F);
  CHECK(v.trans_y == -250.0F);
  CHECK(v.scale_xy == magic_fit(100.0F) / 10.0F);
  CHECK_THAT(static_cast<double>(v.scale_xy), WithinRel(static_cast<double>(0.00181818F), 1e-5));
  CHECK(v.scale_z == 250.0F);
  CHECK(v.prescale_z == static_cast<float>(0.0001) / 10.0F);
  CHECK_THAT(static_cast<double>(v.prescale_z), WithinRel(static_cast<double>(0.00001F), 1e-5));
}

TEST_CASE("default view halves odd extents by arithmetic shift", "[cap:view-3d][req:Default view]") {
  const auto odd = view3d::default_view(Rect{0, 0, 101, 51}, view3d::Mode::layout_layer, 1.0F);
  CHECK(odd.trans_x == -50.0F);  // -(0 + (101 >> 1))
  CHECK(odd.trans_y == -25.0F);
  CHECK(odd.scale_xy == magic_fit(101.0F));

  // Negative origin: -(x0 + (W >> 1)) = -(-101 + 50) = 51, not the centre 50.5.
  const auto neg = view3d::default_view(Rect{-101, -51, 0, 0}, view3d::Mode::layout_layer, 1.0F);
  CHECK(neg.trans_x == 51.0F);
  CHECK(neg.trans_y == 26.0F);

  // The limiting extent picks the scale.
  const auto tall = view3d::default_view(Rect{-10, -300, 10, 100}, view3d::Mode::layout_layer, 1.0F);
  CHECK(tall.scale_xy == magic_fit(400.0F));
  CHECK(tall.trans_y == 100.0F);  // -(-300 + 200)
}

TEST_CASE("default view of an empty cell stays finite", "[cap:view-3d][req:Default view]") {
  // Regression: Magic divides by a zero extent (an infinite scale).
  const auto point = view3d::default_view(Rect{5, 5, 5, 5}, view3d::Mode::layout_layer, 1.0F);
  CHECK(std::isfinite(point.scale_xy));
  CHECK(point.scale_xy == magic_fit(1.0F));
  CHECK(point.trans_x == -5.0F);
  const auto line = view3d::default_view(Rect{0, 0, 200, 0}, view3d::Mode::layout_layer, 1.0F);
  CHECK(line.scale_xy == magic_fit(200.0F));
}

TEST_CASE("rescale is w3dRescale and ignores a non-positive factor", "[cap:view-3d][req:Default view]") {
  auto v = view3d::default_view(Rect{0, 0, 100, 50}, view3d::Mode::layout_layer, 1.0F);
  const auto before = v;
  view3d::rescale(v, 4.0F);
  CHECK(v.scale_xy == before.scale_xy / 4.0F);
  CHECK(v.prescale_z == before.prescale_z / 4.0F);
  CHECK(v.scale_z == 100.0F);
  CHECK(v.trans_x == -200.0F);
  CHECK(v.trans_y == -100.0F);
  CHECK(v.trans_z == 0.0F);

  // Regression: a zero, negative or NaN factor would divide by zero.
  const auto kept = v;
  view3d::rescale(v, 0.0F);
  view3d::rescale(v, -2.0F);
  view3d::rescale(v, std::nanf(""));
  CHECK(v == kept);

  // Magic's 1.0 / (float)f is a double division rounded to float.
  CHECK(view3d::inverse_factor(3.0F) == static_cast<float>(1.0 / 3.0));
}

// --- 4.2 viewing matrix --------------------------------------------------

TEST_CASE("top-down default shows the layout", "[cap:view-3d][req:Viewing transform and shading]") {
  const auto v = view3d::default_view(Rect{0, 0, 100, 100}, view3d::Mode::layout_layer, 1.0F);
  const std::array<std::array<int, 2>, 3> sizes{{{500, 500}, {800, 500}, {500, 800}}};
  for (const auto& [w, h] : sizes) {
    INFO(w << " x " << h);
    const auto m = view3d::matrix(v, w, h, {});
    const auto ll = project(m, {0.0F, 0.0F, 0.0F}, w, h);
    const auto ur = project(m, {100.0F, 100.0F, 0.0F}, w, h);
    const float limiting = static_cast<float>(std::min(w, h));
    // About 91% (1/1.1) of the limiting dimension, square on screen, centred.
    CHECK_THAT(static_cast<double>(ur.x - ll.x), WithinRel(static_cast<double>(limiting / 1.1F), 1e-4));
    CHECK_THAT(static_cast<double>(ur.y - ll.y), WithinRel(static_cast<double>(limiting / 1.1F), 1e-4));
    CHECK_THAT(static_cast<double>((ll.x + ur.x) * 0.5F), WithinAbs(static_cast<double>(w) / 2.0, 1e-2));
    CHECK_THAT(static_cast<double>((ll.y + ur.y) * 0.5F), WithinAbs(static_cast<double>(h) / 2.0, 1e-2));
  }
}

TEST_CASE("the aspect correction is Magic's in a wide window", "[cap:view-3d][req:Viewing transform and shading]") {
  view3d::ViewState v;  // identity S, angles 0, no translation
  const auto m = view3d::matrix(v, 800, 500, {});
  CHECK(m[0] == 500.0F / 800.0F);  // A = diag(H / W, 1, 1)
  CHECK(m[5] == 1.0F);
  CHECK(m[10] == 1.0F);
  // A square or degenerate drawable needs no correction (and never divides by zero).
  CHECK(view3d::matrix(v, 500, 500, {})[0] == 1.0F);
  CHECK(all_finite(view3d::matrix(v, 0, 0, {})));
  CHECK(all_finite(view3d::matrix(v, 300, -1, {})));
}

TEST_CASE("the matrix applies S, the rotations and T in Magic's order",
          "[cap:view-3d][req:Viewing transform and shading]") {
  view3d::ViewState v;
  v.trans_x = 10.0F;
  v.scale_xy = 0.01F;
  v.prescale_z = 0.0001F;
  // Front preset: model heights (z) point up on the screen.
  const auto front = view3d::preset(view3d::Preset::front);
  v.view_x = front.x;
  v.view_y = front.y;
  v.view_z = front.z;
  const auto m = view3d::matrix(v, 500, 500, {});
  const auto base = project(m, {-10.0F, 0.0F, 0.0F}, 500, 500);
  const auto high = project(m, {-10.0F, 0.0F, 1000.0F}, 500, 500);
  CHECK_THAT(static_cast<double>(base.x), WithinAbs(250.0, 1e-3));  // T moves x = -10 to the centre
  CHECK_THAT(static_cast<double>(base.y), WithinAbs(250.0, 1e-3));
  CHECK(high.y > base.y);
  // S comes after the rotations: a rotated height is scaled by s_xy, not p_z.
  CHECK_THAT(static_cast<double>(high.y - base.y), WithinRel(static_cast<double>(1000.0F * 0.01F * 250.0F), 1e-4));

  // Top-down: a point higher in z is nearer to the viewer (larger z').
  view3d::ViewState top;
  top.prescale_z = 0.0001F;
  const auto mt = view3d::matrix(top, 500, 500, {});
  CHECK(project(mt, {0.0F, 0.0F, 100.0F}, 500, 500).depth > project(mt, {0.0F, 0.0F, 0.0F}, 500, 500).depth);
}

TEST_CASE("the perspective projection frames the z = 0 plane as the orthographic one",
          "[cap:view-3d][req:Viewing transform and shading]") {
  const auto v = view3d::default_view(Rect{0, 0, 100, 100}, view3d::Mode::layout_layer, 1.0F);
  const view3d::ProjectionParams persp{view3d::Projection::perspective, 45.0F};
  const auto mo = view3d::matrix(v, 800, 500, {});
  const auto mp = view3d::matrix(v, 800, 500, persp);
  for (const auto& p : std::array<std::array<float, 3>, 3>{{{0, 0, 0}, {100, 100, 0}, {30, 70, 0}}}) {
    const auto o = project(mo, p, 800, 500);
    const auto q = project(mp, p, 800, 500);
    CHECK_THAT(static_cast<double>(q.x), WithinAbs(static_cast<double>(o.x), 1e-2));
    CHECK_THAT(static_cast<double>(q.y), WithinAbs(static_cast<double>(o.y), 1e-2));
  }
  // Points nearer the viewer look bigger and are deeper in z'.
  const float z = 4000.0F;  // 0.4 view units with p_z = 0.0001
  const auto near_ll = project(mp, {0.0F, 0.0F, z}, 800, 500);
  const auto near_ur = project(mp, {100.0F, 100.0F, z}, 800, 500);
  const auto far_ll = project(mp, {0.0F, 0.0F, 0.0F}, 800, 500);
  const auto far_ur = project(mp, {100.0F, 100.0F, 0.0F}, 800, 500);
  CHECK(near_ur.x - near_ll.x > far_ur.x - far_ll.x);
  CHECK(near_ll.depth > far_ll.depth);
  // Any field of view gives a finite matrix.
  CHECK(all_finite(view3d::matrix(v, 800, 500, {view3d::Projection::perspective, 0.0F})));
  CHECK(all_finite(view3d::matrix(v, 800, 500, {view3d::Projection::perspective, 179.0F})));
  CHECK(all_finite(view3d::matrix(v, 800, 500, {view3d::Projection::perspective, std::nanf("")})));
}

// --- 4.3 gestures and presets -------------------------------------------

TEST_CASE("gestures change exactly what view, scroll and zoom would",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const auto start = view3d::default_view(Rect{0, 0, 100, 50}, view3d::Mode::layout_layer, 1.0F);

  SECTION("orbit is `view dy 0 dx rel`") {
    auto v = start;
    view3d::apply(v, gesture(view3d::Gesture::Kind::orbit, 10.0F, -20.0F));
    auto expected = start;
    expected.view_x += -20.0F;
    expected.view_z += 10.0F;
    CHECK(v == expected);
  }
  SECTION("pan is `scroll dx dy 0 rel`") {
    auto v = start;
    view3d::apply(v, gesture(view3d::Gesture::Kind::pan, 0.5F, -0.25F));
    auto expected = start;
    expected.trans_x += 0.5F / start.scale_xy;
    expected.trans_y += -0.25F / start.scale_xy;
    CHECK(v == expected);
  }
  SECTION("zoom is `zoom f 1 rel`, or `zoom f f rel` with the modifier") {
    auto v = start;
    view3d::apply(v, view3d::zoom_pinch(2.0F));
    auto expected = start;
    expected.scale_xy *= 2.0F;
    CHECK(v == expected);
    view3d::apply(v, view3d::zoom_pinch(0.5F, true));
    expected.scale_xy *= 0.5F;
    expected.scale_z *= 0.5F;
    CHECK(v == expected);
  }
  SECTION("a non-positive zoom factor changes nothing, as Magic's error") {
    auto v = start;
    view3d::apply(v, view3d::zoom_pinch(0.0F));
    view3d::apply(v, view3d::zoom_pinch(-3.0F, true));
    view3d::apply(v, view3d::zoom_pinch(std::nanf("")));
    CHECK(v == start);
  }
  SECTION("a preset sets the absolute angles and nothing else") {
    auto v = start;
    v.view_x = 12.0F;
    auto g = gesture(view3d::Gesture::Kind::preset);
    g.preset = view3d::Preset::isometric;
    view3d::apply(v, g);
    auto expected = start;
    expected.view_x = -54.7356F;
    expected.view_z = 45.0F;
    CHECK(v == expected);
  }
  SECTION("reset is `defaults` when the caller passes the home view") {
    auto v = start;
    view3d::apply(v, view3d::orbit_drag(40.0F, 40.0F));
    view3d::apply(v, gesture(view3d::Gesture::Kind::reset), start);
    CHECK(v == start);
    auto w = start;
    w.view_x = 5.0F;
    const auto moved = w;
    view3d::apply(w, gesture(view3d::Gesture::Kind::reset));
    CHECK(w == moved);  // without a home view, nothing changes
  }
}

TEST_CASE("presets give the documented angles", "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  CHECK(view3d::preset(view3d::Preset::top) == view3d::Angles{0.0F, 0.0F, 0.0F});
  CHECK(view3d::preset(view3d::Preset::isometric) == view3d::Angles{-54.7356F, 0.0F, 45.0F});
  CHECK(view3d::preset(view3d::Preset::front) == view3d::Angles{-90.0F, 0.0F, 0.0F});
  CHECK(view3d::preset(view3d::Preset::side) == view3d::Angles{-90.0F, 0.0F, -90.0F});
}

TEST_CASE("touch input maps to orbit and zoom gestures for M14",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const auto drag = view3d::touch_one_finger_drag(20.0F, -10.0F);
  CHECK(drag.kind == view3d::Gesture::Kind::orbit);
  CHECK(drag.dx == 20.0F * view3d::kOrbitDegreesPerPixel);
  CHECK(drag.dy == -10.0F * view3d::kOrbitDegreesPerPixel);
  const auto pinch = view3d::touch_pinch(1.5F);
  CHECK(pinch.kind == view3d::Gesture::Kind::zoom);
  CHECK(pinch.factor == 1.5F);
  CHECK_FALSE(pinch.zoom_z);
}

TEST_CASE("a pan drag moves the top-down layout with the pointer",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const std::array<std::array<int, 2>, 2> sizes{{{800, 500}, {500, 800}}};
  for (const auto& [w, h] : sizes) {
    INFO(w << " x " << h);
    auto v = view3d::default_view(Rect{0, 0, 100, 50}, view3d::Mode::layout_layer, 1.0F);
    const auto before = project(view3d::matrix(v, w, h, {}), {10.0F, 10.0F, 0.0F}, w, h);
    view3d::apply(v, view3d::pan_drag(30.0F, 20.0F, w, h));
    const auto after = project(view3d::matrix(v, w, h, {}), {10.0F, 10.0F, 0.0F}, w, h);
    CHECK_THAT(static_cast<double>(after.x - before.x), WithinAbs(30.0, 1e-2));
    CHECK_THAT(static_cast<double>(after.y - before.y), WithinAbs(-20.0, 1e-2));  // screen y grows downward
  }
}

// --- 4.4 Q-MISC-78 ---------------------------------------------------------

namespace {

// Enter mask-layer mode with f = 10, change the output style to f = 5, then
// `cif` back to layout-layer mode.
view3d::ViewState cif_round_trip(bool magic_compat, const view3d::ViewState& layout) {
  auto v = layout;
  float applied = 10.0F;
  view3d::rescale(v, applied);
  applied = view3d::change_output_style(v, magic_compat, applied, 5.0F);
  view3d::rescale(v, view3d::inverse_factor(view3d::exit_mask_factor(magic_compat, applied, 5.0F)));
  return v;
}

}  // namespace

TEST_CASE("cif after an output-style change drifts as Magic with magic-compat ON",
          "[cap:view-3d][req:3D view commands]") {
  const auto layout = view3d::default_view(Rect{0, 0, 100, 50}, view3d::Mode::layout_layer, 1.0F);
  const auto v = cif_round_trip(true, layout);
  // Magic: x10 on entering, then x(1/5) on leaving: trans x2 and s_xy / 2.
  const float fifth = view3d::inverse_factor(5.0F);
  CHECK(v.trans_x == -500.0F * fifth);
  CHECK(v.trans_y == -250.0F * fifth);
  CHECK(v.scale_xy == (layout.scale_xy / 10.0F) / fifth);
  CHECK(v.scale_z == (25.0F * 10.0F) * fifth);
  CHECK_THAT(static_cast<double>(v.trans_x), WithinRel(static_cast<double>(-100.0F), 1e-6));
  CHECK_THAT(static_cast<double>(v.scale_xy), WithinRel(static_cast<double>(layout.scale_xy / 2.0F), 1e-6));
}

TEST_CASE("cif after an output-style change returns the layout view with magic-compat OFF",
          "[cap:view-3d][req:3D view commands]") {
  const auto layout = view3d::default_view(Rect{0, 0, 100, 50}, view3d::Mode::layout_layer, 1.0F);
  const auto v = cif_round_trip(false, layout);
  CHECK_THAT(static_cast<double>(v.trans_x), WithinRel(static_cast<double>(layout.trans_x), 1e-6));
  CHECK_THAT(static_cast<double>(v.trans_y), WithinRel(static_cast<double>(layout.trans_y), 1e-6));
  CHECK_THAT(static_cast<double>(v.scale_xy), WithinRel(static_cast<double>(layout.scale_xy), 1e-6));
  CHECK_THAT(static_cast<double>(v.scale_z), WithinRel(static_cast<double>(layout.scale_z), 1e-6));
  CHECK_THAT(static_cast<double>(v.prescale_z), WithinRel(static_cast<double>(layout.prescale_z), 1e-6));
}

TEST_CASE("the Q-MISC-78 branch point picks the factor per mode", "[cap:view-3d][req:3D view commands]") {
  CHECK(view3d::exit_mask_factor(true, 10.0F, 5.0F) == 5.0F);
  CHECK(view3d::exit_mask_factor(false, 10.0F, 5.0F) == 10.0F);
  // ON keeps the view and the applied factor on a style change.
  auto v = view3d::default_view(Rect{0, 0, 100, 50}, view3d::Mode::mask_layer, 10.0F);
  const auto before = v;
  CHECK(view3d::change_output_style(v, true, 10.0F, 5.0F) == 10.0F);
  CHECK(v == before);
  // OFF ignores an invalid factor instead of dividing by zero.
  CHECK(view3d::change_output_style(v, false, 0.0F, 5.0F) == 0.0F);
  CHECK(v == before);
}

// --- Q-MISC-84: framing a 3D box (task 16.5) --------------------------------

namespace {

// A sky130 standard cell's bbox in internal units with heights up to 6.63
// (metal5's top in the stack's draw units).
const view3d::Extent3 kCellBox{-19.0, -24.0, 755.0, 296.0, 0.0, 6.63};

// The clip-space |x| and |y| bound of the box's corners and its centre,
// as the renderer draws it (z = +h * scale_z).
struct ClipBox {
  double extent = 0.0;
  double cx = 0.0;
  double cy = 0.0;
};

ClipBox clip_box(const view3d::ViewState& v, const view3d::Extent3& b, int w, int h) {
  const view3d::Matrix4 m = view3d::matrix(v, w, h, {});
  std::array<double, 2> lo{1e30, 1e30};
  std::array<double, 2> hi{-1e30, -1e30};
  for (unsigned c = 0; c < 8U; ++c) {
    const double x = (c & 1U) != 0U ? b.x1 : b.x0;
    const double y = (c & 2U) != 0U ? b.y1 : b.y0;
    const double z = ((c & 4U) != 0U ? b.z1 : b.z0) * static_cast<double>(v.scale_z);
    for (std::size_t r = 0; r < 2; ++r) {
      const auto e = [&](std::size_t i) { return static_cast<double>(m[i]); };
      const double p = (e(r) * x) + (e(4 + r) * y) + (e(8 + r) * z) + e(12 + r);
      lo[r] = std::fmin(lo[r], p);
      hi[r] = std::fmax(hi[r], p);
    }
  }
  ClipBox out;
  out.extent = std::fmax(std::fmax(-lo[0], hi[0]), std::fmax(-lo[1], hi[1]));
  out.cx = 0.5 * (lo[0] + hi[0]);
  out.cy = 0.5 * (lo[1] + hi[1]);
  return out;
}

// A square cell (the inverter demo's shape) with a 10-unit stack.
const view3d::Extent3 kSquareBox{0.0, 0.0, 1000.0, 1000.0, 0.0, 10.0};

view3d::ViewState preset_default(view3d::Preset p, const Rect& bbox = Rect{-19, -24, 755, 296}) {
  auto v = view3d::default_view(bbox, view3d::Mode::layout_layer, 1.0F);
  view3d::Gesture g;
  g.kind = view3d::Gesture::Kind::preset;
  g.preset = p;
  view3d::apply(v, g);
  return v;
}

constexpr std::array<std::array<int, 2>, 3> kSizes{{{500, 500}, {800, 500}, {500, 800}}};

}  // namespace

TEST_CASE("frame_view centres the cell's 3D box and fills 90% of the limiting dimension",
          "[cap:view-3d][req:Default view]") {
  for (const auto& [w, h] : kSizes) {
    for (const view3d::Preset p :
         {view3d::Preset::top, view3d::Preset::isometric, view3d::Preset::front, view3d::Preset::side}) {
      INFO(w << " x " << h << ", preset " << static_cast<int>(p));
      const view3d::ViewState v = preset_default(p);
      const view3d::ViewState framed = view3d::frame_view(v, kCellBox, w, h);
      // The angles and z scales are kept; only scroll/zoom values change.
      CHECK(framed.view_x == v.view_x);
      CHECK(framed.view_y == v.view_y);
      CHECK(framed.view_z == v.view_z);
      CHECK(framed.scale_z == v.scale_z);
      CHECK(framed.prescale_z == v.prescale_z);
      const ClipBox b = clip_box(framed, kCellBox, w, h);
      CHECK_THAT(b.extent, WithinAbs(view3d::kFrameFill, 1e-4));
      CHECK_THAT(b.cx, WithinAbs(0.0, 1e-4));
      CHECK_THAT(b.cy, WithinAbs(0.0, 1e-4));
    }
  }
}

TEST_CASE("an isometric view right after render3d fits the window once framed",
          "[cap:view-3d][req:Default view]") {
  // Round-3 open item: the default view fits the cell's x/y extent in the
  // top view; turned to the isometric preset, a square cell's diagonal and
  // the stack's height overflow a square window.
  const view3d::ViewState v = preset_default(view3d::Preset::isometric, Rect{0, 0, 1000, 1000});
  CHECK(clip_box(v, kSquareBox, 500, 500).extent > 1.0);
  for (const auto& [w, h] : kSizes) {
    INFO(w << " x " << h);
    const view3d::ViewState framed = view3d::frame_view(v, kSquareBox, w, h);
    CHECK_THAT(clip_box(framed, kSquareBox, w, h).extent, WithinAbs(view3d::kFrameFill, 1e-4));
  }
}

TEST_CASE("frame_view's result is what absolute scroll and zoom commands set",
          "[cap:view-3d][req:3D view commands]") {
  const view3d::ViewState v = preset_default(view3d::Preset::isometric);
  const view3d::ViewState framed = view3d::frame_view(v, kCellBox, 800, 500);
  // `scroll tx ty tz` and `zoom s_xy s_z` (absolute) replace exactly these.
  view3d::ViewState commanded = v;
  commanded.trans_x = framed.trans_x;
  commanded.trans_y = framed.trans_y;
  commanded.trans_z = framed.trans_z;
  commanded.scale_xy = framed.scale_xy;
  commanded.scale_z = framed.scale_z;
  CHECK(commanded == framed);
  CHECK(framed.trans_x == -368.0F);
  CHECK(framed.trans_y == -136.0F);
  CHECK(framed.trans_z == static_cast<float>(-0.5 * 6.63 * 25.0));
}

TEST_CASE("frame_view leaves the view unchanged for a box it cannot frame",
          "[cap:view-3d][req:Default view]") {
  const view3d::ViewState v = preset_default(view3d::Preset::isometric);
  CHECK(view3d::frame_view(v, view3d::Extent3{0, 0, std::nan(""), 1, 0, 1}, 500, 500) == v);
  // A point keeps the scale and is centred.
  const view3d::ViewState p = view3d::frame_view(v, view3d::Extent3{10, 20, 10, 20, 0, 0}, 500, 500);
  CHECK(p.scale_xy == v.scale_xy);
  CHECK(p.trans_x == -10.0F);
  CHECK(p.trans_y == -20.0F);
  CHECK(view3d::frame_view(v, kCellBox, 0, 0).scale_xy > 0.0F);  // a non-positive size is square
}
