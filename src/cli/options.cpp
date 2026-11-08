// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "options.hpp"

#include <utility>

namespace hefesto::cli {

namespace {

class ArgParser {
 public:
  explicit ArgParser(std::span<const std::string_view> args) : args_(args) {}

  ParsedArgs run() {
    while (!result_.error && index_ < args_.size()) {
      const std::string_view arg = args_[index_++];
      if (options_ended_ || arg == "-" || arg.empty() || arg.front() != '-') {
        add_positional(arg);
      } else {
        parse_option(arg);
      }
    }
    return std::move(result_);
  }

 private:
  void add_positional(std::string_view arg) {
    using Kind = ScriptSource::Kind;
    const Kind kind = arg == "-" ? Kind::standard_input : Kind::file;
    result_.options.sources.push_back({kind, std::string{arg}});
  }

  // Value of an option given as "--name=value" or "--name value" / "-x value".
  std::optional<std::string_view> value_of(std::string_view arg, std::string_view name) {
    if (arg.size() > name.size() && arg[name.size()] == '=') {
      return arg.substr(name.size() + 1);
    }
    if (index_ >= args_.size()) {
      fail(std::string{name} + " needs an argument");
      return std::nullopt;
    }
    return args_[index_++];
  }

  [[nodiscard]] static bool is_option(std::string_view arg, std::string_view name) {
    return arg == name || (arg.size() > name.size() && arg.substr(0, name.size()) == name && arg[name.size()] == '=');
  }

  void parse_option(std::string_view arg) {
    if (parse_flag(arg)) {
      return;
    }
    if (is_option(arg, "-T")) {
      set_value(arg, "-T", [this](std::string_view v) { result_.options.tech = std::string{v}; });
    } else if (is_option(arg, "-c")) {
      set_value(arg, "-c", [this](std::string_view v) {
        result_.options.sources.push_back({ScriptSource::Kind::inline_text, std::string{v}});
      });
    } else if (is_option(arg, "--diagnostics")) {
      set_value(arg, "--diagnostics", [this](std::string_view v) { set_format(v); });
    } else if (is_option(arg, "--log-dir")) {
      set_value(arg, "--log-dir", [this](std::string_view v) { result_.options.log_dir = std::filesystem::path{v}; });
    } else {
      fail("unknown option '" + std::string{arg} + "'");
    }
  }

  // Options without an argument; false when `arg` is not one of them.
  bool parse_flag(std::string_view arg) {
    auto& o = result_.options;
    if (arg == "--help" || arg == "-h") {
      o.show_help = true;
    } else if (arg == "--version") {
      o.show_version = true;
    } else if (arg == "--diagnostic-codes") {
      o.show_codes = true;
    } else if (arg == "--no-log") {
      o.no_log = true;
    } else if (arg == "--") {
      options_ended_ = true;
    } else {
      return false;
    }
    return true;
  }

  template <typename Apply>
  void set_value(std::string_view arg, std::string_view name, Apply apply) {
    if (const auto value = value_of(arg, name)) {
      apply(*value);
    }
  }

  void set_format(std::string_view value) {
    if (const auto fmt = diag::parse_format(value)) {
      result_.options.diagnostics = *fmt;
    } else {
      fail("--diagnostics expects text or json, got '" + std::string{value} + "'");
    }
  }

  void fail(std::string text) {
    if (!result_.error) {
      result_.error = std::move(text);
    }
  }

  std::span<const std::string_view> args_;
  std::size_t index_ = 0;
  bool options_ended_ = false;
  ParsedArgs result_;
};

}  // namespace

ParsedArgs parse_args(std::span<const std::string_view> args) {
  return ArgParser{args}.run();
}

std::string usage_text() {
  return "usage: hefesto-cli [options] [script ...]\n"
         "\n"
         "Runs Hefesto command scripts headless. Scripts and -c commands run in\n"
         "command-line order; '-' reads a script from standard input.\n"
         "\n"
         "options:\n"
         "  -T <tech>                 technology to load (recorded; loading arrives in M1)\n"
         "  -c <commands>             run commands, e.g. -c \"echo hi; version\"\n"
         "  --diagnostics=text|json   diagnostic format on stderr (default text)\n"
         "  --log-dir=<dir>           session log directory (default: app storage)\n"
         "  --no-log                  do not write a session log\n"
         "  --diagnostic-codes        print the diagnostic code table (Markdown)\n"
         "  --version                 print the version and exit\n"
         "  -h, --help                print this help and exit\n"
         "\n"
         "exit status: 0 success, 1 a command or script failed, 2 usage error\n";
}

}  // namespace hefesto::cli
