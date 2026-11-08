// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/diag/codes.hpp"

#include <algorithm>
#include <array>

namespace hefesto::diag {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kTable = {
    CodeInfo{"CLI-LOG-001", warning, "The session log file cannot be opened; the run continues without it."},
    CodeInfo{"CLI-SCRIPT-001", error, "A script file named on the command line cannot be read."},
    CodeInfo{"CLI-TECH-001", warning, "`-T <tech>` was recorded but technology loading is not implemented yet (M1)."},
    CodeInfo{"CLI-USAGE-001", error, "Invalid command-line option or option argument."},
    CodeInfo{"CMD-DISPATCH-001", error, "Unknown command name; the command was not run."},
    CodeInfo{"CMD-INTERNAL-001", error, "A command handler raised an unexpected internal error; the command failed."},
    CodeInfo{"CMD-PARSE-001", error, "Command script has an unterminated double quote."},
    CodeInfo{"CMD-PARSE-002", error, "Command script has an unterminated brace group."},
    CodeInfo{"CMD-PARSE-003", error, "Extra characters after a closing quote or brace in a command script."},
    CodeInfo{"CMD-REGISTRY-001", error, "A command with the same name is already registered."},
    CodeInfo{"CMD-USAGE-001", error, "Wrong number or form of command arguments."},
    CodeInfo{"PLATFORM-IO-001", error, "A file cannot be read."},
    CodeInfo{"PLATFORM-IO-002", error, "A file cannot be written."},
    CodeInfo{"PLATFORM-IO-003", error, "A directory cannot be created, listed or removed."},
    CodeInfo{"PLATFORM-IO-004", error, "A file or directory does not exist."},
    CodeInfo{"PLATFORM-LOG-001", warning, "The session log cannot be opened; the session continues without it."},
    CodeInfo{"PLATFORM-PDK-001", error, "A technology was not found in any PDK or technology root."},
    CodeInfo{"PLATFORM-PROCESS-001", error, "An external process could not be started or waited for."},
    CodeInfo{"PLATFORM-UNSUPPORTED-001", error, "The requested platform service is not available on this platform."},
    CodeInfo{"SETTINGS-FILE-001", error, "A project settings file is malformed."},
    CodeInfo{"SETTINGS-NAME-001", error, "Unknown project setting name."},
    CodeInfo{"SETTINGS-VALUE-001", error, "Invalid value for a project setting."},
};

constexpr bool is_upper_alnum(char c) noexcept {
  return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

constexpr bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

// True when `part` is non-empty and made of upper-case letters and digits.
constexpr bool is_word(std::string_view part) noexcept {
  return !part.empty() && std::all_of(part.begin(), part.end(), is_upper_alnum);
}

}  // namespace

std::span<const CodeInfo> code_table() noexcept {
  return kTable;
}

std::optional<CodeInfo> find_code(std::string_view code) noexcept {
  const auto it = std::lower_bound(kTable.begin(), kTable.end(), code,
                                    [](const CodeInfo& info, std::string_view c) { return info.code < c; });
  if (it == kTable.end() || it->code != code) {
    return std::nullopt;
  }
  return *it;
}

bool is_well_formed_code(std::string_view code) noexcept {
  const auto first = code.find('-');
  const auto last = code.rfind('-');
  if (first == std::string_view::npos || first == last) {
    return false;
  }
  const auto number = code.substr(last + 1);
  return is_word(code.substr(0, first)) && is_word(code.substr(first + 1, last - first - 1)) &&
         number.size() == 3 && std::all_of(number.begin(), number.end(), is_digit);
}

std::string render_code_table_markdown() {
  std::string out = "| Code | Default severity | Meaning |\n|---|---|---|\n";
  for (const auto& info : kTable) {
    out += "| `";
    out += info.code;
    out += "` | ";
    out += severity_name(info.severity);
    out += " | ";
    out += info.summary;
    out += " |\n";
  }
  return out;
}

}  // namespace hefesto::diag
