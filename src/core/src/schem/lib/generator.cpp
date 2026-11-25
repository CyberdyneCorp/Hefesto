// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Tcl symbol generators (capability schematic-symbols-and-libraries "Symbol
// generators"; Q-SCHEM-11). The generator runs in its own sandboxed
// interpreter, bounded in time (the sandbox's time limit, which a script
// cannot catch) and in script and output size (Q-SCHEM-40's 256 MiB), in
// both modes of `xschem-compat`. The symbol cache bounds the total time of
// all runs (kGeneratorTotalTimeLimit).
//
// Owner: SYMLIB.

#include "hefesto/schem/lib/generator.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"
#include "schem/lib/run_guards.hpp"

namespace hefesto::schem {

namespace {

constexpr std::string_view kTimeLimitCode = "SCRIPT-TCL-003";

bool is_blank(char c) noexcept {
  return c == ' ' || c == '\t';
}

bool is_separator(char c) noexcept {
  return c == '(' || c == ')' || c == ',' || c == ' ';
}

std::string_view trim_trailing_blanks(std::string_view text) noexcept {
  while (!text.empty() && is_blank(text.back())) text.remove_suffix(1);
  return text;
}

// `puts ?-nonewline? ?channel? string`, writing to stdout into `out`.
class PutsCapture {
 public:
  explicit PutsCapture(std::size_t max_output) : max_output_(max_output) {}

  cmd::CommandResult operator()(const cmd::Invocation& invocation) {
    const auto& args = invocation.args;
    const bool newline = args.empty() || args.front() != "-nonewline" || args.size() == 1;
    const std::size_t first = newline ? 0 : 1;
    const std::size_t count = args.size() - first;
    if (count != 1 && count != 2) {
      return cmd::CommandResult::failure(
          diag::make("SCHEM-LIB-012", "wrong # args: should be \"puts ?-nonewline? ?channelId? string\""));
    }
    if (count == 2 && args[first] != "stdout") return cmd::CommandResult::success();
    const std::string& text = args.back();
    if (text.size() + (newline ? 1 : 0) > max_output_ - out_.size()) {
      too_long_ = true;
      return cmd::CommandResult::failure(diag::make("SCHEM-LIB-012", "generator output too long"));
    }
    out_ += text;
    if (newline) out_ += '\n';
    return cmd::CommandResult::success();
  }

  [[nodiscard]] bool too_long() const noexcept { return too_long_; }
  [[nodiscard]] std::string take() { return std::move(out_); }

