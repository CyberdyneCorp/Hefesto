// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mesh3dRenderer through the real Vulkan path (openspec/changes/m10-3d-view
// tasks.md 5.3, 5.4; design D8, D10): MoltenVK on macOS, Mesa lavapipe in
// the Linux CI job gpu-readback-lavapipe. The GPU tests SKIP without a
// Vulkan device unless HEFESTO_REQUIRE_VULKAN_DEVICE is set.
//
// Reference images are Hefesto's own (never Magic's pixels): the pick image
// of a scene as a character map (tests/data/render/mesh3d/*.txt; '.' the
// background, 'A' + layer a rectangle face, 'a' + layer a side face, '0' +
// layer a split-tile triangle), compared with a one-pixel edge tolerance,
// plus the color of every interior pixel against the shading of the face
// it shows. HEFESTO_UPDATE_RENDER_REFERENCES=1 rewrites the maps.
// Owner: RENDER3D.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../view3d/mesh/mesh_fixture.hpp"
#include "hefesto/render/headless.hpp"
#include "hefesto/render/mesh3d_pick.hpp"
#include "hefesto/render/mesh3d_shading.hpp"
#include "hefesto/render/mesh3d_target.hpp"
#include "hefesto/render/mesh3d_vulkan_renderer.hpp"
#include "hefesto/view3d/camera/view_state.hpp"

namespace db = hefesto::db;
namespace display = hefesto::display;
namespace render = hefesto::render;
namespace view3d = hefesto::view3d;
using db::Rect;

namespace {

// --- device and styles -----------------------------------------------------

std::unique_ptr<render::HeadlessCanvas> make_device() {
  const char* env = std::getenv("HEFESTO_REQUIRE_VULKAN_DEVICE");
  const std::string required = env != nullptr ? env : "";
  std::unique_ptr<render::HeadlessCanvas> canvas;
  try {
    canvas = std::make_unique<render::HeadlessCanvas>(render::HeadlessOptions{16, 16, false});
  } catch (const render::VulkanError& e) {
    if (!required.empty()) {
      FAIL("no Vulkan device, but HEFESTO_REQUIRE_VULKAN_DEVICE=" << required << ": " << e.what());
    }
    SKIP("no Vulkan device: " << e.what());
  }
  if (!required.empty()) {
    REQUIRE(canvas->device_name().find(required) != std::string::npos);
  }
  return canvas;
}

constexpr std::size_t kGreen = display::kFixedStyleCount;
constexpr std::size_t kRed = display::kFixedStyleCount + 1;
constexpr std::size_t kBlue = display::kFixedStyleCount + 2;
constexpr std::size_t kOutline = display::kFixedStyleCount + 3;
constexpr std::size_t kYellow = display::kFixedStyleCount + 4;  // stippled: drawn solid

display::StyleSet style_set() {
  display::StyleSet s;
  s.colors.entries = {{40, 40, 40, "bg"},     {60, 200, 60, "green"}, {220, 60, 60, "red"},
                      {70, 90, 230, "blue"}, {230, 210, 60, "yellow"}};
  s.table.styles.resize(display::kFixedStyleCount + 5);
  s.table.layout_count = 5;
  s.table.styles[display::kStyleTransparent].color = 0;
  const auto set = [&](std::size_t i, std::int32_t color, display::FillKind fill) {
    s.table.styles[i].color = color;
    s.table.styles[i].mask = 127;
    s.table.styles[i].fill = fill;
  };
  set(kGreen, 1, display::FillKind::solid);
  set(kRed, 2, display::FillKind::solid);
  set(kBlue, 3, display::FillKind::solid);
  set(kOutline, 2, display::FillKind::outline);
  set(kYellow, 4, display::FillKind::stipple);
  return s;
}

// --- scenes ----------------------------------------------------------------

constexpr view3d::LayerId kNdiff = 0;  // 'A': green, h 0, t 0.2
constexpr view3d::LayerId kPoly = 1;   // 'B': red, h 0.3, t 0.2
constexpr view3d::LayerId kMetal = 2;  // 'C': blue, h 1.0, t 0.5

struct Scene {
  view3d::test::MeshTech tech = view3d::test::build_tech();
  view3d::LayerStack stack;
  view3d::MeshModel mesh;
  db::Rect bbox;
};

std::vector<view3d::StackLayer> layers_of(const view3d::test::MeshTech& t) {
  return {view3d::test::layer("ndiff", t.ndiff, 0.0F, 0.2F, kGreen),
          view3d::test::layer("poly", t.poly, 0.3F, 0.2F, kRed),
          view3d::test::layer("metal1", t.metal1, 1.0F, 0.5F, kBlue)};
}

view3d::LayerStack stack_for(const view3d::test::MeshTech& t, bool magic_compat) {
  std::vector<view3d::DrawHeight> heights(hefesto::tech::kMaxTileTypes);
  for (const auto& l : layers_of(t)) heights[*l.type] = view3d::DrawHeight{l.height, l.thickness};
  return view3d::LayerStack{layers_of(t), std::move(heights), magic_compat, 0.01};
}

void finish(Scene& s, db::CellDef& cell, const Rect& clip, bool cut_box) {
  db::recompute_bbox(cell);
  s.bbox = cut_box ? clip : cell.bbox();
  auto mesh = view3d::test::build(s.stack, cell, clip, cut_box);
  REQUIRE(mesh.has_value());
  s.mesh = std::move(*mesh);
}

/// ndiff and poly side by side on the active plane, metal1 across both.
Scene basic_scene(bool magic_compat = false) {
  Scene s;
  s.stack = stack_for(s.tech, magic_compat);
  db::CellDef cell("basic", s.tech.tech->plane_count());
  view3d::test::paint(cell, s.tech, Rect{0, 0, 30, 60}, s.tech.ndiff);
  view3d::test::paint(cell, s.tech, Rect{40, 0, 60, 60}, s.tech.poly);
  view3d::test::paint(cell, s.tech, Rect{10, 20, 80, 40}, s.tech.metal1);
  finish(s, cell, Rect{-1, -1, 81, 61}, false);
  return s;
}

/// A metal1 triangle, and an ndiff | poly split tile. With magic-compat ON
/// the poly triangle is drawn with ndiff's h and t (own_height, Q-MISC-79);
/// with it OFF with poly's own.
Scene split_scene(bool magic_compat = true) {
  Scene s;
  s.stack = stack_for(s.tech, magic_compat);
  db::CellDef cell("split", s.tech.tech->plane_count());
  REQUIRE(db::paint_triangle(cell, *s.tech.tech, Rect{0, 0, 40, 40}, db::Diagonal::ne_sw, false, s.tech.metal1).ok());
  view3d::test::paint(cell, s.tech, Rect{50, 0, 90, 40}, s.tech.ndiff);
  REQUIRE(db::paint_triangle(cell, *s.tech.tech, Rect{50, 0, 90, 40}, db::Diagonal::ne_sw, true, s.tech.poly).ok());
  finish(s, cell, Rect{-1, -1, 91, 41}, false);
  return s;
}

/// metal1 and ndiff cut by a cut box: closed faces on the box.
Scene cutbox_scene() {
  Scene s;
  s.stack = stack_for(s.tech, false);
  db::CellDef cell("cut", s.tech.tech->plane_count());
  view3d::test::paint(cell, s.tech, Rect{0, 0, 100, 100}, s.tech.metal1);
  view3d::test::paint(cell, s.tech, Rect{20, 20, 60, 140}, s.tech.ndiff);
  finish(s, cell, Rect{10, 10, 70, 70}, true);
  return s;
}

view3d::ViewState view_of(const Scene& s, view3d::Preset preset) {
  view3d::ViewState v = view3d::default_view(s.bbox, view3d::Mode::layout_layer, 1.0F);
  view3d::apply(v, view3d::Gesture{view3d::Gesture::Kind::preset, 0, 0, 1, false, preset});
  return v;
}

// --- rendering and readback ------------------------------------------------

constexpr int kWidth = 96;
constexpr int kHeight = 64;

struct Rendered {
  render::TargetReadback image;
};

struct Fixture {
  std::unique_ptr<render::HeadlessCanvas> canvas = make_device();
  render::VulkanOffscreenTarget target{canvas->vulkan_device(), kWidth, kHeight};
  render::VulkanMesh3dRenderer renderer{target};
  display::StyleSet styles = style_set();

