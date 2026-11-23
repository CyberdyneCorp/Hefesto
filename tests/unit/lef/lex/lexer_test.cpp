// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Unit tests of the LEF/DEF tokenizer, END matching and message reporting
// (capability lef-read; openspec/changes/archive/2026-11-22-m6-lef-def-oasis tasks.md 5.1).

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/lef/lex/lexer.hpp"
#include "hefesto/lef/lex/statements.hpp"

using namespace hefesto;
using lef::EolMode;

namespace {

std::vector<std::byte> bytes(std::string_view text) {
  std::vector<std::byte> out;
  for (const char c : text) {
    out.push_back(static_cast<std::byte>(c));
  }
  return out;
}

/// A lexer over `text` with its own source.
struct Lexed {
  explicit Lexed(std::string_view text) : source(bytes(text)), lexer(source, text.size()) {}

  std::vector<std::string> tokens() {
    std::vector<std::string> out;
    while (const auto token = lexer.take()) {
      out.push_back(token->text);
    }
    return out;
  }

  gds::MemorySource source;
  lef::Lexer lexer;
};

/// A lexer with a message target collecting into a sink.
struct Messaged : Lexed {
  explicit Messaged(std::string_view text) : Lexed(text), target{counter, sink, lexer} {}

  std::vector<std::string> texts() {
    std::vector<std::string> out;
    for (const auto& d : sink.take()) {
      out.push_back(d.text);
    }
    return out;
  }

  lef::MessageCounter counter;
  diag::CollectingSink sink;
  lef::MessageTarget target;
};

using Strings = std::vector<std::string>;

}  // namespace

TEST_CASE("comment after tokens", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  Lexed lexed{"RECT 0 0 1 1 ; # pin area\n"};
  CHECK(lexed.tokens() == Strings{"RECT", "0", "0", "1", "1", ";"});
}

TEST_CASE("quoted token keeps its quotes and whitespace", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  Lexed lexed{"PROPERTY note \"a b\" ;\n"};
  CHECK(lexed.tokens() == Strings{"PROPERTY", "note", "\"a b\"", ";"});
}

TEST_CASE("a semicolon glued to a number stays in its token", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  Lexed lexed{"SIZE 1.0 BY 2.0;\nSYMMETRY X Y ;\nCLASS CORE ;\n"};
  CHECK(lexed.tokens() == Strings{"SIZE", "1.0", "BY", "2.0;", "SYMMETRY", "X", "Y", ";", "CLASS", "CORE", ";"});

  // The statement skip after SIZE's values ends at the first `;` token, so
  // it consumes SYMMETRY's statement (lef-read scenario "Attached semicolon
  // swallows the next statement").
  Lexed skipped{"2.0;\nSYMMETRY X Y ;\nCLASS CORE ;\n"};
  CHECK(lef::parse_number(skipped.lexer.take()->text) == 2.0F);
  CHECK(lef::skip_statement(skipped.lexer) == 3);
  CHECK(skipped.lexer.take()->text == "CLASS");
}

TEST_CASE("comment and blank lines are skipped but counted", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  Lexed lexed{"# header\n\n   \t\nVERSION 5.7 ;\n  # indented comment\nEND LIBRARY\n"};
  auto token = lexed.lexer.take();
  REQUIRE(token);
  CHECK(token->text == "VERSION");
  CHECK(token->line == 4);
  CHECK(token->offset == 15);
  (void)lexed.lexer.take();
  (void)lexed.lexer.take();
  token = lexed.lexer.take();
  REQUIRE(token);
  CHECK(token->text == "END");
  CHECK(token->line == 6);
}

TEST_CASE("a hash inside a token is part of it", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  Lexed lexed{"PIN a#b #c\nX\n"};
  CHECK(lexed.tokens() == Strings{"PIN", "a#b", "X"});
}

