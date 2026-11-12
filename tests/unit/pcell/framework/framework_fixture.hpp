// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Test fixture of pcell/framework: a headless session with every command,
// the test technology (tests/data/pcell/framework/pcell.tech), a project interpreter, and a minimal stand-in
// for BINDING's bind_registry (each named command defined as `::magic::<n>`
// and `::<n>` dispatching through the session's dispatcher), so device
// procedures written in Tcl can call engine commands and the framework's
// builders before the binding workstream lands.
//
// The test PDK library `tpdk` (tests/data/pcell/framework/tpdk.tcl) follows
// the procedure contract of capability pcell-toolkit.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cell/fonts.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pcell/framework/gencell.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/ui/line.hpp"

namespace hefesto::test {

/// A fresh, process-unique temporary directory that is the current directory
/// while alive (generated cells are saved there; ctest runs test cases in
/// parallel processes).
class TempDirectory {
 public:
  TempDirectory()
      : previous_(std::filesystem::current_path()),
        dir_(std::filesystem::temp_directory_path() /
             ("hefesto-pcell-" + std::to_string(::getpid()) + "-" + std::to_string(counter()++))) {
    std::filesystem::remove_all(dir_);
    std::filesystem::create_directories(dir_);
    std::filesystem::current_path(dir_);
  }
  ~TempDirectory() {
    std::error_code ec;
    std::filesystem::current_path(previous_, ec);
    std::filesystem::remove_all(dir_, ec);
  }
  TempDirectory(const TempDirectory&) = delete;
  TempDirectory& operator=(const TempDirectory&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return dir_; }

 private:
  static int& counter() {
    static int n = 0;
    return n;
  }
  std::filesystem::path previous_;
  std::filesystem::path dir_;
};

class PcellSession {
 public:
  PcellSession() {
    REQUIRE(cmd::register_all_commands(registry));
    dispatcher.set_editor(&editor);
    script::SandboxPolicy policy;
    policy.granted_roots.push_back(HEFESTO_TEST_DATA_DIR);
    interp = &script::open_project_interpreter(editor, policy);
    bind_commands();
    // Workaround (open item for TCL, 2026-11-11): in the scaffold
    // interpreter an unknown command fails with `expected boolean value but
    // got ""` after auto_load instead of `invalid command name "x"`;
    // auto_noload skips that path, as tclsh's message is what Magic prints.
    REQUIRE(tcl("set ::auto_noload 1").ok());
    const auto tech = run("tech load " + std::string{HEFESTO_TEST_DATA_DIR} + "/pcell.tech");
    INFO((tech.diagnostics.empty() ? std::string{} : tech.diagnostics.front().text));
    REQUIRE(tech.ok());
    REQUIRE(run("load top").ok());
    // The startup layout window of a headless session (app/startup.cpp).
    (void)editor.windows.open_frame(editor.edit_cell);
    REQUIRE(run("box values 0 0 0 0").ok());
    load_fonts();
    const auto sourced = tcl("source " + std::string{HEFESTO_TEST_DATA_DIR} + "/tpdk.tcl");
    INFO((sourced.diagnostics.empty() ? std::string{} : sourced.diagnostics.front().text));
    REQUIRE(sourced.ok());
  }

  /// A command line through the dispatcher (as typed in a script).
  cmd::CommandResult run(std::string_view line) {
    cmd::CommandResult last = cmd::CommandResult::success();
    for (auto& r : ui::run_command_line(dispatcher, line, cmd::Origin::script)) {
      collect(r);
      last = std::move(r);
    }
    return last;
  }

  /// A Tcl script in the project interpreter.
  cmd::CommandResult tcl(std::string_view script_text) { return interp->evaluate(script_text); }

  /// The Tcl result of `script_text` as a string (REQUIREs success).
  std::string value(std::string_view script_text) {
    const auto r = tcl(script_text);
    INFO(script_text);
    REQUIRE(r.ok());
    return pcell::tcl_string(r.value);
  }

  /// True when Magic's default fonts (FreeSans, ...) were loaded from the
  /// reference tree (build/ref/magic/graphics), as Magic's startup file does.
  bool fonts_loaded = false;

  /// Every PCELL-GEN-002 text (`puts stderr` of the framework) seen so far.
  std::vector<std::string> printed_errors() const { return errors; }

  platform::PortableServices services{
      platform::PortableOptions{std::filesystem::temp_directory_path() / "hefesto-pcell-tests", {}, false, true}};
  settings::ProjectSettings project;
  cmd::ScriptSession session{project};
  cmd::CommandRegistry registry;
  editor::EditorState editor;
  cmd::Dispatcher dispatcher{registry, services.view(), project, session};
  script::Interpreter* interp = nullptr;
  std::vector<std::string> errors;

 private:
  void load_fonts() {
    const std::filesystem::path dir = std::string{HEFESTO_SOURCE_DIR} + "/build/ref/magic/graphics";
    std::error_code ec;
    if (!std::filesystem::exists(dir / "FreeSans.pt3", ec)) {
      return;
    }
    diag::NullSink sink;
    fonts_loaded = cell::load_default_fonts(services.view().files, {dir}, editor.fonts, sink) > 0;
  }

  void collect(const cmd::CommandResult& r) {
    for (const auto& d : r.diagnostics) {
      if (d.code == "PCELL-GEN-002") {
        errors.push_back(d.text);
      }
    }
  }

  void bind(const std::string& name, bool global) {
    auto handler = [this, name](const cmd::Invocation& inv) {
      cmd::CommandResult r = dispatcher.execute(cmd::Invocation{name, inv.args, cmd::Origin::script, inv.source});
      collect(r);
      return r;
    };
    interp->define_command("::magic::" + name, handler);
    if (global) {
      interp->define_command("::" + name, handler);
    }
  }

  void bind_commands() {
    for (const char* name : {"box", "paint", "erase", "label", "select", "getcell", "array", "identify", "delete",
                             "property", "cellname", "instance", "what", "units", "pushstack", "popstack",
                             "suspendall", "resumeall", "redraw", "load", "edit", "expand", "drc", "setpoint", "port",
                             "goto", "save", "pushbox", "popbox"}) {
      bind(name, true);
    }
    for (const auto& name : pcell::framework_command_names()) {
      bind(name, false);
    }
  }
};

}  // namespace hefesto::test
