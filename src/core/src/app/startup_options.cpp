// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Command-line options, file arguments and display selection (app-startup
// "Command-line options", "Command-line file arguments", "Display type
// selection"). The option letters, their prefix rules and the messages
// follow Magic 8.3.694 (fc5daa9e) utils/main.c:253-510 (mainDoArgs,
// mainArg), utils/args.c:58-94 (ArgStr) and graphics/grMain.c:102-146,
// 246-328 (grDisplayTypes, GrSetDisplay); Copyright (C) 1985, 1990
// Regents of the University of California, see THIRD_PARTY_NOTICES.md.
// File arguments follow Q-UI-25 (kept with -nowindow, exact extensions).

#include <algorithm>
#include <array>
#include <cctype>

#include "hefesto/app/diag_codes.hpp"
#include "hefesto/app/startup.hpp"

namespace hefesto::app {

namespace {

constexpr std::string_view kUsage =
    "Usage:  magic [-g gPort] [-d devType] [-m monType] [-i tabletPort] [-D] [-F objFile saveFile]\n"
    "[-T technology] [-rcfile startupFile | -norcfile][-noconsole] [-nowindow] [-wrapper] [file]";

// grDisplayTypes, in Magic's table order.
constexpr std::array<std::string_view, 11> kDisplayTypes = {"XWIND",   "X11", "8BIT",   "16BIT", "24BIT", "OPEN_GL",
                                                             "OGL",     "OPENGL", "CAIRO", "XR",    "NULL"};

std::string lower(std::string_view text) {
  std::string out{text};
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

std::string upper(std::string_view text) {
  std::string out{text};
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return out;
}

std::string_view skip_space(std::string_view text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
    text.remove_prefix(1);
  }
  return text;
}

FileKind kind_of_extension(std::string_view extension) {
  const std::string ext = lower(extension);
  if (ext == ".lef") {
    return FileKind::lef;
  }
  if (ext == ".def") {
    return FileKind::def;
  }
  if (ext == ".cif") {
    return FileKind::cif;
  }
  if (ext == ".gds") {
    return FileKind::gds;
  }
  return ext == ".tcl" ? FileKind::script : FileKind::cell;
}

// mainDoArgs as a small state machine over the words.
class OptionParser {
 public:
  explicit OptionParser(std::span<const std::string_view> args) : args_(args) {}

  diag::Outcome<StartupOptions> run() {
    while (index_ < args_.size() && !error_) {
      const std::string_view word = args_[index_++];
      if (word.starts_with('-')) {
        option(word);
      } else if (file(word)) {
        break;  // a `.tcl` script: the rest belongs to the script
      }
    }
    if (error_) {
      return *error_;
    }
    return std::move(options_);
  }

 private:
  void fail(std::string_view code, std::string text) {
    if (!error_) {
      error_ = diag::make(code, std::move(text));
    }
  }

  // mainArg/ArgStr: the attached argument (leading whitespace skipped) or
  // the next word; a value starting with '-' is rejected.
  std::optional<std::string> argument(std::string_view word, std::string_view what) {
    std::string_view value = word.size() > 2 ? skip_space(word.substr(2)) : std::string_view{};
    if (word.size() <= 2) {
      if (index_ >= args_.size()) {
        fail(codes::kOptionMissing, "-" + std::string{word.substr(1, 1)} + " requires a following " + std::string{what});
        return std::nullopt;
      }
      value = args_[index_++];
    }
    if (value.starts_with('-')) {
      fail(codes::kOptionBadName,
           "Bad name after '-" + std::string{word.substr(1, 1)} + "' option: '" + std::string{value} + "'");
      return std::nullopt;
    }
    return std::string{value};
  }

  void option(std::string_view word) {
    const char letter = word.size() > 1 ? word[1] : '\0';
    switch (letter) {
      case 'g':
        graphics(word);
        break;
      case 'i':
        have_dash_i_ = true;
        set(options_.tablet_port, argument(word, "tty name"));
        break;
      case 'd':
        set(options_.display_type, argument(word, "display type"));
        break;
      case 'm':
        if (auto value = argument(word, "monitor type")) {
          options_.monitor_type = std::move(*value);
        }
        break;
      case 'T':
        set(options_.technology, argument(word, "technology"));
        break;
      case 'r':
        r_option(word);
        break;
      case 'D':
        options_.debug = true;
        break;
      case 'w':
        options_.no_window = true;
        break;
      case 'n':
        n_option(word);
        break;
      default:
        fail(codes::kOptionUnknown, "Unknown option: '" + std::string{word} + "'\n" + std::string{kUsage});
        break;
    }
  }

