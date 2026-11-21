// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cross-workstream tests of M10 round 1 (INTEGRATION): the layout-layer
// LayerStack that TECH builds from a technology's extract style feeds the
// mesh MESH builds, and the client's Q-MISC-78 branch point forwards to
// the camera's rule. Owner: INTEGRATION.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "hefesto/db/cell.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/extract/tech/height_view.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/view3d/camera/view_state.hpp"
#include "hefesto/view3d/client/session.hpp"
#include "mesh/mesh_fixture.hpp"

namespace db = hefesto::db;
namespace diag = hefesto::diag;
namespace display = hefesto::display;
namespace extract = hefesto::extract;
namespace platform = hefesto::platform;
namespace tech = hefesto::tech;
namespace view3d = hefesto::view3d;

namespace {

std::shared_ptr<const tech::Technology> load(const std::string& extract_body) {
  const std::string text =
      "tech\n format 35\n stackmesh\nend\n"
      "planes\n active,a\n metal1,m1\nend\n"
      "types\n active polysilicon,poly\n metal1 metal1,m1\nend\n"
      "contact\nend\nstyles\n styletype mos\nend\ncompose\nend\nconnect\nend\n"
      "cifoutput\nend\ncifinput\nend\ndrc\nend\nextract\n" +
      extract_body + "end\n";
  platform::MemoryFileSystem fs;
  diag::CollectingSink sink;
  auto loaded = tech::load_technology_text(text, "/t/stackmesh.tech", fs, tech::LoadOptions{}, sink);
  REQUIRE(loaded.ok());
  return loaded.value();
}

tech::TileType type_of(const tech::Technology& t, std::string_view name) {
  const auto found = t.find_type(name);
  REQUIRE(found.ok());
  return *found.id;
}

constexpr std::size_t kPolyStyle = display::kFixedStyleCount + 1;
constexpr std::size_t kMetal1Style = display::kFixedStyleCount + 2;

struct Built {
  std::shared_ptr<const tech::Technology> tech;
  view3d::LayerStack stack;
  view3d::LayerId poly = 0;
  view3d::LayerId metal1 = 0;
};

// `with_heights` false: no extraction style at all (extract::HeightView{}).
Built stack_for(const std::string& extract_body, bool with_heights = true) {
  Built b;
  b.tech = load(extract_body);
  diag::NullSink sink;
  const auto heights = with_heights ? extract::HeightView::from_technology(*b.tech, sink) : extract::HeightView{};
  display::TypeStyleMap styles;
  styles.styles_of_type.resize(tech::kMaxTileTypes);
  styles.styles_of_type[type_of(*b.tech, "poly")] = {kPolyStyle};
  styles.styles_of_type[type_of(*b.tech, "metal1")] = {kMetal1Style};
  b.stack = view3d::build_layout_stack(*b.tech, heights, styles, true, 0.01);
  REQUIRE(b.stack.size() == 2);
  b.poly = *b.stack.find(type_of(*b.tech, "poly"), kPolyStyle);
  b.metal1 = *b.stack.find(type_of(*b.tech, "metal1"), kMetal1Style);
  return b;
}

void paint_cell(const Built& b, db::CellDef& def) {
  REQUIRE(db::paint(def, *b.tech, db::Rect{0, 0, 100, 40}, type_of(*b.tech, "metal1")).ok());
  REQUIRE(db::paint(def, *b.tech, db::Rect{10, 50, 30, 90}, type_of(*b.tech, "poly")).ok());
  db::recompute_bbox(def);
}

const std::string kStyle = " style s\n planeorder active 0\n planeorder metal1 1\n";

}  // namespace

TEST_CASE("a TECH layout stack drives the MESH faces of each layer", "[cap:view-3d][req:Rendered geometry]") {
  // metal1 is a prism from its `height` line; poly's thickness 0 takes the
  // extract default 0.05 (ExtTech.c), so poly is a prism too.
  const Built b = stack_for(kStyle + " height metal1 1.0 0.5\n height poly 0.3 0\n");
  CHECK(b.stack.draw_height(b.metal1) == view3d::DrawHeight{1.0F, 0.5F});
  CHECK(b.stack.draw_height(b.poly) == view3d::DrawHeight{0.3F, 0.05F});
  db::CellDef def("top", b.tech->plane_count());
  paint_cell(b, def);
  const auto mesh = view3d::test::build(b.stack, def, db::Rect{-10, -10, 110, 100});
  REQUIRE(mesh.has_value());
  CHECK(view3d::test::face_area(*mesh, b.metal1) == 100.0 * 40.0);
  CHECK(view3d::test::side_length(*mesh, b.metal1) == 2.0 * (100.0 + 40.0));
  CHECK(view3d::test::open_edge(*mesh, b.metal1).empty());
  CHECK(view3d::test::face_area(*mesh, b.poly) == 20.0 * 40.0);
  CHECK(view3d::test::side_length(*mesh, b.poly) == 2.0 * (20.0 + 40.0));
  CHECK(view3d::test::open_edge(*mesh, b.poly).empty());
}

TEST_CASE("without an extract style every layer is flat: top faces only",
          "[cap:view-3d][req:Rendered geometry]") {
  // h = t = 0 for every layer (no source), so the drawn thickness is 0 and
  // the mesh has no side faces (W3Dmain.c:287).
  const Built b = stack_for("", false);
  CHECK(b.stack.layers()[b.metal1].source == view3d::ValueSource::none);
  CHECK(b.stack.draw_height(b.metal1) == view3d::DrawHeight{});
  db::CellDef def("top", b.tech->plane_count());
  paint_cell(b, def);
  const auto mesh = view3d::test::build(b.stack, def, db::Rect{-10, -10, 110, 100});
  REQUIRE(mesh.has_value());
  CHECK(view3d::test::face_area(*mesh, b.metal1) == 100.0 * 40.0);
  CHECK(view3d::test::face_area(*mesh, b.poly) == 20.0 * 40.0);
  CHECK(view3d::test::side_length(*mesh, b.metal1) == 0.0);
  CHECK(view3d::test::side_length(*mesh, b.poly) == 0.0);
}

TEST_CASE("plane-order default heights give every layer side faces",
          "[cap:view-3d][req:Layer height and thickness for the layout-layer view]") {
  // No `height` lines: h = 0.1 * order, t = 0.05 for both types.
  const Built b = stack_for(kStyle);
  CHECK(b.stack.layers()[b.poly].source == view3d::ValueSource::extract_default);
  db::CellDef def("top", b.tech->plane_count());
  paint_cell(b, def);
  const auto mesh = view3d::test::build(b.stack, def, db::Rect{-10, -10, 110, 100});
  REQUIRE(mesh.has_value());
  CHECK(view3d::test::side_length(*mesh, b.poly) == 2.0 * (20.0 + 40.0));
  CHECK(view3d::test::side_length(*mesh, b.metal1) == 2.0 * (100.0 + 40.0));
}

TEST_CASE("the window's exit factor follows the camera's Q-MISC-78 rule",
          "[cap:view-3d][req:3D view commands]") {
  view3d::WindowState window;
  window.applied_factor = 10.0F;
  CHECK(window.exit_mask_factor(true, 5.0F) == view3d::exit_mask_factor(true, 10.0F, 5.0F));
  CHECK(window.exit_mask_factor(true, 5.0F) == 5.0F);
  CHECK(window.exit_mask_factor(false, 5.0F) == 10.0F);
}
