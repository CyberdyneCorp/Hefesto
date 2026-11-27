// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs-command: `lvs` syntax, input and setup resolution (Q-LVS-31), the
// background run with cancellation, headless use, the result browser, the
// status chip and cross-probing through the result's references. The
// layout source (`lvs -layout`) and `.sch` sources are deferred
// (CORE-NOTIMPL-009). Every netlist is Hefesto-authored.
//
// Owner: LVSCMD.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../hier/hier_fixture.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/lvs/cmd/codes.hpp"
#include "hefesto/lvs/cmd/commands.hpp"
#include "hefesto/lvs/cmd/lvs_command.hpp"
#include "hefesto/lvs/cmd/view_model.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace {

namespace cmd = hefesto::cmd;
namespace diag = hefesto::diag;
namespace lvs = hefesto::lvs;
namespace platform = hefesto::platform;
using lvs_hier_test::circuit;
using lvs_hier_test::contains;
using lvs_hier_test::kInv;

/// A dispatcher with `lvs` over an in-memory file system and inline tasks.
struct Console {
  platform::PortableServices services{platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  diag::CollectingSink sink;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  Console() {
    REQUIRE(lvs::register_lvs_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    dispatcher->add_sink(sink);
  }

  void file(const std::string& path, std::string_view text) { REQUIRE(services.files().write_atomic(path, text).ok()); }

  [[nodiscard]] std::string read(const std::string& path) {
    auto text = services.files().read(path);
    REQUIRE(text.ok());
    return text.value();
  }

  cmd::CommandResult lvs(std::vector<std::string> args) {
    cmd::Invocation invocation;
    invocation.name = "lvs";
    invocation.args = std::move(args);
    return dispatcher->execute(invocation);
  }

  /// The console text of the command (LVS-CMD-001 lines).
  [[nodiscard]] std::string printed() const {
    std::string text;
    for (const auto& d : sink.snapshot()) {
      if (d.code == lvs::cmd_codes::kOutput) {
        text += d.text + "\n";
      }
    }
    return text;
  }

  [[nodiscard]] bool emitted(std::string_view code) const {
    for (const auto& d : sink.snapshot()) {
      if (d.code == code) {
        return true;
      }
    }
    return false;
  }
};

const diag::Diagnostic* find_code(const cmd::CommandResult& result, std::string_view code) {
  for (const auto& d : result.diagnostics) {
    if (d.code == code) {
      return &d;
    }
  }
  return nullptr;
}

bool has_code(const cmd::CommandResult& result, std::string_view code) {
  return find_code(result, code) != nullptr;
}

const std::string kTop = std::string{kInv} + ".subckt top A Y VDD VSS\nXinv1 A n VDD VSS inv\nXinv2 n Y VDD VSS inv\n.ends\n";

}  // namespace

// ------------------------------------------------------------ syntax

TEST_CASE("lvs arguments and defaults", "[cap:lvs-command][req:lvs command syntax]") {
  auto args = lvs::parse_lvs_arguments({"a.spice", "b.spice"});
  REQUIRE(args.ok());
  CHECK(args.value().setup == "setup.tcl");
  CHECK(args.value().log == "comp.out");
  args = lvs::parse_lvs_arguments({"-json", "a.spice", "b.spice", "s.tcl", "out.log", "-list", "-blackbox",
                                   "-noflatten={x y}"});
  REQUIRE(args.ok());
  CHECK(args.value().json);
  CHECK(args.value().list);
  CHECK(args.value().blackbox);
  CHECK(args.value().setup == "s.tcl");
  CHECK(args.value().log == "out.log");
  REQUIRE(args.value().noflatten);
  CHECK(*args.value().noflatten == "{x y}");
  CHECK_FALSE(lvs::parse_lvs_arguments({"a.spice"}).ok());
  CHECK_FALSE(lvs::parse_lvs_arguments({"a", "b", "c", "d", "e"}).ok());
  // Options are matched exactly.
  const auto bad = lvs::parse_lvs_arguments({"a", "b", "-js"});
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().code == lvs::cmd_codes::kUsage);
}

