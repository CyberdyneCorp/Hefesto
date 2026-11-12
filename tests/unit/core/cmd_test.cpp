// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/cmd/value.hpp"
#include "hefesto/version.hpp"
#include "test_session.hpp"

using hefesto::cmd::CommandResult;
using hefesto::cmd::Invocation;
using hefesto::cmd::Origin;
using hefesto::cmd::Value;
namespace codes = hefesto::diag::codes;

#define CMD_API_TAGS "[cap:hefesto-architecture][req:Single command API]"
#define RESULT_TAGS "[cap:hefesto-diagnostics][req:Command results and errors]"

TEST_CASE("every command returns status, value and diagnostics", RESULT_TAGS) {
  hefesto::test::Session s;
  const CommandResult ok = s.exec("echo", {"a", "b"});
  CHECK(ok.ok());
  CHECK(ok.value == Value{"a b"});
  CHECK(ok.diagnostics.empty());

  const CommandResult failed = s.exec("fail");
  CHECK_FALSE(failed.ok());
  CHECK(failed.value.is_none());
  REQUIRE(failed.diagnostics.size() == 1U);
  CHECK(failed.diagnostics[0].code == codes::kCmdUsage);
}

TEST_CASE("unknown command is a dispatch failure", RESULT_TAGS) {
  hefesto::test::Session s;
  Invocation inv{"nonexistent", {}, Origin::cli, hefesto::diag::SourceLocation{"run.tcl", 3}};
  const CommandResult r = s.dispatcher().execute(inv);
  CHECK(r.status == hefesto::cmd::Status::failed);
  REQUIRE(r.diagnostics.size() == 1U);
  CHECK(r.diagnostics[0].code == "CMD-DISPATCH-001");
  CHECK(r.diagnostics[0].severity == hefesto::diag::Severity::error);
  CHECK(std::get<hefesto::diag::SourceLocation>(r.diagnostics[0].location).line == 3U);
}

TEST_CASE("diagnostics emitted through the context are attached to the result", RESULT_TAGS) {
  hefesto::test::Session s;
  (void)s.registry().add({"warn", "warn", "", [](hefesto::cmd::CommandContext& ctx, const Invocation&) {
                            ctx.emit(hefesto::diag::make(codes::kCliLog, "first"));
                            CommandResult r = CommandResult::success(1);
                            r.diagnostics.push_back(hefesto::diag::make(codes::kCliLog, "second"));
                            return r;
                          }});
  const CommandResult r = s.exec("warn");
  REQUIRE(r.diagnostics.size() == 2U);
  CHECK(r.diagnostics[0].text == "first");
  CHECK(r.diagnostics[1].text == "second");
  // The logging service records every diagnostic.
  const auto lines = s.logger().lines();
  CHECK(std::count_if(lines.begin(), lines.end(),
                      [](const std::string& l) { return l.find("CLI-LOG-001") != std::string::npos; }) == 2);
}

TEST_CASE("a removed sink no longer receives diagnostics", RESULT_TAGS) {
  hefesto::test::Session s;
  hefesto::diag::CollectingSink sink;
  s.dispatcher().add_sink(sink);
  (void)s.exec("fail");
  s.dispatcher().remove_sink(sink);
  (void)s.exec("fail");
  CHECK(sink.take().size() == 1U);
}

TEST_CASE("a throwing handler fails the command instead of crashing", RESULT_TAGS) {
  hefesto::test::Session s;
  (void)s.registry().add({"boom", "boom", "", [](hefesto::cmd::CommandContext&, const Invocation&) -> CommandResult {
                            throw std::runtime_error("kaput");
                          }});
  const CommandResult r = s.exec("boom");
  CHECK_FALSE(r.ok());
  CHECK(r.diagnostics.at(0).code == codes::kCmdInternal);
}

TEST_CASE("duplicate registration is rejected and keeps the first command", CMD_API_TAGS) {
  hefesto::test::Session s;
  const auto dup = s.registry().add({"echo", "echo", "", [](hefesto::cmd::CommandContext&, const Invocation&) {
                                       return CommandResult::success("other");
                                     }});
  REQUIRE(dup.has_value());
  CHECK(dup->code == codes::kCmdRegistryDuplicate);
  CHECK(s.exec("echo", {"x"}).value == Value{"x"});
  CHECK_FALSE(hefesto::cmd::register_builtins(s.registry()));
}

