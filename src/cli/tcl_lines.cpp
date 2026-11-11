// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "tcl_lines.hpp"

#include <algorithm>
#include <variant>

#include "hefesto/script/binding/tags.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::cli {

bool is_complete(script::Interpreter& interp, std::string_view text) {
  const auto r = interp.evaluate("::info complete " + script::quote_list_element(text));
  return !r.ok() || r.value.to_string() == "1";
}

std::optional<TclChunk> ChunkReader::next() {
  if (pos_ >= text_.size()) {
    return std::nullopt;
  }
  TclChunk chunk{{}, line_};
  while (pos_ < text_.size()) {
    const std::size_t end = std::min(text_.find('\n', pos_), text_.size());
    chunk.text.append(text_.substr(pos_, end - pos_));
    chunk.text += '\n';
    pos_ = end + 1;
    ++line_;
    if (is_complete(interp_, chunk.text)) {
      break;
    }
  }
  return chunk;
}

diag::Diagnostic located_error(const cmd::CommandResult& failed, std::string_view source, std::uint32_t first_line) {
  diag::Diagnostic error = failed.diagnostics.empty()
                               ? diag::make("SCRIPT-TCL-001", "script evaluation failed")
                               : failed.diagnostics.front();
  std::uint32_t line = first_line;
  if (const auto* at = std::get_if<diag::SourceLocation>(&error.location); at != nullptr && at->line > 0) {
    line = first_line + at->line - 1;
  }
  error.location = diag::SourceLocation{std::string{source}, line};
  return error;
}

}  // namespace hefesto::cli
