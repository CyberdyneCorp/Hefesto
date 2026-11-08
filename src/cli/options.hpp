// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-cli command-line options.

#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/format.hpp"

namespace hefesto::cli {

/// One script to run, in command-line order.
struct ScriptSource {
  enum class Kind { inline_text, file, standard_input };
  Kind kind = Kind::inline_text;
  std::string value;  // command text or file path

  friend bool operator==(const ScriptSource&, const ScriptSource&) = default;
};

struct Options {
  std::optional<std::string> tech;  // -T <tech>
  std::vector<ScriptSource> sources;
  diag::Format diagnostics = diag::Format::text;
  std::optional<std::filesystem::path> log_dir;
  bool no_log = false;
  bool show_help = false;
  bool show_version = false;
  bool show_codes = false;
};

struct ParsedArgs {
  Options options;
  std::optional<std::string> error;  // usage error text
};

/// Parses argv[1..]. Never exits or prints.
[[nodiscard]] ParsedArgs parse_args(std::span<const std::string_view> args);

/// The --help text.
[[nodiscard]] std::string usage_text();

}  // namespace hefesto::cli
