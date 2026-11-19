// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF reader's state and section handlers (private to lef/read;
// design D6). read_stream (top_level.cpp) runs LefRead's loop; each section
// has its own file: layer_section.cpp (LefReadLayerSection for LAYER),
// via_section.cpp (VIA and VIARULE), nondefault_rule.cpp
// (LefReadNonDefaultRule) and macro.cpp (LefReadMacro for MACRO and SITE,
// with pin.cpp and port_geometry.cpp).
//
// Owner: LEFR (design D2).

#pragma once

#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "hefesto/lef/read/stream.hpp"
#include "hefesto/lef/via/context.hpp"

namespace hefesto::lef::detail {

class ViaBody;

/// The sink of the via code's messages (LEFVIA): each becomes a LefError
/// line under the dialect Magic prints it with (via_message_dialect).
class ViaMessages final : public diag::DiagnosticSink {
 public:
  explicit ViaMessages(const MessageTarget& messages) : messages_(messages) {}
  void emit(const diag::Diagnostic& d) override {
    messages_.report(via_message_dialect(d.code).value_or(Dialect::lef), d.severity, d.code, d.text);
  }

 private:
  const MessageTarget& messages_;
};

class Reader {
 public:
  Reader(Lexer& lexer, const MessageTarget& messages, const StreamOptions& options, StreamTarget& target)
      : lexer_(lexer), messages_(messages), options_(options), target_(target), via_messages_(messages) {}

  /// LefRead's loop.
  StreamResult run();

  // --- shared helpers ---
  std::optional<Token> token() { return lexer_.take(); }
  std::size_t skip_statement() { return lef::skip_statement(lexer_); }
  /// LefSkipSection; nullopt: an END alone on its line.
  void skip_section(std::optional<std::string_view> name) { lef::skip_section(lexer_, name, messages_); }
  /// LefParseEndStatement; nullopt: the END must end its line.
  EndMatch parse_end(std::optional<std::string_view> name) { return lef::parse_end(lexer_, name, messages_); }

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
  /// The via code's context: the technology, output style, compat
  /// settings and the reader's messages.
  [[nodiscard]] ViaContext via_context() noexcept {
    return ViaContext{options_.tech, options_.style, options_.compat, via_messages_};
  }

  // --- sections ---
  void layer_section();                                    // layer_section.cpp
  /// LefReadLayerSection's body over `entry` (`key`: the table name;
  /// `end_name`: the name END must match); `via`: the via geometry target
  /// of a via entry, nullptr for a layer.
  void layer_body(std::string_view key, const std::string& end_name, const LayerEntry& entry, ViaBody* via);
  void nondefault_rule(const std::string& name);           // nondefault_rule.cpp
  void via_section(const std::string& name, bool viarule);  // via_section.cpp
  void via_definition(const std::string& name);            // via_section.cpp
  void macro_section(const std::string& name);             // macro.cpp

  /// lefDefInitHash: whether `name` is a cell a MACRO or SITE section of
  /// this read created, and recording one.
  [[nodiscard]] bool made_here(const std::string& name) const { return made_.contains(name); }
  void note_made(const std::string& name) { made_.insert(name); }

 private:
  bool top_statement(const Token& token);
  bool end_statement();

  bool end_library_ = false;
  std::set<std::string, std::less<>> made_;

  Lexer& lexer_;
  const MessageTarget& messages_;
  const StreamOptions& options_;
  StreamTarget& target_;
  ViaMessages via_messages_;
};

}  // namespace hefesto::lef::detail