  render::DrawParams params(const view3d::LayerStack& stack, int level = 0) const {
    render::DrawParams p;
    p.level = level;
    p.stack = &stack;
    p.styles = &styles;
    return p;
  }

  render::TargetReadback draw(const Scene& s, const view3d::ViewState& v, const render::DrawParams& p) {
    renderer.upload(s.mesh);
    renderer.draw(v, p, target);
    return target.read_back();
  }
};

std::size_t pixel(const render::TargetReadback& r, int x, int y) {
  return (static_cast<std::size_t>(y) * static_cast<std::size_t>(r.width)) + static_cast<std::size_t>(x);
}

std::array<int, 3> rgb_of(std::uint32_t c) {
  return {static_cast<int>(c & 0xFFU), static_cast<int>((c >> 8U) & 0xFFU), static_cast<int>((c >> 16U) & 0xFFU)};
}

char map_char(const view3d::MeshModel& mesh, view3d::PickId id) {
  const auto ref = render::decode_pick(mesh, id);
  if (!ref.has_value()) {
    return id == view3d::kNoPick ? '.' : '?';
  }
  const view3d::Chunk& c = mesh.chunks[ref->chunk];
  switch (ref->kind) {
    case render::PickKind::rect:
      return static_cast<char>('A' + c.rects[ref->index].layer);
    case render::PickKind::side:
      return static_cast<char>('a' + c.sides[ref->index].layer);
    case render::PickKind::triangle:
      return static_cast<char>('0' + c.triangles[ref->index].layer);
  }
  return '?';
}

std::vector<std::string> pick_map(const view3d::MeshModel& mesh, const render::TargetReadback& r) {
  std::vector<std::string> rows(static_cast<std::size_t>(r.height), std::string(static_cast<std::size_t>(r.width), '.'));
  for (int y = 0; y < r.height; ++y) {
    for (int x = 0; x < r.width; ++x) {
      rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = map_char(mesh, r.pick[pixel(r, x, y)]);
    }
  }
  return rows;
}

std::string reference_path(const std::string& name) {
  return std::string(HEFESTO_RENDER_REFERENCE_DIR) + "/" + name + ".txt";
}

std::vector<std::string> read_reference(const std::string& name) {
  std::ifstream in(reference_path(name));
  std::vector<std::string> rows;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line[0] != '#') rows.push_back(line);
  }
  return rows;
}

void write_reference(const std::string& name, const std::vector<std::string>& rows) {
  std::ofstream out(reference_path(name));
  out << "# Mesh3dRenderer reference pick map '" << name << "' (" << kWidth << "x" << kHeight
      << "): '.' background, 'A'+layer rect, 'a'+layer side, '0'+layer triangle.\n";
  for (const std::string& row : rows) out << row << '\n';
}

bool has_neighbour(const std::vector<std::string>& rows, int x, int y, char c) {
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      const int nx = x + dx;
      const int ny = y + dy;
      if (ny >= 0 && ny < static_cast<int>(rows.size()) && nx >= 0 &&
          nx < static_cast<int>(rows[static_cast<std::size_t>(ny)].size()) &&
          rows[static_cast<std::size_t>(ny)][static_cast<std::size_t>(nx)] == c) {
        return true;
      }
    }
  }
  return false;
}

/// Compares a pick map with its reference: a differing pixel must sit on an
/// edge (each side's character appears next to it in the other map), and at
/// most 2% of the pixels may differ.
void check_reference(const std::string& name, const std::vector<std::string>& actual) {
  if (std::getenv("HEFESTO_UPDATE_RENDER_REFERENCES") != nullptr) {
    write_reference(name, actual);
    WARN("rewrote " << reference_path(name));
    return;
  }
  const std::vector<std::string> expected = read_reference(name);
  REQUIRE(expected.size() == actual.size());
  int differing = 0;
  for (int y = 0; y < static_cast<int>(actual.size()); ++y) {
    REQUIRE(expected[static_cast<std::size_t>(y)].size() == actual[static_cast<std::size_t>(y)].size());
    for (int x = 0; x < static_cast<int>(actual[static_cast<std::size_t>(y)].size()); ++x) {
      const char e = expected[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      const char a = actual[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      if (e == a) continue;
      ++differing;
      INFO(name << ": pixel (" << x << ", " << y << ") is '" << a << "', the reference has '" << e << "'");
      CHECK(has_neighbour(expected, x, y, a));
      CHECK(has_neighbour(actual, x, y, e));
    }
  }
  CHECK(differing * 50 <= kWidth * kHeight);
}

/// The outward normal of the face a pick id shows (the top face of a rect
/// or triangle in these views).
std::array<float, 3> face_normal(const view3d::MeshModel& mesh, const render::PickRef& ref) {
  if (ref.kind != render::PickKind::side) {
    return {0.0F, 0.0F, 1.0F};
  }
  const view3d::SideInstance& s = mesh.chunks[ref.chunk].sides[ref.index];
  float dx = s.x1 - s.x0;
  float dy = s.y1 - s.y0;
  if ((s.flags & view3d::side_flag::outward_left) != 0) {
    dx = -dx;
    dy = -dy;
  }
  const float len = std::hypot(dx, dy);
  return {dy / len, -dx / len, 0.0F};
}

view3d::LayerId layer_of(const view3d::MeshModel& mesh, const render::PickRef& ref) {
  const view3d::Chunk& c = mesh.chunks[ref.chunk];
  switch (ref.kind) {
    case render::PickKind::rect:
      return c.rects[ref.index].layer;
    case render::PickKind::side:
      return c.sides[ref.index].layer;
    case render::PickKind::triangle:
      return c.triangles[ref.index].layer;
  }
  return 0;
}

bool interior(const render::TargetReadback& r, int x, int y) {
  if (x < 1 || y < 1 || x + 1 >= r.width || y + 1 >= r.height) return false;
  const std::uint32_t id = r.pick[pixel(r, x, y)];
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (r.pick[pixel(r, x + dx, y + dy)] != id) return false;
    }
  }
  return true;
}

bool near_rgb(const std::array<int, 3>& a, const std::array<float, 3>& expected, int tolerance) {
  for (std::size_t i = 0; i < 3; ++i) {
    if (std::abs(a[i] - static_cast<int>(std::lround(expected[i] * 255.0F))) > tolerance) return false;
  }
  return true;
}

/// The board intensity of a face with displayed normal `normal` in view `v`.
float board(const view3d::ViewState& v, const std::array<float, 3>& normal) {
  return render::light_intensity(render::Lighting::board, render::board_light(v.view_z), normal);
}

/// Board lighting (both modes, Q-MISC-84): every interior pixel has the lit
/// color of the face its pick id names, and the background elsewhere.
void check_colors(const Scene& s, const display::StyleSet& styles, const render::TargetReadback& r,
                  const view3d::ViewState& v) {
  const auto paints = render::layer_paints(s.stack, &styles);
  const auto bg = render::background_color(&styles);
  int checked = 0;
  for (int y = 0; y < r.height; ++y) {
    for (int x = 0; x < r.width; ++x) {
      if (!interior(r, x, y)) continue;
      const auto ref = render::decode_pick(s.mesh, r.pick[pixel(r, x, y)]);
      const std::array<float, 3> expected =
          ref.has_value() ? render::lit_color(paints[layer_of(s.mesh, *ref)].rgb, board(v, face_normal(s.mesh, *ref)))
                          : bg;
      INFO("pixel (" << x << ", " << y << ")");
      CHECK(near_rgb(rgb_of(r.color[pixel(r, x, y)]), expected, 2));
      ++checked;
    }
  }
  CHECK(checked > kWidth * kHeight / 2);
}