TEST_CASE("every front end goes through the dispatcher and is logged with its origin", CMD_API_TAGS) {
  hefesto::test::Session s;
  // A touch gesture issues the same command a script would.
  (void)s.dispatcher().execute({"echo", {"box", "1 2"}, Origin::touch, std::nullopt});
  (void)s.dispatcher().execute({"echo", {"box", "1 2"}, Origin::script, std::nullopt});
  const auto history = s.log().history();
  REQUIRE(history.size() == 2U);
  CHECK(history[0] == history[1]);
  CHECK(history[0] == "echo box {1 2}");  // a Tcl command line
  const auto lines = s.logger().lines();
  CHECK(lines.at(0) == "info cmd[touch] echo box {1 2} -> ok");
  CHECK(lines.at(1) == "info cmd[script] echo box {1 2} -> ok");
}

TEST_CASE("internal commands are logged but kept out of the history", CMD_API_TAGS) {
  hefesto::test::Session s;
  (void)s.dispatcher().execute({"echo", {"tick"}, Origin::internal, std::nullopt});
  (void)s.exec("echo", {"user"});
  CHECK(s.log().history() == std::vector<std::string>{"echo user"});
  CHECK(s.logger().lines().at(0) == "info cmd[internal] echo tick -> ok");
}

namespace {

// Records each diagnostic text and whether the command had returned by then.
struct LiveRecorder final : hefesto::diag::DiagnosticSink {
  explicit LiveRecorder(const bool& returned) : returned_(returned) {}
  void emit(const hefesto::diag::Diagnostic& d) override { seen.emplace_back(d.text, returned_); }
  std::vector<std::pair<std::string, bool>> seen;

 private:
  const bool& returned_;
};

CommandResult emit_two_steps(hefesto::cmd::CommandContext& ctx, const Invocation& /*invocation*/) {
  ctx.emit(hefesto::diag::make(codes::kCmdUsage, "step 1"));
  ctx.emit(hefesto::diag::make(codes::kCmdUsage, "step 2"));
  CommandResult r = CommandResult::success();
  r.diagnostics.push_back(hefesto::diag::make(codes::kCmdUsage, "returned"));
  return r;
}

}  // namespace

TEST_CASE("live sinks see context diagnostics while the command runs", CMD_API_TAGS) {
  hefesto::test::Session s;
  REQUIRE_FALSE(s.registry().add({"progress", "progress", "Emits two steps.", emit_two_steps}));
  bool returned = false;
  LiveRecorder live{returned};
  LiveRecorder after{returned};
  s.dispatcher().add_live_sink(live);
  s.dispatcher().add_sink(after);
  const CommandResult r = s.exec("progress");
  returned = true;
  // Live: only the diagnostics emitted through the context, while it ran.
  CHECK(live.seen == std::vector<std::pair<std::string, bool>>{{"step 1", false}, {"step 2", false}});
  // The ordinary sinks and the result still get all three after it returns.
  REQUIRE(after.seen.size() == 3U);
  CHECK(after.seen.back().first == "returned");
  CHECK(r.diagnostics.size() == 3U);
  s.dispatcher().remove_live_sink(live);
  (void)s.exec("progress");
  CHECK(live.seen.size() == 2U);
}

namespace {

struct TextRecorder final : hefesto::diag::DiagnosticSink {
  void emit(const hefesto::diag::Diagnostic& d) override { seen.push_back(d.text); }
  std::vector<std::string> seen;
};

}  // namespace

// Regression (SCRIPT-PCELL-15, SCRIPT-PDK-15): a command that runs another
// one (magic::gencell evaluating a device's Tcl draw, which calls pushbox)
// printed its own earlier messages only when it returned, after the inner
// command's; Magic prints each message when it is made.
TEST_CASE("an outer command's messages are printed before those of a command it runs", CMD_API_TAGS) {
  hefesto::test::Session s;
  hefesto::cmd::Dispatcher& d = s.dispatcher();
  REQUIRE_FALSE(s.registry().add({"inner", "inner", "Emits one message.",
                                  [](hefesto::cmd::CommandContext& ctx, const Invocation& /*inv*/) {
                                    ctx.emit(hefesto::diag::make(codes::kCmdUsage, "inner"));
                                    return CommandResult::success();
                                  }}));
  REQUIRE_FALSE(s.registry().add({"outer", "outer", "Emits, runs inner, emits.",
                                  [&d](hefesto::cmd::CommandContext& ctx, const Invocation& /*inv*/) {
                                    ctx.emit(hefesto::diag::make(codes::kCmdUsage, "before"));
                                    (void)d.execute(Invocation{"inner", {}, Origin::script, std::nullopt});
                                    ctx.emit(hefesto::diag::make(codes::kCmdUsage, "after"));
                                    return CommandResult::success();
                                  }}));
  TextRecorder printed;
  d.add_sink(printed);
  const CommandResult r = s.exec("outer");
  CHECK(printed.seen == std::vector<std::string>{"before", "inner", "after"});
  // The result still carries every message the command emitted.
  CHECK(r.diagnostics.size() == 2U);
  d.remove_sink(printed);
}