TEST_CASE("a quoted token may span lines without counting them", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  Lexed lexed{"PROPERTY p \"first\nsecond \\\" third\" ;\nNEXT\n"};
  CHECK(lexed.lexer.take()->text == "PROPERTY");
  CHECK(lexed.lexer.take()->text == "p");
  const auto quoted = lexed.lexer.take();
  REQUIRE(quoted);
  CHECK(quoted->text == "\"first\nsecond \\\" third\"");
  CHECK(lexed.lexer.take()->text == ";");
  const auto next = lexed.lexer.take();
  REQUIRE(next);
  CHECK(next->text == "NEXT");
  CHECK(next->line == 2);  // the continuation read is not a line
}

TEST_CASE("the character after a closing quote is dropped", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  // LefNextToken terminates the token by overwriting the next character.
  Lexed lexed{"\"ab\"cd e\n"};
  CHECK(lexed.tokens() == Strings{"\"ab\"", "d", "e"});
}

TEST_CASE("a line longer than 2048 characters is read in pieces", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  // 2046 characters of padding, then a token that crosses the boundary.
  const std::string text = std::string(2046, ' ') + "ABCD EF\nNEXT\n";
  Lexed lexed{text};
  const auto first = lexed.lexer.take();
  REQUIRE(first);
  CHECK(first->text == "AB");
  CHECK(first->line == 1);
  const auto second = lexed.lexer.take();
  REQUIRE(second);
  CHECK(second->text == "CD");
  CHECK(second->line == 2);  // every piece is a line read
  CHECK(lexed.lexer.take()->text == "EF");
  const auto next = lexed.lexer.take();
  REQUIRE(next);
  CHECK(next->line == 3);
  CHECK_FALSE(lexed.lexer.take());
  CHECK(lexed.lexer.estimate() == 1.0);
}

TEST_CASE("a NUL ends the line as C string code sees it", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  const std::string text{"A B\0 C D\nE\n", 11};
  Lexed lexed{text};
  CHECK(lexed.tokens() == Strings{"A", "B", "E"});
}

TEST_CASE("end-of-line mode yields a line-end token before a new line", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  Lexed lexed{"END\nX Y\n"};
  CHECK(lexed.lexer.take(EolMode::report)->end_of_line);  // the first read is a new line too
  CHECK(lexed.lexer.take(EolMode::report)->text == "END");
  const auto eol = lexed.lexer.take(EolMode::report);
  REQUIRE(eol);
  CHECK(eol->end_of_line);
  CHECK(eol->text == "\n");
  CHECK(lexed.lexer.take(EolMode::report)->text == "X");
  CHECK(lexed.lexer.take(EolMode::report)->text == "Y");
}

TEST_CASE("numbers parse from the longest numeric prefix", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  CHECK(lef::parse_number("1.5abc") == 1.5F);
  CHECK(lef::parse_number("0.0125") == 0.0125F);
  CHECK(lef::parse_number("-2e1") == -20.0F);
  CHECK(lef::parse_number("+.5;") == 0.5F);
  CHECK_FALSE(lef::parse_number("abc"));
  CHECK_FALSE(lef::parse_number(";"));
}

TEST_CASE("keywords match case-insensitively and in full", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  constexpr std::string_view table[] = {"LAYER", "LAYERS"};
  CHECK(lef::lookup_full("layer", table) == 0U);
  CHECK(lef::lookup_full("LaYeRs", table) == 1U);
  CHECK_FALSE(lef::lookup_full("LAY", table));
  CHECK_FALSE(lef::lookup_full("LAYERSX", table));
}

TEST_CASE("an empty or unreadable input ends at once", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  Lexed empty{""};
  CHECK_FALSE(empty.lexer.take());
  CHECK_FALSE(empty.lexer.take());
  CHECK(empty.lexer.line() == 0);
  CHECK(empty.lexer.estimate() == 0.0);  // a zero size is unknown

  Lexed open_quote{"A \"never closed\n"};
  CHECK(open_quote.tokens() == Strings{"A"});
  CHECK_FALSE(open_quote.lexer.failure());
}