  static void set(std::optional<std::string>& field, std::optional<std::string> value) {
    if (value) {
      field = std::move(value);
    }
  }

  void graphics(std::string_view word) {
    if (auto value = argument(word, "tty name")) {
      if (!have_dash_i_) {
        options_.tablet_port = *value;
      }
      options_.graphics_port = std::move(value);
    }
  }

  // -r / -re... recover; -rc... FILE (only when another word follows).
  void r_option(std::string_view word) {
    if (word.size() <= 2 || word[2] == 'e') {
      options_.recover = true;
    } else if (word[2] == 'c' && index_ < args_.size()) {
      set(options_.rc_file, argument("-r", "startup file"));
      options_.no_rc = false;  // RCFileName: the last of -rcfile and -norcfile wins
    } else {
      fail(codes::kOptionUnknown, "Unknown option: '" + std::string{word} + "'");
    }
  }

  void n_option(std::string_view word) {
    if (word.size() < 4) {
      fail(codes::kOptionAmbiguous,
           "Ambiguous option " + std::string{word} + ":  use -nowindow, -noconsole, or -norcfile");
    } else if (word[3] == 'c') {
      options_.no_console = true;
    } else if (word[3] == 'w') {
      options_.no_window = true;
    } else if (word[3] == 'r') {
      options_.no_rc = true;
      options_.rc_file.reset();
    } else {
      fail(codes::kOptionUnknown, "Unknown option: '" + std::string{word} + "'");
    }
  }

  // Records a file argument; true when it was a script (ends processing).
  bool file(std::string_view word) {
    FileArgument arg = classify_file_argument(word);
    const bool script = arg.kind == FileKind::script;
    options_.files.push_back(std::move(arg));
    if (script) {
      for (; index_ < args_.size(); ++index_) {
        options_.script_args.emplace_back(args_[index_]);
      }
    }
    return script;
  }

  std::span<const std::string_view> args_;
  std::size_t index_ = 0;
  bool have_dash_i_ = false;
  StartupOptions options_;
  std::optional<diag::Diagnostic> error_;
};

}  // namespace

std::string_view file_kind_name(FileKind kind) noexcept {
  switch (kind) {
    case FileKind::cell:
      return "cell";
    case FileKind::lef:
      return "LEF";
    case FileKind::def:
      return "DEF";
    case FileKind::cif:
      return "CIF";
    case FileKind::gds:
      return "GDS";
    case FileKind::script:
      return "script";
  }
  return "cell";
}

FileArgument classify_file_argument(std::string_view word) {
  constexpr std::string_view kSuffix = ".mag";
  std::string name{word};
  if (name.size() > kSuffix.size() && name.ends_with(kSuffix)) {
    name.resize(name.size() - kSuffix.size());
  }
  const auto dot = name.rfind('.');
  const FileKind kind = dot == std::string::npos ? FileKind::cell : kind_of_extension(std::string_view{name}.substr(dot));
  return FileArgument{std::move(name), kind};
}

diag::Outcome<StartupOptions> parse_startup_options(std::span<const std::string_view> args) {
  return OptionParser{args}.run();
}

std::string_view startup_usage_line() noexcept {
  return kUsage;
}

diag::Outcome<DisplayChoice> resolve_display(const std::optional<std::string>& display_type, bool display_available) {
  const std::string type = display_type ? upper(skip_space(*display_type)) : (display_available ? "XWIND" : "NULL");
  const auto it = std::find_if(kDisplayTypes.begin(), kDisplayTypes.end(),
                               [&](std::string_view name) { return std::string_view{type}.starts_with(name); });
  if (it == kDisplayTypes.end()) {
    std::string text = "Unknown display type:  " + type + "\nThese display types are available in this version of Magic:\n";
    for (const auto name : kDisplayTypes) {
      text += "        " + std::string{name} + "\n";
    }
    return diag::make(codes::kDisplayUnknown, text + "Use '-d NULL' if you don't need graphics.");
  }
  return DisplayChoice{std::string{*it}, *it == "NULL"};
}

}  // namespace hefesto::app
