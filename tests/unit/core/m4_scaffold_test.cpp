// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M4 scaffold (openspec/changes/m4-scripting tasks.md 2.3-2.6): the embedded
// Tcl evaluates in a project interpreter, every M4 command is registered
// with its CORE-NOTIMPL-004 stub, the four M4 code areas exist, and each
// open project (EditorState) gets its own interpreter, created when the
// project opens and deleted when it closes. Owners replace these
// [m4-scaffold] tests with [cap:][req:] ones as the stubs go away.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/gui/commands.hpp"
#include "hefesto/pcell/framework/gencell.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/script/tcl/embedded_library.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"
#include "hefesto/ui/line.hpp"

namespace cmd = hefesto::cmd;
namespace diag = hefesto::diag;
namespace editor = hefesto::editor;
namespace script = hefesto::script;

namespace {

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  editor::EditorState state;
  cmd::Dispatcher dispatcher{registry, services.view(), project, script};

  Session() {
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher.set_editor(&state);
  }

  cmd::CommandResult run(std::string_view line, cmd::Origin origin = cmd::Origin::script) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : hefesto::ui::run_command_line(dispatcher, line, origin)) {
      last = std::move(r);
    }
    return last;
  }
};

// The commands the scaffold registered as stubs (design D3).
std::vector<std::string> m4_commands() {
  std::vector<std::string> names = {"tag", "cif", "readspice", "mazeroute", "drc_save_report", "drc_load_report"};
  for (const auto& name : hefesto::pcell::framework_command_names()) {
    names.push_back(name);
  }
  for (const auto& name : hefesto::gui::gui_entry_command_names()) {
    names.push_back(name);
  }
  return names;
}

}  // namespace

// --- tasks.md 2.4/2.5: code areas and CORE-NOTIMPL-004 ---------------------

TEST_CASE("CORE-NOTIMPL-004 exists and the M4 code areas are declared", "[m4-scaffold]") {
  const auto code = diag::find_code(diag::codes::kCoreNotImplemented);
  REQUIRE(code.has_value());
  CHECK(code->code == "CORE-NOTIMPL-004");
  CHECK(code->severity == diag::Severity::error);
  for (const auto* area : {"script", "pcell", "tools", "gui"}) {
    INFO(area);
    CHECK(diag::find_code_area(area).has_value());
  }
  // The script table is assembled from fragments and sorted while compiling.
  const auto script_area = diag::find_code_area("script");
  REQUIRE(script_area.has_value());
  CHECK(diag::find_code("SCRIPT-TCL-001").has_value());
}

// --- tasks.md 2.5: every M4 command is registered ----------------------------

// The workstreams replaced every scaffold stub (round 1); a command run
// without its arguments or project interpreter fails with its own usage or
// area error, never CORE-NOTIMPL-004 (tasks.md 13.5).
TEST_CASE("every M4 command is registered and no longer a stub", "[m4-scaffold]") {
  Session s;
  for (const auto& name : m4_commands()) {
    INFO(name);
    REQUIRE(s.registry.find(name) != nullptr);
    const auto result = s.run(name);
    for (const auto& d : result.diagnostics) {
      CHECK(d.code != "CORE-NOTIMPL-004");
    }
  }
  // Reachable through the magic:: namespace like every command.
  const auto gencell = s.run("magic::gencell");
  REQUIRE_FALSE(gencell.diagnostics.empty());
  CHECK(gencell.diagnostics.front().code != "CORE-NOTIMPL-004");
}

// Regression: the Tcl-side procedures (drc_save_report, drcmanager,
// get_gencell_name, update_dialog, ...) are outside Magic's window command
// tables, so a typed abbreviation such as `drc` or `upd` must resolve as
// before; they are found by exact name only (cmd::kScriptClient).
TEST_CASE("M4 Tcl-side stubs never make a typed abbreviation ambiguous", "[m4-scaffold]") {
  Session s;
  for (const auto& name : m4_commands()) {
    INFO(name);
    if (name != "cif") {
      CHECK(s.registry.find(name, cmd::kScriptClient) != nullptr);
    }
  }
  CHECK(s.registry.resolve("drc", cmd::kLayoutClient).status == cmd::LookupStatus::found);
  CHECK(s.registry.resolve("drc", cmd::kLayoutClient).spec->name == "drc");
  CHECK(s.registry.resolve("get", cmd::kLayoutClient).status != cmd::LookupStatus::ambiguous);
  CHECK(s.registry.resolve("drc_save_report", cmd::kLayoutClient).status == cmd::LookupStatus::found);
  CHECK(s.registry.resolve("drc_save", cmd::kLayoutClient).status == cmd::LookupStatus::not_found);
  // A typed command-bar line (abbreviated lookup).
  CHECK(s.run("drc off", cmd::Origin::gui).ok());
  CHECK(s.run("drc on", cmd::Origin::gui).ok());
}

