// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// logcommands, sleep, send, history, source, *bypass, completion and the
// post-command update (capabilities ui-command-dispatch,
// ui-wizard-and-debug, hefesto-input-and-touch "Command entry on every
// platform").

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <vector>

#include "hefesto/ui/command_log.hpp"
#include "hefesto/ui/complete.hpp"
#include "hefesto/ui/history.hpp"
#include "hefesto/ui/line.hpp"
#include "ui_fixture.hpp"

namespace cmd = hefesto::cmd;
namespace ui = hefesto::ui;
using hefesto::test::UiSession;

namespace {

std::string read(UiSession& s, const char* path) {
  const auto text = s.files().read(path);
  REQUIRE(text.ok());
  return text.value();
}

// The log without its date line.
std::string without_date(const std::string& log) {
  std::string out;
  std::size_t pos = 0;
  while (pos < log.size()) {
    const std::size_t nl = log.find('\n', pos);
    const std::string line = log.substr(pos, nl - pos);
    if (!line.starts_with("# Date: ")) {
      out += line + "\n";
    }
    pos = nl == std::string::npos ? log.size() : nl + 1;
  }
  return out;
}

}  // namespace

// --- Command logging ------------------------------------------------------------------

TEST_CASE("logcommands writes the Tcl-build log", "[cap:ui-command-dispatch][req:Command logging][golden:UI-G-016]") {
  UiSession s;
  // Regression (UI-G-016): a file name alone is a usage error in Magic 8.3.694.
  CHECK(UiSession::first_error(s.run("logcommands /app/log.txt")).find(
            "Usage: logcommands [start|stop|update|suspend|resume [filename]]") != std::string::npos);
  const auto started = s.run("logcommands start /app/log.txt");
  REQUIRE(started.ok());
  CHECK(UiSession::printed(started) == R"(Logging commands to file "/app/log.txt")");
  s.run("box values 0 0 10 10");
  // "Logged quoting".
  s.run(R"(getcell "my net" n)");
  s.run("setpoint 1 1");
  s.run("*bypass box values 1 1 2 2");
  s.run("wire show");
  s.run("wire leg");
  CHECK(UiSession::first_error(s.run("logcommands start /app/other.txt")) == "UI-LOG-001: There is already a log file open!");
  CHECK(UiSession::printed(s.run("logcommands stop")) == "Ending command logging to file.");
  s.run("box");
  CHECK(without_date(read(s, "/app/log.txt")) ==
        "# Magic command log file\n"
        "# Using technology: " + s.editor.technology->name() + "\n"
        "box values 0 0 10 10\n"
        "getcell \"my net\" n\n"
        "wire leg\n");
  CHECK(read(s, "/app/log.txt").find("\n# Date: ") != std::string::npos);
  CHECK(ui::log_line(cmd::Invocation{"magic::logcommands", {}, cmd::Origin::cli, {}}).empty());
}

TEST_CASE("a command's logged form replaces what was typed in the log and for observers",
          "[cap:ui-command-dispatch][req:Command logging]") {
  // Magic's TxRebuildCommand: a handler that read the pointer logs the
  // explicit form (`box values ...`, `paint pick ...`, `wire leg to ...`).
  UiSession s;
  REQUIRE_FALSE(s.registry
                    .add(cmd::CommandSpec{"pointed", "pointed", "test command logged as `pointed at 3i 4i`",
                                          [](cmd::CommandContext&, const cmd::Invocation&) {
                                            cmd::CommandResult r = cmd::CommandResult::success();
                                            r.logged_as = cmd::Invocation{"pointed", {"at", "3i", "4i"},
                                                                          cmd::Origin::script, std::nullopt};
                                            return r;
                                          }})
                    .has_value());
  struct Seen final : cmd::CommandObserver {
    std::vector<cmd::Invocation> seen;
    void on_command(const cmd::Invocation& inv, const cmd::CommandResult&) override { seen.push_back(inv); }
  } observer;
  s.dispatcher->add_observer(observer);
  REQUIRE(s.run("logcommands start /app/logged.txt").ok());
  REQUIRE(s.run("pointed", cmd::Origin::key).ok());
  REQUIRE(s.run("logcommands stop").ok());
  CHECK(without_date(read(s, "/app/logged.txt")).ends_with("pointed at 3i 4i\n"));
  REQUIRE(observer.seen.size() >= 2);
  const cmd::Invocation& logged = observer.seen[1];
  CHECK(logged.args == std::vector<std::string>{"at", "3i", "4i"});
  CHECK(logged.origin == cmd::Origin::key);  // the dispatcher keeps the typed origin
}

