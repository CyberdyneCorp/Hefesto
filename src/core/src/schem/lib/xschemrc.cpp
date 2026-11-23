// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `xschemrc` files read as data (capability schematic-symbols-and-libraries
// "xschemrc as data", "Script execution permission"; Q-SCHEM-23,
// Q-SCHEM-25, Q-SCHEM-32): the PDK's file, then the document folder's, run in
// a sandboxed interpreter that may only read the PDK, the rc file's folder,
// XSCHEM_SHAREDIR and the document folder, with a synthetic `env` array.
// A command the interpreter does not know raises Tcl's `invalid command
// name` error (SCHEM-LIB-006, the settings made before it kept), except the
// GUI-only commands the three PDK `xschemrc` files call
// (xschemrc_gui_commands()), which are defined as no-ops while the files run
// (Q-SCHEM-38 option (a), approved 2026-11-22, both modes); `puts` output
// becomes SCHEM-LIB-008 notes instead of reaching the process's streams.
//
// The evaluation of both files is limited to XschemrcInput::time_limit (10 s,
// Q-SCHEM-32) in total: while a file runs, the interpreter's progress hook
// checks the deadline and interrupts the evaluation once it has passed (an
// interrupt cannot be caught by the script), so `while 1 {}` stops too.
//
// Owner: SYMLIB.

#include "hefesto/schem/lib/xschemrc.hpp"

#include <algorithm>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"

namespace hefesto::schem {

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::string_view kExecuteScripts = "xschem_execute_scripts";
constexpr std::string_view kInterrupted = "SCRIPT-TCL-002";

// Defines each GUI-only command that is not already a command as a no-op
// and records it, so restore() removes exactly those.
std::string gui_stub_script() {
  std::string script = "set ::hefesto::schem::rc_gui {}\n";
  for (const std::string_view name : xschemrc_gui_commands()) {
    const std::string command = "::" + std::string{name};
    script += "if {[info commands " + command + "] eq {}} {proc " + command +
              " {args} {return {}}; lappend ::hefesto::schem::rc_gui " + command + "}\n";
  }
  return script;
}

std::set<std::string> split_lines(const std::string& text) {
  std::set<std::string> lines;
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) {
    if (!line.empty()) lines.insert(line);
  }
  return lines;
}

cmd::CommandResult capture_puts(const cmd::Invocation& invocation, std::vector<diag::Diagnostic>& out) {
  if (!invocation.args.empty()) {
    out.push_back(diag::make("SCHEM-LIB-008", "xschemrc: " + invocation.args.back()));
  }
  return cmd::CommandResult::success();
}

// `10 s`, or `0.25 s` for a limit that is not whole seconds.
std::string seconds_text(std::chrono::milliseconds limit) {
  const auto ms = limit.count();
  if (ms % 1000 == 0) return std::to_string(ms / 1000) + " s";
  std::string fraction = std::to_string(1000 + ms % 1000).substr(1);
  while (fraction.back() == '0') fraction.pop_back();
  return std::to_string(ms / 1000) + "." + fraction + " s";
}

// Installs a progress hook that interrupts the running evaluation once
// `deadline` has passed, chained to the hook that was there; restores it.
class DeadlineHook {
 public:
  DeadlineHook(script::ProgressReporting& progress, script::Interpreter& interp, Clock::time_point deadline)
      : progress_(progress), saved_(progress.handler) {
    progress_.handler = [this, &interp, deadline](const script::ScriptProgress& report) {
      if (saved_) saved_(report);
      if (!report.done && Clock::now() >= deadline) {
        expired_ = true;
        interp.request_interrupt();
      }
    };
  }
  ~DeadlineHook() { progress_.handler = std::move(saved_); }
  DeadlineHook(const DeadlineHook&) = delete;
  DeadlineHook& operator=(const DeadlineHook&) = delete;

  [[nodiscard]] bool expired() const noexcept { return expired_; }

 private:
  script::ProgressReporting& progress_;
  script::ProgressHandler saved_;
  bool expired_ = false;
};

class XschemrcRun {
 public:
  XschemrcRun(script::Interpreter& interp, script::ProgressReporting& progress, platform::FileSystem& fs,
              const XschemrcInput& input)
      : interp_(interp), progress_(progress), fs_(fs), input_(input) {}

  XschemrcResult run() {
    std::set<std::string> builtin = global_names();
    builtin.insert({"errorInfo", "errorCode"});  // set by Tcl when an rc file fails
    prepare();
    std::vector<platform::Path> files;
    if (!input_.rc_file.empty()) files.push_back(input_.rc_file);
    const platform::Path local = input_.document_folder / "xschemrc";
    if (!input_.document_folder.empty() && local != input_.rc_file && fs_.exists(local)) files.push_back(local);
    evaluate(files);
    restore();
    collect(builtin);
    return std::move(result_);
  }

 private:
  std::set<std::string> global_names() {
    const auto names = interp_.evaluate("join [lsort [info globals]] \\n");
    return names.ok() ? split_lines(names.value.to_string()) : std::set<std::string>{};
  }

  // `puts` is replaced and the GUI-only commands defined while the files
  // run, and both undone afterwards, so an interpreter that goes on to
  // evaluate `tcleval` values keeps Tcl's own `puts` (the capturing one
  // refers to this run) and no GUI stubs.
  void prepare() {
    for (const auto& [name, value] : input_.variables) interp_.set_var(name, cmd::Value{value});
    (void)interp_.evaluate(
        "namespace eval ::hefesto::schem {}\n"
        "catch {rename ::puts ::hefesto::schem::rc_saved_puts}\n" +
        gui_stub_script());
    interp_.define_command("::puts", [this](const cmd::Invocation& inv) { return capture_puts(inv, result_.diagnostics); });
  }

