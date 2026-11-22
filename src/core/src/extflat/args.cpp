// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EFArgs and efSymAdd (see hefesto/extflat/args.hpp; EFargs.c:100-345,
// EFsym.c, utils/args.c ArgStr).
//
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13).

#include "hefesto/extflat/args.hpp"

#include <algorithm>
#include <climits>
#include <utility>

#include "state.hpp"
#include "text.hpp"

namespace hefesto::extflat {

namespace {

constexpr std::string_view kUsage =
    "Standard arguments: [-R] [-C] [-r rthresh] [-c cthresh] [-v]\n"
    "[-p searchpath] [-s sym=value] [-S symfile] [-t trimchars]\n"
    "[rootfile]";

constexpr float kInfiniteCap = 1.0E38F;  // INFINITE_THRESHOLD_F

bool str_is_int(std::string_view s) noexcept {
  if (!s.empty() && (s.front() == '-' || s.front() == '+')) {
    s.remove_prefix(1);
  }
  return std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

std::vector<std::string> split_path(std::string_view path) {
  std::vector<std::string> out;
  std::size_t start = 0;
  for (;;) {
    const std::size_t colon = path.find(':', start);
    out.emplace_back(path.substr(start, colon == std::string_view::npos ? std::string_view::npos : colon - start));
    if (colon == std::string_view::npos) {
      return out;
    }
    start = colon + 1;
  }
}

diag::Diagnostic usage_error(std::string_view why) {
  std::string text{why};
  if (!text.empty()) {
    text += '\n';
  }
  text += kUsage;
  return diag::make(detail::codes::kBadArgument, std::move(text));
}

// The parse of one run.
class ArgParser {
 public:
  ArgParser(std::span<const std::string> args, const ExtraFlags& extra) : args_(args), extra_(extra) {}

  diag::Outcome<CommonArgs> parse();

 private:
  // ArgStr: the rest of the flag word, else the next word.
  std::optional<std::string> value(std::string_view what);
  bool flag(char c);
  void store(char c, std::string v);
  bool extra_flag();
  void add_symbol_arg(const std::string& text);

  std::span<const std::string> args_;
  const ExtraFlags& extra_;
  std::size_t i_ = 0;
  CommonArgs out_;
  std::map<std::string, std::int32_t, std::less<>> symbols_;
  std::optional<diag::Diagnostic> error_;
};

std::optional<std::string> ArgParser::value(std::string_view what) {
  const std::string& word = args_[i_];
  if (word.size() < 2) {
    error_ = usage_error("Bad argument " + word);
    return std::nullopt;
  }
  if (word.size() > 2) {
    std::size_t at = 2;
    while (at < word.size() && detail::c_isspace(word[at])) {
      ++at;
    }
    return word.substr(at);
  }
  if (i_ + 1 < args_.size()) {
    return args_[++i_];
  }
  error_ = usage_error(std::string{"-"} + word[1] + " requires a following " + std::string{what});
  return std::nullopt;
}

void ArgParser::add_symbol_arg(const std::string& text) {
  if (const auto rejected = add_symbol(symbols_, text)) {
    out_.messages.push_back(diag::make(detail::codes::kArgumentWarning, *rejected));
    return;
  }
  out_.symbols.push_back(text);
}

bool ArgParser::extra_flag() {
  if (!extra_) {
    error_ = usage_error("");
    return false;
  }
  const auto next = extra_(args_, i_);
  if (!next || *next <= i_) {
    error_ = usage_error("");
    return false;
  }
  i_ = *next - 1;
  return true;
}

// The ArgStr description of a flag that takes a value; null for the others.
const char* value_kind(char c) noexcept {
  switch (c) {
    case 'c': return "cap threshold";
    case 'p': return "search path";
    case 'r': return "resist threshold";
    case 's': return "symbolic name";
    case 't': return "trim characters";
    case 'S': return "symbol file";
    case 'T': return "tech name";
    case 'o': return "output file";
    case 'n': return "nodename";  // debugging: watched nodes are not reported
    case 'N': return "filename";
    default: return nullptr;
  }
}

// The value of a flag that takes one.
void ArgParser::store(char c, std::string v) {
  switch (c) {
    case 'c': out_.cap_threshold = static_cast<double>(static_cast<float>(detail::c_atof(v))); break;
    case 'p': out_.search_path = split_path(v); break;
    case 'r': out_.resist_threshold = detail::c_atoi(v); break;
    case 's': add_symbol_arg(v); break;
    case 't': out_.trim_chars += v; break;
    case 'S': out_.symbol_file = std::move(v); break;
    case 'T': out_.tech = std::move(v); break;
    case 'o': out_.output = std::move(v); break;
    default: break;  // -n, -N
  }
}

// One flag of EFArgs' switch; false when the parse stops.
bool ArgParser::flag(char c) {
  switch (c) {
    case 'C':
      out_.cap_infinite = true;
      out_.cap_threshold = static_cast<double>(kInfiniteCap);
      return true;
    case 'R':
      out_.resist_infinite = true;
      out_.resist_threshold = INT_MAX;
      return true;
    case 'v': out_.verbose = true; return true;
    case 'z': return true;
    case 'h':
      error_ = usage_error("");
      return false;
    default: break;
  }
  const char* kind = value_kind(c);
  if (kind == nullptr) {
    return extra_flag();
  }
  if (auto v = value(kind)) {
    store(c, std::move(*v));
  }
  return !error_;
}

diag::Outcome<CommonArgs> ArgParser::parse() {
  for (i_ = 0; i_ < args_.size(); ++i_) {
    const std::string& word = args_[i_];
    if (word.empty() || word[0] != '-') {
      if (out_.cell) {
        out_.messages.push_back(diag::make(detail::codes::kArgumentWarning,
                                           "Warning: multiple input files specified; ignoring \"" + *out_.cell + "\""));
      }
      out_.cell = word;
      continue;
    }
    if (!flag(word.size() > 1 ? word[1] : '\0')) {
      return *error_;
    }
  }
  if (out_.cell && out_.cell->size() >= 4) {
    const std::size_t dot = out_.cell->rfind('.');
    if (dot != std::string::npos && out_.cell->compare(dot, std::string::npos, ".ext") == 0) {
      out_.cell->resize(dot);
    }
  }
  return std::move(out_);
}

}  // namespace

diag::Outcome<CommonArgs> parse_common_args(std::span<const std::string> args, const ExtraFlags& extra) {
  return ArgParser{args, extra}.parse();
}

std::string_view common_args_usage() noexcept {
  return kUsage;
}

std::optional<std::string> add_symbol(std::map<std::string, std::int32_t, std::less<>>& symbols,
                                      std::string_view assignment) {
  const std::size_t eq = assignment.find('=');
  if (eq == std::string_view::npos) {
    return std::string{"Missing '=' in symbol assignment"};
  }
  const std::string_view value = assignment.substr(eq + 1);
  if (!str_is_int(value)) {
    return "Symbol value must be numeric; ignoring \"" + std::string{assignment} + "\"";
  }
  const std::string name{assignment.substr(0, eq)};
  if (symbols.count(name) != 0) {
    return "Symbol \"" + name + "\" already defined";
  }
  symbols.emplace(name, detail::c_atoi(value));
  return std::nullopt;
}

}  // namespace hefesto::extflat
