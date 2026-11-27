// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M10 plot/route scaffold (openspec/changes/m10-plot-route tasks.md 3.1):
// the route code area exists, the technology and EditorState carry the
// router state, `iroute`, `*iroute` and `*mzroute` are registered, and
// every area-contract stub of route/ answers its documented value or
// CORE-NOTIMPL-010. The owners replace these checks with their requirement
// tests as the stubs go away.
//
// Owner: SCAFFOLD.

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string_view>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/route/diag/commands.hpp"
#include "hefesto/route/iroute/commands.hpp"
#include "hefesto/route/iroute/session.hpp"
#include "hefesto/route/maze/route.hpp"
#include "hefesto/route/tech/styles.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/tech/loader.hpp"

namespace cmd = hefesto::cmd;
namespace diag = hefesto::diag;
namespace route = hefesto::route;

namespace {

constexpr std::string_view kNotImplemented = "CORE-NOTIMPL-010";

bool is_not_implemented(const std::vector<diag::Diagnostic>& diagnostics) {
  return diagnostics.size() == 1 && diagnostics.front().code == kNotImplemented;
}

bool is_not_implemented(const diag::Status& status) {
  return !status.ok() && status.error().code == kNotImplemented;
}

route::MazeStyle named(std::string_view name) {
  route::MazeStyle style;
  style.name = std::string{name};
  return style;
}

}  // namespace

TEST_CASE("the route code area is declared", "[m10-plot-route-scaffold]") {
  const auto area = diag::find_code_area("route");
  REQUIRE(area.has_value());
  CHECK(area->prefix == "ROUTE");
  CHECK(area->doc == "docs/diagnostics/codes/route.md");
}

TEST_CASE("a technology without an mzrouter section has no maze style", "[m10-plot-route-scaffold]") {
  const auto tech = hefesto::tech::builtin_technology();
  CHECK(tech->maze_styles().styles.empty());
  CHECK(route::find_style(tech->maze_styles(), "irouter") == nullptr);
}

TEST_CASE("a new session has no irouter style", "[m10-plot-route-scaffold]") {
  const hefesto::editor::EditorState state;
  CHECK(state.iroute.styles.styles.empty());
  CHECK_FALSE(state.iroute.irouter.has_value());
  CHECK_FALSE(state.iroute.wizard_window.has_value());
}

TEST_CASE("Magic's maze parameter defaults", "[m10-plot-route-scaffold]") {
  const route::MazeStyle style;
  CHECK(style.penalty.mantissa == 2048);
  CHECK(style.penalty.exponent == 1);
  CHECK(style.window_width == 10000);
  CHECK(style.window_rate == 500);
  CHECK(style.bloom_delta_cost == 1);
  CHECK(style.bounds_increment == -1);
  CHECK(style.estimate);
  CHECK(style.expand_endpoints);
  CHECK_FALSE(style.top_hints_only);
  CHECK(style.max_walk_length == -1);
  CHECK(style.verbosity == 1);
  CHECK(style.bloom_limit == 0);
  CHECK_FALSE(style.derived);
}

TEST_CASE("find_style and reset_from pick the irouter style", "[m10-plot-route-scaffold]") {
  route::MazeStyles styles;
  styles.styles = {named("garouter"), named("irouter")};
  REQUIRE(route::find_style(styles, "irouter") != nullptr);
  CHECK(route::find_style(styles, "irouter")->name == "irouter");
  CHECK(route::find_style(styles, "nosuch") == nullptr);

  route::IrouteSession session;
  session.reset_from(styles);
  REQUIRE(session.irouter.has_value());
  CHECK(session.irouter->name == "irouter");
  CHECK(session.styles.styles.size() == 2);
  session.reset_from(route::MazeStyles{});
  CHECK_FALSE(session.irouter.has_value());
}

TEST_CASE("an empty mzrouter section reads no style and derive keeps the given one", "[m10-plot-route-scaffold]") {
  const auto tech = hefesto::tech::builtin_technology();
  diag::CollectingSink sink;
  const hefesto::tech::RawSection section{"mzrouter", {}};
  route::MazeStyles styles = route::read_mzrouter_section(&section, *tech, sink);
  CHECK(styles.styles.empty());
  styles.styles.push_back(named("irouter"));
  route::derive_after_load(styles, *tech, nullptr, hefesto::lef::LayerTable{}, true, sink);
  CHECK(styles.styles.size() == 1);
  CHECK(sink.snapshot().empty());
}

TEST_CASE("the maze engine stub fails with CORE-NOTIMPL-010", "[m10-plot-route-scaffold]") {
  const route::maze::RouteResult result = route::maze::route(route::maze::RouteRequest{});
  CHECK(result.code == route::maze::ResultCode::failure);
  CHECK(result.paint.empty());
  CHECK(result.path.empty());
  CHECK(is_not_implemented(result.messages));
}

TEST_CASE("save_parameters reports CORE-NOTIMPL-010", "[m10-plot-route-scaffold]") {
  const auto tech = hefesto::tech::builtin_technology();
  std::ostringstream out;
  CHECK(is_not_implemented(route::save_parameters(route::IrouteSession{}, *tech, true, out)));
  CHECK(out.str().empty());
}

TEST_CASE("iroute, *iroute and *mzroute are registered and report CORE-NOTIMPL-010",
          "[m10-plot-route-scaffold]") {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  REQUIRE(route::register_iroute_commands(registry));
  REQUIRE(route::register_route_diag_commands(registry));
  CHECK_FALSE(route::register_iroute_commands(registry));      // duplicate
  CHECK_FALSE(route::register_route_diag_commands(registry));  // duplicate

  hefesto::editor::EditorState state;
  cmd::Dispatcher dispatcher{registry, services.view(), project, script};
  dispatcher.set_editor(&state);
  for (const char* name : {"iroute", "*iroute", "*mzroute"}) {
    INFO(name);
    REQUIRE(registry.find(name) != nullptr);
    cmd::Invocation invocation;
    invocation.name = name;
    invocation.args = {"help"};
    const auto result = dispatcher.execute(invocation);
    CHECK_FALSE(result.ok());
    CHECK(is_not_implemented(result.diagnostics));
  }
}

TEST_CASE("the full command set registers the router commands once", "[m10-plot-route-scaffold]") {
  cmd::CommandRegistry registry;
  REQUIRE(cmd::register_all_commands(registry));
  for (const char* name : {"iroute", "*iroute", "*mzroute"}) {
    INFO(name);
    CHECK(registry.find(name) != nullptr);
  }
}
