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

template <typename T>
bool is_not_implemented(const diag::Outcome<T>& outcome) {
  return !outcome.ok() && outcome.error().code == kNotImplemented;
}

lvs::Source spice(std::string_view text) {
  return lvs::Source{"in.spice", std::string{text}};
}

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

TEST_CASE("read_netlist reads a SPICE source into the database",
          "[cap:lvs-netlist-input][req:Netlist database model]") {
  // LVSREAD replaced the scaffold stub (tasks.md 6.1, 6.2); Verilog, .sim
  // and .ext still report CORE-NOTIMPL-009 until 6.3.
  lvs::NetlistDb db;
  const auto top = lvs::read_netlist(db, spice("R1 a b 1k\n"), lvs::Format::spice, {});
  REQUIRE(top.ok());
  CHECK(db.cells.at(top.value()).name == "in.spice");
  CHECK(is_not_implemented(lvs::read_netlist(db, spice("module a; endmodule\n"), lvs::Format::verilog, {})));
}

TEST_CASE("detect_format detects a SPICE source", "[cap:lvs-netlist-input][req:Format selection]") {
  CHECK(lvs::detect_format(spice(".subckt a x\n.ends\n")).value() == lvs::Format::spice);
}

TEST_CASE("scaffold evaluate_setup reports CORE-NOTIMPL-009", "[cap:lvs-setup-file][req:Setup file evaluation]") {
  lvs::Circuit first;
  lvs::Circuit second;
  lvs::SetupContext context{&first, &second, {}, {}};
  CHECK(is_not_implemented(lvs::evaluate_setup(lvs::Source{"setup.tcl", std::string{"permute default\n"}}, context)));
}

// reduce, compare_pair and the property checks left the scaffold in M9 R2:
// their requirement tests are in tests/unit/lvs/{reduce,cmp,prop}/.

// run_lvs, the report writers and the `lvs` command left the scaffold in
// M9 R3: their requirement tests are in tests/unit/lvs/{hier,report,cmd}/.

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
