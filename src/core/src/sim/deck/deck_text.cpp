// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: SIMDECK. See deck_text.hpp.

#include "sim/deck/deck_text.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <utility>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/sim/deck/spice_text.hpp"

namespace hefesto::sim::deck {

namespace {

constexpr std::array<std::string_view, 10> kForbidden = {"shell", "cd",     "codemodel", "osdi",   "pre_osdi",
                                                         "load",  "source", "write",     "wrdata", "quit"};
constexpr std::array<std::string_view, 10> kAnalysisCards = {".op", ".dc", ".ac",  ".tran", ".noise",
                                                             ".tf", ".pz", ".sp", ".pss",  ".disto"};

bool is_blank_or_comment(std::string_view line) {
  const std::size_t at = line.find_first_not_of(" \t\r");
  return at == std::string_view::npos || line[at] == '*';
}

bool is_continuation(std::string_view line) {
  const std::size_t at = line.find_first_not_of(" \t");
  return at != std::string_view::npos && line[at] == '+';
}

std::string where(std::string_view source, std::size_t index) {
  return std::string{source} + ":" + std::to_string(index + 1);
}

std::vector<std::string> split_text(const std::string& text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start < text.size()) {
    std::size_t end = text.find('\n', start);
    if (end == std::string::npos) {
      end = text.size();
    }
    std::string line = text.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    out.push_back(std::move(line));
    start = end + 1;
  }
  return out;
}

// The file of an `.include` line: a quoted path, or the first token.
std::string include_path(std::string_view line) {
  std::string_view rest = line;
  const std::size_t card_end = rest.find_first_of(" \t", rest.find_first_not_of(" \t"));
  rest = card_end == std::string_view::npos ? std::string_view{} : rest.substr(card_end);
  const std::size_t at = rest.find_first_not_of(" \t");
  if (at == std::string_view::npos) {
    return {};
  }
  rest.remove_prefix(at);
  if (rest.front() == '"' || rest.front() == '\'') {
    const std::size_t close = rest.find(rest.front(), 1);
    return std::string{rest.substr(1, close == std::string_view::npos ? std::string_view::npos : close - 1)};
  }
  return std::string{rest.substr(0, rest.find_first_of(" \t"))};
}

class Inliner {
 public:
  explicit Inliner(const IncludeContext& context) : context_(context) {}

  diag::Status walk(const std::vector<std::string>& lines, const std::filesystem::path& dir, std::string_view name,
                    std::size_t depth) {
    for (std::size_t i = 0; i < lines.size(); ++i) {
      const std::string card = card_of(lines[i]);
      diag::Status status = card == ".include" || card == ".inc" ? include(lines[i], dir, where(name, i), depth)
                                                                  : add(lines[i], name);
      if (!status.ok()) {
        return status;
      }
    }
    return diag::Done{};
  }

  std::vector<std::string> take() { return std::move(out_); }

 private:
  diag::Status include(std::string_view line, const std::filesystem::path& dir, const std::string& at,
                       std::size_t depth) {
    const std::string file = include_path(line);
    if (file.empty()) {
      return diag::make("SIM-DECK-001", at + ": .include names no file");
    }
    std::filesystem::path path{file};
    if (path.is_relative()) {
      path = dir / path;
    }
    path = path.lexically_normal();
    if (depth + 1 > context_.limits.max_include_depth) {
      return diag::make("SIM-DECK-002", "include depth limit " + std::to_string(context_.limits.max_include_depth) +
                                            " exceeded by " + path.generic_string() + " (" + at + ")");
    }
    auto text = context_.files->read_at_most(path, context_.limits.max_deck_bytes + 1);
    if (!text.ok()) {
      return diag::make("SIM-DECK-001", "File not found: " + path.generic_string() + " (" + at + ")");
    }
    const std::string name = path.generic_string();
    if (diag::Status s = add("* hefesto include " + name, name); !s.ok()) {
      return s;
    }
    if (diag::Status s = walk(split_text(text.value()), path.parent_path(), name, depth + 1); !s.ok()) {
      return s;
    }
    return add("* hefesto end include", name);
  }

