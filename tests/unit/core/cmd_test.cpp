// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

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
  CHECK(history[0] == "echo box \"1 2\"");
  const auto lines = s.logger().lines();
  CHECK(lines.at(0) == "info cmd[touch] echo box \"1 2\" -> ok");
  CHECK(lines.at(1) == "info cmd[script] echo box \"1 2\" -> ok");
}

TEST_CASE("the command log replays a session", CMD_API_TAGS) {
  hefesto::test::Session original;
  REQUIRE(original.script("setting set magic-compat off\nhefesto::strict on\necho \"a;b\" {c d}\n").ok());
  hefesto::test::Session replay;
  const auto run = replay.script(original.log().history_script());
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

  const auto help = s.exec("help").value.to_string();
  CHECK(help.find("hefesto::strict ?on|off?") != std::string::npos);
  CHECK(help.find("setting ?get name") != std::string::npos);
  CHECK(s.exec("help", {"quit"}).ok());
  CHECK_FALSE(s.exec("help", {"nosuch"}).ok());

  CHECK(s.exec("setting").value.to_string() == "magic-compat on netgen-compat on script-strict off");
  CHECK(s.exec("setting", {"set", "netgen-compat", "off"}).ok());
  CHECK(s.exec("setting", {"get", "netgen-compat"}).value == Value{"off"});
  CHECK(s.exec("setting", {"get", "nope"}).diagnostics.at(0).code == codes::kSettingsName);
  CHECK(s.exec("setting", {"frob"}).diagnostics.at(0).code == codes::kCmdUsage);

  CHECK_FALSE(s.session().quit_requested);
  CHECK(s.exec("quit").ok());
  CHECK(s.session().quit_requested);
}

TEST_CASE("values render as Tcl results", CMD_API_TAGS) {
  CHECK(Value{}.to_string().empty());
  CHECK(Value{42}.to_string() == "42");
  CHECK(Value{std::int64_t{-7}}.to_string() == "-7");
  CHECK(Value{0.5}.to_string() == "0.5");
  CHECK(Value{2.0}.to_string() == "2.0");
  CHECK(Value{Value::List{"a", "b c", "", Value::List{1, 2}, "x{"}}.to_string() == "a {b c} {} {1 2} x\\{");
}
