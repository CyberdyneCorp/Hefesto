// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "hefesto/shell/command_bar_model.hpp"

using namespace hefesto::shell;

namespace {

class RecordingSink final : public CommandSink {
 public:
  CommandOutcome submit(std::string_view line) override {
    lines.emplace_back(line);
    if (line == "boom") {
      return CommandOutcome{OutcomeStatus::failed, "boom failed", {"error: TEST-SINK-001: boom"}, {}, {}};
    }
    return CommandOutcome{OutcomeStatus::ok, {}, {}, {}, {}};
  }
  std::vector<std::string> lines;
};

}  // namespace

TEST_CASE("command bar trims and forwards one line to the command API",
          "[cap:hefesto-architecture][req:Single command API]") {
  RecordingSink sink;
  CommandBarModel bar(sink);
  CHECK(bar.submit("  paint m1 \n").ok());
  CHECK(bar.submit("   ").ok());  // blank: not sent
  const CommandOutcome failed = bar.submit("boom");
  CHECK_FALSE(failed.ok());
  CHECK(failed.diagnostics.size() == 1);
  CHECK(sink.lines == std::vector<std::string>{"paint m1", "boom"});
  CHECK(bar.status_text() == "boom failed");
}

TEST_CASE("command bar history walks back and forward", "[cap:hefesto-architecture][req:Single command API]") {
  RecordingSink sink;
  CommandBarModel bar(sink, 3);
  for (const char* line : {"a", "b", "b", "c", "d"}) {
    bar.submit(line);
  }
  CHECK(bar.history() == std::vector<std::string>{"b", "c", "d"});  // deduped, limited to 3
  CHECK(bar.previous() == "d");
  CHECK(bar.previous() == "c");
  CHECK(bar.previous() == "b");
  CHECK(bar.previous() == "b");  // stays on the oldest
  CHECK(bar.next() == "c");
  CHECK(bar.next() == "d");
  CHECK(bar.next().empty());  // past the newest: empty line
}

TEST_CASE("trim removes ASCII blanks only at both ends", "[cap:hefesto-architecture][req:Single command API]") {
  CHECK(trim("\t a b \r\n") == "a b");
  CHECK(trim("").empty());
  CHECK(trim(" \t ").empty());
}

// Regression: multi-line results (e.g. `help`) were only written to stdout and
// never shown in the window; the Console panel now lists every line.
TEST_CASE("console lines echo the command and every result line",
          "[cap:hefesto-diagnostics][req:Diagnostic presentation and logging]") {
  CHECK(console_lines("help", "box\nhelp\nquit") == std::vector<std::string>{"> help", "box", "help", "quit"});
  CHECK(console_lines("version", "0.0.1") == std::vector<std::string>{"> version", "0.0.1"});
  CHECK(console_lines("zoom 2", "") == std::vector<std::string>{"> zoom 2"});
  CHECK(console_lines("help", "a\n\nb\n") == std::vector<std::string>{"> help", "a", "", "b"});
}

TEST_CASE("the console shows the diagnostics of a line that raised errors",
          "[cap:hefesto-diagnostics][req:Diagnostic presentation and logging]") {
  // Regression: a line that succeeded with an error diagnostic printed only "ok".
  CommandOutcome raised{OutcomeStatus::ok, "", {"error: CELL-PATH-002: Cell x couldn't be read", "info: Creating new cell"}, {}, {}};
  CHECK(console_result(raised, "ok") == "error: CELL-PATH-002: Cell x couldn't be read\ninfo: Creating new cell");
  CommandOutcome failed{OutcomeStatus::failed, "Unrecognized layer: q", {"error: DB-PAINT-001: Unrecognized layer: q"}, {}, {}};
  CHECK(console_result(failed, "Unrecognized layer: q") == "error: DB-PAINT-001: Unrecognized layer: q");
  CommandOutcome valued{OutcomeStatus::ok, "3", {"warning: X-1: careful"}, {}, {}};
  CHECK(console_result(valued, "3") == "warning: X-1: careful\n3");
  CHECK(console_result(CommandOutcome{}, "ok") == "ok");
}
