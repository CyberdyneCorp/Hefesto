// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Technology-file reader: physical lines, comments, continuation, tokens,
// sections, `include` and `repeat` (capability tech-file-format). It produces
// the sections as token lines; section semantics are applied by the loader
// (hefesto/tech/loader.hpp), which runs the same reader.
//
// Owner: TECH (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::tech {

/// One logical line: its tokens after quoting rules, and where it started.
struct TechLine {
  std::vector<std::string> tokens;
  diag::SourceLocation where;
};

/// One section between its header line and `end`.
struct TechSection {
  std::string name;  // the registered name (an alias header such as `images` gives `contact`)
  std::vector<TechLine> lines;
  diag::SourceLocation where;
  bool closed = false;  // false when the file ended before the section's `end`
};

/// A technology file with `include` expanded and `repeat` blocks unrolled.
struct TechDocument {
  std::vector<TechSection> sections;  // file order
  bool failed = false;                // an include was missing or a repeat block crossed a file
};

/// Expands a `~user/...` or `$VAR/...` file name (host mechanism, informative
/// per "File search rules"); std::nullopt leaves the name unexpanded.
using NameExpander = std::function<std::optional<std::string>(std::string_view)>;

/// Where `include` files and technology files are looked for.
struct SearchOptions {
  bool magic_compat = true;
  /// The system library path, searched after the working directory ".".
  std::vector<platform::Path> search_path;
  NameExpander expand;
};

/// Reader options: the search rules plus the include suffix of this load
/// (".tech", or ".tech27" after the legacy fallback; Q-TECH-1).
struct ReadOptions : SearchOptions {
  std::string include_suffix = ".tech";
};

/// "File search rules": opens `name` + `suffix` (the suffix is always
/// appended) directly for `~`, `$`, `/`, `./` and `../` names, otherwise
/// through ".", then each directory of `options.search_path`.
[[nodiscard]] std::optional<platform::Path> find_file(platform::FileSystem& fs, std::string_view name,
                                                      std::string_view suffix, const SearchOptions& options);

/// Reads technology text. `file_name` labels source locations; `include`
/// targets are resolved through `options` and then relative to `base_dir`.
/// Recoverable problems are emitted to `sink`; reading never stops early.
[[nodiscard]] diag::Outcome<TechDocument> read_tech_text(std::string_view text, std::string_view file_name,
                                                         platform::FileSystem& fs, const platform::Path& base_dir,
                                                         diag::DiagnosticSink& sink,
                                                         const ReadOptions& options = ReadOptions{});

/// Reads a technology file from disk (or the in-memory file system).
[[nodiscard]] diag::Outcome<TechDocument> read_tech_file(platform::FileSystem& fs, const platform::Path& path,
                                                         diag::DiagnosticSink& sink,
                                                         const ReadOptions& options = ReadOptions{});

/// Splits one logical line into tokens ("Tokenization and quoting").
[[nodiscard]] std::vector<std::string> tokenize_tech_line(std::string_view line);

}  // namespace hefesto::tech