/// The pixel where the model point (x, y, z) (z = -h * s_z) lands: CAMERA's
/// matrix applied to the displayed scene, the model mirrored in z
/// (Q-MISC-84).
std::pair<int, int> project(const view3d::ViewState& v, double x, double y, double z) {
  const view3d::Matrix4 m = view3d::matrix(v, kWidth, kHeight, view3d::ProjectionParams{});
  const auto at = [&](std::size_t i) { return static_cast<double>(m[i]); };
  const double cx = (at(0) * x) + (at(4) * y) - (at(8) * z) + at(12);
  const double cy = (at(1) * x) + (at(5) * y) - (at(9) * z) + at(13);
  const double px = (cx + 1.0) * 0.5 * kWidth;
  const double py = (1.0 - cy) * 0.5 * kHeight;  // y up in the view, down in the image
  return {static_cast<int>(px), static_cast<int>(py)};
}

/// The Vulkan depth of model point (x, y, z) in the renderer's mapping:
/// (w - z') / 2 of the displayed (z-mirrored) scene (Q-MISC-81, Q-MISC-84),
/// higher layers nearer.
float depth_of(const view3d::ViewState& v, double x, double y, double z) {
  const view3d::Matrix4 m = view3d::matrix(v, kWidth, kHeight, view3d::ProjectionParams{});
  const auto at = [&](std::size_t i) { return static_cast<double>(m[i]); };
  const double cz = (at(2) * x) + (at(6) * y) - (at(10) * z) + at(14);
  return static_cast<float>(0.5 * (1.0 - cz));
}

/// The model z of the face at a layer's h + t end, the one the top view
/// shows (z = -(h + t) * s_z).
double far_end_z(const view3d::ViewState& v, float h, float t) {
  return -static_cast<double>(h + t) * static_cast<double>(v.scale_z);
}

view3d::PickId pick_at(Fixture& f, const view3d::ViewState& v, double x, double y, double z) {
  const auto [px, py] = project(v, x, y, z);
  return f.renderer.read_pick(px, py);
}

std::optional<view3d::LayerId> picked_layer(const Scene& s, view3d::PickId id) {
  const auto ref = render::decode_pick(s.mesh, id);
  if (!ref.has_value()) return std::nullopt;
  return layer_of(s.mesh, *ref);
}

}  // namespace

// --- pick ids (no device) ------------------------------------------------------

TEST_CASE("pick ids number every instance of the model from 1, chunk by chunk",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  view3d::MeshModel mesh;
  mesh.chunks.resize(2);
  mesh.chunks[0].rects.resize(2);
  mesh.chunks[0].sides.resize(3);
  mesh.chunks[1].triangles.resize(1);
  mesh.chunks[1].rects.resize(1);
  using render::PickKind;
  using render::PickRef;
  CHECK(render::encode_pick(mesh, PickRef{0, PickKind::rect, 0}) == 1);
  CHECK(render::encode_pick(mesh, PickRef{0, PickKind::side, 2}) == 5);
  CHECK(render::encode_pick(mesh, PickRef{1, PickKind::rect, 0}) == 6);
  CHECK(render::encode_pick(mesh, PickRef{1, PickKind::triangle, 0}) == 7);
  CHECK(render::encode_pick(mesh, PickRef{1, PickKind::side, 0}) == view3d::kNoPick);
  for (view3d::PickId id = 1; id <= 7; ++id) {
    const auto ref = render::decode_pick(mesh, id);
    REQUIRE(ref.has_value());
    CHECK(render::encode_pick(mesh, *ref) == id);
  }
  CHECK_FALSE(render::decode_pick(mesh, view3d::kNoPick).has_value());
  CHECK_FALSE(render::decode_pick(mesh, 8).has_value());
}

TEST_CASE("3D colors: style colors, only solid and stippled styles drawn, the transparent style's background",
          "[cap:view-3d][req:Viewing transform and shading]") {
  const display::StyleSet styles = style_set();
  const auto t = view3d::test::build_tech();
  const view3d::LayerStack stack{view3d::Mode::layout_layer,
                                 {view3d::test::layer("a", t.ndiff, 0, 0, kRed),
                                  view3d::test::layer("b", t.poly, 0, 0, kOutline),
                                  view3d::test::layer("c", t.metal1, 0, 0, kYellow)},
                                 true,
                                 0.01};
  const auto paints = render::layer_paints(stack, &styles);
  REQUIRE(paints.size() == 3);
  CHECK(paints[0] == render::LayerPaint{{220.0F / 255.0F, 60.0F / 255.0F, 60.0F / 255.0F}, true});
  CHECK_FALSE(paints[1].drawable);
  CHECK(paints[2].drawable);
  CHECK(render::background_color(&styles) == std::array<float, 3>{40.0F / 255.0F, 40.0F / 255.0F, 40.0F / 255.0F});
  const auto lit = render::lit_color({0.5F, 0.8F, 0.1F}, 1.6F);  // clamped to 1
  CHECK(std::abs(lit[0] - 0.8F) < 1e-6F);
  CHECK(lit[1] == 1.0F);
  CHECK(std::abs(lit[2] - 0.16F) < 1e-6F);
}

// --- images (device) -------------------------------------------------------------

TEST_CASE("3D renderer top view matches its reference image",
          "[cap:view-3d][req:Viewing transform and shading]") {
  Fixture f;
  const Scene s = basic_scene();
  const view3d::ViewState v = view_of(s, view3d::Preset::top);
  const render::TargetReadback r = f.draw(s, v, f.params(s.stack));
  check_reference("top", pick_map(s.mesh, r));
  check_colors(s, f.styles, r, v);
  // Higher layers are in front (R1, Q-MISC-81): metal1 covers ndiff and
  // poly where they overlap.
  CHECK(picked_layer(s, pick_at(f, v, 20, 30, 0)) == kMetal);
  CHECK(picked_layer(s, pick_at(f, v, 5, 5, 0)) == kNdiff);
  CHECK(picked_layer(s, pick_at(f, v, 70, 30, 0)) == kMetal);
  CHECK(pick_at(f, v, 35, 50, 0) == view3d::kNoPick);
}

TEST_CASE("3D renderer top view shows metal1 in front of poly where they overlap, magic-compat ON and OFF (R1)",
          "[cap:view-3d][req:Viewing transform and shading]") {
  // Regression (R1, Q-MISC-81, approved 2026-11-22, both modes): the depth
  // used to follow Magic's faces, which put the viewer on the physical
  // underside, so poly (h 0.3) covered metal1 (h 1.0) in the top view.
  Fixture f;
  for (const bool magic_compat : {true, false}) {
    INFO("magic-compat " << (magic_compat ? "ON" : "OFF"));
    const Scene s = basic_scene(magic_compat);
    const view3d::ViewState v = view_of(s, view3d::Preset::top);
    for (const int level : {0, 1}) {
      INFO("level " << level);
      const render::TargetReadback r = f.draw(s, v, f.params(s.stack, level));
      const auto [px, py] = project(v, 50, 30, 0);  // poly under metal1
      const auto ref = render::decode_pick(s.mesh, r.pick[pixel(r, px, py)]);
      REQUIRE(ref.has_value());
      CHECK(ref->kind == render::PickKind::rect);
      CHECK(layer_of(s.mesh, *ref) == kMetal);
      // The face shown is metal1's at h + t (1.5), the nearest.
      CHECK(std::abs(r.depth[pixel(r, px, py)] - depth_of(v, 50, 30, far_end_z(v, 1.0F, 0.5F))) < 1e-6F);
      const auto [qx, qy] = project(v, 50, 50, 0);  // poly alone
      CHECK(picked_layer(s, r.pick[pixel(r, qx, qy)]) == kPoly);
    }
  }
}

