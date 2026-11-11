// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// app-startup "quit command and exit status" (decision D-36): the quit
// policies. The desktop and CLI end the session with the status
// (cmd::ExitQuitPolicy); on iPadOS `quit` closes the project and returns to
// the project browser with a diagnostic naming the script and line
// (app::ProjectBrowserQuitPolicy, here with a test host).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include "hefesto/app/diag_codes.hpp"
#include "hefesto/app/quit_policy.hpp"
#include "hefesto/app/startup.hpp"
#include "hefesto/cmd/builtins.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/quit_policy.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/script/project.hpp"
#include "temp_dir.hpp"

namespace app = hefesto::app;
namespace cmd = hefesto::cmd;
namespace diag = hefesto::diag;
using hefesto::test::TempDir;
using hefesto::test::write_file;

#define QUIT_TAGS "[cap:app-startup][req:quit command and exit status]"

namespace {

// The iPad shell's side of the policy: it records what it was asked to show.
class TestBrowserHost final : public app::ProjectBrowserHost {
 public:
  void close_project(const diag::Diagnostic& diagnostic) override { shown.push_back(diagnostic); }
  std::vector<diag::Diagnostic> shown;
};

// A startup session with the built-in commands (`quit`), a `tech` whose
// `tech name` answers "minimum" and a recording `load`.
class Session {
 public:
  Session()
      : services_(hefesto::platform::PortableOptions{.storage_root = "/storage", .pdk_roots = {}, .in_memory = true,
                                                     .inline_tasks = true}),
        dispatcher_(registry_, services_.view(), project_, script_session_) {
    REQUIRE(cmd::register_builtins(registry_));
    (void)registry_.add(cmd::CommandSpec{"tech", "tech", "fake", [](cmd::CommandContext&, const cmd::Invocation& inv) {
                                           return !inv.args.empty() && inv.args[0] == "name"
                                                      ? cmd::CommandResult::success(std::string{"minimum"})
                                                      : cmd::CommandResult::success();
                                         }});
    (void)registry_.add(cmd::CommandSpec{"load", "load", "fake", [this](cmd::CommandContext&, const cmd::Invocation& inv) {
                                           loads.push_back(inv.args.empty() ? std::string{} : inv.args[0]);
                                           return cmd::CommandResult::success();
                                         }});
    dispatcher_.set_editor(&editor);
    env.home = dir.path() / "home";
    env.working_directory = dir.path() / "work";
    std::filesystem::create_directories(env.home);
    std::filesystem::create_directories(env.working_directory);
  }
  ~Session() { hefesto::script::close_project_interpreter(editor); }
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  // Startup with `rc` as the -rcfile startup file and `a.mag` as a file argument.
  void start_with_rc(const std::string& rc) {
    const auto path = dir.path() / "guard.magicrc";
    write_file(path, rc);
    rc_path = path.string();
    app::StartupOptions options;
    options.rc_file = rc_path;
    options.files.push_back(app::classify_file_argument("a.mag"));
    (void)app::run_startup(dispatcher_, project_, fs, options, env, sink);
  }

  cmd::ScriptSession& script() { return script_session_; }
  [[nodiscard]] std::optional<cmd::Value> var(std::string_view name) const { return editor.script.interp->get_var(name); }

  TempDir dir;
  hefesto::platform::LocalFileSystem fs;
  app::StartupEnvironment env;
  diag::CollectingSink sink;
  hefesto::editor::EditorState editor;
  std::vector<std::string> loads;
  std::string rc_path;

 private:
  hefesto::settings::ProjectSettings project_;
  hefesto::platform::PortableServices services_;
  cmd::ScriptSession script_session_{project_};
  cmd::CommandRegistry registry_;
  cmd::Dispatcher dispatcher_;
};

constexpr const char* kGuard =
    "set before 1\n"
    "if {[tech name] != \"sky130A\"} {quit -noprompt}\n"
    "set after 1\n";

}  // namespace

TEST_CASE("desktop quit from a startup file ends the session with its status", QUIT_TAGS) {
  Session s;
  s.start_with_rc("set before 1\nquit 3 -noprompt\nset after 1\n");
  CHECK(s.script().quit_requested);
  CHECK(s.script().exit_status == 3);
  CHECK(s.var("before").has_value());
  CHECK_FALSE(s.var("after").has_value());  // nothing after `quit` runs
  CHECK(s.loads.empty());                   // startup ends: no file argument is loaded
}

TEST_CASE("PDK guard quit on iPadOS", QUIT_TAGS) {
  Session s;
  TestBrowserHost host;
  app::ProjectBrowserQuitPolicy policy{host};
  s.script().quit_policy = &policy;
  s.start_with_rc(kGuard);
  // The app keeps running: no exit status, the script and startup stop.
  CHECK(s.script().quit_requested);
  CHECK(s.script().exit_status == 0);
  CHECK(s.var("before").has_value());
  CHECK_FALSE(s.var("after").has_value());
  CHECK(s.loads.empty());  // the project is not opened
  // The host returns to the project browser with a diagnostic naming the script and line.
  REQUIRE(host.shown.size() == 1);
  const diag::Diagnostic& shown = host.shown.front();
  CHECK(shown.code == app::codes::kQuitClosedProject);
  CHECK(shown.severity == diag::Severity::warning);
  CHECK(shown.native_only);
  const auto* where = std::get_if<diag::SourceLocation>(&shown.location);
  REQUIRE(where != nullptr);
  CHECK(where->file == s.rc_path);
  CHECK(where->line == 2U);
  CHECK(shown.text.find(s.rc_path) != std::string::npos);
  CHECK(shown.text.find("line 2") != std::string::npos);
}

TEST_CASE("iPadOS quit without -noprompt runs the exit checks first", QUIT_TAGS) {
  Session s;
  TestBrowserHost host;
  app::ProjectBrowserQuitPolicy policy{host};
  s.script().quit_policy = &policy;
  bool keep = true;
  s.script().exit_checks.emplace_back([&keep] { return !keep; });  // "discard modified cells?"
  s.start_with_rc("quit\nset after 1\n");
  CHECK_FALSE(s.script().quit_requested);  // vetoed: the project stays open
  CHECK(host.shown.empty());
  CHECK(s.var("after").has_value());
  CHECK(s.loads.size() == 1U);  // startup goes on and loads the file argument
}

TEST_CASE("an iPadOS quit typed outside a script names no location", QUIT_TAGS) {
  Session s;
  TestBrowserHost host;
  app::ProjectBrowserQuitPolicy policy{host};
  s.script().quit_policy = &policy;
  s.start_with_rc("");
  REQUIRE(s.editor.script.interp != nullptr);
  (void)s.editor.script.interp->evaluate("quit 2 -noprompt");
  CHECK(s.script().quit_requested);
  CHECK(s.script().exit_status == 0);
  REQUIRE(host.shown.size() == 1);
  CHECK(std::holds_alternative<std::monostate>(host.shown.front().location));
  CHECK(host.shown.front().text.starts_with("The project was closed by \"quit\";"));
}
