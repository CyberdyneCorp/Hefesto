// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Command-line splitting (ui-command-dispatch "Command line splitting and
// quoting") and command-file lines ("Script files and source", Q-UI-13).
// Owner: DISPATCH.
// (Magic ref: utils/parser.c:55-160)

#include "hefesto/ui/line.hpp"

#include <utility>

#include "hefesto/ui/diag_codes.hpp"

namespace hefesto::ui {

namespace {

bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

// The splitter's state over one line.
class Splitter {
 public:
  explicit Splitter(std::string_view line) : line_(line) {}

  SplitLine run() {
    skip_spaces();
    while (pos_ < line_.size()) {
      const char c = line_[pos_];
      if (c == ';') {
        end_command();
        ++pos_;
        skip_spaces();
        continue;
      }
      if (is_space(c)) {
        end_word();
        skip_spaces();
        continue;
      }
      in_word_ = true;
      if (c == '"' || c == '\'') {
        quoted_span(c);
      } else {
        word_ += c;
        ++pos_;
      }
    }
    end_command();
    return std::move(result_);
  }

 private:
  void skip_spaces() {
    while (pos_ < line_.size() && is_space(line_[pos_])) {
      ++pos_;
    }
  }

  void quoted_span(char quote) {
    ++pos_;
    while (pos_ < line_.size() && line_[pos_] != quote) {
      if (line_[pos_] == '\\' && pos_ + 1 < line_.size()) {
        ++pos_;
      }
      word_ += line_[pos_++];
    }
    if (pos_ < line_.size()) {
      ++pos_;
    } else if (!result_.unmatched_quote) {
      result_.unmatched_quote = quote;
    }
  }

  void end_word() {
    if (in_word_) {
      words_.push_back(std::move(word_));
      word_.clear();
      in_word_ = false;
    }
  }

  void end_command() {
    end_word();
    if (!words_.empty()) {
      result_.commands.push_back(std::move(words_));
      words_.clear();
    }
  }

  std::string_view line_;
  std::size_t pos_ = 0;
  std::string word_;
  bool in_word_ = false;
  std::vector<std::string> words_;
  SplitLine result_;
};

}  // namespace

SplitLine split_command_line(std::string_view line) {
  return Splitter{line}.run();
}

diag::Diagnostic unmatched_quote_warning(char quote) {
  return diag::make(codes::kUnmatchedQuote,
                    std::string{"Unmatched "} + quote + " in string, I'll pretend that there is one at the end");
}

std::vector<cmd::CommandResult> run_command_line(cmd::Dispatcher& dispatcher, std::string_view line,
                                                 cmd::Origin origin) {
  const SplitLine split = split_command_line(line);
  if (split.unmatched_quote) {
    dispatcher.report(unmatched_quote_warning(*split.unmatched_quote));
  }
  std::vector<cmd::CommandResult> results;
  for (const auto& words : split.commands) {
    cmd::Invocation inv{words.front(), {words.begin() + 1, words.end()}, origin, std::nullopt};
    results.push_back(dispatcher.execute(inv));
  }
  return results;
}

std::vector<std::string> command_file_lines(std::string_view text) {
  std::vector<std::string> out;
  std::string current;
  std::size_t pos = 0;
  while (pos <= text.size()) {
    const std::size_t nl = text.find('\n', pos);
    std::string_view line = text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
    pos = nl == std::string_view::npos ? text.size() + 1 : nl + 1;
    if (!current.empty()) {
      const std::size_t first = line.find_first_not_of(" \t");
      line = first == std::string_view::npos ? std::string_view{} : line.substr(first);
    }
    if (line.ends_with('\\')) {
      current += line.substr(0, line.size() - 1);
      current += ' ';
      continue;
    }
    current += line;
    const std::size_t first = current.find_first_not_of(" \t\r");
    if (first != std::string::npos && current[first] != '#') {
      std::string_view kept = std::string_view{current}.substr(first);
      if (kept.starts_with(':') && !kept.starts_with("::")) {
        kept.remove_prefix(1);
      }
      out.emplace_back(kept);
    }
    current.clear();
  }
  return out;
}

}  // namespace hefesto::ui
