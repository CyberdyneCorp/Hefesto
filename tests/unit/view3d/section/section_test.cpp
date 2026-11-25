// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cross-sections (openspec/changes/m10-3d-view tasks.md 11.1; design D10
// "Cross-sections (P2)"; hefesto-3d-visualization "Interactive 3D
// inspection", scenario "Cross-section"): the covered intervals of an
// axis-aligned cut line per layer, in µm from its start, with the layer's
// physical height, read from the mesh model. Owner: INSPECT (P2).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../mesh/mesh_fixture.hpp"
#include "hefesto/view3d/section/section.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace view3d = hefesto::view3d;
using Catch::Matchers::WithinAbs;
using db::Rect;
using view3d::CrossSection;

namespace {

constexpr double kMpu = 0.01;  // µm per internal unit

// --- a synthetic inverter -----------------------------------------------------

struct InvTech {
  std::shared_ptr<const tech::Technology> tech;
  tech::TileType nwell = 0;
  tech::TileType ndiff = 0;
  tech::TileType pdiff = 0;
  tech::TileType nfet = 0;
  tech::TileType pfet = 0;
  tech::TileType metal1 = 0;
};

InvTech inverter_tech() {
  tech::TechnologyBuilder b;
  b.set_name("inv");
  REQUIRE(b.add_plane("well").ok());
  REQUIRE(b.add_plane("active").ok());
  REQUIRE(b.add_plane("metal1").ok());
  InvTech t;
  t.nwell = b.add_type("well", "nwell").value();
  t.ndiff = b.add_type("active", "ndiff").value();
  t.pdiff = b.add_type("active", "pdiff").value();
  t.nfet = b.add_type("active", "nfet").value();
  t.pfet = b.add_type("active", "pfet").value();
  t.metal1 = b.add_type("metal1", "metal1").value();
  auto fin = b.finalize();
  REQUIRE(fin.ok());
  t.tech = fin.value();
  return t;
}

// Layer ids of the inverter stack (internal units: h, t).
constexpr view3d::LayerId kNwell = 0;   // 0, 20
[[maybe_unused]] constexpr view3d::LayerId kNdiff = 1;   // 10, 10
constexpr view3d::LayerId kPdiff = 2;   // 10, 10
constexpr view3d::LayerId kNfet = 3;    // 10, 40
constexpr view3d::LayerId kPfet = 4;    // 10, 40
constexpr view3d::LayerId kMetal1 = 5;  // 100, 40

view3d::LayerStack inverter_stack(const InvTech& t, bool magic_compat, double mpu = kMpu) {
  using view3d::test::layer;
  std::vector<view3d::StackLayer> layers{layer("nwell", t.nwell, 0, 20),  layer("ndiff", t.ndiff, 10, 10),
                                         layer("pdiff", t.pdiff, 10, 10), layer("nfet", t.nfet, 10, 40),
                                         layer("pfet", t.pfet, 10, 40),   layer("metal1", t.metal1, 100, 40)};
  std::vector<view3d::DrawHeight> heights(tech::kMaxTileTypes);
  for (const auto& l : layers) heights[*l.type] = view3d::DrawHeight{l.height, l.thickness};
  return view3d::LayerStack{std::move(layers), std::move(heights), magic_compat, mpu};
}

void paint(db::CellDef& cell, const InvTech& t, const Rect& r, tech::TileType type) {
  REQUIRE(db::paint(cell, *t.tech, r, type).ok());
}

/// The nmos at the bottom, the pmos in an nwell at the top, the gates at
/// x 45..55, supply rails and an output strap in metal1.
view3d::MeshModel inverter_mesh(const InvTech& t, const view3d::LayerStack& stack) {
  db::CellDef cell("inv", t.tech->plane_count());
  paint(cell, t, Rect{0, 60, 100, 120}, t.nwell);
  paint(cell, t, Rect{20, 10, 45, 40}, t.ndiff);
  paint(cell, t, Rect{45, 10, 55, 40}, t.nfet);
  paint(cell, t, Rect{55, 10, 80, 40}, t.ndiff);
  paint(cell, t, Rect{20, 70, 45, 100}, t.pdiff);
  paint(cell, t, Rect{45, 70, 55, 100}, t.pfet);
  paint(cell, t, Rect{55, 70, 80, 100}, t.pdiff);
  paint(cell, t, Rect{0, 0, 100, 10}, t.metal1);
  paint(cell, t, Rect{0, 110, 100, 120}, t.metal1);
  paint(cell, t, Rect{60, 40, 70, 105}, t.metal1);
  db::recompute_bbox(cell);
  auto mesh = view3d::test::build(stack, cell, Rect{-1, -1, 101, 121}, false);
  REQUIRE(mesh.has_value());
  return std::move(*mesh);
}

struct Expected {
  view3d::LayerId layer;
  double from, to, bottom, top;
};

void check_intervals(const CrossSection& s, const std::vector<Expected>& want) {
  REQUIRE(s.intervals.size() == want.size());
  for (std::size_t i = 0; i < want.size(); ++i) {
    INFO("interval " << i);
    const auto& iv = s.intervals[i];
    CHECK(iv.layer == want[i].layer);
    CHECK_THAT(iv.from_um, WithinAbs(want[i].from, 1e-9));
    CHECK_THAT(iv.to_um, WithinAbs(want[i].to, 1e-9));
    CHECK_THAT(iv.bottom_um, WithinAbs(want[i].bottom, 1e-7));
    CHECK_THAT(iv.top_um, WithinAbs(want[i].top, 1e-7));
  }
}

bool has_code(const CrossSection& s, std::string_view code) {
  for (const auto& d : s.diagnostics) {
    if (d.code == code) return true;
  }
  return false;
}

}  // namespace