  void restore() {
    (void)interp_.evaluate(
        "catch {rename ::puts {}}\n"
        "catch {rename ::hefesto::schem::rc_saved_puts ::puts}\n"
        "foreach command $::hefesto::schem::rc_gui {catch {rename $command {}}}\n"
        "unset -nocomplain ::hefesto::schem::rc_gui");
  }

  // Sources each file until the time limit stops one; later files are lost.
  void evaluate(const std::vector<platform::Path>& files) {
    const Clock::time_point deadline = Clock::now() + input_.time_limit;
    for (const platform::Path& file : files) {
      if (Clock::now() >= deadline) return report_stop(file);
      DeadlineHook hook(progress_, interp_, deadline);
      if (!source(file, hook)) return;
    }
  }

  // False when the time limit stopped `file`.
  bool source(const platform::Path& file, const DeadlineHook& hook) {
    const auto before = interp_.get_var(kExecuteScripts);
    auto outcome = interp_.source(file);
    const bool stopped = hook.expired() && !outcome.ok();
    if (stopped) {
      std::erase_if(outcome.diagnostics, [](const diag::Diagnostic& d) { return d.code == kInterrupted; });
      report_stop(file);
    } else if (!outcome.ok()) {
      const std::string why = outcome.diagnostics.empty() ? std::string{"error"} : outcome.diagnostics.front().text;
      result_.diagnostics.push_back(
          diag::make("SCHEM-LIB-006", "Error in " + file.generic_string() + ": " + why + "; its later settings are lost"));
    }
    result_.diagnostics.insert(result_.diagnostics.end(), outcome.diagnostics.begin(), outcome.diagnostics.end());
    const auto after = interp_.get_var(kExecuteScripts);
    if (after.has_value() && after != before) log_execute_scripts(file, after->to_string());
    return !stopped;
  }

  void report_stop(const platform::Path& file) {
    result_.diagnostics.push_back(diag::make("SCHEM-LIB-010", "xschemrc " + file.generic_string() +
                                                                  ": evaluation stopped after " +
                                                                  seconds_text(input_.time_limit)));
  }

  void log_execute_scripts(const platform::Path& file, const std::string& value) {
    result_.diagnostics.push_back(diag::make(
        "SCHEM-LIB-007", "xschem_execute_scripts " + value + " in " + file.generic_string() +
                             " ignored: tcleval values always run in the script sandbox and launchers are confirmed "
                             "once per document"));
  }

  void collect(const std::set<std::string>& builtin) {
    for (const std::string& name : global_names()) {
      if (builtin.count(name) != 0) continue;
      if (auto value = interp_.get_var(name)) result_.variables.emplace(name, value->to_string());
    }
    const auto path = result_.variables.find("XSCHEM_LIBRARY_PATH");
    const std::string value = path == result_.variables.end() ? std::string{} : path->second;
    std::map<std::string, std::string> env(input_.env.begin(), input_.env.end());
    env.try_emplace("HOME", input_.document_folder.generic_string());
    result_.search_path = SearchPath::parse(value, env, fs_, input_.document_folder);
  }

  script::Interpreter& interp_;
  script::ProgressReporting& progress_;
  platform::FileSystem& fs_;
  const XschemrcInput& input_;
  XschemrcResult result_;
};

}  // namespace

const std::vector<std::string_view>& xschemrc_gui_commands() {
  // The `rc_commands` of tests/data/schem/inventory.json that are Tk-only:
  // neither Tcl built-ins, procedures the PDK defines, procedures xschem
  // provides (schem/tcl/procedures.hpp) nor the `xschem` command. Sorted;
  // the inventory test of xschemrc_test.cpp fails when a PDK bump adds one.
  static const std::vector<std::string_view> names{"button", "destroy",  "focus", "label", "menu", "pack",
                                                   "radiobutton", "raise", "toplevel", "winfo", "wm"};
  return names;
}

script::SandboxPolicy xschemrc_policy(const XschemrcInput& input) {
  script::SandboxPolicy policy;
  const auto add_root = [&policy](const std::string& path) {
    if (!path.empty() && platform::Path{path}.is_absolute()) policy.read_only_roots.emplace_back(path);
  };
  add_root(input.rc_file.parent_path().generic_string());
  add_root(input.document_folder.generic_string());
  for (const char* name : {"PDK_ROOT", "XSCHEM_SHAREDIR"}) {
    if (const auto it = input.variables.find(name); it != input.variables.end()) add_root(it->second);
    if (const auto it = input.env.find(name); it != input.env.end()) add_root(it->second);
  }
  policy.environment.insert(input.env.begin(), input.env.end());
  const std::string folder = input.document_folder.generic_string();
  policy.environment.try_emplace("HOME", folder);
  policy.environment.try_emplace("PWD", folder);
  return policy;
}

XschemrcResult evaluate_xschemrc(script::Interpreter& interp, script::ProgressReporting& progress,
                                 platform::FileSystem& fs, const XschemrcInput& input) {
  XschemrcRun run(interp, progress, fs, input);
  return run.run();
}

XschemrcResult SearchPath::from_xschemrc(platform::FileSystem& fs, const XschemrcInput& input) {
  editor::EditorState editor;
  auto interp = script::create_project_interpreter(editor, xschemrc_policy(input));
  XschemrcResult result = evaluate_xschemrc(*interp, editor.script.progress, fs, input);
  interp.reset();  // before editor, whose events it posts to
  return result;
}

}  // namespace hefesto::schem
