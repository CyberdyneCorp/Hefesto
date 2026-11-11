// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// (Magic ref: tcltk/readspice.tcl:58-98)

#include "hefesto/tools/spice_lines.hpp"

namespace hefesto::tools {

namespace {

// Tcl's default `split` characters (Tcl_SplitObjCmd: " \n\t\r").
constexpr std::string_view kWhitespace = " \n\t\r";
// The Spectre re-split of readspice.tcl:95.
constexpr std::string_view kSpectreSeparators = " \t()";

// readspice.tcl:62-66: a CDL `*.PININFO` comment loses its `*`.
bool is_cdl_pininfo(std::string_view line) {
  return line.size() >= 2 && line.substr(0, 2) == "*." && ascii_lower(line.substr(2, 7)) == "pininfo";
}

}  // namespace

std::string ascii_lower(std::string_view text) {
  std::string out{text};
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return out;
}

std::vector<std::string> tcl_split(std::string_view text, std::string_view separators) {
  std::vector<std::string> out;
  if (text.empty()) {
    return out;  // Tcl: splitting an empty string gives an empty list
  }
  std::size_t start = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (separators.find(text[i]) != std::string_view::npos) {
      out.emplace_back(text.substr(start, i - start));
      start = i + 1;
    }
  }
  out.emplace_back(text.substr(start));
  return out;
}

SpiceLine tokenize_spice_line(std::string_view line) {
  SpiceLine out;
  out.tokens = tcl_split(line, kWhitespace);
  out.keyword = out.tokens.empty() ? std::string{} : ascii_lower(out.tokens.front());
  if (out.keyword == "inline" && out.tokens.size() > 1 && ascii_lower(out.tokens[1]) == "subckt") {
    std::vector<std::string> spectre = tcl_split(line, kSpectreSeparators);
    out.tokens.assign(spectre.begin() + 1, spectre.end());
    out.keyword = ".subckt";
  }
  return out;
}

SpiceLineReader::SpiceLineReader(std::string_view text, ContinuationMode mode, bool cdl)
    : text_(text), mode_(mode), cdl_(cdl) {}

std::optional<std::string_view> SpiceLineReader::physical_line() {
  if (position_ >= text_.size()) {
    return std::nullopt;
  }
  const std::size_t end = text_.find_first_of("\r\n", position_);
  if (end == std::string_view::npos) {
    const std::string_view line = text_.substr(position_);
    position_ = text_.size();
    return line;
  }
  const std::string_view line = text_.substr(position_, end - position_);
  position_ = end + 1;
  if (text_[end] == '\r' && position_ < text_.size() && text_[position_] == '\n') {
    ++position_;
  }
  return line;
}

void SpiceLineReader::append_continuation(std::string_view line) {
  const bool space_first = mode_ == ContinuationMode::magic ? line.back() != ' '
                                                            : pending_.empty() || pending_.back() != ' ';
  if (space_first) {
    pending_ += ' ';
  }
  pending_ += line.substr(1);
}

std::optional<std::string> SpiceLineReader::next() {
  while (const std::optional<std::string_view> physical = physical_line()) {
    std::string_view line = *physical;
    if (cdl_ && is_cdl_pininfo(line)) {
      line.remove_prefix(1);
    }
    if (!line.empty() && line.front() == '*') {
      continue;  // a comment never breaks a continuation
    }
    if (!line.empty() && line.front() == '+') {
      append_continuation(line);
      continue;
    }
    std::string done = std::move(pending_);
    pending_ = std::string{line};
    return done;
  }
  if (finished_) {
    return std::nullopt;
  }
  finished_ = true;
  return std::move(pending_);
}

}  // namespace hefesto::tools
