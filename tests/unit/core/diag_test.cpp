// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "hefesto/diag/codes.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/format.hpp"
#include "hefesto/diag/sink.hpp"

namespace diag = hefesto::diag;

#define RECORD_TAGS "[cap:hefesto-diagnostics][req:Structured diagnostic records]"
#define PRESENTATION_TAGS "[cap:hefesto-diagnostics][req:Diagnostic presentation and logging]"

TEST_CASE("a diagnostic carries severity, stable code, text, location and related object", RECORD_TAGS) {
  diag::Diagnostic d = diag::make(diag::codes::kCmdUsage, "bad", diag::SourceLocation{"x.tech", 42});
  d.related = diag::ObjectRef{diag::ObjectKind::rule, "width"};
  CHECK(d.severity == diag::Severity::error);  // default severity from the code table
  CHECK(d.code == "CMD-USAGE-001");
  CHECK(std::get<diag::SourceLocation>(d.location).line == 42U);
  CHECK(diag::make(diag::codes::kCliLog, "t").severity == diag::Severity::warning);
}

TEST_CASE("severity and object-kind names round-trip", RECORD_TAGS) {
  for (const auto s : {diag::Severity::error, diag::Severity::warning, diag::Severity::info, diag::Severity::debug}) {
    CHECK(diag::parse_severity(diag::severity_name(s)) == s);
  }
  CHECK_FALSE(diag::parse_severity("fatal").has_value());
  CHECK(diag::object_kind_name(diag::ObjectKind::instance) == "instance");
}

TEST_CASE("every code in the table is well formed, unique and sorted", RECORD_TAGS) {
  const auto table = diag::code_table();
  REQUIRE_FALSE(table.empty());
  std::set<std::string_view> seen;
  for (std::size_t i = 0; i < table.size(); ++i) {
    INFO(table[i].code);
    CHECK(diag::is_well_formed_code(table[i].code));
    CHECK(seen.insert(table[i].code).second);
    CHECK_FALSE(table[i].summary.empty());
    if (i > 0) {
      CHECK(table[i - 1].code < table[i].code);
    }
    CHECK(diag::find_code(table[i].code).has_value());
  }
  CHECK_FALSE(diag::find_code("NOPE-NOPE-001").has_value());
  CHECK_FALSE(diag::is_well_formed_code("CMD-001"));
  CHECK_FALSE(diag::is_well_formed_code("cmd-x-001"));
  CHECK_FALSE(diag::is_well_formed_code("CMD-X-01"));
  CHECK(diag::is_well_formed_code("CELL-READ-001"));
}

namespace {

// The table text between the BEGIN/END markers of a code document.
std::string generated_table(const std::string& relative_path) {
  std::ifstream in(std::string{HEFESTO_SOURCE_DIR} + "/" + relative_path);
  REQUIRE(in);
  std::stringstream buffer;
  buffer << in.rdbuf();
  const std::string doc = buffer.str();
  const std::string begin_marker = "<!-- BEGIN DIAGNOSTIC CODES";
  const std::string end_marker = "<!-- END DIAGNOSTIC CODES -->";
  const auto begin = doc.find(begin_marker);
  const auto end = doc.find(end_marker);
  REQUIRE(begin != std::string::npos);
  REQUIRE(end != std::string::npos);
  const auto table_start = doc.find('\n', begin) + 1;
  return doc.substr(table_start, end - table_start);
}

}  // namespace

TEST_CASE("diagnostic code table matches docs/diagnostics/codes.md", RECORD_TAGS) {
  // Regenerate with: hefesto-cli --diagnostic-codes
  CHECK(generated_table("docs/diagnostics/codes.md") == diag::render_code_table_markdown());
}

TEST_CASE("each area's code table matches its document and prefix", RECORD_TAGS) {
  // Regenerate with: hefesto-cli --diagnostic-codes=<area>
  for (const auto& area : diag::code_areas()) {
    INFO(area.name);
    CHECK(generated_table(std::string{area.doc}) == diag::render_code_table_markdown(area.name));
    for (std::size_t i = 0; i < area.codes.size(); ++i) {
      INFO(area.codes[i].code);
      if (!area.prefix.empty()) {
        CHECK(area.codes[i].code.starts_with(std::string{area.prefix} + "-"));
      }
      if (i > 0) {
        CHECK(area.codes[i - 1].code < area.codes[i].code);
      }
    }
  }
  CHECK(diag::find_code_area("tech").has_value());
  CHECK_FALSE(diag::find_code_area("nope").has_value());
}