TEST_CASE("cross-section of an inverter through the pmos: intervals and heights match the layer stack",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const InvTech t = inverter_tech();
  for (const bool magic_compat : {true, false}) {
    INFO("magic-compat " << (magic_compat ? "ON" : "OFF"));
    const auto stack = inverter_stack(t, magic_compat);
    const auto mesh = inverter_mesh(t, stack);
    const CrossSection s = view3d::section(mesh, stack, view3d::Segment{{0, 85}, {100, 85}});
    check_intervals(s, {{kNwell, 0.0, 1.0, 0.0, 0.2},
                        {kPdiff, 0.2, 0.45, 0.1, 0.2},
                        {kPdiff, 0.55, 0.8, 0.1, 0.2},
                        {kPfet, 0.45, 0.55, 0.1, 0.5},
                        {kMetal1, 0.6, 0.7, 1.0, 1.4}});
    CHECK(s.no_height.empty());
    CHECK(s.diagnostics.empty());
    CHECK_THAT(s.length_um, WithinAbs(1.0, 1e-12));
  }
}

TEST_CASE("cross-section of an inverter along the gates: a vertical line, merged rails",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const InvTech t = inverter_tech();
  const auto stack = inverter_stack(t, true);
  const auto mesh = inverter_mesh(t, stack);
  const CrossSection s = view3d::section(mesh, stack, view3d::Segment{{50, 0}, {50, 120}});
  check_intervals(s, {{kNwell, 0.6, 1.2, 0.0, 0.2},
                      {kNfet, 0.1, 0.4, 0.1, 0.5},
                      {kPfet, 0.7, 1.0, 0.1, 0.5},
                      {kMetal1, 0.0, 0.1, 1.0, 1.4},
                      {kMetal1, 1.1, 1.2, 1.0, 1.4}});
}

