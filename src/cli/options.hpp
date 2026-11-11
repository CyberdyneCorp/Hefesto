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
#include <utility>
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
  /// Layout files named on the command line (`.mag`, `.lef`, `.def`, `.cif`,
  /// `.gds`), loaded at startup in command-line order (app-startup
  /// "Command-line file arguments"); other words are scripts.
  std::vector<std::string> files;
  /// --magic: the remaining words are Magic's command line (option letters,
  /// file arguments, `.tcl` script), parsed by app::parse_startup_options;
  /// commands are then read from standard input.
  bool magic_mode = false;
  /// --interactive: after the scripts, read commands line by line from
  /// standard input at a prompt, with the background DRC checker running
  /// while it waits (m3-drc design D8). Implied in --magic mode without
  /// -c or scripts when standard input is a terminal.
  bool interactive = false;
  std::vector<std::string> magic_args;
  std::optional<std::filesystem::path> project;   // --project=<dir> (default: . when it has hefesto.project)
  std::optional<std::filesystem::path> pdk_root;  // --pdk-root=<dir>
  std::vector<std::pair<std::string, std::string>> settings;  // --setting <name>=<value> (session layer)
  diag::Format diagnostics = diag::Format::text;
  std::optional<std::filesystem::path> log_dir;
  bool no_log = false;
  bool show_help = false;
  bool show_version = false;
  bool show_codes = false;
  std::string codes_area = "core";  // --diagnostic-codes[=<area>]
};

struct ParsedArgs {
  Options options;
  std::optional<std::string> error;  // usage error text
};

/// Parses argv[1..]. Never exits or prints.
[[nodiscard]] ParsedArgs parse_args(std::span<const std::string_view> args);

/// True for a word that names a layout file rather than a script.
[[nodiscard]] bool is_layout_file(std::string_view word);

/// The --help text.
[[nodiscard]] std::string usage_text();

}  // namespace hefesto::cli
