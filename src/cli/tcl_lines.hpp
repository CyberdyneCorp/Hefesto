// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-cli's Tcl input (m4-scripting design D3/D16, tasks.md 13.2): every
// script the CLI runs — `-c` text, a script file, standard input and the
// interactive prompt — is Tcl evaluated in the project's interpreter, one
// complete command at a time, as a Tcl console reads it (`info complete`
// over whole lines). A chunk is one or more lines holding complete
// commands; its result is the result of its last command.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::script {
class Interpreter;
}

namespace hefesto::cli {

/// True when `text` holds only complete Tcl commands (Tcl's `info complete`).
[[nodiscard]] bool is_complete(script::Interpreter& interp, std::string_view text);

/// One complete command (or several on one line) and the line it starts on.
struct TclChunk {
  std::string text;
  std::uint32_t line = 1;
};

/// Splits a script into chunks line by line: a line that leaves a brace
/// group, a quote or a bracket open continues on the next one.
class ChunkReader {
 public:
  ChunkReader(script::Interpreter& interp, std::string_view text) : interp_(interp), text_(text) {}

  /// The next chunk; nullopt at the end. An unfinished last command is
  /// returned as is (its evaluation reports Tcl's parse error).
  [[nodiscard]] std::optional<TclChunk> next();

 private:
  script::Interpreter& interp_;
  std::string_view text_;
  std::size_t pos_ = 0;
  std::uint32_t line_ = 1;
};

/// The Tcl error of a failed evaluation of a chunk that starts at `first_line`
/// of `source` ("-c", a file name, "-" for standard input), located at the
/// line Tcl reports.
[[nodiscard]] diag::Diagnostic located_error(const cmd::CommandResult& failed, std::string_view source,
                                             std::uint32_t first_line);

}  // namespace hefesto::cli