TEST_CASE("3D renderer draws the face of each rectangle toward the viewer, from above and from below",
          "[cap:view-3d][req:Viewing transform and shading]") {
  // A rectangle is one 4-vertex strip: the shader emits its h + t face or
  // its h face, whichever faces the viewer (task 13.2), where the rasterizer
  // used to cull one of the two. Seen from below (view_x 180) the h faces
  // are shown and poly (h 0.3) is in front of metal1 (h 1.0); orthographic
  // and perspective, levels 0 and 1.
  Fixture f;
  const Scene s = basic_scene();
  for (const view3d::Projection kind : {view3d::Projection::orthographic, view3d::Projection::perspective}) {
    for (const float view_x : {0.0F, 180.0F}) {
      for (const int level : {0, 1}) {
        INFO("projection " << static_cast<int>(kind) << ", view_x " << view_x << ", level " << level);
        view3d::ViewState v = view_of(s, view3d::Preset::top);
        v.view_x = view_x;
        render::DrawParams p = f.params(s.stack, level);
        p.projection.kind = kind;
        const render::TargetReadback r = f.draw(s, v, p);
        const bool below = view_x > 90.0F;
        // Orthographic pixel positions; the probes sit well inside each
        // region, so the perspective frame (same framing of z = 0) hits them.
        const auto [px, py] = project(v, 50, 30, 0);  // poly under metal1
        CHECK(picked_layer(s, r.pick[pixel(r, px, py)]) == (below ? kPoly : kMetal));
        const auto [mx, my] = project(v, 70, 30, 0);  // metal1 alone
        const auto ref = render::decode_pick(s.mesh, r.pick[pixel(r, mx, my)]);
        REQUIRE(ref.has_value());
        CHECK(ref->kind == render::PickKind::rect);
        CHECK(layer_of(s.mesh, *ref) == kMetal);
        if (kind == view3d::Projection::orthographic) {
          const double z = below ? -1.0 * static_cast<double>(v.scale_z) : far_end_z(v, 1.0F, 0.5F);
          CHECK(std::abs(r.depth[pixel(r, mx, my)] - depth_of(v, 70, 30, z)) < 1e-6F);
        }
      }
    }
  }
}

TEST_CASE("3D renderer isometric view matches its reference image",
          "[cap:view-3d][req:Viewing transform and shading]") {
  Fixture f;
  const Scene s = basic_scene();
  const view3d::ViewState v = view_of(s, view3d::Preset::isometric);
  const render::TargetReadback r = f.draw(s, v, f.params(s.stack));
  check_reference("isometric", pick_map(s.mesh, r));
  check_colors(s, f.styles, r, v);
}

TEST_CASE("3D renderer draws split tiles as triangles with their own h and t",
          "[cap:view-3d][req:Rendered geometry]") {
  Fixture f;
  const Scene s = split_scene();
  const view3d::ViewState v = view_of(s, view3d::Preset::isometric);
  const render::TargetReadback r = f.draw(s, v, f.params(s.stack));
  check_reference("split", pick_map(s.mesh, r));

  // The poly triangle of the ndiff | poly tile sits at ndiff's height (h 0,
  // t 0.2), not poly's (0.3, 0.2): the face the top view shows lies at
  // z = -(0 + 0.2) * s_z.
  const view3d::ViewState top = view_of(s, view3d::Preset::top);
  const render::TargetReadback rt = f.draw(s, top, f.params(s.stack));
  const auto [px, py] = project(top, 85, 10, 0);  // inside the poly triangle (right side)
  const auto ref = render::decode_pick(s.mesh, rt.pick[pixel(rt, px, py)]);
  REQUIRE(ref.has_value());
  CHECK(ref->kind == render::PickKind::triangle);
  CHECK(layer_of(s.mesh, *ref) == kPoly);
  CHECK(std::abs(rt.depth[pixel(rt, px, py)] - depth_of(top, 85, 10, far_end_z(top, 0.0F, 0.2F))) < 1e-6F);
}

TEST_CASE("3D renderer draws a split tile's triangle at its own type's height with magic-compat OFF (Q-MISC-79)",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  Fixture f;
  const Scene s = split_scene(false);
  const view3d::ViewState iso = view_of(s, view3d::Preset::isometric);
  check_colors(s, f.styles, f.draw(s, iso, f.params(s.stack)), iso);
  const view3d::ViewState top = view_of(s, view3d::Preset::top);
  const render::TargetReadback rt = f.draw(s, top, f.params(s.stack));
  const auto [px, py] = project(top, 85, 10, 0);  // inside the poly triangle (right side)
  const auto ref = render::decode_pick(s.mesh, rt.pick[pixel(rt, px, py)]);
  REQUIRE(ref.has_value());
  CHECK(ref->kind == render::PickKind::triangle);
  CHECK(layer_of(s.mesh, *ref) == kPoly);
  // The face shown lies at poly's h + t (0.5), nearer than ndiff's h + t
  // (0.2) where magic-compat ON draws it.
  CHECK(std::abs(rt.depth[pixel(rt, px, py)] - depth_of(top, 85, 10, far_end_z(top, 0.3F, 0.2F))) < 1e-6F);
  CHECK(rt.depth[pixel(rt, px, py)] + 1e-4F < depth_of(top, 85, 10, far_end_z(top, 0.0F, 0.2F)));
}

TEST_CASE("3D renderer closes the faces cut by the cut box", "[cap:view-3d][req:Rendered geometry]") {
  Fixture f;
  const Scene s = cutbox_scene();
  const view3d::ViewState v = view_of(s, view3d::Preset::isometric);
  const render::TargetReadback r = f.draw(s, v, f.params(s.stack));
  check_reference("cutbox", pick_map(s.mesh, r));
  check_colors(s, f.styles, r, v);
}

TEST_CASE("3D renderer: a type in two styles shows the last style's color on its top face (Q-MISC-77)",
          "[cap:view-3d][req:Viewing transform and shading]") {
  Fixture f;
  Scene s;
  // metal1 in the blue style, then in the (stippled) yellow style: two
  // coplanar prisms; LESS_OR_EQUAL lets the later one win.
  std::vector<view3d::StackLayer> layers{view3d::test::layer("metal1", s.tech.metal1, 1.0F, 0.5F, kBlue),
                                         view3d::test::layer("metal1", s.tech.metal1, 1.0F, 0.5F, kYellow)};
  s.stack = view3d::LayerStack{view3d::Mode::layout_layer, std::move(layers), false, 0.01};
  db::CellDef cell("two", s.tech.tech->plane_count());
  view3d::test::paint(cell, s.tech, Rect{0, 0, 40, 20}, s.tech.metal1);
  finish(s, cell, Rect{-1, -1, 41, 21}, false);
  const view3d::ViewState v = view_of(s, view3d::Preset::top);
  for (const int level : {0, 1}) {
    INFO("level " << level);
    const render::TargetReadback r = f.draw(s, v, f.params(s.stack, level));
    const auto [px, py] = project(v, 20, 10, 0);
    CHECK(picked_layer(s, r.pick[pixel(r, px, py)]) == view3d::LayerId{1});
    const auto yellow = render::lit_color(render::layer_paints(s.stack, &f.styles)[1].rgb, board(v, {0, 0, 1}));
    CHECK(near_rgb(rgb_of(r.color[pixel(r, px, py)]), yellow, 2));
  }
}