TEST_CASE("Netlist names are a file or {file cell}", "[cap:lvs-command][req:lvs command syntax]") {
  auto name = lvs::parse_netlist_name("inv.spice inv");
  REQUIRE(name.ok());
  CHECK(name.value().file == "inv.spice");
  CHECK(name.value().cell == "inv");
  name = lvs::parse_netlist_name("{my dir/a.spice} top");
  REQUIRE(name.ok());
  CHECK(name.value().file == "my dir/a.spice");
  name = lvs::parse_netlist_name("a.spice");
  REQUIRE(name.ok());
  CHECK(name.value().cell.empty());
  CHECK_FALSE(lvs::parse_netlist_name("a b c").ok());
}

TEST_CASE("netgen-style invocation", "[cap:lvs-command][req:lvs command syntax]") {
  Console console;
  console.file("inv.spice", kTop);
  console.file("inv_schem.spice", kTop);
  const auto result = console.lvs({"inv.spice top", "inv_schem.spice top", "setup.tcl", "comp.out", "-json"});
  REQUIRE(result.ok());
  const auto log = console.read("comp.out");
  CHECK(contains(log, "\nFinal result: Circuits match uniquely.\n.\n"));
  const auto json = console.read("comp.json");
  CHECK(json.starts_with("[\n  {\n"));
  const auto printed = console.printed();
  CHECK(contains(printed, "Reading netlist file inv.spice for top\n"));
  CHECK(contains(printed, "Comparison output logged to file comp.out\n"));
  CHECK(contains(printed, "Generating JSON file result\n"));
  CHECK(contains(printed, "LVS Done.\n"));
  CHECK(console.emitted(lvs::cmd_codes::kNoSetup));  // no setup.tcl: default settings
}

TEST_CASE("lvs -list returns the list result", "[cap:lvs-command][req:lvs command syntax]") {
  Console console;
  console.file("a.spice", kInv);
  console.file("b.spice", kInv);
  const auto result = console.lvs({"a.spice", "b.spice", "-list"});
  REQUIRE(result.ok());
  const auto text = result.value.to_string();
  CHECK(text.starts_with("{name {inv inv} devices"));
  CHECK(contains(console.printed(), "Generating list result\n"));
}

TEST_CASE("lvs checks its two netlists", "[cap:lvs-command][req:lvs command syntax]") {
  Console console;
  console.file("a.spice", kInv);
  console.file("b.spice", kInv);
  auto result = console.lvs({"a.spice inv", "a.spice inv"});
  CHECK_FALSE(result.ok());
  CHECK(has_code(result, lvs::cmd_codes::kSameNetlist));
  result = console.lvs({"a.spice nosuch", "b.spice inv"});
  CHECK_FALSE(result.ok());
  REQUIRE(has_code(result, lvs::cmd_codes::kNoCell));
  CHECK(find_code(result, lvs::cmd_codes::kNoCell)->text == "Cannot find cell nosuch in file a.spice");
  result = console.lvs({"missing.spice", "b.spice"});
  CHECK_FALSE(result.ok());
  REQUIRE(console.editor.lvs.headless_status);
  CHECK(*console.editor.lvs.headless_status == 2);
}

TEST_CASE("Deferred sources report CORE-NOTIMPL-009", "[cap:lvs-command][req:lvs command syntax]") {
  // lvs -layout waits for M7's G3 and MAPS; a .sch side waits for M8's G4.
  Console console;
  console.file("b.spice", kInv);
  auto result = console.lvs({"-layout", "inv", "b.spice"});
  CHECK_FALSE(result.ok());
  CHECK(has_code(result, diag::codes::kM9NotImplemented));
  result = console.lvs({"inv.sch", "b.spice"});
  CHECK_FALSE(result.ok());
  CHECK(has_code(result, diag::codes::kM9NotImplemented));
}

TEST_CASE("A log name with nolog writes no log", "[cap:lvs-command][req:lvs command syntax]") {
  Console console;
  console.file("a.spice", kInv);
  console.file("b.spice", kInv);
  REQUIRE(console.lvs({"a.spice", "b.spice", "setup.tcl", "nolog"}).ok());
  CHECK_FALSE(console.services.files().exists("nolog"));
  CHECK(contains(console.printed(), "Subcircuit summary:"));  // echo stays on
}

