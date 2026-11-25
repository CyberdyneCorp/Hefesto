// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M9 scaffold (openspec/changes/m9-lvs-simulation tasks.md 4.1, 4.2): the
// lvs code area exists, EditorState carries an lvs::Session, and every
// area-contract stub reports CORE-NOTIMPL-009. The owners replace these
// checks with their requirement tests as the stubs go away.
//
// Owner: SCAFFOLD.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lvs/cmd/commands.hpp"
#include "hefesto/lvs/cmp/compare.hpp"
#include "hefesto/lvs/hier/run.hpp"
#include "hefesto/lvs/prop/properties.hpp"
#include "hefesto/lvs/read/reader.hpp"
#include "hefesto/lvs/reduce/reduce.hpp"
#include "hefesto/lvs/report/report.hpp"
#include "hefesto/lvs/session.hpp"
#include "hefesto/lvs/setup/setup.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace cmd = hefesto::cmd;
namespace diag = hefesto::diag;
namespace lvs = hefesto::lvs;

namespace {

constexpr std::string_view kNotImplemented = "CORE-NOTIMPL-009";

bool is_not_implemented(const std::vector<diag::Diagnostic>& diagnostics) {
  return diagnostics.size() == 1 && diagnostics.front().code == kNotImplemented;
}

template <typename T>
bool is_not_implemented(const diag::Outcome<T>& outcome) {
  return !outcome.ok() && outcome.error().code == kNotImplemented;
}

lvs::Source spice(std::string_view text) {
  return lvs::Source{"in.spice", std::string{text}};
}

class NoProgress final : public lvs::ProgressSink {
 public:
  void pair_started(std::string_view /*cell1*/, std::string_view /*cell2*/, std::size_t /*index*/,
                    std::size_t /*count*/) override {}
};

}  // namespace

TEST_CASE("CORE-NOTIMPL-009 and the lvs code area are declared",
          "[cap:hefesto-diagnostics][req:Structured diagnostic records]") {
  const auto code = diag::find_code(kNotImplemented);
  REQUIRE(code.has_value());
  CHECK(code->severity == diag::Severity::error);
  CHECK(diag::codes::kM9NotImplemented == kNotImplemented);
  const auto area = diag::find_code_area("lvs");
  REQUIRE(area.has_value());
  CHECK(area->prefix == "LVS");
  CHECK(area->doc == "docs/diagnostics/codes/lvs.md");
}

TEST_CASE("EditorState carries an idle LVS session", "[cap:lvs-command][req:Execution model]") {
  const hefesto::editor::EditorState state;
  CHECK(state.lvs.last_results.empty());
  CHECK_FALSE(state.lvs.running.has_value());
  REQUIRE(state.lvs.cancel != nullptr);
  CHECK_FALSE(state.lvs.cancel->load());
  CHECK(state.lvs.names1 == nullptr);
  CHECK(state.lvs.names2 == nullptr);
}

TEST_CASE("lvs cancel token follows its flag",
          "[cap:lvs-comparison-algorithm][req:Long-running comparisons]") {
  CHECK_FALSE(lvs::CancelToken{}.requested());
  std::atomic<bool> flag{false};
  const lvs::CancelToken token{&flag};
  CHECK_FALSE(token.requested());
  flag = true;
  CHECK(token.requested());
}

TEST_CASE("scaffold read_netlist reports CORE-NOTIMPL-009", "[cap:lvs-netlist-input][req:Netlist database model]") {
  lvs::NetlistDb db;
  CHECK(is_not_implemented(lvs::read_netlist(db, spice("R1 a b 1k\n"), lvs::Format::spice, {})));
  CHECK(db.cells.empty());
}

TEST_CASE("scaffold detect_format reports CORE-NOTIMPL-009", "[cap:lvs-netlist-input][req:Format selection]") {
  CHECK(is_not_implemented(lvs::detect_format(spice(".subckt a x\n.ends\n"))));
}

TEST_CASE("scaffold evaluate_setup reports CORE-NOTIMPL-009", "[cap:lvs-setup-file][req:Setup file evaluation]") {
  lvs::Circuit first;
  lvs::Circuit second;
  lvs::SetupContext context{&first, &second, {}, {}};
  CHECK(is_not_implemented(lvs::evaluate_setup(lvs::Source{"setup.tcl", std::string{"permute default\n"}}, context)));
}

