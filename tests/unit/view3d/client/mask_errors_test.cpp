// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Generation errors of a 3D mask-layer build (R5 item 20, approved
// 2026-11-26; view-3d "Rendered geometry"): with magic-compat ON a full
// build reports them once to its sink (never as feedback; Magic's
// W3DCIFredisplay adds feedback on every redraw); incremental chunk
// rebuilds report nothing; with magic-compat OFF nothing is reported.
// Owner: CORE (round 6).

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

#include "client_fixture.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/view3d/client/scene.hpp"
#include "hefesto/view3d/mesh/worker.hpp"

namespace db = hefesto::db;
namespace diag = hefesto::diag;
namespace view3d = hefesto::view3d;
using view3d::test::ClientSession;

namespace {

// The MISC-3D technology whose output style cuts contacts too large for
// any metal1 area: every metal1 region is "no room for contacts in area!"
// (MASK-OPER-004).
constexpr std::string_view kErrTech = R"(tech
    format 35
    v3derr
end
planes
    metal1,m1
    metal2,m2
end
types
    metal1  metal1,m1
    metal2  metal2,m2
end
styles
    styletype mos
    metal1 20
    metal2 21
end
contact
end
compose
end
connect
    metal1  metal1
    metal2  metal2
end
cifoutput
style gdsii
    scalefactor 10
    layer MET1 m1
        calma 68 20
    layer CUT m1
        squares 0 100000 100
        calma 70 20
    render MET1 metal1 1.0 0.5
end
cifinput
end
drc
end
extract
style default
    lambda 1
    step 100
    sidehalo 0
    planeorder metal1 0
    planeorder metal2 1
    height metal1 0.5 0.3
end
)";

constexpr std::string_view kNoRoom = "MASK-OPER-004";

std::size_t count_code(const std::vector<diag::Diagnostic>& ds, std::string_view code) {
  std::size_t n = 0;
  for (const diag::Diagnostic& d : ds) {
    n += d.code == code ? 1U : 0U;
  }
  return n;
}

struct ErrSession : ClientSession {
  explicit ErrSession(bool compat) : ClientSession(compat, kErrTech) {
    ok("specialopen wind3d c3d");
    REQUIRE(window().mode == view3d::Mode::mask_layer);
  }

  std::vector<diag::Diagnostic> scene_errors() {
    diag::CollectingSink sink;
    view3d::BuildContext ctx;
    ctx.sink = &sink;
    const view3d::Scene scene = view3d::build_scene(editor, window(), project.compat.magic_compat, ctx);
    REQUIRE(scene.mesh);
    return sink.take();
  }

  // A worker build, a local edit rebuilt incrementally, then a new full
  // build: the errors reported after each step.
  std::array<std::vector<diag::Diagnostic>, 3> worker_errors() {
    diag::CollectingSink sink;
    diag::NullSink quiet;
    view3d::WindowBuild wb = view3d::window_build(editor, window(), project.compat.magic_compat, quiet);
    wb.inputs.sink = &sink;
    wb.inputs.max_chunk_tiles = 2;
    view3d::MeshWorker worker;
    std::array<std::vector<diag::Diagnostic>, 3> out;
    worker.build(wb.inputs);
    REQUIRE(worker.wait_idle(20000));
    REQUIRE(worker.current());
    out[0] = sink.take();
    const view3d::MeshHandle before = worker.current();
    paint("metal1", db::Rect{70, 5, 90, 20});
    const std::array<db::Rect, 1> dirty{db::Rect{70, 5, 90, 20}};
    worker.changed(dirty);
    REQUIRE(worker.wait_idle(20000));
    REQUIRE(worker.current() != before);  // rebuilt
    out[1] = sink.take();
    worker.build(wb.inputs);
    REQUIRE(worker.wait_idle(20000));
    out[2] = sink.take();
    return out;
  }
};

}  // namespace

TEST_CASE("Mask generation errors reported once per build with magic-compat ON",
          "[cap:view-3d][req:Rendered geometry]") {
  ErrSession s(true);
  SECTION("build_scene reports the error to its sink once") {
    CHECK(count_code(s.scene_errors(), kNoRoom) == 1);
    CHECK(s.editor.feedback.size() == 0);  // never as feedback
  }
  SECTION("the worker's full builds report; the incremental rebuild does not repeat it") {
    const auto steps = s.worker_errors();
    CHECK(count_code(steps[0], kNoRoom) == 1);
    CHECK(steps[1].empty());
    CHECK(count_code(steps[2], kNoRoom) == 1);
  }
}

TEST_CASE("Mask generation errors not reported with magic-compat OFF", "[cap:view-3d][req:Rendered geometry]") {
  ErrSession s(false);
  SECTION("build_scene") {
    CHECK(s.scene_errors().empty());
  }
  SECTION("the worker") {
    for (const auto& step : s.worker_errors()) {
      CHECK(step.empty());
    }
  }
}