TEST_CASE("text rendering", PRESENTATION_TAGS) {
  diag::Diagnostic d{diag::Severity::warning, "CELL-READ-001", "odd", {}, {}};
  CHECK(diag::format_text(d) == "warning: CELL-READ-001: odd");
  d.location = diag::SourceLocation{"a.mag", 7};
  CHECK(diag::format_text(d) == "warning: CELL-READ-001: odd at a.mag:7");
  d.location = diag::LayoutLocation{"inv", -10, 25};
  CHECK(diag::format_text(d) == "warning: CELL-READ-001: odd in inv (-10, 25)");
}

TEST_CASE("JSON-lines rendering has a byte-stable key order", PRESENTATION_TAGS) {
  diag::Diagnostic d{diag::Severity::error, "DRC-RULE-001", "spacing \"m1\"\n", {}, {}};
  CHECK(diag::format_json(d) ==
        R"({"severity":"error","code":"DRC-RULE-001","text":"spacing \"m1\"\n","location":null,"related":null})");
  d.location = diag::LayoutLocation{"top", 100, -5};
  d.related = diag::ObjectRef{diag::ObjectKind::rule, "m1.2"};
  CHECK(diag::format_json(d) ==
        R"({"severity":"error","code":"DRC-RULE-001","text":"spacing \"m1\"\n",)"
        R"("location":{"cell":"top","x":100,"y":-5},"related":{"kind":"rule","name":"m1.2"}})");
  d.location = diag::SourceLocation{"t.tech", 42};
  d.related.reset();
  CHECK(diag::format_json(d) ==
        R"({"severity":"error","code":"DRC-RULE-001","text":"spacing \"m1\"\n",)"
        R"("location":{"file":"t.tech","line":42},"related":null})");
  CHECK(diag::format(d, diag::Format::text) == diag::format_text(d));
}

TEST_CASE("JSON rendering never throws on invalid UTF-8", PRESENTATION_TAGS) {
  const diag::Diagnostic d{diag::Severity::info, "X-Y-001", "bad \xFF byte", {}, {}};
  CHECK(diag::format_json(d).find("\xEF\xBF\xBD") != std::string::npos);  // U+FFFD
}

TEST_CASE("Magic rendering prints the text, with TechPrintLine context for technology lines", PRESENTATION_TAGS) {
  diag::Diagnostic d{diag::Severity::error, "TECH-LINE-001", "Line must contain exactly 2 lists of types",
                     diag::SourceLocation{"./input/tiny.tech", 60}, diag::ObjectRef{diag::ObjectKind::section, "connect"}};
  CHECK(diag::format_magic(d) ==
        "./input/tiny.tech: line 60: section connect:\n\tLine must contain exactly 2 lists of types");
  CHECK(diag::format(d, diag::Format::magic) == diag::format_magic(d));
  d.related.reset();  // not a technology-file line: the text alone
  CHECK(diag::format_magic(d) == "Line must contain exactly 2 lists of types");
  d.location = diag::SourceLocation{"f.tech", 0};
  d.related = diag::ObjectRef{diag::ObjectKind::section, "connect"};
  CHECK(diag::format_magic(d) == "Line must contain exactly 2 lists of types");
  CHECK(diag::object_kind_name(diag::ObjectKind::section) == "section");
}

TEST_CASE("format names parse", PRESENTATION_TAGS) {
  CHECK(diag::parse_format("magic") == diag::Format::magic);
  CHECK(diag::parse_format("json") == diag::Format::json);
  CHECK(diag::parse_format("text") == diag::Format::text);
  CHECK_FALSE(diag::parse_format("xml").has_value());
}

TEST_CASE("collecting sink keeps emission order", RECORD_TAGS) {
  diag::CollectingSink sink;
  sink.emit({diag::Severity::info, "A-B-001", "1", {}, {}});
  CHECK_FALSE(sink.has_errors());
  sink.emit({diag::Severity::error, "A-B-002", "2", {}, {}});
  CHECK(sink.has_errors());
  const auto all = sink.take();
  REQUIRE(all.size() == 2U);
  CHECK(all[0].text == "1");
  CHECK(diag::first_error(all)->code == "A-B-002");
  CHECK(sink.snapshot().empty());
}
