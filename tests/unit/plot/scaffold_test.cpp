// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M10 plot/route scaffold (openspec/changes/m10-plot-route tasks.md 3.1):
// the plot code area exists, the technology and EditorState carry the plot
// state, `plot` is registered, and every area-contract stub of plot/ and
// display/export/ answers its documented value or CORE-NOTIMPL-010. The
// owners replace these checks with their requirement tests as the stubs go
// away.
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
#include "hefesto/display/export/export.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/plot/cmd/commands.hpp"
#include "hefesto/plot/io/atomic_file.hpp"
#include "hefesto/plot/params/params.hpp"
#include "hefesto/plot/pnm/pnm.hpp"
#include "hefesto/plot/ps/postscript.hpp"
#include "hefesto/plot/tech/plot_tech.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/tech/loader.hpp"

namespace cmd = hefesto::cmd;
namespace diag = hefesto::diag;
namespace plot = hefesto::plot;

namespace {

constexpr std::string_view kNotImplemented = "CORE-NOTIMPL-010";

bool is_not_implemented(const std::vector<diag::Diagnostic>& diagnostics) {
  return diagnostics.size() == 1 && diagnostics.front().code == kNotImplemented;
}

template <typename T>
bool is_not_implemented(const diag::Outcome<T>& outcome) {
  return !outcome.ok() && outcome.error().code == kNotImplemented;
}

}  // namespace

TEST_CASE("CORE-NOTIMPL-010 and the plot code area are declared", "[m10-plot-route-scaffold]") {
  const auto code = diag::find_code(kNotImplemented);
  REQUIRE(code.has_value());
  CHECK(code->severity == diag::Severity::error);
  CHECK(diag::codes::kPlotRouteNotImplemented == kNotImplemented);
  const auto area = diag::find_code_area("plot");
  REQUIRE(area.has_value());
  CHECK(area->prefix == "PLOT");
  CHECK(area->doc == "docs/diagnostics/codes/plot.md");
}

TEST_CASE("EditorState starts with Magic's plot parameter defaults", "[m10-plot-route-scaffold]") {
  const hefesto::editor::EditorState state;
  const plot::Params& p = state.plot;
  CHECK(p.show_cell_names);
  CHECK(p.ps_id_font == "/Helvetica");
  CHECK(p.ps_name_font == "/HelveticaBold");
  CHECK(p.ps_label_font == "/Helvetica");
  CHECK(p.ps_id_size == 8);
  CHECK(p.ps_name_size == 12);
  CHECK(p.ps_label_size == 12);
  CHECK(p.ps_boundary);
  CHECK(p.ps_width == 612);
  CHECK(p.ps_height == 792);
  CHECK(p.ps_margin == 72);
  CHECK(p.pnm_maxmem == 64 * 1024);
  CHECK(p.pnm_downsample == 0);
  CHECK(p.pnm_background == 0xff);
}

TEST_CASE("a technology without a plot section has an empty PlotTech", "[m10-plot-route-scaffold]") {
  const auto tech = hefesto::tech::builtin_technology();
  const plot::PlotTech& plot_tech = tech->plot();
  CHECK_FALSE(plot_tech.has_section);
  CHECK_FALSE(plot_tech.init_failed);
  CHECK(plot_tech.ps_styles.empty());
  CHECK(plot_tech.ps_colors.empty());
  CHECK(plot_tech.ps_patterns.empty());
}

TEST_CASE("the plot command is registered and reports CORE-NOTIMPL-010", "[m10-plot-route-scaffold]") {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  REQUIRE(plot::register_plot_command(registry));
  CHECK_FALSE(plot::register_plot_command(registry));  // duplicate
  REQUIRE(registry.find("plot") != nullptr);

  hefesto::editor::EditorState state;
  cmd::Dispatcher dispatcher{registry, services.view(), project, script};
  dispatcher.set_editor(&state);
  cmd::Invocation invocation;
  invocation.name = "plot";
  invocation.args = {"postscript", "out"};
  const auto result = dispatcher.execute(invocation);
  CHECK_FALSE(result.ok());
  CHECK(is_not_implemented(result.diagnostics));
}

TEST_CASE("the full command set registers plot once", "[m10-plot-route-scaffold]") {
  cmd::CommandRegistry registry;
  REQUIRE(cmd::register_all_commands(registry));
  CHECK(registry.find("plot") != nullptr);
}

TEST_CASE("the plot stubs answer CORE-NOTIMPL-010 or their documented values", "[m10-plot-route-scaffold]") {
  const hefesto::editor::EditorState state;
  CHECK(is_not_implemented(plot::resolve_region(state, plot::PlotRequest{})));

  plot::Params params;
  CHECK(is_not_implemented(params.set("pnmmaxmem", "1024")));
  std::ostringstream listing;
  params.print(listing);
  CHECK(listing.str().empty());

  CHECK(is_not_implemented(plot::open_atomic("out.ps")));
  CHECK(plot::with_default_extension("out", ".ps", true) == "out");
  CHECK(plot::escape_ps_text("a(b)") == "a(b)");

  std::ostringstream out;
  const plot::PlotTech plot_tech;
  CHECK(is_not_implemented(plot::write_postscript(plot::Region{}, plot_tech, params, plot::PlotContext{}, out)));
  CHECK(is_not_implemented(plot::write_pnm(plot::Region{}, plot_tech, params, plot::PlotContext{}, 1000, out)));
  CHECK(out.str().empty());
}

TEST_CASE("the view export stubs report CORE-NOTIMPL-010 and write nothing", "[m10-plot-route-scaffold]") {
  const hefesto::display::LayoutScene scene;
  const hefesto::display::ExportOptions options;
  std::ostringstream out;
  CHECK(is_not_implemented(hefesto::display::export_svg(scene, options, out)));
  CHECK(is_not_implemented(hefesto::display::export_png(scene, options, out)));
  CHECK(out.str().empty());
}
