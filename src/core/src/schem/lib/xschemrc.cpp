// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `xschemrc` files read as data (capability schematic-symbols-and-libraries
// "xschemrc as data", "Script execution permission"; Q-SCHEM-23,
// Q-SCHEM-25): the PDK's file, then the document folder's, run in a fresh
// sandboxed interpreter that may only read the PDK, the rc file's folder,
// XSCHEM_SHAREDIR and the document folder, with a synthetic `env` array.
// Commands the interpreter does not know (xschem GUI commands such as Tk
// widgets) are accepted and ignored through `unknown`; `puts` output becomes
// SCHEM-LIB-008 notes instead of reaching the process's streams.
//
// Owner: SYMLIB.

#include <memory>
#include <set>
#include <sstream>
#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"

namespace hefesto::schem {

namespace {

constexpr std::string_view kExecuteScripts = "xschem_execute_scripts";

std::set<std::string> split_lines(const std::string& text) {
  std::set<std::string> lines;
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) {
    if (!line.empty()) lines.insert(line);
  }
  return lines;
}

script::SandboxPolicy policy_for(const XschemrcInput& input) {
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

cmd::CommandResult capture_puts(const cmd::Invocation& invocation, std::vector<diag::Diagnostic>& out) {
  if (!invocation.args.empty()) {
    out.push_back(diag::make("SCHEM-LIB-008", "xschemrc: " + invocation.args.back()));
  }
  return cmd::CommandResult::success();
}

class XschemrcRun {
 public:
  XschemrcRun(platform::FileSystem& fs, const XschemrcInput& input)
      : fs_(fs), input_(input), interp_(script::create_project_interpreter(editor_, policy_for(input))) {}

  XschemrcRun(const XschemrcRun&) = delete;
  XschemrcRun& operator=(const XschemrcRun&) = delete;
  ~XschemrcRun() { interp_.reset(); }  // before editor_, whose events it posts to

  XschemrcResult run() {
    std::set<std::string> builtin = global_names();
    builtin.insert({"errorInfo", "errorCode"});  // set by Tcl when an rc file fails
    prepare();
    if (!input_.rc_file.empty()) source(input_.rc_file);
    const platform::Path local = input_.document_folder / "xschemrc";
    if (!input_.document_folder.empty() && local != input_.rc_file && fs_.exists(local)) source(local);
    collect(builtin);
    return std::move(result_);
  }

 private:
  std::set<std::string> global_names() {
    const auto names = interp_->evaluate("join [lsort [info globals]] \\n");
    return names.ok() ? split_lines(names.value.to_string()) : std::set<std::string>{};
  }

  void prepare() {
    for (const auto& [name, value] : input_.variables) interp_->set_var(name, cmd::Value{value});
    interp_->define_command("puts", [this](const cmd::Invocation& inv) { return capture_puts(inv, result_.diagnostics); });
    (void)interp_->evaluate("proc unknown {args} {return {}}");
  }

  void source(const platform::Path& file) {
    const auto before = interp_->get_var(kExecuteScripts);
    const auto outcome = interp_->source(file);
    if (!outcome.ok()) {
      const std::string why = outcome.diagnostics.empty() ? std::string{"error"} : outcome.diagnostics.front().text;
      result_.diagnostics.push_back(
          diag::make("SCHEM-LIB-006", "Error in " + file.generic_string() + ": " + why + "; its later settings are lost"));
    }
    result_.diagnostics.insert(result_.diagnostics.end(), outcome.diagnostics.begin(), outcome.diagnostics.end());
    const auto after = interp_->get_var(kExecuteScripts);
    if (after.has_value() && after != before) log_execute_scripts(file, after->to_string());
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
      if (auto value = interp_->get_var(name)) result_.variables.emplace(name, value->to_string());
    }
    const auto path = result_.variables.find("XSCHEM_LIBRARY_PATH");
    const std::string value = path == result_.variables.end() ? std::string{} : path->second;
    std::map<std::string, std::string> env(input_.env.begin(), input_.env.end());
    env.try_emplace("HOME", input_.document_folder.generic_string());
    result_.search_path = SearchPath::parse(value, env, fs_);
  }

  platform::FileSystem& fs_;
  const XschemrcInput& input_;
  editor::EditorState editor_;
  std::unique_ptr<script::Interpreter> interp_;
  XschemrcResult result_;
};

}  // namespace

XschemrcResult SearchPath::from_xschemrc(platform::FileSystem& fs, const XschemrcInput& input) {
  XschemrcRun run(fs, input);
  return run.run();
}

}  // namespace hefesto::schem