// --- tasks.md 2.3: the embedded Tcl -------------------------------------------

TEST_CASE("a project interpreter evaluates expr {1+1}", "[m4-scaffold]") {
  editor::EditorState state;
  auto& interp = script::open_project_interpreter(state, script::SandboxPolicy{});
  const auto result = interp.evaluate("expr {1+1}");
  REQUIRE(result.ok());
  CHECK(result.value.to_string() == "2");

  const auto error = interp.evaluate("error boom");
  REQUIRE_FALSE(error.ok());
  CHECK(error.diagnostics.front().code == "SCRIPT-TCL-001");
  CHECK(error.diagnostics.front().text == "boom");
}

TEST_CASE("the interpreter's library is the embedded one", "[m4-scaffold]") {
  REQUIRE(script::find_embedded_file("init.tcl").has_value());
  REQUIRE(script::find_embedded_file("clock.tcl").has_value());
  CHECK_FALSE(script::find_embedded_file("dde/pkgIndex.tcl").has_value());
  editor::EditorState state;
  auto& interp = script::open_project_interpreter(state, script::SandboxPolicy{});
  const std::string root{script::kVirtualLibraryRoot};
  CHECK(interp.evaluate("info library").value.to_string() == root);
  CHECK(interp.evaluate("set ::auto_path").value.to_string() == root);
  CHECK(interp.evaluate("info patchlevel").value.to_string() == "8.6.16");
  // init.tcl ran from memory: its procedures exist.
  CHECK(interp.has_command("::unknown"));
  CHECK(interp.has_command("::auto_load"));
}

TEST_CASE("native commands, variables and interrupt", "[m4-scaffold]") {
  editor::EditorState state;
  auto& interp = script::open_project_interpreter(state, script::SandboxPolicy{});
  std::vector<std::string> seen;
  interp.define_command("::magic::probe", [&](const cmd::Invocation& call) {
    seen = call.args;
    return cmd::CommandResult::success(cmd::Value{cmd::Value::List{1, "two words"}});
  });
  CHECK(interp.has_command("::magic::probe"));
  const auto result = interp.evaluate("llength [::magic::probe a {b c}]");
  REQUIRE(result.ok());
  CHECK(result.value.to_string() == "2");
  CHECK(seen == std::vector<std::string>{"a", "b c"});

  REQUIRE(interp.set_var("x", cmd::Value{42}));
  CHECK(interp.get_var("x").value().to_string() == "42");
  CHECK_FALSE(interp.get_var("nosuch").has_value());
  CHECK(interp.info_script().empty());

  // An interrupt requested during an evaluation stops it with an error.
  interp.define_command("::stop", [&](const cmd::Invocation&) {
    interp.request_interrupt();
    return cmd::CommandResult::success();
  });
  const auto stopped = interp.evaluate("while 1 { ::stop }");
  CHECK_FALSE(stopped.ok());
}

// --- tasks.md 2.6: one interpreter per open project -------------------------

TEST_CASE("two open projects get two interpreters", "[m4-scaffold]") {
  editor::EditorState first;
  editor::EditorState second;
  CHECK(first.script.interp == nullptr);
  auto& a = script::open_project_interpreter(first, script::SandboxPolicy{});
  auto& b = script::open_project_interpreter(second, script::SandboxPolicy{});
  CHECK(&a != &b);
  REQUIRE(a.evaluate("set PDK sky130A; namespace eval magic {}; proc magic::helper {} { return a }").ok());
  REQUIRE(b.evaluate("set PDK gf180mcuD").ok());
  CHECK(a.evaluate("set PDK").value.to_string() == "sky130A");
  CHECK(b.evaluate("set PDK").value.to_string() == "gf180mcuD");
  CHECK(a.has_command("::magic::helper"));
  CHECK_FALSE(b.has_command("::magic::helper"));

  script::close_project_interpreter(first);
  CHECK(first.script.interp == nullptr);
  CHECK(b.evaluate("set PDK").value.to_string() == "gf180mcuD");
}

TEST_CASE("the shared script interfaces have their defaults", "[m4-scaffold]") {
  editor::EditorState state;
  CHECK(state.script.tags.empty());
  CHECK(state.script.menu_model.menus().empty());
  CHECK(state.script.pdk_trust.entries().empty());
  CHECK_FALSE(state.script.gencell.dialog.has_value());
  CHECK(state.script.events.size() == 0);
  CHECK(state.script.sandbox.removed_commands ==
        std::vector<std::string>{"exec", "socket", "load", "unload", "exit"});
}