// ------------------------------------------------------------ namespace

TEST_CASE("lvs does not shadow Magic's commands", "[cap:lvs-command][req:netgen command namespace]") {
  cmd::CommandRegistry registry;
  REQUIRE(cmd::register_all_commands(registry));
  const auto* lvs_spec = registry.find("lvs");
  REQUIRE(lvs_spec != nullptr);
  const auto* flatten = registry.find("flatten");
  REQUIRE(flatten != nullptr);
  CHECK(flatten->synopsis.find("-circuit1") == std::string::npos);  // Magic's flatten
  CHECK(contains(lvs_spec->help, "netgen"));
}

// ------------------------------------------------------------ resolution

TEST_CASE("Bare PDK setup name", "[cap:lvs-command][req:Setup file and input resolution]") {
  const std::vector<platform::Path> roots{"/pdks"};
  const auto candidates = lvs::setup_candidates("sky130A", "sky130A", roots);
  CHECK(candidates.front() == platform::Path{"sky130A"});
  CHECK(std::find(candidates.begin(), candidates.end(),
                  platform::Path{"/pdks/sky130A/libs.tech/netgen/sky130A_setup.tcl"}) != candidates.end());
  platform::MemoryFileSystem files;
  REQUIRE(files.write_atomic("/pdks/sky130A/libs.tech/netgen/sky130A_setup.tcl", "# setup\n").ok());
  const auto found = lvs::resolve_setup(files, "sky130A", "sky130A", roots);
  REQUIRE(found);
  CHECK(*found == platform::Path{"/pdks/sky130A/libs.tech/netgen/sky130A_setup.tcl"});
}

TEST_CASE("Bare IHP setup name", "[cap:lvs-command][req:Setup file and input resolution]") {
  const std::vector<platform::Path> roots{"/store"};
  platform::MemoryFileSystem files;
  REQUIRE(files.write_atomic("/store/ihp-sg13g2/libs.tech/netgen/ihp-sg13g2_setup.tcl", "# setup\n").ok());
  const auto found = lvs::resolve_setup(files, "ihp-sg13g2", "ihp-sg13g2", roots);
  REQUIRE(found);
  CHECK(*found == platform::Path{"/store/ihp-sg13g2/libs.tech/netgen/ihp-sg13g2_setup.tcl"});
  // No sg13g2 alias (Q-LVS-31).
  CHECK_FALSE(lvs::resolve_setup(files, "sg13g2", "ihp-sg13g2", roots));
}

TEST_CASE("Setup names are tried as given first", "[cap:lvs-command][req:Setup file and input resolution]") {
  platform::MemoryFileSystem files;
  REQUIRE(files.write_atomic("my_setup.tcl", "# here\n").ok());
  REQUIRE(files.write_atomic("/p/sky130A/libs.tech/netgen/my_setup.tcl", "# pdk\n").ok());
  const std::vector<platform::Path> roots{"/p"};
  CHECK(*lvs::resolve_setup(files, "my_setup.tcl", "sky130A", roots) == platform::Path{"my_setup.tcl"});
  // A name with a directory part is only tried as given.
  CHECK(lvs::setup_candidates("dir/x.tcl", "sky130A", roots).size() == 1);
}

TEST_CASE("The -noflatten value is a list or a file", "[cap:lvs-command][req:Setup file and input resolution]") {
  platform::MemoryFileSystem files;
  auto list = lvs::parse_noflatten("nand2 {inv_.*}", files);
  REQUIRE(list.ok());
  CHECK(list.value() == std::vector<std::string>{"nand2", "inv_.*"});
  REQUIRE(files.write_atomic("keep.txt", "# comment\nnand2\n\nbuf_1 buf_2\n").ok());
  list = lvs::parse_noflatten("keep.txt", files);
  REQUIRE(list.ok());
  CHECK(list.value() == std::vector<std::string>{"nand2", "buf_1", "buf_2"});
  // Crafted-input limits (both netgen-compat modes).
  const auto long_entry = lvs::parse_noflatten(std::string(4097, 'x'), files);
  REQUIRE_FALSE(long_entry.ok());
  CHECK(long_entry.error().code == lvs::cmd_codes::kNoFlattenLimit);
  std::string many;
  for (int i = 0; i <= 65536; ++i) {
    many += "c ";
  }
  CHECK_FALSE(lvs::parse_noflatten(many, files).ok());
}