TEST_CASE("cross-section positions run from the line's start, also when it is drawn right to left",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const InvTech t = inverter_tech();
  const auto stack = inverter_stack(t, false);
  const auto mesh = inverter_mesh(t, stack);
  const CrossSection s = view3d::section(mesh, stack, view3d::Segment{{90, 85}, {10, 85}});
  check_intervals(s, {{kNwell, 0.0, 0.8, 0.0, 0.2},
                      {kPdiff, 0.1, 0.35, 0.1, 0.2},
                      {kPdiff, 0.45, 0.7, 0.1, 0.2},
                      {kPfet, 0.35, 0.45, 0.1, 0.5},
                      {kMetal1, 0.2, 0.3, 1.0, 1.4}});
  CHECK_THAT(s.length_um, WithinAbs(0.8, 1e-12));
}

TEST_CASE("a cut line along a tile edge samples the material above it (a tile owns its bottom edge)",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const InvTech t = inverter_tech();
  const auto stack = inverter_stack(t, true);
  const auto mesh = inverter_mesh(t, stack);
  const auto layers_on = [&](db::Coord y) {
    std::vector<view3d::LayerId> out;
    for (const auto& iv : view3d::section(mesh, stack, view3d::Segment{{0, y}, {100, y}}).intervals) {
      if (out.empty() || out.back() != iv.layer) out.push_back(iv.layer);
    }
    return out;
  };
  CHECK(layers_on(70) == std::vector<view3d::LayerId>{kNwell, kPdiff, kPfet, kMetal1});  // pdiff's bottom edge
  CHECK(layers_on(100) == std::vector<view3d::LayerId>{kNwell, kMetal1});                // pdiff's top edge
}

TEST_CASE("a cut line that is not axis-aligned, or a point, gives an empty section and VIEW3D-SECTION-001",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const InvTech t = inverter_tech();
  const auto stack = inverter_stack(t, true);
  const auto mesh = inverter_mesh(t, stack);
  for (const view3d::Segment line : {view3d::Segment{{0, 0}, {100, 120}}, view3d::Segment{{50, 50}, {50, 50}}}) {
    const CrossSection s = view3d::section(mesh, stack, line);
    CHECK(s.intervals.empty());
    CHECK(s.no_height.empty());
    CHECK(s.length_um == 0.0);
    REQUIRE(s.diagnostics.size() == 1);
    CHECK(s.diagnostics[0].code == "VIEW3D-SECTION-001");
    CHECK(s.diagnostics[0].severity == hefesto::diag::Severity::error);
  }
}

TEST_CASE("cross-section leaves out the layers hidden in the 3D view",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const InvTech t = inverter_tech();
  const auto stack = inverter_stack(t, true);
  const auto mesh = inverter_mesh(t, stack);
  const view3d::Segment line{{0, 85}, {100, 85}};
  std::vector<bool> hidden(stack.size(), false);
  hidden[kNwell] = true;
  hidden[kPdiff] = true;
  const CrossSection s = view3d::section(mesh, stack, line, hidden);
  check_intervals(s, {{kPfet, 0.45, 0.55, 0.1, 0.5}, {kMetal1, 0.6, 0.7, 1.0, 1.4}});
  // The three-argument form keeps every layer of the mesh.
  CHECK(view3d::section(mesh, stack, line).intervals.size() == 5);
}

TEST_CASE("cross-section merges abutting pieces of one layer from abutting cells",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const InvTech t = inverter_tech();
  const auto stack = inverter_stack(t, true);
  db::CellDef leaf("leaf", t.tech->plane_count());
  paint(leaf, t, Rect{0, 0, 50, 10}, t.metal1);
  db::recompute_bbox(leaf);
  db::CellDef top("top", t.tech->plane_count());
  view3d::test::place(top, leaf, db::Transform{1, 0, 0, 0, 1, 0}, "a");
  view3d::test::place(top, leaf, db::Transform{1, 0, 50, 0, 1, 0}, "b");
  const auto mesh = view3d::test::build(stack, top, Rect{-1, -1, 101, 11}, false);
  REQUIRE(mesh.has_value());
  const CrossSection s = view3d::section(*mesh, stack, view3d::Segment{{-1, 5}, {101, 5}});
  check_intervals(s, {{kMetal1, 0.01, 1.01, 1.0, 1.4}});
}

