// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF reader's state and section handlers (private to lef/read;
// design D6). read_stream (top_level.cpp) runs LefRead's loop; each section
// has its own file: layer_section.cpp (LefReadLayerSection for LAYER),
// nondefault_rule.cpp (LefReadNonDefaultRule), and the stubs of the
// sections later tasks fill in (top_level.cpp).
//
// Owner: LEFR (design D2).

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "hefesto/lef/read/stream.hpp"

namespace hefesto::lef::detail {

class Reader {
 public:
  Reader(Lexer& lexer, const MessageTarget& messages, const StreamOptions& options, StreamTarget& target)
      : lexer_(lexer), messages_(messages), options_(options), target_(target) {}

  /// LefRead's loop.
  StreamResult run();

  // --- shared helpers ---
  std::optional<Token> token() { return lexer_.take(); }
  std::size_t skip_statement() { return lef::skip_statement(lexer_); }
  void skip_section(std::string_view name) { lef::skip_section(lexer_, name, messages_); }
  EndMatch parse_end(std::string_view name) { return lef::parse_end(lexer_, name, messages_); }

  void error(std::string_view text) const;
  void warning(std::string_view text) const;
  /// LEF_INFO, or DEF_INFO where Magic's LEF code reports one.
  void info(std::string_view text, Dialect dialect = Dialect::lef) const;

  /// Reads a value token and converts it (round(v / s), or with
  /// `half_units` round(2v / s)); nullopt when the input ended, the token
  /// is not a number (Magic would use an uninitialised value) or the value
  /// does not fit.
  std::optional<db::Coord> read_value(bool half_units = false);

  [[nodiscard]] Lexer& lexer() noexcept { return lexer_; }
  [[nodiscard]] const MessageTarget& messages() const noexcept { return messages_; }
  [[nodiscard]] const StreamOptions& options() const noexcept { return options_; }
  [[nodiscard]] StreamTarget& target() noexcept { return target_; }
  [[nodiscard]] LayerContext layer_context() const noexcept {
    return LayerContext{target_.layers, options_.tech, options_.compat, messages_};
  }
  [[nodiscard]] bool magic_compat() const noexcept { return options_.compat.magic_compat; }

  // --- sections ---
  void layer_section();                                    // layer_section.cpp
  void nondefault_rule(const std::string& name);           // nondefault_rule.cpp
  void via_section(const std::string& name, bool viarule);  // top_level.cpp (stub, LEFVIA)
  void via_definition(const std::string& name);            // top_level.cpp (stub, LEFVIA)
  void macro_section(const std::string& name);             // top_level.cpp (stub, LEFR 5.4)

 private:
  bool top_statement(const Token& token);
  bool end_statement();

  bool end_library_ = false;

  Lexer& lexer_;
  const MessageTarget& messages_;
  const StreamOptions& options_;
  StreamTarget& target_;
};

}  // namespace hefesto::lef::detail