TEST_CASE("logcommands update mode and suspension", "[cap:ui-command-dispatch][req:Command logging][golden:UI-G-017]") {
  UiSession s;
  CHECK(UiSession::printed(s.run("logcommands /app/u.txt update")) ==
        "Logging commands to file \"/app/u.txt\"\nForcing display refresh after logged commands.");
  s.run("box values 0 0 1 1");
  s.run("logcommands suspend");
  s.run("box values 0 0 2 2");
  s.run("logcommands resume");
  CHECK(UiSession::printed(s.run("logcommands update")) == "No display refresh after logged commands.");
  s.run("box values 0 0 3 3");
  s.run("logcommands");  // no argument stops logging
  CHECK(without_date(read(s, "/app/u.txt")).ends_with(
      "box values 0 0 1 1\nupdatedisplay\nbox values 0 0 3 3\n"));
  CHECK(UiSession::first_error(s.run("logcommands update")) ==
        "UI-LOG-001: There is no log file to set an update flag on.");
  CHECK(UiSession::first_error(s.run("logcommands a b c")).starts_with("UI-USAGE-001: Usage: logcommands"));
  CHECK(UiSession::first_error(s.run("logcommands /app/u.txt stop")).starts_with("UI-USAGE-001"));
}

TEST_CASE("the log writes the window view and box when a window exists",
          "[cap:ui-command-dispatch][req:Command logging]") {
  UiSession s;
  auto* def = s.editor.cells.create("top", s.editor.technology->plane_count()).value();
  auto w = s.editor.windows.open(def, hefesto::view::ScreenRect{0, 0, 99, 99});
  REQUIRE(w.ok());
  s.editor.edit_cell = def;  // Regression (UI-G-016): the title is the caption of the edit state
  w.value()->move_to(hefesto::db::Rect{0, 0, 100, 100});
  s.editor.box = hefesto::db::Rect{1, 2, 3, 4};
  s.run("logcommands start /app/w.txt");
  const std::string log = read(s, "/app/w.txt");
  CHECK(log.find("# Title: top EDITING top\n") != std::string::npos);
  CHECK(log.find("\nview ") != std::string::npos);
  CHECK(log.find("\nbox values 1i 2i 3i 4i\n") != std::string::npos);
}

TEST_CASE("bypassed commands are not logged and do not count",
          "[cap:ui-wizard-and-debug][req:Window-client wizard commands]") {
  UiSession s;
  s.run("logcommands start /app/b.txt");
  const auto before = s.editor.command_count;
  // "Bypass not logged".
  s.run("*bypass box values 0 0 4 4", cmd::Origin::gui);
  CHECK(s.editor.command_count == before);
  CHECK(s.calls_of("box") == std::vector<std::string>{"box values 0 0 4 4"});
  s.run("box values 0 0 5 5", cmd::Origin::gui);
  CHECK(s.editor.command_count == before + 1);
  CHECK(without_date(read(s, "/app/b.txt")).ends_with("# Using technology: " + s.editor.technology->name() +
                                                     "\nbox values 0 0 5 5\n"));
}

// --- Command counter and post-command update ------------------------------------------------

TEST_CASE("every dispatched command runs the post-command update",
          "[cap:ui-command-dispatch][req:Command counter and post-command update]") {
  UiSession s;
  s.run("box values 0 0 10 10; getcell a; *bypass getcell b");
  CHECK(s.editor.command_count == 2);  // *bypass is not counted
  // Client update handlers run in order (step 3).
  int updates = 0;
  ui::state(s.editor).update_handlers.push_back([&](hefesto::editor::EditorState&) { ++updates; });
  ui::run_client_updates(s.editor);
  CHECK(updates == 1);
  // An interrupt during a command adds [Interrupted].
  ui::request_interrupt(s.editor);
  const auto r = s.run("getcell c");
  REQUIRE_FALSE(r.diagnostics.empty());
  CHECK(r.diagnostics.back().code == ui::codes::kInterrupted);
  CHECK(r.diagnostics.back().text == "[Interrupted]");
  CHECK_FALSE(ui::state(s.editor).interrupt_pending.load());
}

