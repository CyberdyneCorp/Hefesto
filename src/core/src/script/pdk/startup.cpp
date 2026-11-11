// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The script-host startup (design D7; scripting-pdk-integration
// "Script-host startup sequence", "Command-line options in the script
// host", "Command-line file arguments", "Startup files are scripts").
// Ported from Magic 8.3.694 (fc5daa9e) tcltk/magic.tcl.in:158-237,363-439
// and utils/main.c:840-1060. Copyright (C) 1985, 1990 Regents of the
// University of California, see THIRD_PARTY_NOTICES.md.

#include "hefesto/script/pdk/startup.hpp"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <regex>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/pdk/trust.hpp"
#include "hefesto/script/project.hpp"

namespace hefesto::script {

namespace {

constexpr std::string_view kStartupError = "SCRIPT-PDK-001";
constexpr std::string_view kBadLocal = "SCRIPT-PDK-002";
constexpr std::string_view kFileEntry = "SCRIPT-PDK-006";
constexpr std::string_view kCannotLoad = "SCRIPT-PDK-007";
constexpr std::string_view kNoInterpreter = "SCRIPT-PDK-008";

// ---- startup files ----------------------------------------------------------

std::string quoted_name(const StartupFile& file) {
  return "\"" + (file.name.empty() ? file.path.string() : file.name) + "\"";
}

// The error line of utils/main.c for each kind of file.
std::string error_line(const StartupFile& file, std::string_view message) {
  const std::string tail = quoted_name(file) + ": " + std::string{message};
  switch (file.kind) {
    case StartupKind::pre:
      return "Error parsing pre-startup file " + tail;
    case StartupKind::system:
      return "Error parsing system startup file " + tail;
    case StartupKind::user:
      return "Error parsing user " + tail;
    case StartupKind::pdk:
    case StartupKind::local:
      break;
  }
  return file.legacy_name ? "Error parsing local " + tail : "Error parsing " + tail;
}

// Runs one file, after consent for a PDK's rc file; true when it was run
// without an error.
bool run_one(editor::EditorState& editor, const StartupPlan& plan, const StartupFile& file,
             cmd::CommandResult& out) {
  if (file.pdk) {
    auto consent = request_consent(editor.script.pdk_trust, editor.script.events, plan.consent, *file.pdk, file.path,
                                   false);
    out.diagnostics.insert(out.diagnostics.end(), consent.diagnostics.begin(), consent.diagnostics.end());
    if (!consent.run) {
      return false;
    }
  }
  const auto result = editor.script.interp->source(file.path);
  if (!result.ok()) {
    const std::string message = result.diagnostics.empty() ? std::string{} : result.diagnostics.front().text;
    const auto lines = startup_error(file, message);
    out.diagnostics.insert(out.diagnostics.end(), lines.begin(), lines.end());
  }
  return result.ok();
}

// ---- command line -----------------------------------------------------------

enum class OptionAction : std::uint8_t {
  no_wrapper,     // -now...
  dnull,          // -dnull: no wrapper, passed through
  recover,        // -r...
  pass_next,      // passed through with the next word
  pass_two,       // passed through with the next two words
  print_version,  // --version
  print_commit,   // --commit
  print_prefix,   // --prefix
  pass,           // passed through
};

struct OptionRow {
  CommandLineOption option;
  OptionAction action;
};

const std::vector<OptionRow>& option_rows() {
  static const std::vector<OptionRow> rows = {
      {{"^-now(rap)?(per)?$", 0, "disable the wrapper"}, OptionAction::no_wrapper},
      {{"^-dnull", 0, "disable the wrapper and pass the argument through"}, OptionAction::dnull},
      {{"^-r(e)?(cover)?$", 0, "request crash recovery"}, OptionAction::recover},
      {{"^-rc(file)?$", 1, "pass through with the next argument (startup file)"}, OptionAction::pass_next},
      {{"^-d", 1, "pass through with the next argument (display)"}, OptionAction::pass_next},
      {{"^-g", 1, "pass through with the next argument (graphics port)"}, OptionAction::pass_next},
      {{"^-m", 1, "pass through with the next argument (monitor)"}, OptionAction::pass_next},
      {{"^-i", 1, "pass through with the next argument (tablet port)"}, OptionAction::pass_next},
      {{"^-T", 1, "pass through with the next argument (technology)"}, OptionAction::pass_next},
      {{"^-F", 2, "pass through with the next two arguments"}, OptionAction::pass_two},
      {{"^--version", 0, "print <version>.<revision> and exit 0"}, OptionAction::print_version},
      {{"^--commit", 0, "print the commit id and exit 0"}, OptionAction::print_commit},
      {{"^--prefix", 0, "print $CAD_ROOT and exit 0"}, OptionAction::print_prefix},
      {{"^-D", 0, "pass through"}, OptionAction::pass},
      {{"^-n*", 0, "pass through (matches every remaining word that begins with -)"}, OptionAction::pass},
  };
  return rows;
}

const std::vector<std::regex>& option_patterns() {
  static const std::vector<std::regex> patterns = [] {
    std::vector<std::regex> out;
    for (const auto& row : option_rows()) {
      out.emplace_back(row.option.pattern, std::regex::ECMAScript);
    }
    return out;
  }();
  return patterns;
}

// The first row matching `word` (Tcl `switch -regexp`), or nullptr.
const OptionRow* match_option(const std::string& word) {
  const auto& patterns = option_patterns();
  for (std::size_t i = 0; i < patterns.size(); ++i) {
    if (std::regex_search(word, patterns[i])) {
      return &option_rows()[i];
    }
  }
  return nullptr;
}

// `lindex $argv $i` past the end is the empty string.
std::string word_at(std::span<const std::string> args, std::size_t i) {
  return i < args.size() ? args[i] : std::string{};
}

// Applies one matched option; returns the number of extra words consumed,
// or nullopt when the command line ends here (--version and friends).
std::optional<std::size_t> apply_option(const OptionRow& row, std::span<const std::string> args, std::size_t i,
                                        const ScriptHostInfo& info, ScriptHostCommandLine& out) {
  const std::string& word = args[i];
  switch (row.action) {
    case OptionAction::no_wrapper:
      out.wrapper = false;
      return 0;
    case OptionAction::dnull:
      out.wrapper = false;
      out.engine_args.push_back(word);
      return 0;
    case OptionAction::recover:
      out.recover = true;
      return 0;
    case OptionAction::pass:
      out.engine_args.push_back(word);
      return 0;
    case OptionAction::pass_next:
    case OptionAction::pass_two: {
      const std::size_t extra = row.action == OptionAction::pass_two ? 2 : 1;
      out.engine_args.push_back(word);
      for (std::size_t k = 1; k <= extra; ++k) {
        out.engine_args.push_back(word_at(args, i + k));
      }
      return extra;
    }
    case OptionAction::print_version:
      out.print_and_exit = info.version;
      return std::nullopt;
    case OptionAction::print_commit:
      out.print_and_exit = info.commit;
      return std::nullopt;
    case OptionAction::print_prefix:
      out.print_and_exit = info.cad_root;
      return std::nullopt;
  }
  return 0;
}

// ---- file arguments -----------------------------------------------------------

std::string tcl_command(std::initializer_list<std::string_view> words) {
  cmd::Value::List list;
  for (const std::string_view word : words) {
    list.emplace_back(std::string{word});
  }
  return cmd::Value{std::move(list)}.to_string();
}

diag::Diagnostic stdout_line(std::string text) {
  return diag::make(kFileEntry, std::move(text));
}

diag::Diagnostic stderr_line(std::string text) {
  return diag::make(kCannotLoad, std::move(text));
}

// The reader of an uncompressed extension, or empty.
std::string reader_command(std::string_view ext, std::string_view file) {
  static constexpr std::array<std::string_view, 6> kGds = {".gds", ".GDS", ".gds2", ".GDS2", ".gdsii", ".GDSII"};
  if (ext == ".lef" || ext == ".LEF") {
    return tcl_command({"lef", "read", file});
  }
  if (ext == ".def" || ext == ".DEF") {
    return tcl_command({"def", "read", file});
  }
  if (std::find(kGds.begin(), kGds.end(), ext) != kGds.end()) {
    return tcl_command({"gds", "read", file});
  }
  if (ext == ".cif" || ext == ".CIF") {
    return tcl_command({"cif", "read", file});
  }
  if (ext == ".tcl") {
    return tcl_command({"source", file});
  }
  if (ext == ".mag" || ext.empty()) {
    return tcl_command({"magic::load", file});
  }
  return {};
}

// Wrapper mode's `.gz` branch.
void compressed_entry(std::string_view file, FileEntry& out) {
  const std::string_view root = file.substr(0, file.size() - 3);  // [file root]: strip ".gz"
  const std::string inner = file_extension(root);
  out.messages.push_back(
      stdout_line("Handling compressed entry " + std::string{file} + " extension " + inner));
  if (inner == ".gds") {
    out.command = tcl_command({"gds", "read", file});
  } else if (inner == ".mag") {
    out.command = tcl_command({"magic::load", file});
  } else {
    out.messages.push_back(stderr_line("only compressed .mag and .gds are handled."));
  }
}

}  // namespace

std::vector<diag::Diagnostic> startup_error(const StartupFile& file, std::string_view message) {
  std::vector<diag::Diagnostic> out{diag::make(kStartupError, error_line(file, message))};
  if (file.kind == StartupKind::local && !file.legacy_name) {
    out.push_back(diag::make(kBadLocal, "Bad local startup file " + quoted_name(file) + ", continuing without."));
  }
  return out;
}

cmd::CommandResult run_startup_files(editor::EditorState& editor, const StartupPlan& plan) {
  if (editor.script.interp == nullptr) {
    return cmd::CommandResult::failure(
        diag::make(kNoInterpreter, "Startup files cannot run: the project has no script interpreter."));
  }
  cmd::CommandResult out = cmd::CommandResult::success();
  cmd::Value::List ran;
  for (const StartupFile& file : plan.files) {
    if (run_one(editor, plan, file, out)) {
      ran.emplace_back(file.path.string());
    }
    if (plan.after_file) {
      plan.after_file(file);
    }
    if (plan.stop_requested && plan.stop_requested()) {
      break;
    }
  }
  out.value = cmd::Value{std::move(ran)};
  return out;
}

std::span<const CommandLineOption> command_line_table() {
  static const std::vector<CommandLineOption> table = [] {
    std::vector<CommandLineOption> out;
    for (const auto& row : option_rows()) {
      out.push_back(row.option);
    }
    return out;
  }();
  return table;
}

ScriptHostCommandLine classify_command_line(std::span<const std::string> args, const ScriptHostInfo& info) {
  ScriptHostCommandLine out;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const OptionRow* row = match_option(args[i]);
    if (row == nullptr) {
      out.files.push_back(args[i]);
      out.engine_args.push_back(args[i]);
      continue;
    }
    const auto consumed = apply_option(*row, args, i, info, out);
    if (!consumed) {
      return out;
    }
    i += *consumed;
  }
  return out;
}

std::string file_extension(std::string_view file) {
  const std::size_t slash = file.find_last_of('/');
  const std::string_view tail = slash == std::string_view::npos ? file : file.substr(slash + 1);
  const std::size_t dot = tail.rfind('.');
  return dot == std::string_view::npos ? std::string{} : std::string{tail.substr(dot)};
}

FileEntry script_host_file_entry(std::string_view file, bool wrapper) {
  FileEntry out;
  const std::string ext = file_extension(file);
  const std::string name{file};
  if (!wrapper) {
    out.messages.push_back(stdout_line("handling file entry " + name + " extension " + ext));
  } else if (ext != ".gz" && !ext.empty()) {
    out.messages.push_back(stdout_line("Handling file entry " + name + " extension " + ext));
  }
  if (wrapper && ext == ".gz") {
    compressed_entry(file, out);
    return out;
  }
  out.command = reader_command(ext, file);
  if (out.command.empty()) {
    out.messages.push_back(stderr_line("don't know how to load file " + name));
  }
  return out;
}

}  // namespace hefesto::script