TEST_CASE("cross-section merges pieces of one layer from different chunks",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const InvTech t = inverter_tech();
  const auto stack = inverter_stack(t, false);
  view3d::MeshModel mesh;
  mesh.region = Rect{0, 0, 100, 10};
  mesh.layer_heights.resize(stack.size());
  for (std::size_t i = 0; i < stack.size(); ++i) mesh.layer_heights[i] = stack.draw_height(static_cast<view3d::LayerId>(i));
  view3d::Chunk a;
  a.id = 0;
  a.area = Rect{0, 0, 50, 10};
  a.rects.push_back(view3d::RectInstance{0, 0, 50, 10, kMetal1});
  view3d::Chunk b;
  b.id = 1;
  b.origin_x = 50.0;
  b.area = Rect{50, 0, 100, 10};
  b.rects.push_back(view3d::RectInstance{0, 0, 30, 10, kMetal1});
  b.rects.push_back(view3d::RectInstance{30, 0, 50, 10, kMetal1});
  mesh.chunks = {a, b};
  const CrossSection s = view3d::section(mesh, stack, view3d::Segment{{0, 5}, {100, 5}});
  check_intervals(s, {{kMetal1, 0.0, 1.0, 1.0, 1.4}});
}

TEST_CASE("cross-section cuts split-tile triangles and merges them with the abutting rectangle",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const InvTech t = inverter_tech();
  const auto stack = inverter_stack(t, true);
  db::CellDef cell("split", t.tech->plane_count());
  // The lower-right triangle (0,0) (40,0) (40,40), then a rectangle to its right.
  REQUIRE(db::paint_triangle(cell, *t.tech, Rect{0, 0, 40, 40}, db::Diagonal::ne_sw, true, t.metal1).ok());
  paint(cell, t, Rect{40, 0, 60, 40}, t.metal1);
  db::recompute_bbox(cell);
  const auto mesh = view3d::test::build(stack, cell, Rect{-1, -1, 61, 41}, false);
  REQUIRE(mesh.has_value());
  REQUIRE(!view3d::test::triangles_of(*mesh, kMetal1).empty());
  check_intervals(view3d::section(*mesh, stack, view3d::Segment{{0, 10}, {60, 10}}),
                  {{kMetal1, 0.1, 0.6, 1.0, 1.4}});
  check_intervals(view3d::section(*mesh, stack, view3d::Segment{{20, 0}, {20, 40}}),
                  {{kMetal1, 0.0, 0.2, 1.0, 1.4}});
}

TEST_CASE("a split tile's triangle is reported at its own layer's height in both magic-compat modes",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  // With magic-compat ON the 3D view draws the pfet triangle with its left
  // type's (pdiff's) h and t (Q-MISC-79); the section reports µm heights of
  // the layer in both modes (Q-MISC-72).
  const InvTech t = inverter_tech();
  for (const bool magic_compat : {true, false}) {
    INFO("magic-compat " << (magic_compat ? "ON" : "OFF"));
    const auto stack = inverter_stack(t, magic_compat);
    db::CellDef cell("split", t.tech->plane_count());
    paint(cell, t, Rect{0, 0, 40, 40}, t.pdiff);
    REQUIRE(db::paint_triangle(cell, *t.tech, Rect{0, 0, 40, 40}, db::Diagonal::ne_sw, true, t.pfet).ok());
    db::recompute_bbox(cell);
    const auto mesh = view3d::test::build(stack, cell, Rect{-1, -1, 41, 41}, false);
    REQUIRE(mesh.has_value());
    const CrossSection s = view3d::section(*mesh, stack, view3d::Segment{{0, 10}, {40, 10}});
    check_intervals(s, {{kPdiff, 0.0, 0.1, 0.1, 0.2}, {kPfet, 0.1, 0.4, 0.1, 0.5}});
  }
}