// ------------------------------------------------------------ execution

TEST_CASE("Progress per pair and one run per document", "[cap:lvs-command][req:Execution model]") {
  platform::InlineExecutor tasks;
  lvs::Session session;
  lvs::LvsJob job;
  job.document = "a.spice";
  job.first = circuit(kTop, "a.spice", 0);
  job.second = circuit(kTop, "b.spice", 1);
  std::vector<std::string> progress;
  job.on_progress = [&progress](const std::string& text) { progress.push_back(text); };
  const auto result = lvs::run_job(tasks, session, std::move(job), {});
  REQUIRE(result.ok());
  CHECK(progress == std::vector<std::string>{"1/2 inv vs inv", "2/2 top vs top"});
  CHECK_FALSE(session.running);
  CHECK(session.last_results.count("a.spice") == 1);
  session.running = "a.spice";
  const auto busy = lvs::run_job(tasks, session, lvs::LvsJob{}, {});
  REQUIRE_FALSE(busy.ok());
  CHECK(busy.error().code == lvs::cmd_codes::kBusy);
}

TEST_CASE("Cancelling a run", "[cap:lvs-command][req:Execution model]") {
  platform::InlineExecutor tasks;
  lvs::Session session;
  CHECK_FALSE(lvs::cancel_run(session, "a.spice"));
  lvs::LvsJob job;
  job.document = "a.spice";
  job.first = circuit(kTop, "a.spice", 0);
  job.second = circuit(kTop, "b.spice", 1);
  // The progress callback runs on the worker; cancelling there stops the
  // run before its next pair.
  job.on_progress = [&session](const std::string&) { (void)lvs::cancel_run(session, "a.spice"); };
  const auto result = lvs::run_job(tasks, session, std::move(job), {});
  REQUIRE(result.ok());
  CHECK(result.value().cancelled);
  CHECK(lvs::status_chip(session, "a.spice", nullptr).text == "LVS cancelled");
}

// ------------------------------------------------------------ headless

TEST_CASE("Headless exit statuses", "[cap:lvs-command][req:Headless LVS]") {
  lvs::LvsResult result;
  result.clean = true;
  CHECK(lvs::headless_status(result) == 0);
  result.clean = false;
  CHECK(lvs::headless_status(result) == 1);
  result.input_error = true;
  CHECK(lvs::headless_status(result) == 2);
  Console console;
  console.file("a.spice", kInv);
  console.file("b.spice", ".subckt inv A Y VDD VSS\nM1 Y A VDD VDD pfet\n.ends\n");
  REQUIRE(console.lvs({"a.spice", "a_copy.spice"}).diagnostics.size() > 0);
  CHECK(*console.editor.lvs.headless_status == 2);
  REQUIRE(console.lvs({"a.spice", "b.spice"}).ok());
  CHECK(*console.editor.lvs.headless_status == 1);
}

// ------------------------------------------------------------ browser and chip

TEST_CASE("Navigate to failing cell", "[cap:lvs-command][req:Result browser]") {
  const std::string one = ".subckt nand2 A B Y VDD VSS\nM1 Y A VDD VDD pfet\nM2 Y B VDD VDD pfet\n"
                          "M3 Y A n VSS nfet\nM4 n B VSS VSS nfet\n.ends\n"
                          ".subckt top A B Y VDD VSS\nXnand2_1 A B Y VDD VSS nand2\n.ends\n";
  std::string two = one;
  two.replace(two.find("M4 n B VSS VSS nfet"), 19, "M4 n B VSS VSS pfet");
  const auto result = lvs_hier_test::run(one, two);
  const auto model = lvs::browser_model(result);
  CHECK(model.overall == "mismatch");
  REQUIRE(model.pairs.size() == 2);
  CHECK(model.pairs[0].cell1 == "nand2");
  CHECK(model.pairs[0].status == "mismatch");
  CHECK(model.pairs[0].flattened);
  CHECK(model.pairs[1].cell1 == "top");
  CHECK(model.pairs[1].status == "mismatch");
  CHECK(contains(model.comp_out, "Final result:"));
}