  diag::Status add(std::string line, std::string_view name) {
    bytes_ += line.size() + 1;
    if (bytes_ > context_.limits.max_deck_bytes) {
      return diag::make("SIM-DECK-003", "prepared deck larger than the limit of " +
                                            std::to_string(context_.limits.max_deck_bytes) + " bytes in " +
                                            std::string{name});
    }
    out_.push_back(std::move(line));
    return diag::Done{};
  }

  const IncludeContext& context_;
  std::vector<std::string> out_;
  std::size_t bytes_ = 0;
};

// The last line of the `.control` section that starts at `from`.
std::size_t control_end(const std::vector<std::string>& lines, std::size_t from) {
  for (std::size_t i = from + 1; i < lines.size(); ++i) {
    if (card_of(lines[i]) == ".endc") {
      return i;
    }
  }
  return lines.size() - 1;
}

std::optional<std::string> forbidden_command(const std::vector<std::string>& lines, std::size_t from,
                                             std::size_t to) {
  for (std::size_t i = from + 1; i < to; ++i) {
    const std::string command = card_of(lines[i]);
    if (std::find(kForbidden.begin(), kForbidden.end(), command) != kForbidden.end()) {
      return command;
    }
  }
  return std::nullopt;
}

// The subcircuit an `X` instance calls: its last token that is not a
// parameter (`w=1u`, or anything after `params:`).
std::string called_subckt(std::string_view text) {
  std::string last;
  std::string token;
  std::size_t count = 0;
  const auto flush = [&] {
    if (!token.empty()) {
      const std::string lower = ascii_lower(token);
      if (lower == "params:") {
        count = 1U << 30U;
      } else if (token.find('=') == std::string::npos && count < (1U << 30U)) {
        last = lower;
        ++count;
      }
      token.clear();
    }
  };
  for (const char c : text) {
    if (c == ' ' || c == '\t') {
      flush();
    } else {
      token.push_back(c);
    }
  }
  flush();
  return count >= 2 ? last : std::string{};
}

struct Structure {
  std::vector<std::string> defined;
  std::vector<std::pair<std::size_t, std::string>> calls;  // line, subckt
  std::vector<std::pair<std::size_t, std::string>> open;   // .subckt stack
  bool has_lib = false;
  bool has_analysis = false;
};

std::optional<diag::Diagnostic> scan_card(const LogicalLine& line, Structure& s) {
  const std::string card = card_of(line.text);
  const std::vector<std::string> tokens = spice_tokens(line.text);
  if (card == ".subckt") {
    const std::string name = tokens.size() > 1 ? ascii_lower(tokens[1]) : std::string{};
    s.defined.push_back(name);
    s.open.emplace_back(line.first, name);
  } else if (card == ".ends") {
    if (s.open.empty()) {
      return diag::make("SIM-DECK-006", ".ends at line " + std::to_string(line.first + 1) + " has no matching .subckt");
    }
    s.open.pop_back();
  } else if (card == ".lib") {
    s.has_lib = true;
  } else if (std::find(kAnalysisCards.begin(), kAnalysisCards.end(), card) != kAnalysisCards.end()) {
    s.has_analysis = true;
  } else if (card.starts_with("x")) {
    s.calls.emplace_back(line.first, called_subckt(line.text));
  }
  return std::nullopt;
}

diag::Status check_calls(const Structure& s) {
  if (s.has_lib) {
    return diag::Done{};  // PDK subcircuits are exempt and resolved by the engine
  }
  for (const auto& [index, name] : s.calls) {
    if (std::find(s.defined.begin(), s.defined.end(), name) == s.defined.end()) {
      return diag::make("SIM-DECK-007", "undefined subcircuit '" + name + "' called at line " +
                                            std::to_string(index + 1));
    }
  }
  return diag::Done{};
}

diag::Status check_raw(const std::vector<std::string>& lines, const Structure& s) {
  if (!s.has_analysis) {
    return diag::make("SIM-DECK-008", "Deck has no analysis");
  }
  for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
    if (!is_blank_or_comment(*it)) {
      if (card_of(*it) == ".end") {
        return diag::Done{};
      }
      break;
    }
  }
  return diag::make("SIM-DECK-009", "Deck does not end with .end");
}

}  // namespace