TEST_CASE("END with a section name", "[cap:lef-read][req:END statement matching]") {
  Messaged closed{"metal1 ;"};
  CHECK(lef::parse_end(closed.lexer, "METAL1", closed.target) == lef::EndMatch::closed);

  Messaged mismatch{"INVX"};
  CHECK(lef::parse_end(mismatch.lexer, "INV", mismatch.target) == lef::EndMatch::not_matched);

  Messaged end_end{"END"};
  CHECK(lef::parse_end(end_end.lexer, "INV", end_end.target) == lef::EndMatch::end_end);

  // ENDEXT is not END: LookupFull answers 1, not 0.
  Messaged endext{"ENDEXT"};
  CHECK(lef::parse_end(endext.lexer, "INV", endext.target) == lef::EndMatch::not_matched);

  Messaged none{""};
  CHECK(lef::parse_end(none.lexer, "INV", none.target) == lef::EndMatch::not_matched);
  CHECK(none.texts() == Strings{"LEF read, Line 0 (Error): Bad file read while looking for END statement"});
}

TEST_CASE("END without a name closes only at the end of its line", "[cap:lef-read][req:END statement matching]") {
  Messaged last{"END\nRECT"};
  CHECK(last.lexer.take()->text == "END");
  CHECK(lef::parse_end(last.lexer, std::nullopt, last.target) == lef::EndMatch::closed);
  CHECK(last.lexer.take()->text == "RECT");

  Messaged more{"END PORT\n"};
  CHECK(more.lexer.take()->text == "END");
  CHECK(lef::parse_end(more.lexer, std::nullopt, more.target) == lef::EndMatch::not_matched);
}

TEST_CASE("skipping a section consumes through its END", "[cap:lef-read][req:END statement matching]") {
  Messaged units{"DATABASE MICRONS 2000 ;\nEND UNITS\nNEXT\n"};
  lef::skip_section(units.lexer, "UNITS", units.target);
  CHECK(units.lexer.take()->text == "NEXT");

  // END END: the second END is matched again.
  Messaged doubled{"A END END TIMING NEXT\n"};
  lef::skip_section(doubled.lexer, "TIMING", doubled.target);
  CHECK(doubled.lexer.take()->text == "NEXT");

  Messaged ext{"anything END foo ENDEXT NEXT\n"};
  lef::skip_section(ext.lexer, "BEGINEXT", ext.target);
  CHECK(ext.lexer.take()->text == "NEXT");

  Messaged unended{"END OTHER\nX\n"};
  lef::skip_section(unended.lexer, "ARRAY", unended.target);
  CHECK(unended.texts() == Strings{"LEF read, Line 2 (Error): Section ARRAY has no END record!"});

  // A nameless skip (PORT) meeting ENDEXT does not crash (Magic strcmp(NULL)).
  Messaged nameless{"ENDEXT\nEND\nNEXT\n"};
  lef::skip_section(nameless.lexer, std::nullopt, nameless.target);
  CHECK(nameless.lexer.take()->text == "NEXT");
}

TEST_CASE("messages carry Magic's prefixes and the line", "[cap:lef-read][req:LEF/DEF message reporting]") {
  lef::MessageCounter counter;
  diag::CollectingSink sink;
  counter.report(sink, lef::Dialect::lef, diag::Severity::error, "LEF-READ-001", "bad", 7U);
  counter.report(sink, lef::Dialect::def, diag::Severity::warning, "LEF-READ-002", "odd", 8U);
  counter.report(sink, lef::Dialect::lef, diag::Severity::info, "LEF-READ-003", "note", std::nullopt);
  const auto got = sink.take();
  REQUIRE(got.size() == 3);
  CHECK(got[0].text == "LEF read, Line 7 (Error): bad");
  CHECK(got[0].severity == diag::Severity::error);
  CHECK(got[0].code == "LEF-READ-001");
  CHECK(got[1].text == "DEF read, Line 8 (Warning): odd");
  CHECK(got[1].severity == diag::Severity::warning);
  CHECK(got[2].text == "LEF read (Message): note");
  CHECK(got[2].severity == diag::Severity::info);
  CHECK(counter.errors() == 1);
  CHECK(counter.warnings() == 1);
  CHECK(counter.messages() == 1);
}