TEST_CASE("3D renderer draws the same board-shaded frame with magic-compat ON and OFF (Q-MISC-84)",
          "[cap:view-3d][req:Viewing transform and shading]") {
  // Q-MISC-84 (V1, approved 2026-11-24, both modes): Magic's GL-state
  // lighting (normal (0, 0, 1), positional light at (50, 50, 50), per
  // vertex) is no longer drawn; the metal1 face the top view shows is in
  // its style color (I = 1), flat, with magic-compat ON as with it OFF.
  Fixture f;
  const Scene on = basic_scene(true);
  const Scene off = basic_scene(false);
  for (const view3d::Preset preset : {view3d::Preset::top, view3d::Preset::isometric}) {
    for (const int level : {0, 1}) {
      INFO("preset " << static_cast<int>(preset) << ", level " << level);
      const view3d::ViewState v = view_of(on, preset);
      const render::TargetReadback a = f.draw(on, v, f.params(on.stack, level));
      const render::TargetReadback b = f.draw(off, v, f.params(off.stack, level));
      CHECK(a.color == b.color);
      CHECK(a.pick == b.pick);
    }
  }
  const view3d::ViewState v = view_of(on, view3d::Preset::top);
  const render::TargetReadback r = f.draw(on, v, f.params(on.stack));
  const auto metal = render::layer_paints(on.stack, &f.styles)[kMetal].rgb;
  for (const auto& [x, y] : {std::pair{12.0, 22.0}, std::pair{78.0, 38.0}, std::pair{70.0, 30.0}}) {
    const auto [px, py] = project(v, x, y, 0);
    INFO("pixel (" << px << ", " << py << ")");
    REQUIRE(picked_layer(on, r.pick[pixel(r, px, py)]) == kMetal);
    CHECK(near_rgb(rgb_of(r.color[pixel(r, px, py)]), metal, 1));  // Magic's light varied across the face
  }
}

TEST_CASE("3D renderer at level 1 resolves 4x MSAA and keeps the single-sample pick image",
          "[cap:view-3d][req:Viewing transform and shading]") {
  Fixture f;
  const Scene s = basic_scene();
  const view3d::ViewState v = view_of(s, view3d::Preset::isometric);
  const render::TargetReadback r0 = f.draw(s, v, f.params(s.stack, 0));
  const render::TargetReadback r1 = f.draw(s, v, f.params(s.stack, 1));
  CHECK(r0.pick == r1.pick);
  CHECK(r0.depth == r1.depth);
  int differing = 0;
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      if (!interior(r0, x, y)) continue;
      const auto a = rgb_of(r0.color[pixel(r0, x, y)]);
      const auto b = rgb_of(r1.color[pixel(r1, x, y)]);
      CHECK(std::abs(a[0] - b[0]) <= 2);
      CHECK(std::abs(a[1] - b[1]) <= 2);
      CHECK(std::abs(a[2] - b[2]) <= 2);
    }
  }
  if (f.renderer.multisampling()) {
    // Antialiased edges: some edge pixels take a mix of the two sides.
    for (std::size_t i = 0; i < r0.color.size(); ++i) differing += r0.color[i] != r1.color[i] ? 1 : 0;
    CHECK(differing > 0);
  }
}

TEST_CASE("3D renderer uploads only chunks whose id or version changed",
          "[cap:view-3d][req:Rendered geometry]") {
  Fixture f;
  Scene s = basic_scene();
  f.renderer.upload(s.mesh);
  const std::uint64_t first = f.renderer.chunk_uploads();
  CHECK(first == s.mesh.chunks.size());
  f.renderer.upload(s.mesh);
  CHECK(f.renderer.chunk_uploads() == first);

  view3d::MeshModel changed = s.mesh;
  changed.chunks.front().rects.front().x1 -= 5.0F;
  changed.chunks.front().version = changed.chunks.front().version + 1000;
  f.renderer.upload(changed);
  CHECK(f.renderer.chunk_uploads() == first + 1);
  f.renderer.upload(s.mesh);
  CHECK(f.renderer.chunk_uploads() == first + 2);

  // A model made by hand (version 0) is never taken from the cache.
  view3d::MeshModel by_hand = s.mesh;
  for (view3d::Chunk& c : by_hand.chunks) c.version = 0;
  f.renderer.upload(by_hand);
  f.renderer.upload(by_hand);
  CHECK(f.renderer.chunk_uploads() == first + 2 + (2 * s.mesh.chunks.size()));

  f.renderer.upload(view3d::MeshModel{});
  f.renderer.draw(view_of(s, view3d::Preset::top), f.params(s.stack), f.target);
  CHECK(f.renderer.read_pick(kWidth / 2, kHeight / 2) == view3d::kNoPick);
}

TEST_CASE("3D renderer uploads again after a fresh build of another cell with the same chunk ids",
          "[cap:hefesto-3d-visualization][req:Mesh generation from layout]") {
  // MESH versions are process-wide, so a fresh build never reuses an
  // earlier build's (id, version): the renderer keys its cache on the pair
  // alone (no content fingerprint).
  Fixture f;
  const Scene a = basic_scene();
  Scene b;
  b.stack = stack_for(b.tech, false);
  db::CellDef cell("other", b.tech.tech->plane_count());
  view3d::test::paint(cell, b.tech, Rect{0, 0, 30, 30}, b.tech.metal1);
  finish(b, cell, Rect{-1, -1, 81, 61}, false);
  REQUIRE(a.mesh.chunks.size() == b.mesh.chunks.size());
  for (std::size_t i = 0; i < a.mesh.chunks.size(); ++i) {
    REQUIRE(a.mesh.chunks[i].id == b.mesh.chunks[i].id);
    CHECK(a.mesh.chunks[i].version != b.mesh.chunks[i].version);
  }
  const view3d::ViewState v = view_of(a, view3d::Preset::top);
  (void)f.draw(a, v, f.params(a.stack));
  const std::uint64_t first = f.renderer.chunk_uploads();
  CHECK(picked_layer(a, pick_at(f, v, 50, 50, 0)) == kPoly);
  (void)f.draw(b, v, f.params(b.stack));
  CHECK(f.renderer.chunk_uploads() == first + b.mesh.chunks.size());
  CHECK(pick_at(f, v, 50, 50, 0) == view3d::kNoPick);  // a's poly is gone
  CHECK(picked_layer(b, pick_at(f, v, 15, 15, 0)) == kMetal);
}

namespace {

/// basic_scene's layout with every coordinate multiplied by `k`.
Scene scaled_basic_scene(db::Coord k) {
  Scene s;
  s.stack = stack_for(s.tech, false);
  db::CellDef cell("basic", s.tech.tech->plane_count());
  view3d::test::paint(cell, s.tech, Rect{0, 0, 30 * k, 60 * k}, s.tech.ndiff);
  view3d::test::paint(cell, s.tech, Rect{40 * k, 0, 60 * k, 60 * k}, s.tech.poly);
  view3d::test::paint(cell, s.tech, Rect{10 * k, 20 * k, 80 * k, 40 * k}, s.tech.metal1);
  finish(s, cell, Rect{-k, -k, 81 * k, 61 * k}, false);
  return s;
}

/// Pixels where two pick maps differ; each must sit on an edge of `expected`.
int edge_differences(const std::vector<std::string>& expected, const std::vector<std::string>& actual) {
  int differing = 0;
  for (std::size_t y = 0; y < expected.size(); ++y) {
    for (std::size_t x = 0; x < expected[y].size(); ++x) {
      if (expected[y][x] == actual[y][x]) continue;
      ++differing;
      INFO("pixel (" << x << ", " << y << ")");
      CHECK(has_neighbour(expected, static_cast<int>(x), static_cast<int>(y), actual[y][x]));
    }
  }
  return differing;
}

}  // namespace

TEST_CASE("3D renderer scales x and y by DrawParams::xy_scale",
          "[cap:view-3d][req:Viewing transform and shading]") {
  // Mask-layer mode in P1 draws the layout mesh scaled by Scene::xy_factor:
  // the mesh drawn at xy_scale 10 covers the pixels of the same layout built
  // at ten times the coordinates.
  Fixture f;
  const Scene unit = scaled_basic_scene(1);
  const Scene tenfold = scaled_basic_scene(10);
  for (const view3d::Preset preset : {view3d::Preset::top, view3d::Preset::isometric}) {
    INFO("preset " << static_cast<int>(preset));
    const view3d::ViewState v = view_of(tenfold, preset);
    const auto expected = pick_map(tenfold.mesh, f.draw(tenfold, v, f.params(tenfold.stack)));
    render::DrawParams p = f.params(unit.stack);
    p.xy_scale = 10.0F;
    const auto scaled = pick_map(unit.mesh, f.draw(unit, v, p));
    REQUIRE(scaled.size() == expected.size());
    CHECK(edge_differences(expected, scaled) * 100 <= kWidth * kHeight);
    // Unscaled, the unit mesh covers about a hundredth of that area.
    const auto plain = pick_map(unit.mesh, f.draw(unit, v, f.params(unit.stack)));
    CHECK(plain != scaled);
  }
}