namespace {

struct ViaTech {
  std::shared_ptr<const tech::Technology> tech;
  tech::TileType m1 = 0;
  tech::TileType m2 = 0;
  tech::TileType via = 0;
};

ViaTech via_tech() {
  tech::TechnologyBuilder b;
  b.set_name("span");
  REQUIRE(b.add_plane("metal1").ok());
  REQUIRE(b.add_plane("metal2").ok());
  ViaTech t;
  t.m1 = b.add_type("metal1", "m1").value();
  t.m2 = b.add_type("metal2", "m2").value();
  t.via = b.add_type("metal1", "via").value();
  const std::array<std::string_view, 2> residues{"m1", "m2"};
  REQUIRE(b.add_contact("via", residues).ok());
  REQUIRE(b.add_connect("m1,via", "m1,via").ok());
  REQUIRE(b.add_connect("m2,via", "m2,via").ok());
  auto fin = b.finalize();
  REQUIRE(fin.ok());
  t.tech = fin.value();
  return t;
}

}  // namespace

TEST_CASE("cross-section of a contact spanned by \"Span connected layers\" uses the spanned height",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const ViaTech t = via_tech();
  // m1 100..150, m2 300..350, via (tech) 120..150; spanned: 150..300.
  std::vector<view3d::DrawHeight> heights(tech::kMaxTileTypes);
  heights[t.m1] = {100, 50};
  heights[t.m2] = {300, 50};
  heights[t.via] = {120, 30};
  const view3d::LayerStack stack{{view3d::test::layer("m1", t.m1, 100, 50), view3d::test::layer("m2", t.m2, 300, 50),
                                  view3d::test::layer("via", t.via, 120, 30)},
                                 std::move(heights), true, kMpu};
  db::CellDef root("root", t.tech->plane_count());
  REQUIRE(db::paint(root, *t.tech, Rect{0, 0, 10, 10}, t.m1).ok());
  REQUIRE(db::paint(root, *t.tech, Rect{0, 0, 10, 10}, t.m2).ok());
  REQUIRE(db::paint(root, *t.tech, Rect{2, 2, 6, 6}, t.via).ok());
  db::recompute_bbox(root);
  for (const bool span : {false, true}) {
    INFO("span " << span);
    view3d::BuildContext ctx;
    ctx.span_connected_layers = span;
    ctx.technology = t.tech.get();
    const auto mesh = view3d::build_from_layout(stack, root, view3d::ClipArea{Rect{-1, -1, 11, 11}, false, 0}, ctx);
    REQUIRE(mesh.has_value());
    const CrossSection s = view3d::section(*mesh, stack, view3d::Segment{{0, 4}, {10, 4}});
    const double via_bottom = span ? 1.5 : 1.2;
    const double via_top = span ? 3.0 : 1.5;
    // The via (a contact image on both planes) splits m1 and m2 around it;
    // the via's own interval carries the (spanned) height.
    CrossSection via_only = s;
    std::erase_if(via_only.intervals, [](const CrossSection::Interval& iv) { return iv.layer != 2; });
    check_intervals(via_only, {{2, 0.02, 0.06, via_bottom, via_top}});
    for (const auto& iv : s.intervals) {
      if (iv.layer == 0) CHECK_THAT(iv.top_um, WithinAbs(1.5, 1e-7));
      if (iv.layer == 1) CHECK_THAT(iv.bottom_um, WithinAbs(3.0, 1e-7));
    }
  }
}