 private:
  std::size_t max_output_;
  std::string out_;
  bool too_long_ = false;
};

script::SandboxPolicy generator_policy(const GeneratorInput& input) {
  script::SandboxPolicy policy;
  for (const platform::Path& root : {input.script.parent_path(), input.document_folder}) {
    if (!root.empty() && root.is_absolute()) policy.read_only_roots.push_back(root);
  }
  const std::string folder = input.document_folder.generic_string();
  policy.environment = {{"HOME", folder}, {"PWD", folder}};
  policy.limits.time_limit = input.time_limit;
  policy.stdin_readable = false;  // S8-04: a read of stdin blocks, unlimited
  return policy;
}

void set_arguments(script::Interpreter& interp, const GeneratorInput& input) {
  (void)interp.evaluate("set ::argv {}");
  for (const std::string& arg : input.args) {
    interp.set_var("::hefesto_generator_arg", cmd::Value{arg});
    (void)interp.evaluate("lappend ::argv $::hefesto_generator_arg");
  }
  (void)interp.evaluate("unset -nocomplain ::hefesto_generator_arg");
  interp.set_var("::argc", cmd::Value{std::to_string(input.args.size())});
  interp.set_var("::argv0", cmd::Value{input.script.generic_string()});
}

GeneratorOutput failed(const GeneratorInput& input, const std::string& why) {
  GeneratorOutput output;
  output.diagnostics.push_back(
      diag::make("SCHEM-LIB-012", "Symbol generator " + input.script.generic_string() + " failed: " + why));
  return output;
}

// Why a failed run failed, for the SCHEM-LIB-012 message. A sandbox denial
// is reported with its own code (below), so the message names it without
// repeating its text (review of PR #101: it was reported twice).
std::string failure_reason(const cmd::CommandResult& result, const PutsCapture& puts, const GeneratorInput& input) {
  if (puts.too_long()) return "it printed more than " + std::to_string(input.max_output) + " bytes";
  for (const diag::Diagnostic& d : result.diagnostics) {
    if (d.code == kTimeLimitCode) {
      const auto ms = input.time_limit.count();
      return "stopped after " + (ms % 1000 == 0 ? std::to_string(ms / 1000) + " s" : std::to_string(ms) + " ms");
    }
  }
  if (result.diagnostics.empty()) return "error";
  const std::string& error = result.diagnostics.front().text;
  const auto denial = std::find_if(result.diagnostics.begin(), result.diagnostics.end(), [&error](const auto& d) {
    return detail::is_sandbox_denial(d) && d.text == error;
  });
  if (denial != result.diagnostics.end()) return "the script sandbox denied an operation (" + denial->code + ")";
  return error;
}

std::string too_large(const GeneratorInput& input) {
  return "the script is larger than " + std::to_string(input.max_script) + " bytes";
}

// At most max_script + 1 bytes of `file`: one more than allowed tells a
// file that grew after its size was checked (review of PR #101).
diag::Outcome<std::string> read_script(platform::FileSystem& fs, const platform::Path& file, std::size_t max_script) {
  const std::size_t cap = max_script == std::numeric_limits<std::size_t>::max() ? max_script : max_script + 1;
  return fs.read_at_most(file, cap);
}

}  // namespace

bool is_generator_reference(std::string_view reference) noexcept {
  const std::string_view text = trim_trailing_blanks(reference);
  const std::size_t open = text.find('(');
  if (open == 0 || open == std::string_view::npos || text.back() != ')') return false;
  const std::string_view name = text.substr(0, open);
  const std::string_view inside = text.substr(open + 1, text.size() - open - 2);
  return name.find_first_of(" \t)") == std::string_view::npos && inside.find_first_of("()") == std::string_view::npos;
}

std::optional<GeneratorCall> parse_generator(std::string_view reference) {
  if (!is_generator_reference(reference)) return std::nullopt;
  std::vector<std::string> pieces;
  std::string piece;
  for (const char c : trim_trailing_blanks(reference)) {
    if (!is_separator(c)) {
      piece += c;
    } else if (!piece.empty()) {
      pieces.push_back(std::move(piece));
      piece.clear();
    }
  }
  if (!piece.empty()) pieces.push_back(std::move(piece));
  GeneratorCall call;
  call.program = std::move(pieces.front());
  call.args.assign(std::make_move_iterator(pieces.begin() + 1), std::make_move_iterator(pieces.end()));
  return call;
}

bool is_tcl_generator(const platform::Path& file, platform::FileSystem& fs, std::size_t max_script) {
  if (file.extension() == ".tcl") return true;
  const auto size = fs.size(file);
  if (!size.ok() || size.value() > max_script) return false;
  const auto text = read_script(fs, file, max_script);
  if (!text.ok() || text.value().size() > max_script || !text.value().starts_with("#!")) return false;
  const std::string_view first_line = std::string_view{text.value()}.substr(0, text.value().find('\n'));
  return first_line.find("tclsh") != std::string_view::npos || first_line.find("wish") != std::string_view::npos;
}

GeneratorOutput run_tcl_generator(platform::FileSystem& fs, const GeneratorInput& input) {
  const auto size = fs.size(input.script);
  if (size.ok() && size.value() > input.max_script) return failed(input, too_large(input));
  const auto script = read_script(fs, input.script, input.max_script);
  if (!script.ok()) return failed(input, script.error().text);
  if (script.value().size() > input.max_script) return failed(input, too_large(input));
  editor::EditorState editor;
  auto interp = script::create_project_interpreter(editor, generator_policy(input));
  PutsCapture puts(input.max_output);
  interp->define_command("::puts", std::ref(puts));
  set_arguments(*interp, input);
  const cmd::CommandResult result = interp->evaluate(script.value());
  GeneratorOutput output = result.ok() && !puts.too_long() ? GeneratorOutput{puts.take(), {}}
                                                           : failed(input, failure_reason(result, puts, input));
  // Every denial is reported, also one the generator caught (security
  // review, task 12.1: they were dropped with the result).
  std::copy_if(result.diagnostics.begin(), result.diagnostics.end(), std::back_inserter(output.diagnostics),
               detail::is_sandbox_denial);
  interp.reset();  // before editor, whose events it posts to
  return output;
}

}  // namespace hefesto::schem