TEST_CASE("the command log replays a session", CMD_API_TAGS) {
  hefesto::test::Session original;
  REQUIRE(original.script("setting set magic-compat off\nhefesto::strict on\nhefesto::debug::hashorder 8 \"+a;b\" {+c d} +\\{ {+#x} {+$y}\n").ok());
  hefesto::test::Session replay;
  const auto run = replay.script(original.log().history_script());
  INFO(original.log().history_script());
  CHECK(run.ok());
  CHECK(replay.project() == original.project());
  CHECK(replay.session().strict == original.session().strict);
  CHECK(replay.log().history() == original.log().history());
}

TEST_CASE("built-in commands", CMD_API_TAGS) {
  hefesto::test::Session s;
  CHECK(s.exec("version").value == Value{std::string{hefesto::version()}});
  CHECK(s.exec("version", {"x"}).diagnostics.at(0).code == codes::kCmdUsage);
  CHECK(s.exec("echo").value == Value{""});

  // `help` is the ui area's (ui-command-dispatch "help command"), not a built-in.
  CHECK(s.registry().find("help") == nullptr);

  CHECK(s.exec("setting").value.to_string() == "magic-compat on netgen-compat on script-strict off");
  CHECK(s.exec("setting", {"set", "netgen-compat", "off"}).ok());
  CHECK(s.exec("setting", {"get", "netgen-compat"}).value == Value{"off"});
  CHECK(s.exec("setting", {"get", "nope"}).diagnostics.at(0).code == codes::kSettingsName);
  CHECK(s.exec("setting", {"frob"}).diagnostics.at(0).code == codes::kCmdUsage);

  CHECK_FALSE(s.session().quit_requested);
  CHECK(s.exec("quit").ok());
  CHECK(s.session().quit_requested);
}

#define QUIT_TAGS "[cap:app-startup][req:quit command and exit status]"

// Regression: `quit` took no arguments, so Magic's scripted `quit -noprompt`
// and `quit 3` failed with a usage error.
TEST_CASE("quit takes an exit status and -noprompt as the last word", QUIT_TAGS "[regression]") {
  hefesto::test::Session s;
  CHECK(s.exec("quit", {"3", "-noprompt"}).ok());
  CHECK(s.session().quit_requested);
  CHECK(s.session().exit_status == 3);

  hefesto::test::Session plain;
  CHECK(plain.exec("quit", {"-noprompt"}).ok());
  CHECK(plain.session().quit_requested);
  CHECK(plain.session().exit_status == 0);
}

TEST_CASE("quit with a bad status or extra words reports and keeps running", QUIT_TAGS) {
  const auto texts = [](const CommandResult& r) {
    std::vector<std::string> out;
    for (const auto& d : r.diagnostics) {
      out.push_back(d.text);
    }
    return out;
  };
  hefesto::test::Session s;
  const auto word = s.exec("quit", {"abc"});
  CHECK_FALSE(word.ok());
  CHECK(texts(word) == std::vector<std::string>{"Invalid exit_status: abc", "Usage: quit [exit_status] [-noprompt]"});
  // Q-UI-32: Magic truncates to 8 bits; Hefesto rejects the status.
  CHECK(texts(s.exec("quit", {"256", "-noprompt"})) ==
        std::vector<std::string>{"Invalid exit_status: 256", "Usage: quit [exit_status] [-noprompt]"});
  CHECK(texts(s.exec("quit", {"-1"})).front() == "Invalid exit_status: -1");
  // -noprompt is recognized only as the last word.
  CHECK(texts(s.exec("quit", {"-noprompt", "1"})) ==
        std::vector<std::string>{"Usage: quit [exit_status] [-noprompt]"});
  CHECK(texts(s.exec("quit", {"1", "2"})) == std::vector<std::string>{"Usage: quit [exit_status] [-noprompt]"});
  CHECK_FALSE(s.session().quit_requested);
}

