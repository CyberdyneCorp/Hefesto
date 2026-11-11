// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include "test_session.hpp"


#define CMD_API_TAGS "[cap:hefesto-architecture][req:Single command API]"
#define STRICT_TAGS "[cap:hefesto-diagnostics][req:Script strict mode switch]"
#define RESULT_TAGS "[cap:hefesto-diagnostics][req:Command results and errors]"

// The M0 word splitter (cmd::parse_script) is gone (m4-scripting tasks.md
// 13.2): every command line, script, key macro and key-binding file is Tcl.
// What it was tested for is Tcl's command syntax now.

TEST_CASE("commands split on semicolons and newlines", CMD_API_TAGS) {
  hefesto::test::Session s;
  REQUIRE(s.script("count; count;count\n\n  ;; \n\t count").ok());
  CHECK(s.commands().count == 4);
}

TEST_CASE("quotes, braces, escapes and comments", CMD_API_TAGS) {
  hefesto::test::Session s;
  // Each word of `words` as `set w` saw it, through `lindex` (no list formatting).
  const auto word = [&s](std::string_view words, int i) {
    return s.script("lindex [list " + std::string{words} + "] " + std::to_string(i)).value.to_string();
  };
  const std::string_view quoted = R"("a b;c" {x {y} z} "q\"\\")";
  CHECK(word(quoted, 0) == "a b;c");
  CHECK(word(quoted, 1) == "x {y} z");
  CHECK(word(quoted, 2) == R"(q"\)");
  CHECK(s.script("# comment ; still comment\nllength [list hi # a]").value.to_string() == "3");
  CHECK(word(R"(a\ b c\;d)", 0) == "a b");
  CHECK(word(R"(a\ b c\;d)", 1) == "c;d");
  CHECK(s.script("llength [list a \\\n  b]").value.to_string() == "2");
  CHECK(s.script(R"(llength [list "" {}])").value.to_string() == "2");
}

TEST_CASE("malformed scripts report a located Tcl error", RESULT_TAGS) {
  hefesto::test::Session s;
  const auto quote = s.script("count\ncount \"open");
  REQUIRE_FALSE(quote.ok());
  CHECK(quote.diagnostics.front().text == "missing \"");
  CHECK(std::get<hefesto::diag::SourceLocation>(quote.diagnostics.front().location).line == 2U);
  CHECK(s.script("count {a {b}").diagnostics.front().text == "missing close-brace");
  CHECK(s.script("count {a}b").diagnostics.front().text == "extra characters after close-brace");
  CHECK(s.script("count \"a\"b").diagnostics.front().text == "extra characters after close-quote");
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
