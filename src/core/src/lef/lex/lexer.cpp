// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF/DEF tokenizer and message counter, scaffold stubs
// (openspec/changes/m6-lef-def-oasis/design.md D4).
//
// Owner: LEFR (design D2).

#include "hefesto/lef/lex/lexer.hpp"

#include "hefesto/cmd/stubs.hpp"

namespace hefesto::lef {

diag::Outcome<std::optional<Token>> Lexer::next() {
  return cmd::not_implemented_diagnostic("lef lexer");
}

double Lexer::estimate() const noexcept {
  return 0.0;
}

bool MessageCounter::count(diag::Severity severity) {
  if (severity == diag::Severity::error) {
    ++errors_;
  } else if (severity == diag::Severity::warning) {
    ++warnings_;
  }
  return true;
}

}  // namespace hefesto::lef
