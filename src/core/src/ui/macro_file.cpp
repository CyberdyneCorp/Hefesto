// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The key-binding file reader (design.md D7; m4-scripting tasks.md 13.2).
// Owner: DISPATCH.
//
// The file is Tcl, as the macro lines of a Magic startup file are, but only
// `macro` and `imacro` run. It is evaluated in a safe child interpreter
// (`interp create -safe`, made by a script sandbox that grants no file)
// whose own commands are all hidden or deleted; `macro`, `imacro` and
// `unknown` are aliases to this reader. Any other command, on a line of its
// own or substituted (`box 0 0 1 1`, `[exec ...]`), reaches `unknown`, is
// reported (UI-USAGE-001) and does nothing. The file is read one complete
// command at a time (`info complete` over whole lines, as a Tcl console
// reads), so an error stops only its own command; an unclosed brace or
// quote takes the rest of the file with it.

#include "hefesto/ui/macro_file.hpp"

#include <algorithm>
#include <memory>
#include <variant>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/binding/tags.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"
#include "hefesto/ui/commands.hpp"
#include "hefesto/ui/diag_codes.hpp"

namespace hefesto::ui {

namespace {

// The child interpreter: every namespace but the global one deleted (::tcl,
// whose ensembles `namespace` itself uses, last), every global command
// hidden, then the reader's three aliases.
constexpr std::string_view kSetup = R"tcl(
interp create -safe keys
set names [keys eval {info commands}]
keys eval {
    foreach ns [lsort -command {apply {{a b} {expr {($a eq "::tcl") - ($b eq "::tcl")}}}} [namespace children ::]] {
        namespace delete $ns
    }
}
foreach name $names {catch {keys hide $name}} ;# the ensembles of ::tcl went with it
unset names
keys alias macro ::hefesto::keys::macro
keys alias imacro ::hefesto::keys::imacro
keys alias unknown ::hefesto::keys::other
)tcl";

class Reader {
 public:
  Reader(editor::EditorState& editor, std::string_view source_name)
      : editor_(editor), source_name_(source_name), parent_(script::create_project_interpreter(editor, {})) {}

  MacroFileResult run(std::string_view text) {
    if (!set_up()) return std::move(result_);
    std::size_t pos = 0;
    std::uint32_t next_line = 1;
    while (pos < text.size()) {
      line_ = next_line;
      std::string chunk;
      do {
        const std::size_t end = std::min(text.find('\n', pos), text.size());
        chunk.append(text.substr(pos, end - pos)).push_back('\n');
        pos = end + 1;
        ++next_line;
      } while (pos < text.size() && !complete(chunk));
      evaluate(chunk);
    }
    return std::move(result_);
  }

 private:
  bool set_up() {
    parent_->define_command("::hefesto::keys::macro", [this](const cmd::Invocation& inv) { return macro(inv, false); });
    parent_->define_command("::hefesto::keys::imacro", [this](const cmd::Invocation& inv) { return macro(inv, true); });
    parent_->define_command("::hefesto::keys::other", [this](const cmd::Invocation& inv) { return other(inv); });
    const cmd::CommandResult made = parent_->evaluate(kSetup);
    if (!made.ok()) {
      for (const diag::Diagnostic& d : made.diagnostics) add(d);
    }
    return made.ok();
  }

  bool complete(const std::string& chunk) {
    const cmd::CommandResult r = parent_->evaluate("info complete " + script::quote_list_element(chunk));
    return !r.ok() || r.value.to_string() == "1";
  }

  void evaluate(const std::string& chunk) {
    const cmd::CommandResult r = parent_->evaluate("keys eval " + script::quote_list_element(chunk));
    if (r.ok() || r.diagnostics.empty()) return;
    diag::Diagnostic error = r.diagnostics.front();
    error.location = diag::SourceLocation{std::string{source_name_}, line_};
    result_.diagnostics.push_back(std::move(error));
  }

  cmd::CommandResult macro(const cmd::Invocation& inv, bool interactive) {
    cmd::CommandResult r = macro_command(editor_, inv.args, interactive, "layout");
    for (const diag::Diagnostic& d : r.diagnostics) add(d);
    ++result_.applied;
    return cmd::CommandResult::success();
  }

  cmd::CommandResult other(const cmd::Invocation& inv) {
    const std::string name = inv.args.empty() ? std::string{} : inv.args.front();
    add(diag::make(codes::kUsage, "a key-binding file holds only macro and imacro lines, not \"" + name + "\""));
    return cmd::CommandResult::success();
  }

  void add(diag::Diagnostic d) {
    if (std::holds_alternative<std::monostate>(d.location)) {
      d.location = diag::SourceLocation{std::string{source_name_}, line_};
    }
    result_.diagnostics.push_back(std::move(d));
  }

  editor::EditorState& editor_;
  std::string_view source_name_;
  std::unique_ptr<script::Interpreter> parent_;
  std::uint32_t line_ = 1;
  MacroFileResult result_;
};

}  // namespace

MacroFileResult read_macro_file(editor::EditorState& editor, std::string_view text, std::string_view source_name) {
  return Reader{editor, source_name}.run(text);
}

}  // namespace hefesto::ui