TEST_CASE("cross-section reports layers without a height (VIEW3D-STACK-001 in mask-layer mode)",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  // MET1 has a `render` height (µm), MET2 none: it is left out and named.
  // µm in both magic-compat modes (Q-MISC-72), though the draw heights differ.
  for (const bool magic_compat : {true, false}) {
    INFO("magic-compat " << (magic_compat ? "ON" : "OFF"));
    view3d::StackLayer met1;
    met1.name = "MET1";
    met1.mask_layer = 0;
    met1.height = 1.0F;
    met1.thickness = 0.36F;
    met1.source = view3d::ValueSource::render;
    view3d::StackLayer met2;
    met2.name = "MET2";
    met2.mask_layer = 1;
    const view3d::LayerStack stack{view3d::Mode::mask_layer, {met1, met2}, magic_compat, 0.005};
    view3d::MeshModel mesh;
    mesh.mode = view3d::Mode::mask_layer;
    mesh.region = Rect{0, 0, 200, 100};
    mesh.layer_heights = {stack.draw_height(0), stack.draw_height(1)};
    view3d::Chunk c;
    c.area = mesh.region;
    c.rects = {view3d::RectInstance{0, 0, 100, 100, 0}, view3d::RectInstance{100, 0, 200, 100, 1}};
    mesh.chunks = {c};
    const CrossSection s = view3d::section(mesh, stack, view3d::Segment{{0, 50}, {200, 50}});
    check_intervals(s, {{0, 0.0, 0.5, 1.0, 1.36}});
    CHECK(s.no_height == std::vector<view3d::LayerId>{1});
    REQUIRE(s.diagnostics.size() == 1);
    CHECK(s.diagnostics[0].code == "VIEW3D-STACK-001");
    CHECK(s.diagnostics[0].text.find("MET2") != std::string::npos);
    CHECK(s.diagnostics[0].text.find("MET1") == std::string::npos);
  }
}

TEST_CASE("cross-section of a mask-layer mesh in mask units: positions in µm along the line",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  // Regression: the mask-layer mesh is in mask units (internal units times
  // the scale factor, MeshModel::units_per_root) while the stack's
  // microns_per_unit is per internal unit, so positions came out f times
  // too long. Same in both magic-compat modes.
  for (const bool magic_compat : {true, false}) {
    INFO("magic-compat " << (magic_compat ? "ON" : "OFF"));
    view3d::StackLayer met1;
    met1.name = "MET1";
    met1.mask_layer = 0;
    met1.height = 1.0F;
    met1.thickness = 0.36F;
    met1.source = view3d::ValueSource::render;
    // 0.05 µm per internal unit, scale factor 10: 0.005 µm per mask unit.
    const view3d::LayerStack stack{view3d::Mode::mask_layer, {met1}, magic_compat, 0.05};
    view3d::MeshModel mesh;
    mesh.mode = view3d::Mode::mask_layer;
    mesh.units_per_root = 10;
    mesh.region = Rect{0, 0, 200, 100};
    mesh.layer_heights = {stack.draw_height(0)};
    view3d::Chunk c;
    c.area = mesh.region;
    c.rects = {view3d::RectInstance{0, 0, 100, 100, 0}};
    mesh.chunks = {c};
    const CrossSection s = view3d::section(mesh, stack, view3d::Segment{{0, 50}, {200, 50}});
    check_intervals(s, {{0, 0.0, 0.5, 1.0, 1.36}});
    CHECK_THAT(s.length_um, WithinAbs(1.0, 1e-9));
    CHECK(s.diagnostics.empty());
  }
}

TEST_CASE("cross-section in layout-layer mode without µm per unit names every layer it crosses (VIEW3D-SECTION-002)",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const InvTech t = inverter_tech();
  const auto stack = inverter_stack(t, true, 0.0);
  const auto mesh = inverter_mesh(t, stack);
  const CrossSection s = view3d::section(mesh, stack, view3d::Segment{{0, 85}, {100, 85}});
  CHECK(s.intervals.empty());
  CHECK(s.no_height == std::vector<view3d::LayerId>{kNwell, kPdiff, kPfet, kMetal1});
  CHECK(has_code(s, "VIEW3D-SECTION-002"));
  CHECK(s.length_um == 0.0);
}

TEST_CASE("a cut line outside the mesh gives an empty section without diagnostics",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  const InvTech t = inverter_tech();
  const auto stack = inverter_stack(t, true);
  const auto mesh = inverter_mesh(t, stack);
  const CrossSection s = view3d::section(mesh, stack, view3d::Segment{{0, 500}, {100, 500}});
  CHECK(s.intervals.empty());
  CHECK(s.no_height.empty());
  CHECK(s.diagnostics.empty());
}
