// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// TECH-private: the logical-line reader (techGetTokens plus the `include`
// and `repeat` directives of TechLoad) and the section scanner shared by
// read_tech_text() and the loader, with the section registry.
//
// Owner: TECH (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/tech/tech_file.hpp"

namespace hefesto::tech {

/// Section numbers in registration order ("Section registry and prerequisites").
enum SectionId : int {
  kSecTech, kSecVersion, kSecPlanes, kSecTypes, kSecStyles, kSecContact, kSecAliases, kSecCompose,
  kSecConnect, kSecCifOutput, kSecCifInput, kSecMzRouter, kSecDrc, kSecLef, kSecExtract, kSecWiring,
  kSecRouter, kSecPlowing, kSecPlot, kSecCount
};

using SectionMask = std::uint32_t;

[[nodiscard]] constexpr SectionMask section_bit(int section) noexcept {
  return SectionMask{1} << static_cast<unsigned>(section);
}

struct SectionDef {
  std::string_view name;
  std::string_view alias;
  SectionMask prerequisites;
  bool optional;
};

[[nodiscard]] const std::array<SectionDef, kSecCount>& section_table() noexcept;
/// techFindSection: exact name or alias; -1 if unknown.
[[nodiscard]] int find_section(std::string_view name) noexcept;

/// The tokens of the first logical line with any token, with comments,
/// blank lines and continuations handled but no directive processing
/// ("Readability pre-check"); empty when the text has none.
[[nodiscard]] std::vector<std::string> first_logical_line(std::string_view text);

/// Produces logical lines with `include` and `repeat` applied.
class LineReader {
 public:
  LineReader(platform::FileSystem& fs, diag::DiagnosticSink& sink, ReadOptions options);

  /// Starts reading `text` as the top-level file named `file_name`, whose
  /// directory is `dir` (the second place `include` looks).
  void open(std::string_view text, std::string file_name, platform::Path dir);
  /// The next logical line with at least one token, or nullopt at the end.
  [[nodiscard]] std::optional<TechLine> next();
  /// True when an `include` was missing or a repeat block crossed a file.
  [[nodiscard]] bool failed() const noexcept { return failed_; }

 private:
  struct File {
    std::string name;
    platform::Path dir;
    std::vector<std::string> lines;
    std::size_t next = 0;
    std::uint64_t id = 0;  // distinguishes successive files at one include depth
    // Physical lines read so far (Magic's techLineNumber). Unlike `next`, it
    // keeps increasing when a repeat block jumps back.
    std::uint32_t line_no = 0;
    // magic-compat: the next read does not count, reproducing Magic's
    // techLineNumber after an included file ends (see read_physical).
    bool skip_count = false;
  };
  struct Repeat {
    long long remaining = 0;
    std::uint64_t file_id = 0;  // the file the saved position belongs to
    std::size_t position = 0;
  };

  [[nodiscard]] std::optional<TechLine> raw_line();
  [[nodiscard]] bool read_physical(std::string& out, std::uint32_t& line_no);
  bool directive(const TechLine& line);
  void start_repeat(const TechLine& line);
  void end_repeat(const TechLine& line);
  void include(const TechLine& line);
  void push_text(std::string_view text, std::string name, platform::Path dir);
  void warn(std::string_view code, const diag::SourceLocation& where, std::string text);

  platform::FileSystem& fs_;
  diag::DiagnosticSink& sink_;
  ReadOptions options_;
  std::vector<File> files_;
  std::vector<Repeat> repeats_;  // one entry in magic-compat mode
  std::uint64_t next_file_id_ = 0;
  bool failed_ = false;
};

/// Callbacks of the section scanner.
class SectionClient {
 public:
  SectionClient() = default;
  SectionClient(const SectionClient&) = delete;
  SectionClient& operator=(const SectionClient&) = delete;
  virtual ~SectionClient() = default;

  /// A registered header was read with no section open. Returning false
  /// skips the section to its `end` (prerequisites missing, excluded, ...).
  virtual bool begin(int section, const TechLine& header) = 0;
  virtual void line(int section, const TechLine& line) = 0;
  virtual void end(int section, const TechLine& end_line) = 0;
};

/// The section-structure part of TechLoad: headers, `end`, skip mode,
/// "Bad section header line" and "Unrecognized section name".
void scan_sections(LineReader& reader, SectionClient& client, diag::DiagnosticSink& sink, bool magic_compat);

}  // namespace hefesto::tech
