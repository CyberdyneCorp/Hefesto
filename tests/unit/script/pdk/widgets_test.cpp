// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// scripting-pdk-integration "Wrapper widget operations in PDK menu
// callbacks" (tasks.md 6.5; Q-SCRIPT-34, Q-SCRIPT-42, D-32): the PDK menu
// callbacks edit the native MenuModel, `tk_getOpenFile` asks the file
// picker, every other widget statement is skipped with a diagnostic.
// Owner: PDKINT.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_picker.hpp"
#include "hefesto/script/pdk/widgets.hpp"
#include "pdk_fixture.hpp"

namespace script = hefesto::script;
namespace ref = hefesto::test;
using script::MenuItem;
using script::test::PdkSession;
using script::test::TempDir;
using script::test::write_file;

namespace {

class Skips final : public script::ScriptObserver {
 public:
  void on_skipped_statement(const script::SkippedStatement& s) override { seen.push_back(s); }
  void on_menu_model_changed() override { ++changes; }
  std::vector<script::SkippedStatement> seen;
  int changes = 0;
};

struct WidgetSession {
  PdkSession s;
  hefesto::platform::ScriptedFilePicker picker;
  Skips skips;
  hefesto::diag::CollectingSink sink;
  script::WidgetInterceptor interceptor{s.editor.script.menu_model, picker, s.editor.script.events};
  std::vector<std::string> calls;  // the framework procedures the callbacks called

  explicit WidgetSession(const std::optional<std::string>& pdk_root = std::nullopt)
      : s(script::test::pdk_env(pdk_root)) {
    s.editor.script.events.add(skips);
    interceptor.set_sink(&sink);
    REQUIRE(interceptor.install(s.tcl(), ".layout1").ok());
    // PCELL's toolkit menus and netlist import are recorded here.
    for (const char* name : {"add_toolkit_menu", "add_toolkit_command", "add_toolkit_separator", "netlist_to_layout"}) {
      s.tcl().define_command(std::string{"::magic::"} + name, [this, name](const hefesto::cmd::Invocation& inv) {
        std::string line = name;
        for (const auto& a : inv.args) {
          line += " " + a;
        }
        calls.push_back(line);
        return hefesto::cmd::CommandResult::success();
      });
    }
  }
  ~WidgetSession() { s.editor.script.events.remove(skips); }
  WidgetSession(const WidgetSession&) = delete;
  WidgetSession& operator=(const WidgetSession&) = delete;

  [[nodiscard]] std::vector<std::string> labels(std::string_view menu) const {
    std::vector<std::string> out;
    for (const auto& item : s.editor.script.menu_model.items(menu)) {
      out.push_back(item.kind == MenuItem::Kind::separator ? "---" : item.label);
    }
    return out;
  }
};

std::optional<std::filesystem::path> magic_dir(const ref::PinnedPdk& pdk) {
  const auto variant = ref::pinned_variant(pdk);
  return variant ? std::optional{*variant / "libs.tech/magic"} : std::nullopt;
}

}  // namespace

TEST_CASE("sky130 menus in a new window", "[cap:scripting-pdk-integration][req:Wrapper widget operations in PDK menu callbacks]") {
  const auto dir = magic_dir(ref::kSky130A);
  if (!dir) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  WidgetSession w{dir->parent_path().parent_path().parent_path().string()};
  REQUIRE(w.s.tcl().source(*dir / "sky130A.tcl").ok());
  // The wrapper evaluates Opts(callback) with its local `framename`.
  w.s.eval("apply {{framename} {global Opts; eval $Opts(callback)}} .layout1");
  CHECK(w.labels("Drc") == std::vector<std::string>{"DRC Routing"});
  CHECK(w.s.editor.script.menu_model.items("Drc").front().command == "drc style drc(routing)");
  CHECK(w.labels("File") == std::vector<std::string>{"Open...", "Save...", "---", "Flush changes", "---",
                                                     "Import SPICE", "---", "Read GDS", "Write GDS", "---",
                                                     "Save All and Quit", "Quit"});
  const auto& file = w.s.editor.script.menu_model.items("File");
  CHECK_FALSE(file[5].native);
  CHECK(file[5].command == "sky130::importspice");
  CHECK(file[3].native);
  CHECK(w.skips.seen.empty());  // no skipped-statement diagnostic
  CHECK(w.sink.snapshot().empty());
  CHECK(w.skips.changes == 3);
  // The Devices menus go through the toolkit procedures on the pane frame.
  REQUIRE_FALSE(w.calls.empty());
  CHECK(w.calls.front() == "add_toolkit_menu .layout1.pane.top Devices 1 pdk1");
  CHECK(std::find(w.calls.begin(), w.calls.end(), "add_toolkit_menu .layout1.pane.top Devices 2 pdk2") !=
        w.calls.end());
  CHECK(w.s.eval("set Winopts(.layout1,cmdentry)") == "0");
}

