// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A headless command session for the `schematic` and `xschem` tests: local
// file services rooted in a fresh temporary folder, the built-ins and
// `schematic` registered, an editor attached, and a project interpreter whose
// commands run through the dispatcher (so `xschem` reaches `schematic`).
//
// Owner: CMD.

#pragma once

#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/cmd/builtins.hpp"
#include "hefesto/cmd/command_log.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/schem/cmd/commands.hpp"
#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/project.hpp"

namespace hefesto::test {

class TempFolder {
 public:
  TempFolder() {
    static std::atomic<int> counter{0};
    path_ = std::filesystem::canonical(std::filesystem::temp_directory_path()) /
            ("hefesto-schem-cmd-" + std::to_string(::getpid()) + "-" + std::to_string(counter.fetch_add(1)));
    std::filesystem::remove_all(path_);
    std::filesystem::create_directories(path_);
  }
  ~TempFolder() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  TempFolder(const TempFolder&) = delete;
  TempFolder& operator=(const TempFolder&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
  [[nodiscard]] std::string str(const std::filesystem::path& relative = {}) const {
    return (relative.empty() ? path_ : path_ / relative).generic_string();
  }

  void write(const std::filesystem::path& relative, std::string_view text) const {
    std::filesystem::create_directories((path_ / relative).parent_path());
    std::ofstream out(path_ / relative, std::ios::binary);
    out << text;
  }
  [[nodiscard]] std::string read(const std::filesystem::path& relative) const {
    std::ifstream in(path_ / relative, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
  }

 private:
  std::filesystem::path path_;
};

class SchemSession {
 public:
  /// `environment` becomes the project interpreter's synthetic env (PDK_ROOT,
  /// PDK, CAD_ROOT, ...); the temporary folder is its granted root and
  /// PDK_ROOT, when given, a read-only root.
  explicit SchemSession(std::map<std::string, std::string, std::less<>> environment = {})
      : services_(platform::PortableOptions{.storage_root = folder_.path() / "app",
                                            .pdk_roots = {},
                                            .in_memory = false,
                                            .inline_tasks = true}),
        session_(project_) {
    (void)cmd::register_builtins(registry_);
    (void)schem::register_schematic_command(registry_);
    dispatcher_ = std::make_unique<cmd::Dispatcher>(registry_, services_.view(), project_, session_);
    dispatcher_->set_editor(&editor_);
    dispatcher_->add_observer(log_);
    script::SandboxPolicy policy;
    policy.granted_roots.push_back(folder_.path());
    // The PDK store is readable, never writable, as the front ends set it.
    if (const auto root = environment.find("PDK_ROOT"); root != environment.end()) {
      policy.read_only_roots.emplace_back(root->second);
    }
    policy.environment = std::move(environment);
    script::Interpreter& interp = script::open_project_interpreter(editor_, std::move(policy));
    (void)script::bind_registry(interp, registry_, *dispatcher_);
  }
  ~SchemSession() {
    script::close_project_interpreter(editor_);
    dispatcher_->set_editor(nullptr);
  }
  SchemSession(const SchemSession&) = delete;
  SchemSession& operator=(const SchemSession&) = delete;

  /// Runs `schematic <args>` with `origin`.
  cmd::CommandResult schematic(std::vector<std::string> args, cmd::Origin origin = cmd::Origin::script) {
    return dispatcher_->execute({"schematic", std::move(args), origin, std::nullopt});
  }
  /// Evaluates Tcl in the project interpreter.
  cmd::CommandResult tcl(std::string_view text) { return editor_.script.interp->evaluate(text); }

  [[nodiscard]] static std::string message(const cmd::CommandResult& result) {
    return result.diagnostics.empty() ? std::string{} : result.diagnostics.back().text;
  }

  TempFolder& folder() { return folder_; }
  editor::EditorState& editor() { return editor_; }
  schem::Session& schem() { return editor_.schem; }
  cmd::CommandLog& log() { return log_; }
  settings::ProjectSettings& project() { return project_; }
  cmd::ScriptSession& script() { return session_; }

 private:
  TempFolder folder_;
  platform::PortableServices services_;
  settings::ProjectSettings project_;
  cmd::ScriptSession session_;
  cmd::CommandRegistry registry_;
  cmd::CommandLog log_;
  editor::EditorState editor_;
  std::unique_ptr<cmd::Dispatcher> dispatcher_;
};

/// A schematic with one resistor `R1` and one transistor `M1`.
inline constexpr std::string_view kTwoInstances =
    "v {xschem version=3.4.8RC file_version=1.3}\n"
    "G {}\nK {}\nV {}\nS {}\nE {}\n"
    "N 0 30 0 70 {lab=mid}\n"
    "C {res.sym} 0 0 0 0 {name=R1 value=1k}\n"
    "C {nmos4.sym} 100 0 1 0 {name=M1 model=nch w=1.0u l=0.18u}\n";

}  // namespace hefesto::test
