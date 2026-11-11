// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// scripting-pdk-integration "Script-host startup sequence" through
// app::run_startup (tasks.md 6.1): the startup files run after the gencell
// toolkit is defined, and background DRC is off while the command-line
// files load. The session has no interpreter before startup, so startup
// creates and binds it as a front end's would be. Owner: PDKINT.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/app/startup.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "pdk_fixture.hpp"

namespace app = hefesto::app;
namespace cmd = hefesto::cmd;
namespace ref = hefesto::test;
using hefesto::script::test::TempDir;
using hefesto::script::test::write_file;

namespace {

struct StartupSession {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, false, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script_session{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;
  TempDir dir;
  hefesto::platform::LocalFileSystem fs;
  app::StartupEnvironment env;
  hefesto::diag::CollectingSink sink;

  StartupSession() {
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script_session);
    dispatcher->set_editor(&editor);
    env.home = dir.path() / "home";
    env.working_directory = dir.path() / "work";
    std::filesystem::create_directories(env.home);
    std::filesystem::create_directories(env.working_directory);
  }

  hefesto::diag::Outcome<app::StartupResult> start(const app::StartupOptions& options) {
    return app::run_startup(*dispatcher, project, fs, options, env, sink);
  }

  std::string eval(std::string_view script) {
    REQUIRE(editor.script.interp != nullptr);
    const auto result = editor.script.interp->evaluate(script);
    INFO(script);
    REQUIRE(result.ok());
    return result.value.to_string();
  }

  void dump() const {
    for (const auto& d : sink.snapshot()) {
      UNSCOPED_INFO(d.code << " " << d.text);
    }
  }
};

}  // namespace

TEST_CASE("Startup file sees the gencell toolkit", "[cap:scripting-pdk-integration][req:Script-host startup sequence]") {
  StartupSession s;
  std::string rc =
      "set ::had_gencell [llength [info commands ::magic::gencell]]\n"
      "set ::had_toolkit_menu [llength [info commands ::magic::add_toolkit_menu]]\n"
      "set ::had_dialog [llength [info commands ::magic::add_entry]]\n"
      // The PDK helpers stay undefined until the PDK's script defines them.
      "set ::had_spice2float [llength [info commands ::magic::spice2float]]\n";
  const auto variant = ref::pinned_variant(ref::kSky130A);
  if (variant) {
    // The scenario's .magicrc: source the device script, which calls
    // magic::add_toolkit_menu from a window callback and
    // magic::spice2float from its device procedures.
    s.env.pdk_root = std::filesystem::path{HEFESTO_SOURCE_DIR} / ".cache" / "pdks";
    rc = "set PDK_ROOT " + variant->parent_path().string() + "\n" + rc + "source $PDK_ROOT/sky130A/libs.tech/magic/sky130A.tcl\n";
  }
  write_file(s.env.working_directory / ".magicrc", rc);
  const auto run = s.start({});
  s.dump();
  REQUIRE(run.ok());
  CHECK(s.eval("set ::had_gencell") == "1");
  CHECK(s.eval("set ::had_toolkit_menu") == "1");
  CHECK(s.eval("set ::had_dialog") == "1");
  CHECK(s.eval("set ::had_spice2float") == "0");
  if (variant) {
    CHECK(s.eval("llength [info commands ::sky130::addtechmenu]") == "1");
    CHECK(s.eval("llength [info commands ::magic::spice2float]") == "1");
  }
}

TEST_CASE("DRC deferred during startup", "[cap:scripting-pdk-integration][req:Script-host startup sequence]") {
  StartupSession s;
  // The command-line files run with background DRC off: a cell and a `.tcl`
  // argument load in that state (the worker checks nothing while it is off).
  write_file(s.env.working_directory / "probe.tcl", "set ::drc_during [drc status]\n");
  write_file(s.env.working_directory / "inv.mag", "magic\ntech minimum\ntimestamp 0\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n");
  app::StartupOptions options;
  options.display_type = "XWIND";  // wrapper mode: magic.tcl loads the files after `drc off`
  options.files = {app::FileArgument{"inv", app::FileKind::cell}, app::FileArgument{"probe.tcl", app::FileKind::script}};
  REQUIRE(s.editor.drc.background == hefesto::drc::BackgroundMode::on);
  const auto run = s.start(options);
  s.dump();
  REQUIRE(run.ok());
  CHECK(s.eval("set ::drc_during") == "0");
  // At the end of the startup sequence DRC is on again.
  CHECK(s.editor.drc.background == hefesto::drc::BackgroundMode::on);
  CHECK(s.eval("drc status") == "1");
}

TEST_CASE("with NULL graphics the command-line files load with DRC on",
          "[cap:scripting-pdk-integration][req:Script-host startup sequence]") {
  // Magic's C startup loads the files before magic.tcl turns DRC off
  // (utils/main.c:1189-1225), so a script argument sees DRC on.
  for (const std::optional<std::string>& display : {std::optional<std::string>{"NULL"}, std::optional<std::string>{}}) {
    StartupSession s;
    write_file(s.env.working_directory / "probe.tcl", "set ::drc_during [drc status]\n");
    app::StartupOptions options;
    options.display_type = display;
    options.files = {app::FileArgument{"probe.tcl", app::FileKind::script}};
    REQUIRE(s.start(options).ok());
    CHECK(s.eval("set ::drc_during") == "1");
  }
}

TEST_CASE("DRC turned off by a startup file stays off", "[cap:scripting-pdk-integration][req:Script-host startup sequence]") {
  StartupSession s;
  write_file(s.env.working_directory / ".magicrc", "drc off\n");
  write_file(s.env.working_directory / "probe.tcl", "set ::drc_during [drc status]\n");
  app::StartupOptions options;
  options.display_type = "XWIND";
  options.files = {app::FileArgument{"probe.tcl", app::FileKind::script}};
  REQUIRE(s.start(options).ok());
  CHECK(s.eval("set ::drc_during") == "0");
  CHECK(s.editor.drc.background == hefesto::drc::BackgroundMode::off);
}