// --- sleep, echo and send -------------------------------------------------------------------

TEST_CASE("sleep pauses the full count of seconds", "[cap:ui-command-dispatch][req:sleep, echo and send]") {
  UiSession s;
  std::vector<std::chrono::milliseconds> waits;
  ui::state(s.editor).sleeper = [&](std::chrono::milliseconds d) {
    waits.push_back(d);
    return true;
  };
  // "sleep pauses the full count".
  s.run("sleep 3");
  s.run("sleep 0");
  s.run("sleep -2");
  s.run("sleep x");
  REQUIRE(waits.size() == 1);
  CHECK(waits[0] == std::chrono::seconds{3});
  CHECK(UiSession::first_error(s.run("sleep")) == "UI-USAGE-001: Usage: sleep seconds");
  CHECK(s.run("echo a b").value.to_string() == "a b");
}

TEST_CASE("sleep of one second pauses one second", "[cap:ui-command-dispatch][req:sleep, echo and send]") {
  UiSession s;
  const auto start = std::chrono::steady_clock::now();
  REQUIRE(s.run("sleep 1").ok());
  const auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(elapsed >= std::chrono::milliseconds{950});
  CHECK(elapsed < std::chrono::seconds{3});
  // An interrupt ends the pause early.
  ui::request_interrupt(s.editor);
  const auto again = std::chrono::steady_clock::now();
  s.run("sleep 5");
  CHECK(std::chrono::steady_clock::now() - again < std::chrono::seconds{2});
}

TEST_CASE("send runs a command in another client's table", "[cap:ui-command-dispatch][req:sleep, echo and send]") {
  UiSession s;
  REQUIRE_FALSE(s.registry
                    .add({"color", "color ?n?", "fake color",
                          [](cmd::CommandContext&, const cmd::Invocation& inv) {
                            return cmd::CommandResult::success("color " + (inv.args.empty() ? "" : inv.args[0]));
                          }},
                         "color")
                    .has_value());
  CHECK(s.run("send col color 3").value.to_string() == "color 3");
  CHECK_FALSE(s.run("color 3").ok());  // not in the layout or global tables
  CHECK(UiSession::first_error(s.run("send *window help")).starts_with("UI-CLIENT-001:"));
  CHECK(UiSession::first_error(s.run("send wind3d zoom")).starts_with("UI-CLIENT-002:"));
  const auto usage = s.run("send");
  CHECK(UiSession::first_error(usage).starts_with("UI-USAGE-001: Usage: send type command\nValid window types are:"));
  // help sent to a client lists that client's table.
  CHECK(UiSession::printed(s.run("send color help")).find("\nColor Commands\n---------------\ncolor ?n?") !=
        std::string::npos);
}

// --- Command history --------------------------------------------------------------------------

TEST_CASE("history lists entries and expands Readline references",
          "[cap:ui-command-dispatch][req:Command history]") {
  UiSession s;
  auto& history = ui::state(s.editor).history;
  history.add("box");
  history.add("paint m1");
  history.add("");
  // "Reverse listing".
  CHECK(UiSession::printed(s.run("history r 2")) == "\t   2 paint m1\n\t   1 box");
  CHECK(UiSession::printed(s.run("history n")) == "box\npaint m1");
  CHECK(UiSession::first_error(s.run("history 0")) == "UI-USAGE-001: Bad arg to history: '0'");
  CHECK(UiSession::first_error(s.run("history help")).starts_with("UI-USAGE-001: Usage: history [n] [r] [<num>]"));
  history.add("label A12 n");
  const auto expand = [&](std::string_view line) {
    const auto e = history.expand(line);
    REQUIRE(e.ok());
    return e.value().line;
  };
  CHECK(expand("!!") == "label A12 n");
  CHECK(expand("!1") == "box");
  CHECK(expand("!-2") == "paint m1");
  CHECK(expand("!pa") == "paint m1");
  CHECK(expand("!?A12?") == "label A12 n");
  CHECK(expand("echo !$") == "echo n");
  CHECK(expand("echo !^") == "echo A12");
  CHECK(expand("echo !*") == "echo A12 n");
  CHECK(expand("echo !2:1") == "echo m1");
  CHECK(expand("echo !!:0-1") == "echo label A12");
  CHECK(expand("^A12^B7") == "label B7 n");
  CHECK(expand("!!:s/n/s/") == "label A12 s");
  CHECK(expand("!!:gs/l/L/") == "LabeL A12 n");
  CHECK(expand("a ! b") == "a ! b");
  CHECK(expand("'!!'") == "'!!'");
  CHECK(expand("x \\!!") == "x \\!!");
  CHECK(history.expand("!!:p").value().print_only);
  CHECK(expand("!# x") == " x");
  const auto missing = history.expand("!zz");
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().text == "!zz: event not found");
  CHECK(missing.error().code == ui::codes::kHistory);
  CHECK_FALSE(history.expand("!!:9").ok());
  CHECK_FALSE(history.expand("!!:s/zz/y/").ok());
  CHECK(expand("echo !!:t") == "echo label A12 n");
}

