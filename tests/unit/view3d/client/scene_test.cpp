// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What the 3D window draws and how it reaches the renderer (view-3d
// "Rendered geometry"; design D1, D8, D9): build_scene and a WindowDisplay
// over render::Mesh3dRenderer, with a test double of the renderer (the
// Vulkan implementation is RENDER3D's, task 5.3). Owner: WIND3D.

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "client_fixture.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/render/mesh3d_renderer.hpp"
#include "hefesto/view3d/client/scene.hpp"

namespace render = hefesto::render;
namespace view3d = hefesto::view3d;
using view3d::Mode;
using view3d::test::ClientSession;

namespace {

struct FakeTarget final : render::OffscreenTarget {
  int width() const noexcept override { return 500; }
  int height() const noexcept override { return 500; }
  void resize(int /*width*/, int /*height*/) override {}
};

// Records the calls of one frame.
struct FakeRenderer final : render::Mesh3dRenderer {
  int uploads = 0;
  int draws = 0;
  std::size_t chunks = 0;
  view3d::ViewState view;
  int level = -1;
  std::vector<bool> hidden;
  void upload(const view3d::MeshModel& mesh) override {
    ++uploads;
    chunks = mesh.chunks.size();
  }
  void draw(const view3d::ViewState& v, const render::DrawParams& params, render::OffscreenTarget& /*target*/) override {
    ++draws;
    view = v;
    level = params.level;
    hidden = params.hidden;
    REQUIRE(params.stack != nullptr);
  }
  view3d::PickId read_pick(int /*x*/, int /*y*/) override { return view3d::kNoPick; }
};

// The front end's part (SHELL): the window's scene uploaded and drawn with
// its view, level and hidden flags.
struct RendererDisplay final : view3d::WindowDisplay {
  render::Mesh3dRenderer& renderer;
  render::OffscreenTarget& target;
  bool magic_compat = true;
  view3d::Scene scene;
  int closes = 0;
  RendererDisplay(render::Mesh3dRenderer& r, render::OffscreenTarget& t) : renderer(r), target(t) {}
  void redraw(const hefesto::editor::EditorState& editor, const view3d::WindowState& window) override {
    view3d::BuildContext ctx;
    scene = view3d::build_scene(editor, window, magic_compat, ctx);
    if (scene.mesh) {
      renderer.upload(*scene.mesh);
    }
    render::DrawParams params;
    params.level = window.level;
    params.stack = &scene.stack;
    params.hidden = scene.hidden;
    renderer.draw(window.view, params, target);
  }
  void closed(const view3d::WindowState& /*window*/) override { ++closes; }
};

// The display bindings of mos.OpenGL.dstyle's metal1 and metal2 styles (a
// headless session has no style set; the stack's (type, style) pairs come
// from them).
void bind_styles(ClientSession& s) {
  s.editor.styles.emplace();
  auto& types = s.editor.styles->types.styles_of_type;
  types.resize(hefesto::tech::kMaxTileTypes);
  types[*s.editor.technology->find_type("metal1").id] = {hefesto::display::kFixedStyleCount + 20};
  types[*s.editor.technology->find_type("metal2").id] = {hefesto::display::kFixedStyleCount + 21};
}

}  // namespace

TEST_CASE("the window is drawn through Mesh3dRenderer on open and on every change",
          "[cap:view-3d][req:Rendered geometry]") {
  ClientSession s;
  bind_styles(s);
  FakeRenderer renderer;
  FakeTarget target;
  RendererDisplay display{renderer, target};
  s.editor.view3d.display = &display;
  s.ok("specialopen wind3d c3d");
  CHECK(renderer.uploads == 1);
  CHECK(renderer.draws == 1);
  CHECK(renderer.chunks > 0);
  CHECK(renderer.level == 1);
  CHECK(renderer.view == s.window().view);
  s.w3d("level up");
  CHECK(renderer.draws == 2);
  CHECK(renderer.level == 2);
  s.w3d("closewindow");
  CHECK(display.closes == 1);
}

TEST_CASE("the scene holds the layout-layer stack, mesh and hidden flags", "[cap:view-3d][req:Rendered geometry]") {
  ClientSession s;
  bind_styles(s);
  s.ok("specialopen wind3d c3d");
  s.w3d("cif");
  s.w3d("see no metal2");
  view3d::BuildContext ctx;
  const view3d::Scene scene = view3d::build_scene(s.editor, s.window(), true, ctx);
  REQUIRE(scene.mesh != nullptr);
  CHECK(scene.mesh->mode == Mode::layout_layer);
  CHECK(scene.xy_factor == 1.0F);
  CHECK_FALSE(scene.clipped);
  CHECK(scene.clip == hefesto::db::Rect{-1, -1, 101, 51});  // the bounding box grown by 1 (Q-MISC-70)
  REQUIRE(scene.stack.size() == 2);
  CHECK(scene.hidden == std::vector<bool>{false, true});
  CHECK(scene.stack.draw_height(0) == view3d::DrawHeight{0.5F, 0.3F});  // metal1's `height` line
  s.w3d("cutbox 0 0 40 20");
  const view3d::Scene clipped = view3d::build_scene(s.editor, s.window(), true, ctx);
  CHECK(clipped.clipped);
  CHECK(clipped.clip == hefesto::db::Rect{0, 0, 40, 20});
}

TEST_CASE("mask-layer mode draws the layout mesh scaled by f until phase 2", "[cap:view-3d][req:Rendered geometry]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  view3d::BuildContext ctx;
  const view3d::Scene scene = view3d::build_scene(s.editor, s.window(), true, ctx);
  CHECK(scene.xy_factor == 10.0F);
  REQUIRE(scene.mesh != nullptr);
  CHECK(scene.mesh->mode == Mode::layout_layer);
}

TEST_CASE("the mask stack carries the render command's values", "[cap:view-3d][req:Rendered geometry]") {
  ClientSession s;
  s.ok("specialopen wind3d c3d");
  s.w3d("render MET2 4 2 7");
  hefesto::diag::NullSink quiet;
  const view3d::LayerStack stack = view3d::mask_stack(s.editor, true, quiet);
  REQUIRE(stack.size() == 3);
  CHECK(stack.layers()[2].height == 4.0F);
  CHECK(stack.layers()[2].thickness == 2.0F);
  CHECK(view3d::render_style_position(stack.layers()[2].style) == 7);
  CHECK(stack.draw_height(2) == view3d::DrawHeight{4.0F, 2.0F});
}
