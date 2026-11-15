// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Small helpers shared by the libFuzzer targets.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace hefesto::fuzz {

inline std::string_view as_text(const std::uint8_t* data, std::size_t size) noexcept {
  return {reinterpret_cast<const char*>(data), size};  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

/// The first byte selects a mode; the rest is the input proper.
struct Split {
  std::uint8_t mode = 0;
  std::string_view rest;
};

inline Split split_mode(std::string_view text) noexcept {
  if (text.empty()) {
    return {};
  }
  return {static_cast<std::uint8_t>(text.front()), text.substr(1)};
}

/// A file of the source tree (HEFESTO_SOURCE_DIR is set by tests/fuzz/CMakeLists.txt).
inline std::string source_file(std::string_view relative) {
  std::ifstream in{std::string{HEFESTO_SOURCE_DIR} + "/" + std::string{relative}, std::ios::binary};
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

inline std::string_view trimmed_line(std::string_view line) noexcept {
  const auto first = line.find_first_not_of(" \t\r");
  if (first == std::string_view::npos) {
    return {};
  }
  const auto last = line.find_last_not_of(" \t\r");
  return line.substr(first, last - first + 1);
}

/// A technology file's text with its `include <file>` lines replaced by
/// the included file (`<file>` or `<file>.tech` next to it), as the
/// technology reader resolves them, at most `depth` levels deep. The fuzz
/// targets load the text from memory, where no include file can be found.
inline std::string tech_with_includes(const std::filesystem::path& path, int depth = 4) {
  std::ifstream in{path, std::ios::binary};
  std::string out;
  std::string line;
  while (std::getline(in, line)) {
    const std::string_view word = trimmed_line(line);
    if (depth > 0 && word.starts_with("include ")) {
      std::filesystem::path included = path.parent_path() / std::string{trimmed_line(word.substr(8))};
      if (!std::filesystem::exists(included)) {
        included += ".tech";
      }
      if (std::filesystem::exists(included)) {
        out += tech_with_includes(included, depth - 1);
        continue;
      }
    }
    out += line + "\n";
  }
  return out;
}

/// A technology text split around one section: the text without the
/// section, where the first copy stood in it (the end when there is none)
/// and the section's body (every copy of a repeated section).
struct SectionSplit {
  std::string without;
  std::size_t insert_at = std::string::npos;
  std::string body;

  /// The text with `body` as the section, where the original one stood.
  [[nodiscard]] std::string with_body(std::string_view section, std::string_view new_body) const {
    const std::size_t at = std::min(insert_at, without.size());
    std::string text = without.substr(0, at);
    text.append(section).append("\n").append(new_body).append("\nend\n");
    text.append(without, at);
    return text;
  }
};

/// Splits `text` around its `section` sections (a section starts with a
/// lone word outside a section and ends with `end`).
inline SectionSplit split_section(const std::string& text, std::string_view section) {
  SectionSplit out;
  std::istringstream in{text};
  std::string line;
  bool in_section = false;
  bool in_wanted = false;
  while (std::getline(in, line)) {
    const std::string_view word = trimmed_line(line);
    const bool comment = word.empty() || word.front() == '#';
    if (!in_section && !comment && word.find_first_of(" \t") == std::string_view::npos) {
      in_section = true;
      in_wanted = word == section;
      if (in_wanted) {
        out.insert_at = std::min(out.insert_at, out.without.size());
        continue;
      }
    } else if (in_section && word == "end") {
      in_section = false;
      if (in_wanted) {
        in_wanted = false;
        continue;
      }
    }
    (in_wanted ? out.body : out.without) += line + "\n";
  }
  return out;
}

}  // namespace hefesto::fuzz