TEST_CASE("an exit check can veto quit unless -noprompt is given", QUIT_TAGS) {
  hefesto::test::Session s;
  int asked = 0;
  bool discard = false;
  s.session().exit_checks.emplace_back([&] {
    ++asked;
    return discard;  // "discard the modified cells?"
  });
  CHECK(s.exec("quit").ok());
  CHECK(asked == 1);
  CHECK_FALSE(s.session().quit_requested);  // declined: the application keeps running
  CHECK(s.exec("quit", {"3", "-noprompt"}).ok());
  CHECK(asked == 1);  // not asked
  CHECK(s.session().quit_requested);
  CHECK(s.session().exit_status == 3);

  hefesto::test::Session accepted;
  accepted.session().exit_checks.emplace_back([] { return true; });
  CHECK(accepted.exec("quit", {"4"}).ok());
  CHECK(accepted.session().quit_requested);
  CHECK(accepted.session().exit_status == 4);
}

TEST_CASE("values render as Tcl results", CMD_API_TAGS) {
  CHECK(Value{}.to_string().empty());
  CHECK(Value{42}.to_string() == "42");
  CHECK(Value{std::int64_t{-7}}.to_string() == "-7");
  CHECK(Value{0.5}.to_string() == "0.5");
  CHECK(Value{2.0}.to_string() == "2.0");
  CHECK(Value{Value::List{"a", "b c", "", Value::List{1, 2}, "x{"}}.to_string() == "a {b c} {} {1 2} x\\{");
}

// --- shared coordinate units (cmd-coordinates-and-units "Unit suffixes") ------

#define UNIT_TAGS "[cap:cmd-coordinates-and-units][req:Unit suffixes]"

namespace {

double coord(std::string_view text, const hefesto::cmd::CoordinateUnits& units,
             hefesto::cmd::Axis axis = hefesto::cmd::Axis::x, bool relative = false) {
  const auto v = hefesto::cmd::parse_coordinate(text, units, axis, relative);
  REQUIRE(v.ok());
  return v.value();
}

}  // namespace

TEST_CASE("microns with a 10 nm internal unit", UNIT_TAGS) {
  hefesto::cmd::CoordinateUnits u;
  u.cif_scale = std::array<std::int64_t, 2>{1, 1};  // 10*1/(1*k): 10 nm per internal unit
  CHECK(std::lround(coord("1.5um", u)) == 150);
  CHECK(std::lround(coord("0.005um", u)) == 1);  // 0.5 rounds away from zero
  CHECK(std::lround(coord("25nm", u)) == 3);
  CHECK(std::lround(coord("1microns", u)) == 100);
  CHECK(std::lround(coord("1u", u)) == 100);
  CHECK(std::lround(coord("100cu", u)) == 100);
}

TEST_CASE("lambda, internal and grid suffixes", UNIT_TAGS) {
  hefesto::cmd::CoordinateUnits u;
  u.internal_per = 2;  // 2 internal units = 1 lambda
  CHECK(coord("3l", u) == 6.0);
  CHECK(coord("3", u) == 6.0);  // the default unit is lambda
  CHECK(coord("3i", u) == 3.0);
  u.default_unit = hefesto::cmd::DefaultUnit::internal;
  CHECK(coord("3", u) == 3.0);
  CHECK_FALSE(hefesto::cmd::parse_coordinate("2g", u).ok());  // no window: an error (Q-CMDEDIT-4/5)
  u.grid = std::array<std::int64_t, 4>{5, 5, 15, 25};
  CHECK(coord("2g", u) == 25.0);
  CHECK(coord("2g", u, hefesto::cmd::Axis::y, true) == 40.0);
}

TEST_CASE("unknown units are reported and taken as internal units", UNIT_TAGS) {
  hefesto::cmd::CoordinateUnits u;
  hefesto::diag::CollectingSink sink;
  const auto km = hefesto::cmd::parse_coordinate("5km", u, hefesto::cmd::Axis::x, false, &sink);
  REQUIRE(km.ok());
  CHECK(km.value() == 5.0);
  const auto x = hefesto::cmd::parse_coordinate("5zz", u, hefesto::cmd::Axis::x, false, &sink);
  REQUIRE(x.ok());
  const auto notes = sink.take();
  REQUIRE(notes.size() == 2);
  CHECK(notes[0].text == "Unknown metric prefix \"km\"; assuming internal units");
  CHECK(notes[1].text == "Unknown coordinate type at \"zz\"; assuming internal units");
  CHECK_FALSE(hefesto::cmd::parse_coordinate("abc", u).ok());
}

