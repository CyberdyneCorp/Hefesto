// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// route::refresh_session_styles through a command session (Magic's
// MZAfterTech + IRAfterTech): after `tech load`, `scalegrid`, a `.mag` read
// on a finer grid and a CIF read that redefines the grid, the session's
// `irouter` copy has the widths and spacings of the rescaled DRC rules;
// the explicit spacings after a rescale per magic-compat setting
// (Q-LEGACY-106); the last of two styles with the same name (Q-LEGACY-107).
//
// Owner: IROUTE.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <string_view>

#include "../tech/route_tech_fixture.hpp"
#include "hefesto/cif/read/read.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/route/iroute/session.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"

namespace route = hefesto::route;
namespace tech = hefesto::tech;
using route::test::layer_of;
using route::test::type_named;

#define DERIVE_REQ "[cap:router-maze][req:Width and spacing derivation from DRC]"
#define LOOKUP_REQ "[cap:router-maze][req:Style defaults and lookup]"

namespace {

// DRC: metal1 width 3, metal2 width 3 and spacing 4 (route_tech_fixture.hpp).
constexpr std::string_view kIrouter =
    "style irouter\n"
    "layer m2 32 64 256 1\n"
    "layer m1 64 32 256 1\n"
    "spacing m2 m2 5\n";

// The fixture technology with a CIF input style (`L CMF` is metal1, 10
// centimicrons per lambda), so an odd CIF coordinate redefines the grid.
std::string session_tech(std::string_view body) {
  std::string text = route::test::tech_text(body);
  const std::string empty_input = "cifinput\nend\n";
  const auto at = text.find(empty_input);
  REQUIRE(at != std::string::npos);
  text.replace(at, empty_input.size(), "cifinput\nstyle lambda\n    scalefactor 10\n    layer metal1 CMF\nend\n");
  return text;
}

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  hefesto::cmd::ScriptSession script{project};
  hefesto::cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<hefesto::cmd::Dispatcher> dispatcher;

  explicit Session(bool magic_compat) {
    project.compat.magic_compat = magic_compat;
    REQUIRE(hefesto::cmd::register_all_commands(registry));
    dispatcher = std::make_unique<hefesto::cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
  }

  void file(const std::string& path, const std::string& text) {
    REQUIRE(services.view().files.write_atomic(path, text).ok());
  }

  void run(std::string_view line) {
    for (const auto& r : hefesto::ui::run_command_line(*dispatcher, line, hefesto::cmd::Origin::script)) {
      for (const auto& d : r.diagnostics) {
        UNSCOPED_INFO(d.code << ": " << d.text);
      }
      REQUIRE(r.ok());
    }
  }

  void load(std::string_view body = kIrouter) {
    file("/t/route.tech", session_tech(body));
    run("tech load /t/route.tech -noprompt");
  }

  [[nodiscard]] const route::RouteType& irouter_layer(std::string_view type) const {
    REQUIRE(editor.iroute.irouter.has_value());
    const route::RouteLayer* layer = layer_of(*editor.iroute.irouter, type_named(*editor.technology, type));
    REQUIRE(layer != nullptr);
    return layer->route;
  }

  [[nodiscard]] int m2_spacing() const {
    return irouter_layer("metal2").spacing.at(type_named(*editor.technology, "metal2"));
  }
};

}  // namespace

TEST_CASE("regression: scalegrid rescales the irouter widths", DERIVE_REQ) {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(compat);
    s.load();
    CHECK(s.irouter_layer("metal1").width == 3);
    s.run("scalegrid 1 2");
    REQUIRE(s.editor.grid == tech::GridScale{2, 1});
    CHECK(s.irouter_layer("metal1").width == 6);
    CHECK(s.irouter_layer("metal1").length == 6);
  }
}

TEST_CASE("regression: a finer-grid .mag read rescales the irouter widths", DERIVE_REQ) {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(compat);
    s.load();
    s.file("/t/fine.mag", "magic\ntech routetest\nmagscale 1 2\ntimestamp 1\n<< end >>\n");
    s.run("load /t/fine.mag");
    REQUIRE(s.editor.grid == tech::GridScale{2, 1});
    CHECK(s.irouter_layer("metal1").width == 6);
  }
}

TEST_CASE("regression: a CIF read that redefines the grid rescales the irouter widths", DERIVE_REQ) {
  // CIFScaleCoord calls MZAfterTech and IRAfterTech; the read engine's
  // subdivide_grid did not refresh the router styles.
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(compat);
    s.load();
    auto top = s.editor.cells.create("top", s.editor.technology->plane_count());
    REQUIRE(top.ok());
    top.value()->flags().available = true;
    s.editor.edit_cell = top.value();
    hefesto::cif::CifReadOptions options;
    options.settings = s.editor.mask.cif;
    options.warnings = s.editor.mask.warnings;
    const auto read = hefesto::cif::read_text(s.editor, "DS 1 1 1;\n9 odd;\nL CMF;\nB 40 20 25 10;\nDF;\nE\n", options);
    REQUIRE(read.ok());
    REQUIRE(s.editor.grid == tech::GridScale{2, 1});
    CHECK(s.irouter_layer("metal1").width == 6);
  }
}

TEST_CASE("explicit spacing after a rescale with magic-compat on", DERIVE_REQ) {
  // MZAfterTech frees the explicit spacings after its first run (Q-LEGACY-106 ON).
  Session s(true);
  s.load();
  CHECK(s.m2_spacing() == 5);
  s.run("scalegrid 1 2");
  CHECK(s.m2_spacing() == 8);  // the rescaled DRC spacing
  CHECK(s.irouter_layer("metal2").subcell_spacing == 8);
  s.run("scalegrid 2 1");
  CHECK(s.m2_spacing() == 4);
  s.run("tech load /t/route.tech -noprompt");  // a new technology applies them again
  CHECK(s.m2_spacing() == 5);
}

TEST_CASE("explicit spacing after a rescale with magic-compat off", DERIVE_REQ) {
  // Q-LEGACY-106 OFF: the explicit values follow the grid as DRC distances do.
  Session s(false);
  s.load(std::string{kIrouter} + "width m1 4\n");
  CHECK(s.m2_spacing() == 5);
  CHECK(s.irouter_layer("metal1").width == 4);
  s.run("scalegrid 1 2");
  CHECK(s.m2_spacing() == 10);
  CHECK(s.irouter_layer("metal1").width == 8);
  s.run("scalegrid 2 1");
  CHECK(s.m2_spacing() == 5);
  CHECK(s.irouter_layer("metal1").width == 4);
}

TEST_CASE("the irouter copy is the last style named irouter", LOOKUP_REQ) {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(compat);
    s.load("style irouter\nlayer m1 64 32 256 1\nstyle irouter\nlayer m2 32 64 256 1\n");
    REQUIRE(s.editor.iroute.irouter.has_value());
    REQUIRE(s.editor.iroute.irouter->layers.size() == 1);
    CHECK(s.editor.iroute.irouter->layers[0].route.type == type_named(*s.editor.technology, "metal2"));
  }
}