TEST_CASE("3D renderer draws a translucent style opaque in its plain colour, magic-compat ON and OFF (R3)",
          "[cap:view-3d][req:Viewing transform and shading]") {
  // R3 (approved 2026-11-22, both modes): Magic's OpenGL driver sends a
  // translucent style (mask < 127) as 2c - 0.8 blended with alpha
  // mask / 127; the 3D view draws it opaque in c, as the same colour with
  // mask 127.
  Fixture f;
  display::StyleSet translucent = f.styles;
  translucent.table.styles[kBlue].mask = 40;
  for (const bool magic_compat : {true, false}) {
    INFO("magic-compat " << (magic_compat ? "ON" : "OFF"));
    const Scene s = basic_scene(magic_compat);
    const auto opaque_paints = render::layer_paints(s.stack, &f.styles);
    const auto paints = render::layer_paints(s.stack, &translucent);
    CHECK(paints[kMetal] == opaque_paints[kMetal]);
    CHECK(paints[kMetal] == render::LayerPaint{{70.0F / 255.0F, 90.0F / 255.0F, 230.0F / 255.0F}, true});
    const view3d::ViewState v = view_of(s, view3d::Preset::top);
    const render::TargetReadback plain = f.draw(s, v, f.params(s.stack));
    render::DrawParams p = f.params(s.stack);
    p.styles = &translucent;
    const render::TargetReadback r = f.draw(s, v, p);
    CHECK(r.pick == plain.pick);
    const auto [mx, my] = project(v, 75, 30, 0);  // metal1 over the background
    const auto [ox, oy] = project(v, 50, 30, 0);  // metal1 over poly
    for (const auto& [x, y] : {std::pair{mx, my}, std::pair{ox, oy}}) {
      INFO("pixel (" << x << ", " << y << ")");
      CHECK(picked_layer(s, r.pick[pixel(r, x, y)]) == kMetal);
      CHECK(r.color[pixel(r, x, y)] == plain.color[pixel(plain, x, y)]);
      CHECK((r.color[pixel(r, x, y)] >> 24U) == 0xFFU);  // opaque
    }
    const auto lit = render::lit_color(paints[kMetal].rgb, board(v, {0, 0, 1}));
    CHECK(near_rgb(rgb_of(r.color[pixel(r, mx, my)]), lit, 2));
  }
}

// --- picking and transparency (task 5.4) -----------------------------------------

TEST_CASE("3D pick reads the id of the instance at a pixel",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  Fixture f;
  const Scene s = basic_scene();
  CHECK(f.renderer.read_pick(10, 10) == view3d::kNoPick);  // before the first frame
  const view3d::ViewState v = view_of(s, view3d::Preset::top);
  (void)f.draw(s, v, f.params(s.stack));
  const view3d::PickId ndiff = pick_at(f, v, 5, 5, 0);
  const auto ref = render::decode_pick(s.mesh, ndiff);
  REQUIRE(ref.has_value());
  CHECK(ref->kind == render::PickKind::rect);
  const view3d::RectInstance& rect = s.mesh.chunks[ref->chunk].rects[ref->index];
  CHECK(rect.layer == kNdiff);
  CHECK(picked_layer(s, pick_at(f, v, 50, 50, 0)) == kPoly);
  CHECK(picked_layer(s, pick_at(f, v, 75, 30, 0)) == kMetal);
  CHECK(f.renderer.read_pick(-1, 0) == view3d::kNoPick);
  CHECK(f.renderer.read_pick(kWidth, 0) == view3d::kNoPick);
}

TEST_CASE("3D pick sees through a transparent layer and skips hidden ones",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  Fixture f;
  const Scene s = basic_scene();
  const view3d::ViewState v = view_of(s, view3d::Preset::top);
  const auto [px, py] = project(v, 50, 30, 0);  // metal1 over poly (R1)

  render::TargetReadback r = f.draw(s, v, f.params(s.stack));
  CHECK(picked_layer(s, r.pick[pixel(r, px, py)]) == kMetal);
  const auto opaque = rgb_of(r.color[pixel(r, px, py)]);

  for (const int level : {0, 1}) {
    INFO("level " << level);
    render::DrawParams p = f.params(s.stack, level);
    p.alpha = {1.0F, 1.0F, 0.5F};  // metal1 half transparent
    r = f.draw(s, v, p);
    CHECK(picked_layer(s, r.pick[pixel(r, px, py)]) == kPoly);
    const auto paints = render::layer_paints(s.stack, &f.styles);
    const float top = board(v, {0, 0, 1});
    const auto poly = render::lit_color(paints[kPoly].rgb, top);
    const auto metal = render::lit_color(paints[kMetal].rgb, top);
    const auto blended = rgb_of(r.color[pixel(r, px, py)]);
    for (std::size_t i = 0; i < 3; ++i) {
      // Blended in linear light (the sRGB view), half and half.
      const float lin = (0.5F * render::srgb_to_linear(poly[i])) + (0.5F * render::srgb_to_linear(metal[i]));
      CHECK(std::abs(blended[i] - static_cast<int>(std::lround(render::linear_to_srgb(lin) * 255.0F))) <= 2);
    }
    CHECK(blended != opaque);
  }

  render::DrawParams hidden = f.params(s.stack);
  hidden.hidden = {false, false, true};
  r = f.draw(s, v, hidden);
  CHECK(picked_layer(s, r.pick[pixel(r, px, py)]) == kPoly);
  const auto [qx, qy] = project(v, 75, 30, 0);  // metal1 alone
  CHECK(r.pick[pixel(r, qx, qy)] == view3d::kNoPick);
}

TEST_CASE("3D renderer places rectangles from the model's layer heights (J1 span), not the stack's",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  Fixture f;
  Scene s = basic_scene();
  const view3d::ViewState v = view_of(s, view3d::Preset::top);
  REQUIRE(s.mesh.layer_heights.size() > kPoly);
  (void)f.draw(s, v, f.params(s.stack));
  CHECK(picked_layer(s, pick_at(f, v, 50, 30, 0)) == kMetal);  // metal1 (h 1.0) in front of poly (h 0.3)

  // The model's heights win over the stack's: poly lifted above metal1 now
  // lies in front of it, as a contact spanned by the J1 option would.
  s.mesh.layer_heights[kPoly] = view3d::DrawHeight{2.0F, 0.2F};
  (void)f.draw(s, v, f.params(s.stack));
  CHECK(picked_layer(s, pick_at(f, v, 50, 30, 0)) == kPoly);
}

TEST_CASE("3D renderer dims what is not in the highlight set",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  Fixture f;
  const Scene s = basic_scene();
  const view3d::ViewState v = view_of(s, view3d::Preset::top);
  render::TargetReadback r = f.draw(s, v, f.params(s.stack));
  const auto [mx, my] = project(v, 75, 30, 0);
  const auto [nx, ny] = project(v, 5, 5, 0);
  const view3d::PickId metal = r.pick[pixel(r, mx, my)];
  const auto metal_full = rgb_of(r.color[pixel(r, mx, my)]);
  const auto ndiff_full = rgb_of(r.color[pixel(r, nx, ny)]);

  view3d::HighlightSet set{{metal}};
  render::DrawParams p = f.params(s.stack);
  p.highlight = &set;
  r = f.draw(s, v, p);
  CHECK(rgb_of(r.color[pixel(r, mx, my)]) == metal_full);
  const auto dimmed = rgb_of(r.color[pixel(r, nx, ny)]);
  for (std::size_t i = 0; i < 3; ++i) {
    const float c = (static_cast<float>(ndiff_full[i]) / 255.0F) * 0.35F;  // dimmed in display values
    CHECK(std::abs(dimmed[i] - static_cast<int>(std::lround(c * 255.0F))) <= 2);
  }
}

