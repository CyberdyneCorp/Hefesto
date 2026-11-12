// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include "hefesto/cmd/script.hpp"
#include "test_session.hpp"

using hefesto::cmd::parse_script;
using Words = std::vector<std::string>;
namespace codes = hefesto::diag::codes;

#define CMD_API_TAGS "[cap:hefesto-architecture][req:Single command API]"
#define STRICT_TAGS "[cap:hefesto-diagnostics][req:Script strict mode switch]"
#define RESULT_TAGS "[cap:hefesto-diagnostics][req:Command results and errors]"

namespace {
std::vector<Words> words_of(std::string_view text) {
  const auto parsed = parse_script(text);
  REQUIRE_FALSE(parsed.error.has_value());
  std::vector<Words> out;
  for (const auto& c : parsed.commands) {
    out.push_back(c.words);
  }
  return out;
}
}  // namespace

TEST_CASE("commands split on semicolons and newlines", CMD_API_TAGS) {
  CHECK(words_of("load inv; extract all;ext2spice\nquit") ==
        std::vector<Words>{{"load", "inv"}, {"extract", "all"}, {"ext2spice"}, {"quit"}});
  CHECK(words_of("  ;; \n\t ").empty());
}

TEST_CASE("quotes, braces, escapes and comments", CMD_API_TAGS) {
  CHECK(words_of(R"(echo "a b;c" {x {y} z} "q\"\\\n")") ==
        std::vector<Words>{{"echo", "a b;c", "x {y} z", "q\"\\\n"}});
  CHECK(words_of("# comment ; still comment\necho hi # not a comment") ==
        std::vector<Words>{{"echo", "hi", "#", "not", "a", "comment"}});
  CHECK(words_of("echo a\\ b c\\;d") == std::vector<Words>{{"echo", "a b", "c;d"}});
  CHECK(words_of("echo a \\\n  b") == std::vector<Words>{{"echo", "a", "b"}});
  CHECK(words_of(R"(echo {a\}b})") == std::vector<Words>{{"echo", R"(a\}b)"}});
  CHECK(words_of(R"(echo "" {})") == std::vector<Words>{{"echo", "", ""}});
}

TEST_CASE("line numbers follow the source", CMD_API_TAGS) {
  const auto parsed = parse_script("a\n\nb {x\ny}\nc");
  REQUIRE(parsed.commands.size() == 3U);
  CHECK(parsed.commands[0].line == 1U);
  CHECK(parsed.commands[1].line == 3U);
  CHECK(parsed.commands[2].line == 5U);
}

TEST_CASE("malformed scripts report a located error", RESULT_TAGS) {
  const auto quote = parse_script("echo ok\necho \"open", "s.tcl");
  REQUIRE(quote.error.has_value());
  CHECK(quote.error->code == codes::kCmdParseQuote);
  CHECK(std::get<hefesto::diag::SourceLocation>(quote.error->location) == hefesto::diag::SourceLocation{"s.tcl", 2});
  CHECK(parse_script("echo {a {b}").error->code == codes::kCmdParseBrace);
  CHECK(parse_script("echo {a}b").error->code == codes::kCmdParseExtra);
  CHECK(parse_script("echo \"a\"b").error->code == codes::kCmdParseExtra);
}

// ---- Scripts are Tcl in the project's interpreter (m4-scripting 13.2) ----

namespace {
std::string text_of(const hefesto::cmd::CommandResult& r) {
  return r.diagnostics.empty() ? std::string{} : r.diagnostics.front().text;
}
}  // namespace

TEST_CASE("default is Magic's convention: failures do not stop the script", STRICT_TAGS) {
  hefesto::test::Session s;  // new project: script-strict OFF
  CHECK(s.exec("hefesto::strict").value == hefesto::cmd::Value{0});
  const auto run = s.script("fail\ncount");
  CHECK(run.ok());
  CHECK(s.commands().count == 1);
  CHECK(s.script("catch {fail}").value.to_string() == "0");
}

TEST_CASE("strict mode aborts on the first failure with its code and text", STRICT_TAGS) {
  hefesto::test::Session s;
  const auto run = s.script("hefesto::strict on; fail; count");
  REQUIRE_FALSE(run.ok());
  CHECK(text_of(run) == "it failed");
  CHECK(s.commands().count == 0);
  CHECK(s.exec("hefesto::strict").value == hefesto::cmd::Value{1});
  CHECK(s.script("catch {fail} message; set message").value.to_string() == "it failed");
}

TEST_CASE("per-interpreter override", STRICT_TAGS) {
  const hefesto::settings::ProjectSettings project;  // script-strict OFF
  hefesto::test::Session plugin(project);
  hefesto::test::Session main_interp(project);
  REQUIRE(plugin.script("hefesto::strict on").ok());
  CHECK(plugin.script("catch {fail}").value.to_string() == "1");
  CHECK(main_interp.script("catch {fail}").value.to_string() == "0");
}

TEST_CASE("project setting with a local opt-out", STRICT_TAGS) {
  hefesto::settings::ProjectSettings project;
  project.script_strict = true;
  hefesto::test::Session s(project);
  CHECK(s.script("catch {fail}").value.to_string() == "1");
  const auto run = s.script("hefesto::strict off; fail; count");
  CHECK(run.ok());
  CHECK(s.commands().count == 1);
}

TEST_CASE("strict mode is independent of magic-compat", STRICT_TAGS) {
  hefesto::settings::ProjectSettings project;
  project.compat.magic_compat = false;
  hefesto::test::Session s(project);
  CHECK(s.script("fail; count").ok());
  CHECK(s.project().compat.magic_compat == false);
  REQUIRE(s.script("hefesto::strict on").ok());
  CHECK(s.project().compat.magic_compat == false);
}

TEST_CASE("hefesto::strict rejects other arguments", STRICT_TAGS) {
  hefesto::test::Session s;
  const auto r = s.exec("hefesto::strict", {"maybe"});
  CHECK_FALSE(r.ok());
  CHECK(r.diagnostics.at(0).text == "usage: hefesto::strict ?on|off?");
  CHECK(r.raises);
}

// Regression: the usage error used to be an ordinary failure, so a default-mode
// script carried on after it.
TEST_CASE("hefesto::strict with a bad argument aborts the script in default mode", STRICT_TAGS) {
  hefesto::test::Session s;  // script-strict OFF
  const auto run = s.script("hefesto::strict maybe; count");
  REQUIRE_FALSE(run.ok());
  CHECK(text_of(run) == "usage: hefesto::strict ?on|off?");
  CHECK(s.commands().count == 0);
  CHECK(s.exec("hefesto::strict").value == hefesto::cmd::Value{0});  // state unchanged
}

TEST_CASE("dispatch failures abort in both modes", RESULT_TAGS) {
  hefesto::test::Session s;
  const auto run = s.script("count\nnosuch; count");
  REQUIRE_FALSE(run.ok());
  CHECK(text_of(run) == "invalid command name \"nosuch\"");
  CHECK(std::get<hefesto::diag::SourceLocation>(run.diagnostics.front().location).line == 2U);
  CHECK(s.commands().count == 1);
}

TEST_CASE("quit stops the script; a Tcl parse error stops it where it is", RESULT_TAGS) {
  hefesto::test::Session s;
  (void)s.script("count; quit; count");
  CHECK(s.session().quit_requested);
  CHECK(s.commands().count == 1);

  hefesto::test::Session t;
  const auto bad = t.script("count; echo \"unterminated");
  REQUIRE_FALSE(bad.ok());
  CHECK(text_of(bad) == "missing \"");
  CHECK(t.commands().count == 1);  // Tcl parses each command just before it runs
}