TEST_CASE("Status chip states", "[cap:lvs-command][req:LVS status chip]") {
  lvs::Session session;
  CHECK(lvs::status_chip(session, "inv", nullptr).text == "LVS not run");
  lvs::LvsResult clean;
  clean.clean = true;
  session.last_results["inv"] = clean;
  CHECK(lvs::status_chip(session, "inv", nullptr).text == "LVS match");
  lvs::LvsResult mismatch;
  mismatch.pairs.resize(3);
  mismatch.pairs[0].status = lvs::PairStatus::match;
  mismatch.pairs[1].status = lvs::PairStatus::mismatch;
  mismatch.pairs[2].status = lvs::PairStatus::symmetric;
  session.last_results["inv"] = mismatch;
  CHECK(lvs::status_chip(session, "inv", nullptr).text == "LVS mismatch (2)");
  lvs::LvsResult error;
  error.input_error = true;
  session.last_results["inv"] = error;
  CHECK(lvs::status_chip(session, "inv", nullptr).text == "LVS error");
  lvs::LvsResult cancelled;
  cancelled.cancelled = true;
  session.last_results["inv"] = cancelled;
  CHECK(lvs::status_chip(session, "inv", nullptr).text == "LVS cancelled");
}

TEST_CASE("Edit after a match", "[cap:lvs-command][req:LVS status chip]") {
  lvs::Session session;
  lvs::LvsResult clean;
  clean.clean = true;
  session.last_results["inv"] = clean;
  CHECK(lvs::status_chip(session, "inv", nullptr).state == lvs::ChipState::match);
  session.changed.insert("inv");  // the layout edit hook marks the document
  CHECK(lvs::status_chip(session, "inv", nullptr).text == "LVS stale");
}

TEST_CASE("A changed input netlist makes the chip stale", "[cap:lvs-command][req:LVS status chip]") {
  platform::LocalFileSystem files;
  const platform::Path path = std::filesystem::current_path() / "lvs_cmd_chip_input.spice";
  REQUIRE(files.write_atomic(path, kInv).ok());
  lvs::Session session;
  lvs::LvsResult clean;
  clean.clean = true;
  session.last_results["doc"] = clean;
  session.inputs["doc"] = {lvs::stamp_input(files, path)};
  CHECK(lvs::status_chip(session, "doc", &files).text == "LVS match");
  std::filesystem::last_write_time(path, std::filesystem::last_write_time(path) + std::chrono::seconds(5));
  CHECK(lvs::status_chip(session, "doc", &files).text == "LVS stale");
  std::filesystem::remove(path);
}

// ------------------------------------------------------------ cross-probing

TEST_CASE("Cross-probe follows the result's layout references", "[cap:lvs-command][req:Cross-probing]") {
  lvs::LvsResult result;
  lvs::PairReport report;
  lvs::ObjectReference ref;
  ref.kind = lvs::ObjectReference::Kind::net;
  ref.circuit = 1;
  ref.name = "/x1/n3";
  ref.use_path = {"x1"};
  ref.point = {10, 20};
  report.references.push_back(ref);
  result.reports.push_back(report);
  lvs::ObjectRef object;
  object.kind = lvs::ObjectRef::Kind::net;
  object.circuit = 1;
  object.name = "/x1/n3";
  const auto places = lvs::cross_probe(result, object);
  REQUIRE(places.ok());
  REQUIRE(places.value().size() == 1);
  CHECK(places.value()[0].use_path == std::vector<std::string>{"x1"});
  CHECK(places.value()[0].point == hefesto::db::Point{10, 20});
  object.circuit = 2;
  const auto none = lvs::cross_probe(result, object);
  REQUIRE_FALSE(none.ok());
  CHECK(none.error().text == "no layout counterpart");
}