TEST_CASE("3D renderer rejects another target", "[cap:view-3d][req:Platform mechanism of the 3D view]") {
  Fixture f;
  render::VulkanOffscreenTarget other(f.canvas->vulkan_device(), 8, 8);
  const view3d::LayerStack stack;
  CHECK_THROWS_AS(f.renderer.draw(view3d::ViewState{}, f.params(stack), other), std::invalid_argument);
}

// --- round 4: the board look (Q-MISC-84, Q-MISC-85; tasks 16.2-16.4) ---------

TEST_CASE("board lighting: top faces in the style color, the two visible sides darker and distinct",
          "[cap:view-3d][req:Viewing transform and shading]") {
  // Q-MISC-84 (V1, both modes): flat per-face directional shading on the
  // face's own normal in the displayed scene; never lighter than the style.
  const auto iso = render::board_light(45.0F);
  const auto at = [&](std::array<float, 3> n) { return render::light_intensity(render::Lighting::board, iso, n); };
  CHECK(at({0, 0, 1}) == 1.0F);
  const float left = at({-1, 0, 0});  // the -x faces, screen left in the isometric preset
  const float right = at({0, -1, 0});  // the -y faces, screen right
  CHECK(std::abs(left - 0.73F) < 1e-4F);
  CHECK(std::abs(right - 0.64F) < 1e-4F);
  CHECK(left - right > 0.05F);
  CHECK(at({1, 0, 0}) == render::kBoardAmbient);
  CHECK(at({0, 1, 0}) == render::kBoardAmbient);
  CHECK(at({0, 0, -1}) == render::kBoardAmbient);
  CHECK(at({0, 0, 0}) == render::kBoardAmbient);
  // The light turns with the camera's azimuth: tops stay at 1 and the faces
  // toward the screen's left stay the lighter side.
  for (float view_z = 0.0F; view_z < 360.0F; view_z += 30.0F) {
    INFO("view_z " << view_z);
    const auto l = render::board_light(view_z);
    CHECK(std::abs(render::light_intensity(render::Lighting::board, l, {0, 0, 1}) - 1.0F) < 1e-6F);
    CHECK(std::abs((l[0] * l[0]) + (l[1] * l[1]) + (l[2] * l[2]) - 1.0F) < 1e-5F);
  }
  // Orbited 90 degrees (view_z 135), the viewer is toward (-1, 1, 1): the +y
  // faces are on the screen left, the -x faces on the right.
  const auto turned = render::board_light(135.0F);
  CHECK(std::abs(render::light_intensity(render::Lighting::board, turned, {0, 1, 0}) - left) < 1e-5F);
  CHECK(std::abs(render::light_intensity(render::Lighting::board, turned, {-1, 0, 0}) - right) < 1e-5F);
}

namespace {

/// The interior pixels of `r` whose pick decodes to a face of `layer` with
/// outward normal `normal` (a rect or triangle top: (0, 0, 1)).
std::vector<std::uint32_t> face_pixels(const Scene& s, const render::TargetReadback& r, view3d::LayerId layer,
                                       const std::array<float, 3>& normal) {
  std::vector<std::uint32_t> out;
  for (int y = 0; y < r.height; ++y) {
    for (int x = 0; x < r.width; ++x) {
      if (!interior(r, x, y)) continue;
      const auto ref = render::decode_pick(s.mesh, r.pick[pixel(r, x, y)]);
      if (!ref.has_value() || layer_of(s.mesh, *ref) != layer) continue;
      const auto n = face_normal(s.mesh, *ref);
      if (std::abs(n[0] - normal[0]) + std::abs(n[1] - normal[1]) + std::abs(n[2] - normal[2]) < 1e-3F) {
        out.push_back(r.color[pixel(r, x, y)]);
      }
    }
  }
  return out;
}

/// The mean pixel row of the faces of `layer` of kind `kind`.
double mean_row(const Scene& s, const render::TargetReadback& r, view3d::LayerId layer, render::PickKind kind) {
  double sum = 0.0;
  int n = 0;
  for (int y = 0; y < r.height; ++y) {
    for (int x = 0; x < r.width; ++x) {
      const auto ref = render::decode_pick(s.mesh, r.pick[pixel(r, x, y)]);
      if (ref.has_value() && ref->kind == kind && layer_of(s.mesh, *ref) == layer) {
        sum += y;
        ++n;
      }
    }
  }
  REQUIRE(n > 0);
  return sum / n;
}

}  // namespace

TEST_CASE("3D renderer shades a prism flat per face in the isometric preset, top lighter than its sides",
          "[cap:view-3d][req:Viewing transform and shading]") {
  Fixture f;
  for (const bool magic_compat : {true, false}) {
    INFO("magic-compat " << (magic_compat ? "ON" : "OFF"));
    const Scene s = basic_scene(magic_compat);
    const view3d::ViewState v = view_of(s, view3d::Preset::isometric);
    const render::TargetReadback r = f.draw(s, v, f.params(s.stack));
    const auto top = face_pixels(s, r, kMetal, {0, 0, 1});
    const auto left = face_pixels(s, r, kMetal, {-1, 0, 0});
    const auto right = face_pixels(s, r, kMetal, {0, -1, 0});
    REQUIRE(top.size() > 10);
    REQUIRE(!left.empty());
    REQUIRE(right.size() > 10);
    // Flat: every pixel of a face has one color.
    for (const auto* face : {&top, &left, &right}) {
      CHECK(std::all_of(face->begin(), face->end(), [&](std::uint32_t c) { return c == face->front(); }));
    }
    const auto t = rgb_of(top.front());
    const auto metal = render::layer_paints(s.stack, &f.styles)[kMetal].rgb;
    CHECK(near_rgb(t, metal, 1));  // the style color: nothing washed to white
    for (const auto* face : {&left, &right}) {
      const auto c = rgb_of(face->front());
      // Darker in every channel, by one factor (the same hue).
      std::array<double, 3> ratio{};
      for (std::size_t i = 0; i < 3; ++i) {
        CHECK(c[i] < t[i]);
        ratio[i] = static_cast<double>(c[i]) / static_cast<double>(t[i]);
      }
      CHECK(std::abs(ratio[0] - ratio[2]) < 0.04);
      CHECK(std::abs(ratio[1] - ratio[2]) < 0.04);
    }
    CHECK(rgb_of(left.front())[2] - rgb_of(right.front())[2] > 10);  // two distinct side shades
    for (const int c : t) CHECK(c < 255);
  }
}

TEST_CASE("3D renderer isometric preset looks from above, metal1 above poly, magic-compat ON and OFF",
          "[cap:view-3d][req:Viewing transform and shading]") {
  // Regression (Q-MISC-84): round 3 mirrored only the depth, so depth put
  // the viewer above while the projection still looked from Magic's
  // underside: tilted views were drawn upside down, the lowest layers at
  // the top of the screen. The model is now drawn mirrored in z.
  Fixture f;
  for (const bool magic_compat : {true, false}) {
    const Scene s = basic_scene(magic_compat);
    const view3d::ViewState v = view_of(s, view3d::Preset::isometric);
    for (const int level : {0, 1}) {
      INFO("magic-compat " << (magic_compat ? "ON" : "OFF") << ", level " << level);
      const render::TargetReadback r = f.draw(s, v, f.params(s.stack, level));
      // metal1's top face (h + t 1.5) lies higher on the screen than poly's
      // (0.5), though its footprint's x + y is about the same.
      CHECK(mean_row(s, r, kMetal, render::PickKind::rect) + 3.0 <
            mean_row(s, r, kPoly, render::PickKind::rect));
      // The faces shown are the h + t ones, where the projection puts them.
      const double metal_top = far_end_z(v, 1.0F, 0.5F);
      const auto [mx, my] = project(v, 70, 30, metal_top);
      const auto ref = render::decode_pick(s.mesh, r.pick[pixel(r, mx, my)]);
      REQUIRE(ref.has_value());
      CHECK(ref->kind == render::PickKind::rect);
      CHECK(layer_of(s.mesh, *ref) == kMetal);
      CHECK(std::abs(r.depth[pixel(r, mx, my)] - depth_of(v, 70, 30, metal_top)) < 1e-4F);
      const double poly_top = far_end_z(v, 0.3F, 0.2F);
      const auto [px, py] = project(v, 55, 5, poly_top);  // poly alone, in front of metal1
      CHECK(picked_layer(s, r.pick[pixel(r, px, py)]) == kPoly);
      CHECK(std::abs(r.depth[pixel(r, px, py)] - depth_of(v, 55, 5, poly_top)) < 1e-4F);
    }
  }
}