// --- Script files and source -----------------------------------------------------------------

TEST_CASE("command files follow Tcl line rules", "[cap:ui-command-dispatch][req:Script files and source]") {
  // "Continuation and comments".
  const auto lines = ui::command_file_lines("# setup\nbox 0 0 \\\n    10 10\n\n  # note\n:getcell a\n::magic::zoom 2\n");
  REQUIRE(lines.size() == 3);
  CHECK(ui::split_command_line(lines[0]).commands ==
        std::vector<std::vector<std::string>>{{"box", "0", "0", "10", "10"}});
  CHECK(lines[1] == "getcell a");
  CHECK(lines[2] == "::magic::zoom 2");
  // `source` runs a file through the script reader, nested (not logged).
  UiSession s;
  REQUIRE(s.files().write_atomic("/app/script.tcl", "# setup\nbox values 0 0 \\\n    10 10\nsetpoint 1 2\ngetcell x\n")
              .ok());
  s.run("logcommands start /app/src.txt");
  REQUIRE(s.run("source /app/script.tcl").ok());
  CHECK(s.calls_of("box") == std::vector<std::string>{"box values 0 0 10 10"});
  CHECK(s.calls_of("getcell") == std::vector<std::string>{"getcell x"});
  CHECK(without_date(read(s, "/app/src.txt")).ends_with("source /app/script.tcl\n"));
  // The point forced inside the script survives its later commands.
  CHECK(s.editor.command_point.has_value());
  CHECK(UiSession::first_error(s.run("source /app/none.tcl")) == "UI-SOURCE-001: Couldn't read from /app/none.tcl.");
  CHECK(UiSession::first_error(s.run("source")) == "UI-USAGE-001: Usage: source filename");
}

// --- Command entry on every platform ----------------------------------------------------------

TEST_CASE("completion of command, cell and layer names",
          "[cap:hefesto-input-and-touch][req:Command entry on every platform]") {
  UiSession s;
  const auto commands = ui::complete("pu", ui::CompletionContext{ui::CompletionKind::command, &s.registry, nullptr});
  REQUIRE(commands.ok());
  CHECK(commands.value() == std::vector<std::string>{"pushbox", "pushbutton", "pushstack"});
  const auto wizard = ui::complete("*b", ui::CompletionContext{ui::CompletionKind::command, &s.registry, nullptr});
  CHECK(wizard.value() == std::vector<std::string>{"*bypass"});
  CHECK(ui::complete("", ui::CompletionContext{ui::CompletionKind::command, &s.registry, nullptr})
            .value()
            .front() != "*bypass");
  (void)s.editor.cells.create("inv_1", s.editor.technology->plane_count());
  (void)s.editor.cells.create("inv_2", s.editor.technology->plane_count());
  (void)s.editor.cells.create("nand", s.editor.technology->plane_count());
  const auto cells = ui::complete("inv", ui::CompletionContext{ui::CompletionKind::cell, nullptr, &s.editor});
  CHECK(cells.value() == std::vector<std::string>{"inv_1", "inv_2"});
  const auto layers = ui::complete("", ui::CompletionContext{ui::CompletionKind::layer, nullptr, &s.editor});
  REQUIRE(layers.ok());
  CHECK(std::is_sorted(layers.value().begin(), layers.value().end()));
  CHECK_FALSE(ui::complete("x", ui::CompletionContext{ui::CompletionKind::layer, nullptr, nullptr}).ok());
  CHECK_FALSE(ui::complete("x", ui::CompletionContext{ui::CompletionKind::command, nullptr, nullptr}).ok());
}
