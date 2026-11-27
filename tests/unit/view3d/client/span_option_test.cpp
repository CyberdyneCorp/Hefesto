// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The J1 view option "Span connected layers" as window state (R5 item 17,
// approved 2026-11-26; openspec/changes/m10-3d-view/design.md D10):
// WindowState::span_connected_layers, set through
// view3d::set_span_connected_layers, reaches the mesh build through
// window_build and build_scene; a new window starts unspanned. Owner: CORE
// (round 6).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string_view>

#include "hefesto/diag/sink.hpp"
#include "via_fixture.hpp"

namespace view3d = hefesto::view3d;
using view3d::test::ViaSession;

TEST_CASE("window_build carries the window's Span connected layers option",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ViaSession s(compat);
    hefesto::diag::NullSink quiet;
    CHECK_FALSE(s.window().span_connected_layers);
    CHECK_FALSE(view3d::window_build(s.editor, s.window(), compat, quiet).inputs.span_connected_layers);
    REQUIRE(view3d::set_span_connected_layers(s.editor.view3d, "", true));
    CHECK(s.window().span_connected_layers);
    const view3d::WindowBuild build = view3d::window_build(s.editor, s.window(), compat, quiet);
    CHECK(build.inputs.span_connected_layers);
    CHECK(build.inputs.technology == s.editor.technology.get());
  }
}

TEST_CASE("a spanned window's scene draws the via from the top of metal1 to the bottom of metal2",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ViaSession s(compat);
    const view3d::LayerId via = s.layer("via");
    const view3d::Scene plain = s.scene();
    REQUIRE(plain.mesh);
    const view3d::DrawHeight tech_height = plain.stack.draw_height(via);
    CHECK(plain.mesh->layer_heights.at(via) == tech_height);

    REQUIRE(view3d::set_span_connected_layers(s.editor.view3d, ".magic3d", true));
    const view3d::Scene spanned = s.scene();
    REQUIRE(spanned.mesh);
    const view3d::DrawHeight span = spanned.mesh->layer_heights.at(via);
    CHECK_FALSE(span == tech_height);
    // Unitless: the span is the gap between metal1 (thickness 0.3) and
    // metal2, 0.2, whatever the Q-MISC-72 draw unit.
    const view3d::DrawHeight m1 = spanned.stack.draw_height(s.layer("metal1"));
    CHECK(static_cast<double>(span.thickness) * 3.0 == Catch::Approx(static_cast<double>(m1.thickness) * 2.0));
  }
}

TEST_CASE("set_span_connected_layers needs the open window's path; a reopened window starts unspanned",
          "[cap:hefesto-3d-visualization][req:Interactive 3D inspection]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ViaSession s(compat);
    CHECK_FALSE(view3d::set_span_connected_layers(s.editor.view3d, ".render.magic", true));
    CHECK_FALSE(s.window().span_connected_layers);
    REQUIRE(view3d::set_span_connected_layers(s.editor.view3d, ".magic3d", true));
    s.w3d("closewindow");
    REQUIRE_FALSE(s.editor.view3d.has_window());
    CHECK_FALSE(view3d::set_span_connected_layers(s.editor.view3d, "", true));
    s.ok("specialopen wind3d c3d");
    CHECK_FALSE(s.window().span_connected_layers);
    hefesto::diag::NullSink quiet;
    CHECK_FALSE(view3d::window_build(s.editor, s.window(), compat, quiet).inputs.span_connected_layers);
  }
}