TEST_CASE("error cap", "[cap:lef-read][req:LEF/DEF message reporting]") {
  lef::MessageCounter counter;
  diag::CollectingSink sink;
  for (int i = 0; i < 150; ++i) {
    counter.report(sink, lef::Dialect::lef, diag::Severity::error, "LEF-READ-001", "e", 1U);
  }
  for (int i = 0; i < 101; ++i) {
    counter.report(sink, lef::Dialect::lef, diag::Severity::warning, "LEF-READ-002", "w", 1U);
  }
  for (int i = 0; i < 101; ++i) {
    counter.report(sink, lef::Dialect::lef, diag::Severity::info, "LEF-READ-003", "m", 1U);
  }
  auto got = sink.take();
  REQUIRE(got.size() == 303);
  CHECK(got[99].text == "LEF read, Line 1 (Error): e");
  CHECK(got[100].text == "LEF Read:  Further errors will not be reported.");
  CHECK(got[100].code == "LEF-LEX-001");
  CHECK(got[100].severity == diag::Severity::error);
  CHECK(got[201].text == "LEF read:  Further warnings will not be reported.");
  CHECK(got[302].text == "LEF read:  Further messages will not be reported.");
  CHECK(counter.errors() == 150);

  counter.summarize(sink, lef::Dialect::lef);
  got = sink.take();
  REQUIRE(got.size() == 2);
  CHECK(got[0].text == "LEF Read: encountered 150 errors total.");
  CHECK(got[0].code == "LEF-LEX-002");
  CHECK(got[1].text == "LEF Read: encountered 101 warnings total.");
  CHECK(counter.errors() == 0);
  CHECK(counter.warnings() == 0);
  CHECK(counter.messages() == 0);
}

TEST_CASE("totals are singular for one and absent for none", "[cap:lef-read][req:LEF/DEF message reporting]") {
  lef::MessageCounter counter;
  diag::CollectingSink sink;
  CHECK(counter.count(diag::Severity::error));
  counter.summarize(sink, lef::Dialect::def);
  const auto got = sink.take();
  REQUIRE(got.size() == 1);
  CHECK(got[0].text == "DEF Read: encountered 1 error total.");
  counter.summarize(sink, lef::Dialect::def);
  CHECK(sink.take().empty());
}

TEST_CASE("LEFDEF-005 input tokenizes as Magic does", "[cap:lef-read][req:LEF/DEF tokenizer]") {
  // Comments, a quoted PROPERTY value, `;` glued to a number and a line
  // longer than 2048 characters (tests/golden/LEFDEF/CASES.md LEFDEF-005).
  const std::string long_comment = "# " + std::string(2100, 'x') + "\n";
  const std::string text = "VERSION 5.7 ;\n" + long_comment +
                           "MACRO cell1  # trailing comment\n"
                           "  SIZE 1.0 BY 2.0;\n"
                           "  SYMMETRY X Y ;\n"
                           "  PROPERTY note \"two words\" ;\n"
                           "END cell1\n";
  Lexed lexed{text};
  CHECK(lexed.tokens() == Strings{"VERSION", "5.7", ";", std::string(54, 'x') /* the rest of the long comment line */, "MACRO", "cell1",
                                  "SIZE", "1.0", "BY", "2.0;", "SYMMETRY", "X", "Y", ";", "PROPERTY", "note",
                                  "\"two words\"", ";", "END", "cell1"});
  CHECK(lexed.lexer.line() == 8);  // the long comment is two line reads
}