TEST_CASE("Import SPICE uses the native picker", "[cap:scripting-pdk-integration][req:Wrapper widget operations in PDK menu callbacks]") {
  const auto dir = magic_dir(ref::kSky130A);
  if (!dir) {
    SKIP("PDK cache absent (fetch it with scripts/fetch_pdk.py)");
  }
  WidgetSession w{dir->parent_path().parent_path().parent_path().string()};
  REQUIRE(w.s.tcl().source(*dir / "sky130A.tcl").ok());
  w.s.eval("apply {{framename} {global Opts; eval $Opts(callback)}} .layout1");
  const auto& file = w.s.editor.script.menu_model.items("File");
  const auto item = std::find_if(file.begin(), file.end(), [](const MenuItem& i) { return i.label == "Import SPICE"; });
  REQUIRE(item != file.end());
  w.calls.clear();
  w.picker.push_answer(hefesto::platform::Path{"inv.spice"});
  w.s.eval(item->command);  // the shell issues the item's command
  CHECK(w.calls == std::vector<std::string>{"netlist_to_layout inv.spice sky130"});
  REQUIRE(w.picker.requests().size() == 1);
  CHECK_FALSE(w.picker.requests().front().file_types.empty());
  // Cancel: the empty string, nothing imported.
  w.calls.clear();
  w.s.eval(item->command);
  CHECK(w.calls.empty());
}

TEST_CASE("Unknown widget statement", "[cap:scripting-pdk-integration][req:Wrapper widget operations in PDK menu callbacks]") {
  WidgetSession w;
  TempDir dir;
  write_file(dir.path() / "pdk.tcl",
             "proc mypdk::addtechmenu {framename} {\n"
             "   if {[catch {${framename}.titlebar cget -height}]} {\n"
             "      set layoutframe ${framename}.pane.top\n"
             "   } else {\n"
             "      set layoutframe ${framename}\n"
             "   }\n"
             "   ${layoutframe}.titlebar.mbuttons.cell.toolmenu add command -label X -command y\n"
             "   magic::add_toolkit_menu $layoutframe Extra\n"
             "   winfo exists .x\n"
             "}\n");
  w.s.eval("namespace eval mypdk {}");
  REQUIRE(w.s.tcl().source(dir.path() / "pdk.tcl").ok());
  w.s.eval("mypdk::addtechmenu .layout1");
  CHECK(w.calls == std::vector<std::string>{"add_toolkit_menu .layout1.pane.top Extra"});
  REQUIRE(w.skips.seen.size() == 2);
  CHECK(w.skips.seen[0].statement == ".layout1.pane.top.titlebar.mbuttons.cell.toolmenu add command -label X -command y");
  CHECK(w.skips.seen[1].statement == "winfo exists .x");
  // Board M4-Menu-Bar note 11: each row names the procedure the statement ran in.
  CHECK(w.skips.seen[0].procedure == "::mypdk::addtechmenu");
  CHECK(w.skips.seen[1].procedure == "::mypdk::addtechmenu");
  const auto diagnostics = w.sink.snapshot();
  REQUIRE(diagnostics.size() == 2);
  CHECK(diagnostics[0].code == "SCRIPT-PDK-004");
  CHECK(diagnostics[0].text.find(".layout1.pane.top.titlebar.mbuttons.cell.toolmenu add command") != std::string::npos);
  INFO(diagnostics[0].text);
  const auto* where = std::get_if<hefesto::diag::SourceLocation>(&diagnostics[0].location);
  REQUIRE(where != nullptr);
  CHECK(where->file == std::filesystem::weakly_canonical(dir.path() / "pdk.tcl").string());  // TIP 280
  CHECK(where->line == 7);
  CHECK(w.s.editor.script.menu_model.menus().empty());
}

