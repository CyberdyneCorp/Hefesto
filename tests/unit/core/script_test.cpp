// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include "test_session.hpp"

using hefesto::cmd::parse_script;
using hefesto::cmd::quote_word;
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

TEST_CASE("quote_word round-trips through the reader", CMD_API_TAGS) {
  for (const std::string& word : std::vector<std::string>{"plain", "", "a b", "x;y", "#hash", "q\"uote", "back\\slash", "{brace", "nl\nt\tr\r",
                                 std::string{"nul\0byte", 8}}) {
    INFO(word);
    const auto parsed = parse_script("cmd " + quote_word(word));
    REQUIRE_FALSE(parsed.error.has_value());
    REQUIRE(parsed.commands.size() == 1U);
    CHECK(parsed.commands[0].words == Words{"cmd", word});
  }
}

TEST_CASE("default is Magic's convention: failures do not stop the script", STRICT_TAGS) {
  hefesto::test::Session s;  // new project: script-strict OFF
  CHECK(s.exec("hefesto::strict").value == hefesto::cmd::Value{0});
  const auto run = s.script("fail\ncount");
  CHECK_FALSE(run.aborted);
  CHECK(run.failures == 1U);
  CHECK(run.commands_run == 2U);
  CHECK(s.commands().count == 1);
  CHECK_FALSE(run.ok());  // the status is still visible to the CLI
}

TEST_CASE("strict mode aborts on the first failure with its code and text", STRICT_TAGS) {
  hefesto::test::Session s;
  const auto run = s.script("hefesto::strict on; fail; count");
  CHECK(run.aborted);
  REQUIRE(run.error.has_value());
  CHECK(run.error->code == codes::kCmdUsage);
  CHECK(run.error->text == "it failed");
  CHECK(s.commands().count == 0);
  CHECK(s.exec("hefesto::strict").value == hefesto::cmd::Value{1});
}

TEST_CASE("per-interpreter override", STRICT_TAGS) {
  const hefesto::settings::ProjectSettings project;  // script-strict OFF
  hefesto::test::Session plugin(project);
  hefesto::test::Session main_interp(project);
  REQUIRE(plugin.script("hefesto::strict on").ok());
  CHECK(plugin.script("fail; count").aborted);
  CHECK_FALSE(main_interp.script("fail; count").aborted);
  CHECK(main_interp.commands().count == 1);
}

TEST_CASE("project setting with a local opt-out", STRICT_TAGS) {
  hefesto::settings::ProjectSettings project;
  project.script_strict = true;
  hefesto::test::Session s(project);
  CHECK(s.exec("hefesto::strict").value == hefesto::cmd::Value{1});
  const auto run = s.script("hefesto::strict off; fail; count");
  CHECK_FALSE(run.aborted);
  CHECK(s.commands().count == 1);
}

TEST_CASE("strict mode is independent of magic-compat", STRICT_TAGS) {
  hefesto::settings::ProjectSettings project;
  project.compat.magic_compat = false;
  hefesto::test::Session s(project);
  CHECK_FALSE(s.script("fail; count").aborted);
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
  const auto run = s.script("hefesto::strict maybe; count", "x.tcl");
  CHECK(run.aborted);
  REQUIRE(run.error);
  CHECK(run.error->code == codes::kCmdUsage);
  CHECK(run.error->text == "usage: hefesto::strict ?on|off?");
  CHECK(s.commands().count == 0);
  CHECK(s.exec("hefesto::strict").value == hefesto::cmd::Value{0});  // state unchanged
}

TEST_CASE("dispatch failures abort in both modes", RESULT_TAGS) {
  hefesto::test::Session s;
  const auto run = s.script("nosuch; count", "x.tcl");
  CHECK(run.aborted);
  CHECK(run.error->code == codes::kCmdDispatch);
  CHECK(std::get<hefesto::diag::SourceLocation>(run.error->location).file == "x.tcl");
  CHECK(s.commands().count == 0);
}

TEST_CASE("quit stops the script; parse errors run nothing", RESULT_TAGS) {
  hefesto::test::Session s;
  const auto quit = s.script("count; quit; count");
  CHECK(quit.quit);
  CHECK(quit.ok());
  CHECK(s.commands().count == 1);

  hefesto::test::Session t;
  const auto bad = t.script("count; echo \"unterminated");
  CHECK(bad.aborted);
  CHECK(bad.commands_run == 0U);
  CHECK(t.commands().count == 0);
}
