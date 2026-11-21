// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M10 scaffold (openspec/changes/m10-3d-view tasks.md 2.2, 2.3): the view3d
// code area exists, EditorState carries view3d::Session with no window, and
// the area-contract stubs answer their documented neutral values. Owners
// replace these [m10-scaffold] tests with [cap:][req:] ones as the stubs go
// away.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/extract/tech/height_view.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/view3d/camera/view_state.hpp"
#include "hefesto/view3d/client/commands.hpp"
#include "hefesto/view3d/client/session.hpp"
#include "hefesto/view3d/export/export.hpp"
#include "hefesto/view3d/mesh/build.hpp"
#include "hefesto/view3d/overlay/dataset.hpp"
#include "hefesto/view3d/pick/pick.hpp"
#include "hefesto/view3d/section/section.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace diag = hefesto::diag;
namespace extract = hefesto::extract;
namespace view3d = hefesto::view3d;

TEST_CASE("the view3d code area is declared", "[m10-scaffold]") {
  const auto area = diag::find_code_area("view3d");
  REQUIRE(area.has_value());
  CHECK(area->prefix == "VIEW3D");
  CHECK(area->doc == "docs/diagnostics/codes/view3d.md");
}

TEST_CASE("EditorState carries a 3D session with no window", "[m10-scaffold]") {
  const hefesto::editor::EditorState state;
  CHECK_FALSE(state.view3d.has_window());

  const view3d::WindowState window;
  CHECK(window.toolkit_path == ".magic3d");
  CHECK(window.mode == view3d::Mode::layout_layer);
  CHECK(window.level == 1);
  CHECK(window.applied_factor == 1.0F);
  CHECK_FALSE(window.cut_box.has_value());
  CHECK(window.visibility == view3d::VisibilitySets{});
  CHECK(window.mesh == nullptr);
}

TEST_CASE("the TECH stubs answer no extraction style and empty stacks", "[m10-scaffold]") {
  diag::NullSink sink;
  const hefesto::tech::Technology tech;
  const auto heights = extract::HeightView::from_technology(tech, sink);
  CHECK_FALSE(heights.has_style());
  CHECK_FALSE(heights.height(1).has_value());
  const hefesto::display::TypeStyleMap styles;
  const auto layout = view3d::build_layout_stack(tech, heights, styles, true, 0.01);
  CHECK(layout.mode() == view3d::Mode::layout_layer);
  CHECK(layout.size() == 0);
  CHECK(layout.draw_height(0) == view3d::DrawHeight{});
  CHECK_FALSE(layout.physical_height_um(0).has_value());
  const hefesto::mask::OutputStyle ostyle;
  CHECK(view3d::build_mask_stack(ostyle, false).mode() == view3d::Mode::mask_layer);
}

TEST_CASE("the MESH, INSPECT, EXPORT and OVERLAY stubs answer empty results", "[m10-scaffold]") {
  diag::NullSink sink;
  const hefesto::mask::OutputStyle ostyle;
  const auto stack = view3d::build_mask_stack(ostyle, true);
  view3d::BuildContext ctx;
  const view3d::ClipArea clip{hefesto::db::Rect{0, 0, 10, 10}, true, 0};
  const hefesto::mask::LayerPlanes planes;
  const auto mesh = view3d::build_from_planes(stack, planes, clip, ctx);
  REQUIRE(mesh.has_value());
  CHECK(mesh->empty());
  CHECK(mesh->region == clip.area);
  CHECK_FALSE(view3d::resolve(*mesh, 1).has_value());
  CHECK(view3d::section(*mesh, stack, view3d::Segment{}).intervals.empty());
  hefesto::gds::MemorySink out;
  CHECK(view3d::write_stl(*mesh, stack, {}, out, sink).ok());
  CHECK(out.position() == 0);
  CHECK(view3d::overlay::color_of({}, 1.0, 0.0, 2.0) == view3d::overlay::Rgb{0, 0, 0});
}

TEST_CASE("the CAMERA stubs answer the zero view", "[m10-scaffold]") {
  CHECK(view3d::default_view(hefesto::db::Rect{0, 0, 100, 50}, view3d::Mode::layout_layer, 1.0F) ==
        view3d::ViewState{});
  CHECK(view3d::preset(view3d::Preset::isometric) == view3d::Angles{});
  CHECK(view3d::matrix(view3d::ViewState{}, 500, 500, {})[0] == 1.0F);
}

TEST_CASE("the WIND3D stubs keep today's answer", "[m10-scaffold]") {
  // WIND3D: opening still answers UI-CLIENT-002; no command registered.
  hefesto::editor::EditorState state;
  const auto opened = view3d::open_wind3d(state, view3d::OpenRequest{});
  REQUIRE_FALSE(opened.ok());
  CHECK(opened.error().code == "UI-CLIENT-002");
  hefesto::cmd::CommandRegistry registry;
  CHECK(view3d::register_wind3d_commands(registry));
  CHECK(registry.find("cutbox", view3d::kWind3dClient) == nullptr);
}