std::string card_of(std::string_view text) {
  const std::size_t start = text.find_first_not_of(" \t");
  if (start == std::string_view::npos) {
    return {};
  }
  const std::size_t end = text.find_first_of(" \t\r", start);
  return ascii_lower(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
}

std::vector<LogicalLine> logical_lines(const std::vector<std::string>& lines, std::size_t from) {
  std::vector<LogicalLine> out;
  for (std::size_t i = from; i < lines.size(); ++i) {
    if (is_blank_or_comment(lines[i])) {
      continue;
    }
    if (is_continuation(lines[i]) && !out.empty()) {
      const std::string& line = lines[i];
      out.back().text += " " + line.substr(line.find('+') + 1);
      out.back().last = i;
      continue;
    }
    out.push_back(LogicalLine{i, i, lines[i]});
  }
  return out;
}

diag::Outcome<std::vector<std::string>> inline_includes(const std::vector<std::string>& lines,
                                                        const IncludeContext& context) {
  Inliner inliner{context};
  if (diag::Status s = inliner.walk(lines, std::filesystem::path{context.base_dir}, context.source, 0); !s.ok()) {
    return s.error();
  }
  return inliner.take();
}

diag::Status sanitise(std::vector<std::string>& lines, bool allow_control, std::vector<diag::Diagnostic>& warnings) {
  std::vector<std::string> out;
  out.reserve(lines.size());
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (card_of(lines[i]) != ".control") {
      out.push_back(std::move(lines[i]));
      continue;
    }
    const std::size_t end = control_end(lines, i);
    if (const auto command = forbidden_command(lines, i, end)) {
      return diag::make("SIM-DECK-004", "Control command '" + *command + "' is not allowed in simulation decks");
    }
    if (allow_control) {
      for (std::size_t j = i; j <= end; ++j) {
        out.push_back(std::move(lines[j]));
      }
    } else {
      diag::Diagnostic warning = diag::make("SIM-DECK-005", "Removed .control section (lines " +
                                                                std::to_string(i + 1) + "-" + std::to_string(end + 1) +
                                                                ")");
      warning.severity = diag::Severity::warning;
      warnings.push_back(std::move(warning));
    }
    i = end;
  }
  lines = std::move(out);
  return diag::Done{};
}

diag::Status validate(const std::vector<std::string>& lines, bool raw) {
  Structure s;
  bool control = false;
  for (const LogicalLine& line : logical_lines(lines, 1)) {
    const std::string card = card_of(line.text);
    if (card == ".control" || card == ".endc") {
      control = card == ".control";
      continue;
    }
    if (control) {
      continue;
    }
    if (auto error = scan_card(line, s)) {
      return *error;
    }
  }
  if (!s.open.empty()) {
    return diag::make("SIM-DECK-006", ".subckt " + s.open.back().second + " at line " +
                                          std::to_string(s.open.back().first + 1) + " is missing its .ends");
  }
  if (diag::Status calls = check_calls(s); !calls.ok()) {
    return calls;
  }
  return raw ? check_raw(lines, s) : diag::Status{diag::Done{}};
}

std::vector<std::string> split_long_lines(const std::vector<std::string>& lines) {
  std::vector<std::string> out;
  out.reserve(lines.size());
  for (const std::string& line : lines) {
    std::string_view rest = line;
    std::string_view prefix;
    while (prefix.size() + rest.size() > kMaxDeliveredLine) {
      const std::size_t room = kMaxDeliveredLine - prefix.size();
      std::size_t cut = rest.rfind(' ', room);
      if (cut == std::string_view::npos || cut == 0) {
        cut = room;
      }
      out.push_back(std::string{prefix} + std::string{rest.substr(0, cut)});
      rest.remove_prefix(cut);
      prefix = "+";
    }
    out.push_back(std::string{prefix} + std::string{rest});
  }
  return out;
}

std::size_t deck_bytes(const std::vector<std::string>& lines) {
  std::size_t n = 0;
  for (const std::string& line : lines) {
    n += line.size() + 1;
  }
  return n;
}

}  // namespace hefesto::sim::deck