TEST_CASE("skipped statements while a script is sourced name its file and line",
          "[cap:scripting-pdk-integration][req:Wrapper widget operations in PDK menu callbacks]") {
  WidgetSession w;
  TempDir dir;
  write_file(dir.path() / "menu.tcl", "set a 1\nwm title . Magic\n.layout1.foo configure -x 1\nset b 2\n");
  REQUIRE(w.s.tcl().source(dir.path() / "menu.tcl").ok());
  CHECK(w.s.eval("set b") == "2");  // evaluation continues
  REQUIRE(w.skips.seen.size() == 2);
  CHECK(w.skips.seen[0].statement == "wm title . Magic");
  CHECK(std::filesystem::weakly_canonical(w.skips.seen[0].location.file) ==
        std::filesystem::weakly_canonical(dir.path() / "menu.tcl"));
  CHECK(w.skips.seen[0].location.line == 2);
  CHECK(w.skips.seen[1].location.line == 3);
  CHECK(w.skips.seen[0].procedure.empty());  // the file's top level
  CHECK(w.skips.seen[1].procedure.empty());
}

TEST_CASE("a skipped statement names the procedure it ran in",
          "[cap:scripting-pdk-integration][req:Wrapper widget operations in PDK menu callbacks]") {
  WidgetSession w;
  TempDir dir;
  // A procedure defined in a sourced file (TIP 280 location) and one defined
  // without a file: both are named; the interception procedures never are.
  write_file(dir.path() / "menus.tcl",
             "namespace eval mypdk {}\n"
             "proc mypdk::inner {f} {\n"
             "   wm geometry $f 900x700\n"
             "   ${f}.extra configure -x 1\n"
             "}\n");
  REQUIRE(w.s.tcl().source(dir.path() / "menus.tcl").ok());
  w.s.eval("proc ::outer {} { bind . <Key> {} }");
  w.s.eval("mypdk::inner .layout1");
  w.s.eval("::outer");
  const auto diagnostics = w.sink.snapshot();
  REQUIRE(w.skips.seen.size() == 3);
  CHECK(w.skips.seen[0].procedure == "::mypdk::inner");
  CHECK(w.skips.seen[0].location.line == 3);
  CHECK(w.skips.seen[1].procedure == "::mypdk::inner");  // through `unknown`
  CHECK(w.skips.seen[1].location.line == 4);
  CHECK(w.skips.seen[2].procedure == "::outer");
  CHECK(w.skips.seen[2].statement == "bind . <Key> {}");
  REQUIRE(diagnostics.size() == 3);
  CHECK(diagnostics[0].text.find("(in ::mypdk::inner)") != std::string::npos);
}

TEST_CASE("unknown commands still fail through the chained handler",
          "[cap:scripting-pdk-integration][req:Wrapper widget operations in PDK menu callbacks]") {
  WidgetSession w;
  const auto result = w.s.tcl().evaluate("no_such_command 1");
  REQUIRE_FALSE(result.ok());
  CHECK(result.diagnostics.front().text == "invalid command name \"no_such_command\"");
  // A second frame shares the common commands.
  REQUIRE(w.interceptor.install(w.s.tcl(), ".layout2").ok());
  w.s.eval(".layout2.pane.top.titlebar.mbuttons.drc.toolmenu add separator");
  CHECK(w.labels("Drc") == std::vector<std::string>{"---"});
}

TEST_CASE("the menu model appends and inserts like Tk menus",
          "[cap:scripting-pdk-integration][req:Wrapper widget operations in PDK menu callbacks]") {
  script::MenuModel menus;
  menus.append("Drc", {MenuItem::Kind::command, "DRC Routing", "drc style drc(routing)", false});
  menus.insert("File", 4, {MenuItem::Kind::command, "Import SPICE", "x", false});
  menus.insert("Drc", 0, {MenuItem::Kind::separator, "", "", false});
  CHECK(menus.menus() == std::vector<std::string>{"Drc", "File"});
  REQUIRE(menus.items("Drc").size() == 2);
  CHECK(menus.items("Drc").front().kind == MenuItem::Kind::separator);
  CHECK(menus.items("File").size() == 1);  // clamped to the end
  CHECK(menus.items("Nope").empty());
  menus.clear();
  CHECK(menus.menus().empty());
}
