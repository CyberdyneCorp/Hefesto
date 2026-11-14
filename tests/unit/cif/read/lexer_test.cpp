// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CIF lexer (capability cif-format-io "CIF lexical rules"). Owner: CIF.

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <string>

#include "hefesto/cif/read/lexer.hpp"

using hefesto::cif::Lexer;

TEST_CASE("CIF blanks exclude digits, upper case and the CIF punctuation",
          "[cap:cif-format-io][req:CIF lexical rules]") {
  for (const int ch : std::initializer_list<int>{'0', '9', 'A', 'Z', '-', ';', '(', ')', Lexer::kEof}) {
    CHECK_FALSE(Lexer::is_blank(ch));
  }
  for (const int ch : std::initializer_list<int>{' ', '\n', '\t', 'a', 'z', ',', '.', 0xC3}) {
    CHECK(Lexer::is_blank(ch));
  }
}

TEST_CASE("Separators skip upper-case letters before an integer, blanks do not",
          "[cap:cif-format-io][req:CIF lexical rules]") {
  Lexer lex("ox 12 ABC -7 x");
  lex.skip_blanks();
  CHECK(lex.peek() == '1');
  CHECK(lex.signed_integer() == 12);
  CHECK(lex.signed_integer() == -7);  // "ABC" is skipped as separators
  CHECK_FALSE(lex.signed_integer().has_value());
  CHECK(lex.at_end());
}

TEST_CASE("A lone minus sign is consumed but gives no integer", "[cap:cif-format-io][req:CIF lexical rules]") {
  Lexer lex("- ;");
  CHECK_FALSE(lex.signed_integer().has_value());
  CHECK(lex.peek() == ' ');
}

TEST_CASE("Integers saturate instead of overflowing", "[cap:cif-format-io][req:CIF lexical rules]") {
  Lexer lex("99999999999999999999999999999 -99999999999999999999999999999 4611686018427387904");
  CHECK(lex.signed_integer() == Lexer::kIntegerLimit);
  CHECK(lex.signed_integer() == -Lexer::kIntegerLimit);
  CHECK(lex.signed_integer() == Lexer::kIntegerLimit);
}

TEST_CASE("Newlines are counted by the skip functions and comments", "[cap:cif-format-io][req:CIF lexical rules]") {
  Lexer lex("\n\n(a\n(b)\n)\nx;\n");
  lex.skip_blanks();
  CHECK(lex.line() == 3);
  REQUIRE(lex.skip_comment());
  CHECK(lex.line() == 5);
  lex.skip_to_semicolon();
  CHECK(lex.line() == 6);
  CHECK(lex.skip_semicolon());
  CHECK(lex.line() == 7);
  CHECK(lex.at_end());
}

TEST_CASE("A comment reaching the end of file fails", "[cap:cif-format-io][req:CIF lexical rules]") {
  Lexer lex("(open (nested) still open");
  CHECK_FALSE(lex.skip_comment());
  CHECK(lex.at_end());
}

TEST_CASE("Deeply nested comments are skipped without recursion", "[cap:cif-format-io][req:CIF lexical rules]") {
  const std::string text = std::string(200000, '(') + std::string(200000, ')') + ";";
  Lexer lex(text);
  CHECK(lex.skip_comment());
  CHECK(lex.peek() == ';');
}

TEST_CASE("Names end at whitespace or a semicolon; layer names take five characters",
          "[cap:cif-format-io][req:CIF lexical rules]") {
  Lexer lex(" \tnet<3>;  CMFPIN;");
  CHECK(lex.name() == "net<3>");
  CHECK(lex.skip_semicolon());
  CHECK(lex.layer_name() == "CMFPI");
  CHECK(lex.peek() == 'N');
}

TEST_CASE("A missing semicolon consumes nothing", "[cap:cif-format-io][req:CIF lexical rules]") {
  Lexer lex("  X;");
  CHECK_FALSE(lex.skip_semicolon());
  CHECK(lex.peek() == 'X');
}
