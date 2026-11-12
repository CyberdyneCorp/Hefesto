// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "options.hpp"

#include <algorithm>
#include <cctype>
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
    if (is_layout_file(arg)) {
      result_.options.files.emplace_back(arg);
      return;
    }
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
    } else if (is_option(arg, "--project")) {
      set_value(arg, "--project", [this](std::string_view v) { result_.options.project = std::filesystem::path{v}; });
    } else if (is_option(arg, "--pdk-root")) {
      set_value(arg, "--pdk-root", [this](std::string_view v) { result_.options.pdk_root = std::filesystem::path{v}; });
    } else if (is_option(arg, "--setting")) {
      set_value(arg, "--setting", [this](std::string_view v) { add_setting(v); });
    } else if (is_option(arg, "--trust-pdk")) {
      set_value(arg, "--trust-pdk", [this](std::string_view v) { result_.options.trust_pdk.emplace_back(v); });
    } else if (is_option(arg, "--strip-reflibs")) {
      set_value(arg, "--strip-reflibs",
                [this](std::string_view v) { result_.options.strip_reflibs = std::filesystem::path{v}; });
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
    } else if (arg.starts_with("--diagnostic-codes=")) {
      o.show_codes = true;
      o.codes_area = std::string{arg.substr(std::string_view{"--diagnostic-codes="}.size())};
    } else if (arg == "--interactive") {
      o.interactive = true;
    } else if (arg == "--no-log") {
      o.no_log = true;
    } else if (arg == "--") {
      options_ended_ = true;
    } else if (arg == "--magic") {
      o.magic_mode = true;
      o.magic_args.assign(args_.begin() + static_cast<std::ptrdiff_t>(index_), args_.end());
      index_ = args_.size();
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
      fail("--diagnostics expects text, json or magic, got '" + std::string{value} + "'");
    }
  }

  void add_setting(std::string_view text) {
    const auto equals = text.find('=');
    if (equals == std::string_view::npos || equals == 0) {
      fail("--setting expects <name>=<value>, got '" + std::string{text} + "'");
      return;
    }
    result_.options.settings.emplace_back(std::string{text.substr(0, equals)}, std::string{text.substr(equals + 1)});
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

bool is_layout_file(std::string_view word) {
  const auto dot = word.rfind('.');
  if (dot == std::string_view::npos || word.find('/', dot) != std::string_view::npos) {
    return false;
  }
  std::string ext{word.substr(dot)};
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext == ".mag" || ext == ".lef" || ext == ".def" || ext == ".cif" || ext == ".gds";
}

ParsedArgs parse_args(std::span<const std::string_view> args) {
  return ArgParser{args}.run();
}

std::string usage_text() {
  return "usage: hefesto-cli [options] [script | layout-file ...]\n"
         "       hefesto-cli [options] --magic [magic options] [file ...]\n"
         "\n"
         "Runs Hefesto command scripts headless. Startup (settings, technology,\n"
         "layout files) runs first; then scripts and -c commands run in\n"
         "command-line order; '-' reads a script from standard input. Words\n"
         "ending in .mag, .lef, .def, .cif or .gds are layout files.\n"
         "\n"
         "options:\n"
         "  -T <tech>                 technology to load at startup (name or .tech path)\n"
         "  --project=<dir>           project folder (default: . when it has hefesto.project)\n"
         "  --pdk-root=<dir>          PDK root (default: setting pdk-root, $PDK_ROOT, ~/.ciel)\n"
         "  --setting <name>=<value>  session setting (highest settings layer)\n"
         "  --trust-pdk <selector>    let this PDK's scripts run (rc files, device scripts);\n"
         "                            a family (sky130), variant (sky130A), variant@hash or\n"
         "                            family/variant@version; repeatable. Batch runs decline\n"
         "                            every other PDK script unless the PDK store trusts it\n"
         "  --magic ...               Magic's command line: -T, -d, -rcfile, -norcfile, -r,\n"
         "                            cells and a .tcl script, evaluated as Tcl in the\n"
         "                            sandboxed interpreter; Tcl commands from standard input\n"
         "  -c <commands>             run commands, e.g. -c \"echo hi; version\"\n"
         "  --interactive             then read commands from standard input at a prompt\n"
         "                            ('%' idle, ']' while background DRC is busy); implied\n"
         "                            by --magic on a terminal without -c or scripts\n"
         "  --diagnostics=text|json|magic\n"
         "                            diagnostic format on stderr (default text); magic prints\n"
         "                            Magic's console text and each script command's result\n"
         "                            (golden replay against Magic)\n"
         "  --strip-reflibs <dir>     remove the GDS_START/GDS_END lines of every .mag\n"
         "                            file in <dir> (reference-library stripper) and exit\n"
         "  --log-dir=<dir>           session log directory (default: app storage)\n"
         "  --no-log                  do not write a session log\n"
         "  --diagnostic-codes[=AREA] print a diagnostic code table (Markdown); AREA is\n"
         "                            core (default), tech, db, cell, display, view, pdk,\n"
         "                            app, undo, select, connect, ui, wiring, drc, script,\n"
         "                            pcell, tools, gui or hints\n"
         "  --version                 print the version and exit\n"
         "  -h, --help                print this help and exit\n"
         "\n"
         "exit status: 0 success, 1 a command or script failed, 2 usage error\n";
}

}  // namespace hefesto::cli
