// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Statement helpers shared by the LEF and DEF readers (capability lef-read
// "LEF/DEF tokenizer", "END statement matching", "LEF/DEF message
// reporting"; openspec/changes/m6-lef-def-oasis/design.md D6):
// LookupFull, the `%f` number parse, LefEndStatement, LefParseEndStatement
// and LefSkipSection, and MessageTarget, which sends LefError messages
// tagged with the lexer's line.
//
// Owner: LEFR (design D2).

#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/lef/lex/lexer.hpp"

namespace hefesto::lef {

/// Where reader messages go: LefError with the lexer's current line.
struct MessageTarget {
  MessageCounter& counter;
  diag::DiagnosticSink& sink;
  const Lexer& lexer;

  void report(Dialect dialect, diag::Severity severity, std::string_view code, std::string_view text) const {
    counter.report(sink, dialect, severity, code, text, lexer.line());
  }
};

/// LookupFull: the index of the table entry equal to `token` ignoring ASCII
/// case (full words only, no prefixes); nullopt when none.
[[nodiscard]] std::optional<std::size_t> lookup_full(std::string_view token,
                                                     std::span<const std::string_view> table) noexcept;

/// `token` equals `keyword` ignoring ASCII case.
[[nodiscard]] bool keyword_is(std::string_view token, std::string_view keyword) noexcept;

/// sscanf(token, "%f"): the binary32 value of the longest numeric prefix
/// (`1.5abc` is 1.5, `0.5;` is 0.5); nullopt when there is none.
[[nodiscard]] std::optional<float> parse_number(std::string_view token);

/// LEF names are kept to their first 127 characters (`%.127s`).
[[nodiscard]] std::string truncate_name(std::string_view name);

/// LefEndStatement: consumes tokens through the next `;` token (or the end
/// of input). Returns the number of tokens before the `;`.
std::size_t skip_statement(Lexer& lexer);

/// LefParseEndStatement's answer.
enum class EndMatch : std::uint8_t {
  closed,       // 1: `END <name>`, or `END` alone on its line when no name is expected
  not_matched,  // 0
  end_end,      // -1: the token after END is another END
};

/// LefParseEndStatement: reads what follows an END. With `name`, the next
/// token must equal it ignoring case; without, the END must be the last
/// token of its line. At end of input it reports "Bad file read while
/// looking for END statement" (LEF-LEX-003) and answers not_matched.
EndMatch parse_end(Lexer& lexer, std::optional<std::string_view> name, const MessageTarget& messages);

/// LefSkipSection: consumes tokens through `END <section>` (`END` alone on
/// its line when `section` is nullopt), or through the first ENDEXT when
/// `section` is "BEGINEXT". At end of input it reports "Section <N> has no
/// END record!" (LEF-LEX-003).
void skip_section(Lexer& lexer, std::optional<std::string_view> section, const MessageTarget& messages);

}  // namespace hefesto::lef