TEST_CASE("scaffold reduce reports CORE-NOTIMPL-009", "[cap:lvs-device-reduction][req:Reduction schedule]") {
  lvs::Cell cell{"inv"};
  const auto summary = lvs::reduce(cell, {});
  CHECK(summary.parallel_merged == 0);
  CHECK(summary.series_merged == 0);
  CHECK(summary.zero_valued_removed == 0);
  CHECK(is_not_implemented(summary.diagnostics));
}

TEST_CASE("scaffold compare_pair reports CORE-NOTIMPL-009",
          "[cap:lvs-comparison-algorithm][req:Iterative refinement]") {
  const auto result = lvs::compare_pair(lvs::Cell{"a"}, lvs::Cell{"b"}, {}, {});
  CHECK(result.cell1 == "a");
  CHECK(result.cell2 == "b");
  CHECK(result.status == lvs::PairStatus::mismatch);
  CHECK(is_not_implemented(result.diagnostics));
}

TEST_CASE("scaffold check_properties reports CORE-NOTIMPL-009",
          "[cap:lvs-property-matching][req:When properties are checked]") {
  const auto result = lvs::check_properties({}, {});
  CHECK(result.errors.empty());
  CHECK(is_not_implemented(result.diagnostics));
}

TEST_CASE("scaffold run_lvs reports CORE-NOTIMPL-009", "[cap:lvs-hierarchy][req:lvs per-pair flow]") {
  NoProgress progress;
  lvs::RunOptions options;
  options.netgen_compat = false;
  const auto result = lvs::run_lvs({}, {}, {}, options, progress);
  CHECK_FALSE(result.clean);
  CHECK(result.pairs.empty());
  CHECK_FALSE(result.netgen_compat);
  CHECK(is_not_implemented(result.diagnostics));
}

TEST_CASE("scaffold comp.out writer reports CORE-NOTIMPL-009", "[cap:lvs-reports][req:Two-column layout]") {
  CHECK(is_not_implemented(lvs::write_comp_out({})));
}

TEST_CASE("scaffold JSON writer reports CORE-NOTIMPL-009", "[cap:lvs-reports][req:JSON file]") {
  CHECK(is_not_implemented(lvs::write_json({})));
}

TEST_CASE("scaffold list result reports CORE-NOTIMPL-009", "[cap:lvs-reports][req:Structured list result]") {
  CHECK(is_not_implemented(lvs::to_list({})));
}

TEST_CASE("scaffold lvs command is registered and reports CORE-NOTIMPL-009",
          "[cap:lvs-command][req:lvs command syntax]") {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  REQUIRE(lvs::register_lvs_commands(registry));
  CHECK_FALSE(lvs::register_lvs_commands(registry));  // duplicate
  REQUIRE(registry.find("lvs") != nullptr);

  hefesto::editor::EditorState state;
  cmd::Dispatcher dispatcher{registry, services.view(), project, script};
  dispatcher.set_editor(&state);
  cmd::Invocation invocation;
  invocation.name = "lvs";
  invocation.args = {"layout.spice", "schematic.spice"};
  const auto result = dispatcher.execute(invocation);
  CHECK_FALSE(result.ok());
  CHECK(is_not_implemented(result.diagnostics));
}

TEST_CASE("lvs usage names the layout-aware form", "[cap:lvs-command][req:Layout netlist from extraction]") {
  // Regression: the synopsis gave only the netgen form.
  cmd::CommandRegistry registry;
  REQUIRE(lvs::register_lvs_commands(registry));
  const auto* spec = registry.find("lvs");
  REQUIRE(spec != nullptr);
  CHECK(spec->synopsis.find("lvs <name1> <name2> [<setup> [<log>]]") != std::string::npos);
  CHECK(spec->synopsis.find("lvs -layout <cell> <schematic> [<setup> [<log>]]") != std::string::npos);
}

TEST_CASE("lvs is part of the full command set", "[cap:lvs-command][req:lvs command syntax]") {
  cmd::CommandRegistry registry;
  REQUIRE(cmd::register_all_commands(registry));
  CHECK(registry.find("lvs") != nullptr);
}

TEST_CASE("scaffold layout_circuit reports CORE-NOTIMPL-009",
          "[cap:lvs-command][req:Layout netlist from extraction]") {
  hefesto::editor::EditorState state;
  CHECK(is_not_implemented(lvs::layout_circuit(state, "inv", {})));
}

TEST_CASE("scaffold cross_probe reports CORE-NOTIMPL-009", "[cap:lvs-command][req:Cross-probing]") {
  lvs::ObjectRef object;
  object.name = "out";
  CHECK(is_not_implemented(lvs::cross_probe({}, object)));
}
