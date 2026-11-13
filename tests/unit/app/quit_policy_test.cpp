// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// app-startup "quit command and exit status" (decision D-36): the quit
// policies. The desktop and CLI end the session with the status
// (cmd::ExitQuitPolicy); on iPadOS `quit` closes the project and returns to
// the project browser with a diagnostic naming the script and line
// (app::ProjectBrowserQuitPolicy, here with a test host).

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <string>
#include <vector>

#include "hefesto/app/diag_codes.hpp"
#include "hefesto/app/quit_policy.hpp"
#include "hefesto/app/startup.hpp"
#include "hefesto/cmd/builtins.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/prompt.hpp"
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

// A prompt host that answers every question with `answer` and records it.
class ScriptedHost final : public cmd::PromptHost {
 public:
  explicit ScriptedHost(std::size_t answer) : answer_(answer) {}
  std::size_t ask(std::string_view question, std::span<const std::string>, std::size_t) override {
    asked.emplace_back(question);
    return answer_;
  }
  std::vector<std::string> asked;

 private:
  std::size_t answer_;
};

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
  hefesto::settings::ProjectSettings& project() { return project_; }
  cmd::CommandResult run(const std::string& name, std::vector<std::string> args = {}) {
    return dispatcher_.execute(cmd::Invocation{name, std::move(args), cmd::Origin::cli, std::nullopt});
  }
  // A cell with unsaved changes (CDMODIFIED), as an edit leaves it.
  void modify(const std::string& name) {
    auto made = editor.cells.create(name, 0);
    REQUIRE(made.ok());
    made.value()->flags().modified = true;
  }
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

// Regression (Q-SCRIPT-48, user decision 2026-11-12): Magic's layout client
// asks `N Magic cells have been modified` on `quit` (DBWexit, CmdWarnWrite)
// and quits on any answer but "no", so with no `tk_dialog` (-1) it quits.
// Hefesto asked nothing at all; it now asks, and when nobody can be asked it
// quits with magic-compat ON and keeps the session with OFF.
TEST_CASE("quit asks before losing modified cells", QUIT_TAGS) {
  Session s;
  s.modify("a");
  auto internal = s.editor.cells.create("__internal", 0);
  REQUIRE(internal.ok());
  internal.value()->flags().internal = true;
  internal.value()->flags().modified = true;  // internal cells are not counted

  SECTION("an answered no keeps the session in both settings") {
    ScriptedHost no(0);
    s.editor.prompt = &no;
    for (const bool compat : {true, false}) {
      s.project().compat.magic_compat = compat;
      CHECK(s.run("quit").ok());
      CHECK_FALSE(s.script().quit_requested);
    }
    REQUIRE(no.asked.size() == 2U);
    CHECK(no.asked.front() == "1 Magic cell has been modified.\n  Do you want to exit magic and lose it? ");
  }
  SECTION("yes quits; the count is plural for several cells") {
    s.modify("b");
    ScriptedHost yes(1);
    s.editor.prompt = &yes;
    s.project().compat.magic_compat = false;
    CHECK(s.run("quit", {"2"}).ok());
    CHECK(s.script().quit_requested);
    CHECK(s.script().exit_status == 2);
    REQUIRE(yes.asked.size() == 1U);
    CHECK(yes.asked.front() == "2 Magic cells have been modified.\n  Do you want to exit magic and lose them? ");
  }
  SECTION("nobody to ask, magic-compat ON: quits as Magic's -1 does") {
    cmd::HeadlessPromptHost headless;
    s.editor.prompt = &headless;
    CHECK(s.run("quit").ok());
    CHECK(s.script().quit_requested);
  }
  SECTION("nobody to ask, magic-compat OFF: the session stays") {
    s.project().compat.magic_compat = false;
    CHECK(s.run("quit").ok());  // no prompt host at all
    CHECK_FALSE(s.script().quit_requested);
    cmd::HeadlessPromptHost headless;
    s.editor.prompt = &headless;
    CHECK(s.run("quit").ok());
    CHECK_FALSE(s.script().quit_requested);
    CHECK(s.run("quit", {"-noprompt"}).ok());  // -noprompt asks nothing
    CHECK(s.script().quit_requested);
  }
  SECTION("a vetoing exit check runs first and the cells are not asked about") {
    ScriptedHost yes(1);
    s.editor.prompt = &yes;
    s.script().exit_checks.emplace_back([] { return false; });  // the color map: "no"
    CHECK(s.run("quit").ok());
    CHECK_FALSE(s.script().quit_requested);
    CHECK(yes.asked.empty());
  }
}

// D-36 keeps applying after the question: the iPad policy closes the project
// only when the modified-cells check lets `quit` through.
TEST_CASE("iPadOS quit with modified cells follows the same question", QUIT_TAGS) {
  Session s;
  TestBrowserHost host;
  app::ProjectBrowserQuitPolicy policy{host};
  s.script().quit_policy = &policy;
  s.modify("a");
  cmd::HeadlessPromptHost headless;
  s.editor.prompt = &headless;
  s.project().compat.magic_compat = false;
  CHECK(s.run("quit").ok());
  CHECK_FALSE(s.script().quit_requested);
  CHECK(host.shown.empty());  // the project stays open
  s.project().compat.magic_compat = true;
  const cmd::CommandResult closed = s.run("quit", {"3"});
  CHECK(s.script().quit_requested);
  CHECK(s.script().exit_status == 0);  // iPadOS applies no exit status
  REQUIRE(host.shown.size() == 1U);
  CHECK(host.shown.front().code == app::codes::kQuitClosedProject);
  REQUIRE(closed.diagnostics.size() == 1U);
}
