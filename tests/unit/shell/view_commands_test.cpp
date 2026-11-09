// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The command bar's binding to the core command API: lines become
// Invocations with origin `gui`, run through the core Dispatcher, and the
// M0 view commands are ordinary registered commands.

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/context.hpp"
#include "hefesto/shell/command_bar_model.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/view_commands.hpp"

using namespace hefesto;
using namespace hefesto::shell;

namespace {

platform::PortableOptions test_options() {
  return platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true, .inline_tasks = true};
}

struct Recorder {
  ThemeMode theme = ThemeMode::dark;
  double pan_x = 0.0;
  double pan_y = 0.0;

  ViewActions actions() {
    return ViewActions{[this](ThemeMode m) { theme = m; }, [this](double dx, double dy) {
                         pan_x = dx;
                         pan_y = dy;
                       }};
  }
};

class OriginRecorder final : public cmd::CommandObserver {
 public:
  void on_command(const cmd::Invocation& invocation, const cmd::CommandResult& /*result*/) override {
    seen.push_back(invocation);
  }
  std::vector<cmd::Invocation> seen;
};

bool has_code(const CommandOutcome& outcome, std::string_view code) {
  for (const std::string& d : outcome.diagnostics) {
    if (d.find(code) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("view commands are core commands dispatched from the command bar",
          "[cap:hefesto-architecture][req:Single command API]") {
  CoreSession core(test_options());
  // design.md D5: `view` and `zoom` are the core's (register_all_commands);
  // the shell only adds `theme` and `pan`.
  REQUIRE(core.registry().find("zoom") != nullptr);
  REQUIRE(core.registry().find("view") != nullptr);
  Recorder rec;
  REQUIRE(register_view_commands(core.registry(), rec.actions()));
  CoreCommandSink sink(core.dispatcher());

  CHECK(sink.submit("theme light").ok());
  CHECK(rec.theme == ThemeMode::light);
  CHECK(sink.submit("pan -12.5 40").ok());
  CHECK(rec.pan_x == -12.5);
  CHECK(rec.pan_y == 40.0);

  for (const char* bad : {"theme blue", "pan 1", "pan a b"}) {
    const CommandOutcome out = sink.submit(bad);
    CHECK_FALSE(out.ok());
    CHECK(has_code(out, "CMD-USAGE-001"));
  }

  const CommandOutcome help = sink.submit("help");
  REQUIRE(help.ok());
  CHECK(help.message.find("theme dark|light") != std::string::npos);
  CHECK(help.message.find("zoom factor") != std::string::npos);  // the core's synopsis
  CHECK(help.message.find("version") != std::string::npos);      // core built-ins are there too
  CHECK_FALSE(register_view_commands(core.registry(), rec.actions()));  // names already taken
}

TEST_CASE("the core zoom moves the current window, so the canvas follows it",
          "[cap:ui-windows-and-views][req:View mapping]") {
  CoreSession core(test_options());
  auto& windows = core.editor().windows;
  auto opened = windows.open(nullptr, view::ScreenRect{0, 0, 799, 599});
  REQUIRE(opened.ok());
  windows.set_current(opened.value()->id());
  const db::Rect before = windows.current()->mapping().surface;
  CoreCommandSink sink(core.dispatcher());
  REQUIRE(sink.submit("zoom 0.5").ok());
  CHECK(windows.current()->mapping().surface.width() < before.width());
  CHECK_FALSE(sink.submit("zoom -1").ok());
}

TEST_CASE("command bar lines run through the core Dispatcher with origin gui",
          "[cap:hefesto-architecture][req:Single command API]") {
  CoreSession core(test_options());
  OriginRecorder observer;
  core.dispatcher().add_observer(observer);
  CoreCommandSink sink(core.dispatcher());
  CommandBarModel bar(sink);

  const CommandOutcome echo = bar.submit("  echo a {b c}; echo last ");
  REQUIRE(echo.ok());
  CHECK(echo.message == "last");
  REQUIRE(observer.seen.size() == 2);
  CHECK(observer.seen[0].name == "echo");
  CHECK(observer.seen[0].args == std::vector<std::string>{"a", "b c"});
  CHECK(observer.seen[0].origin == cmd::Origin::gui);
}

TEST_CASE("command bar failures carry core diagnostics",
          "[cap:hefesto-diagnostics][req:Command results and errors]") {
  CoreSession core(test_options());
  CoreCommandSink sink(core.dispatcher());

  // `paint` is a registered M1 command now; use a name no area registers.
  const CommandOutcome unknown = sink.submit("nosuchcommand m1; echo never");
  CHECK_FALSE(unknown.ok());
  CHECK(has_code(unknown, "CMD-DISPATCH-001"));
  CHECK(unknown.message.find("nosuchcommand") != std::string::npos);

  const CommandOutcome parse = sink.submit("echo \"unterminated");
  CHECK_FALSE(parse.ok());
  CHECK(has_code(parse, "CMD-PARSE-001"));

  CHECK_FALSE(core.quit_requested());
  CHECK(sink.submit("quit").ok());
  CHECK(core.quit_requested());
}

TEST_CASE("the desktop session keeps a rotating log and a command log",
          "[cap:hefesto-diagnostics][req:Diagnostic presentation and logging]") {
  CoreSession core(test_options(), platform::Path{"/app/logs"});
  CHECK_FALSE(core.log_warning());
  CoreCommandSink sink(core.dispatcher());
  CHECK(sink.submit("echo logged").ok());
  CHECK_FALSE(sink.submit("nosuch").ok());
  CHECK(core.command_log().history() == std::vector<std::string>{"echo logged", "nosuch"});
  const std::string log = core.files().read("/app/logs/hefesto.log").value();
  CHECK(log.find("info session start hefesto ") != std::string::npos);
  CHECK(log.find("info cmd[gui] echo logged -> ok") != std::string::npos);
  CHECK(log.find("CMD-DISPATCH-001") != std::string::npos);
}