TEST_CASE("3D renderer front and side presets show the higher layers up",
          "[cap:view-3d][req:Viewing transform and shading]") {
  Fixture f;
  const Scene s = basic_scene();
  for (const view3d::Preset preset : {view3d::Preset::front, view3d::Preset::side}) {
    INFO("preset " << static_cast<int>(preset));
    const render::TargetReadback r = f.draw(s, view_of(s, preset), f.params(s.stack));
    CHECK(mean_row(s, r, kMetal, render::PickKind::side) < mean_row(s, r, kNdiff, render::PickKind::side));
  }
}

TEST_CASE("3D renderer clears to DrawParams::background", "[cap:view-3d][req:Viewing transform and shading]") {
  // Q-MISC-84: the desktop tab passes its theme token; unset, the
  // transparent style's color (Magic's w3dClear).
  Fixture f;
  const Scene s = basic_scene();
  const view3d::ViewState v = view_of(s, view3d::Preset::isometric);
  for (const int level : {0, 1}) {
    INFO("level " << level);
    render::DrawParams p = f.params(s.stack, level);
    const render::TargetReadback plain = f.draw(s, v, p);
    REQUIRE(plain.pick[pixel(plain, 0, 0)] == view3d::kNoPick);
    CHECK(rgb_of(plain.color[pixel(plain, 0, 0)]) == std::array<int, 3>{40, 40, 40});
    p.background = std::array<float, 3>{0.1F, 0.4F, 0.7F};  // display (sRGB) values
    const render::TargetReadback r = f.draw(s, v, p);
    CHECK(near_rgb(rgb_of(r.color[pixel(r, 0, 0)]), *p.background, 1));
    CHECK(near_rgb(rgb_of(r.color[pixel(r, kWidth - 1, kHeight - 1)]), *p.background, 1));
    CHECK(r.pick == plain.pick);
  }
}

namespace {

constexpr view3d::LayerId kDiff = 0;  // green, h 0, t 0.2
constexpr view3d::LayerId kWell = 1;  // blue, h -0.2, t 0.6: encloses the diff

/// A diff inside a well that covers the whole cell (Q-MISC-85).
Scene well_scene(bool magic_compat) {
  Scene s;
  std::vector<view3d::StackLayer> layers{view3d::test::layer("pdiff", s.tech.ndiff, 0.0F, 0.2F, kGreen),
                                         view3d::test::layer("nwell", s.tech.metal1, -0.2F, 0.6F, kBlue)};
  s.stack = view3d::LayerStack{view3d::Mode::layout_layer, std::move(layers), magic_compat, 0.01};
  db::CellDef cell("well", s.tech.tech->plane_count());
  view3d::test::paint(cell, s.tech, Rect{20, 20, 40, 40}, s.tech.ndiff);
  view3d::test::paint(cell, s.tech, Rect{0, 0, 60, 60}, s.tech.metal1);
  finish(s, cell, Rect{-1, -1, 61, 61}, false);
  return s;
}

}  // namespace

TEST_CASE("a ghosted well shows the diff inside it, blended, and passes its pick through",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  // Q-MISC-85 (V2): wells and other non-conducting area layers are ghosted
  // (alpha < 1): drawn after the opaque layers without depth writes or
  // pick ids, from every side.
  Fixture f;
  for (const bool magic_compat : {true, false}) {
    const Scene s = well_scene(magic_compat);
    const auto paints = render::layer_paints(s.stack, &f.styles);
    for (const view3d::Preset preset : {view3d::Preset::top, view3d::Preset::isometric}) {
      const view3d::ViewState v = view_of(s, preset);
      for (const int level : {0, 1}) {
        INFO("magic-compat " << (magic_compat ? "ON" : "OFF") << ", preset " << static_cast<int>(preset)
                             << ", level " << level);
        render::DrawParams p = f.params(s.stack, level);
        p.alpha = {1.0F, 0.2F};
        const render::TargetReadback r = f.draw(s, v, p);
        const double diff_top = far_end_z(v, 0.0F, 0.2F);
        const auto [px, py] = project(v, 30, 30, diff_top);
        CHECK(picked_layer(s, r.pick[pixel(r, px, py)]) == kDiff);
        CHECK(std::abs(r.depth[pixel(r, px, py)] - depth_of(v, 30, 30, diff_top)) < 1e-4F);
        // The well's top face over the diff's: 0.2 of the well, in linear light.
        const auto c = rgb_of(r.color[pixel(r, px, py)]);
        for (std::size_t i = 0; i < 3; ++i) {
          const float lin = (0.2F * render::srgb_to_linear(paints[kWell].rgb[i])) +
                            (0.8F * render::srgb_to_linear(paints[kDiff].rgb[i]));
          CHECK(std::abs(c[i] - static_cast<int>(std::lround(render::linear_to_srgb(lin) * 255.0F))) <= 2);
        }
      }
    }
  }
}

TEST_CASE("a ghosted well's side faces write no depth and no pick id",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  Fixture f;
  const Scene s = well_scene(false);
  const view3d::ViewState v = view_of(s, view3d::Preset::isometric);
  // A point on the well's -y side face with nothing opaque behind it.
  const auto [px, py] = project(v, 30, 0, -0.1 * static_cast<double>(v.scale_z));
  render::DrawParams p = f.params(s.stack);
  const render::TargetReadback opaque = f.draw(s, v, p);
  const auto ref = render::decode_pick(s.mesh, opaque.pick[pixel(opaque, px, py)]);
  REQUIRE(ref.has_value());
  REQUIRE(ref->kind == render::PickKind::side);
  REQUIRE(layer_of(s.mesh, *ref) == kWell);
  p.alpha = {1.0F, 0.2F};
  const render::TargetReadback ghost = f.draw(s, v, p);
  CHECK(ghost.pick[pixel(ghost, px, py)] == view3d::kNoPick);
  CHECK(ghost.depth[pixel(ghost, px, py)] == 1.0F);
  CHECK(ghost.color[pixel(ghost, px, py)] != opaque.color[pixel(opaque, px, py)]);
}

TEST_CASE("a ghosted layer never hides an opaque one, in every preset",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  // The pick image and the depth are those of the scene without the ghost.
  Fixture f;
  const Scene s = well_scene(true);
  for (const view3d::Preset preset :
       {view3d::Preset::top, view3d::Preset::isometric, view3d::Preset::front, view3d::Preset::side}) {
    INFO("preset " << static_cast<int>(preset));
    const view3d::ViewState v = view_of(s, preset);
    render::DrawParams p = f.params(s.stack);
    p.hidden = {false, true};
    const render::TargetReadback without = f.draw(s, v, p);
    p.hidden = {};
    p.alpha = {1.0F, 0.2F};
    const render::TargetReadback ghost = f.draw(s, v, p);
    CHECK(ghost.pick == without.pick);
    CHECK(ghost.depth == without.depth);
  }
}
