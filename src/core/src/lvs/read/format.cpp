// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Format selection (lvs-netlist-input "Format selection"): the readnet
// keyword by unique prefix, and automatic detection by known suffix,
// appended suffix and first character.
//
// Owner: LVSREAD.

#include <array>
#include <string>

#include "hefesto/lvs/read/codes.hpp"
#include "hefesto/lvs/read/reader.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::lvs {

namespace {

/// A detected or named format: one Hefesto reads, or a netgen format it
/// does not (`supported` false).
struct FormatEntry {
  std::string_view word;
  std::optional<Format> format;
  bool supported;
};

constexpr std::array<FormatEntry, 11> kKeywords{{
    {"automatic", std::nullopt, true},
    {"ext", Format::ext, true},
    {"extflat", Format::ext, true},
    {"sim", Format::sim, true},
    {"prm", std::nullopt, false},
    {"ntk", std::nullopt, false},
    {"spice", Format::spice, true},
    {"verilog", Format::verilog, true},
    {"netgen", std::nullopt, false},
    {"actel", std::nullopt, false},
    {"xilinx", std::nullopt, false},
}};

/// The suffixes in detection order and what they select.
constexpr std::array<FormatEntry, 14> kSuffixes{{
    {".ntk", std::nullopt, false},
    {".ext", Format::ext, true},
    {".sim", Format::sim, true},
    {".prm", std::nullopt, false},
    {".spice", Format::spice, true},
    {".spc", Format::spice, true},
    {".sp", Format::spice, true},
    {".spi", Format::spice, true},
    {".fspc", Format::spice, true},
    {".cir", Format::spice, true},
    {".ckt", Format::spice, true},
    {".v", Format::verilog, true},
    {".sv", Format::verilog, true},
    {".ntg", std::nullopt, false},
}};

diag::Diagnostic unsupported(std::string_view what, std::string_view name) {
  return diag::make(read_codes::kBadFormat,
                    "netgen format '" + std::string{what} + "' of '" + std::string{name} + "' is not read by Hefesto");
}

diag::Diagnostic unknown_type(std::string_view name) {
  return diag::make(read_codes::kUnknownType, "ReadNetlist: don't know type of file '" + std::string{name} + "'");
}

diag::Diagnostic not_found(std::string_view name) {
  return diag::make(read_codes::kFileNotFound, "ReadNetlist: unable to find file '" + std::string{name} + "'");
}

const FormatEntry* known_suffix(std::string_view name) noexcept {
  for (const auto& entry : kSuffixes) {
    if (name.size() > entry.word.size() && name.ends_with(entry.word)) {
      return &entry;
    }
  }
  return nullptr;
}

diag::Outcome<Format> by_entry(const FormatEntry& entry, std::string_view name) {
  if (!entry.supported || !entry.format) {
    return unsupported(entry.word, name);
  }
  return *entry.format;
}

diag::Outcome<Format> by_first_character(std::string_view bytes, std::string_view name) {
  if (bytes.starts_with('*')) {
    return Format::spice;
  }
  if (bytes.starts_with('|')) {
    return Format::sim;
  }
  return unknown_type(name);
}

diag::Outcome<DetectedSource> detected(const Source& source, diag::Outcome<Format> format) {
  if (!format) {
    return format.error();
  }
  return DetectedSource{source, format.value()};
}

}  // namespace

diag::Outcome<Format> detect_format(const Source& source) {
  if (const auto* entry = known_suffix(source.name)) {
    return by_entry(*entry, source.name);
  }
  if (source.bytes) {
    return by_first_character(*source.bytes, source.name);
  }
  return unknown_type(source.name);
}

diag::Outcome<DetectedSource> resolve_source(const Source& source, platform::FileSystem& files) {
  if (source.bytes) {
    return detected(source, detect_format(source));
  }
  if (const auto* entry = known_suffix(source.name)) {
    if (!files.exists(source.name)) {
      return not_found(source.name);
    }
    return detected(source, by_entry(*entry, source.name));
  }
  for (const auto& entry : kSuffixes) {
    Source appended{source.name + std::string{entry.word}, std::nullopt};
    if (files.exists(appended.name)) {
      return detected(appended, by_entry(entry, appended.name));
    }
  }
  if (!files.exists(source.name)) {
    return not_found(source.name);
  }
  auto first = files.read_at_most(source.name, 1);
  if (!first) {
    return first.error();
  }
  return detected(source, by_first_character(first.value(), source.name));
}

diag::Outcome<std::optional<Format>> parse_format_keyword(std::string_view word) {
  const FormatEntry* match = nullptr;
  std::size_t matches = 0;
  for (const auto& entry : kKeywords) {
    if (entry.word == word) {
      match = &entry;
      matches = 1;
      break;
    }
    if (!word.empty() && entry.word.starts_with(word)) {
      match = &entry;
      ++matches;
    }
  }
  if (matches != 1) {
    return diag::make(read_codes::kBadFormat,
                      "'" + std::string{word} + "' is " + (matches == 0 ? "not" : "an ambiguous abbreviation of") +
                          " a netlist format");
  }
  if (!match->supported) {
    return unsupported(match->word, word);
  }
  return match->format;
}

}  // namespace hefesto::lvs
