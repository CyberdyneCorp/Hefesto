// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared fixture of the script/tcl tests: a project interpreter whose
// sandbox grants one temporary project folder, a second temporary folder
// outside the granted roots, and an observer of the denials it publishes.

#pragma once

#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/events.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/script/tcl/sandbox.hpp"

namespace hefesto::test {

namespace fs = std::filesystem;

/// A new empty directory under the system temporary directory, with its
/// symbolic links resolved (macOS: /var → /private/var), removed with the
/// object.
class TempDir {
 public:
  explicit TempDir(std::string_view tag) {
    static std::atomic<int> counter{0};
    const fs::path base = fs::canonical(fs::temp_directory_path());
    path_ = base / ("hefesto-tcl-" + std::string{tag} + "-" + std::to_string(::getpid()) + "-" +
                    std::to_string(counter.fetch_add(1)));
    fs::remove_all(path_);
    fs::create_directories(path_);
  }
  ~TempDir() {
    std::error_code ignored;
    fs::remove_all(path_, ignored);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  [[nodiscard]] const fs::path& path() const noexcept { return path_; }
  [[nodiscard]] std::string str() const { return path_.string(); }

  /// Writes `text` to `relative` (directories created) and returns its path.
  fs::path write(const fs::path& relative, std::string_view text) const {
    const fs::path file = path_ / relative;
    fs::create_directories(file.parent_path());
    std::ofstream out{file, std::ios::binary};
    out << text;
    return file;
  }

 private:
  fs::path path_;
};

/// Collects the DeniedOperation events of one interpreter.
class DenialRecorder final : public script::ScriptObserver {
 public:
  void on_denied(const script::DeniedOperation& operation) override { denials.push_back(operation); }
  std::vector<script::DeniedOperation> denials;
};

/// Restores the process working directory (Tcl's `cd` changes it).
class CwdGuard {
 public:
  CwdGuard() : saved_(fs::current_path()) {}
  ~CwdGuard() {
    std::error_code ignored;
    fs::current_path(saved_, ignored);
  }
  CwdGuard(const CwdGuard&) = delete;
  CwdGuard& operator=(const CwdGuard&) = delete;

 private:
  fs::path saved_;
};

/// A project interpreter granting `project` only.
class SandboxedProject {
 public:
  explicit SandboxedProject(script::SandboxPolicy policy = {}) {
    policy.granted_roots.push_back(project.path());
    state.script.events.add(recorder);
    interp = &script::open_project_interpreter(state, std::move(policy));
  }
  ~SandboxedProject() {
    script::close_project_interpreter(state);
    state.script.events.remove(recorder);
  }
  SandboxedProject(const SandboxedProject&) = delete;
  SandboxedProject& operator=(const SandboxedProject&) = delete;

  /// Evaluates `script` and returns the result.
  cmd::CommandResult run(std::string_view script) { return interp->evaluate(script); }
  /// The string value of an ok evaluation ("<failed: message>" otherwise).
  std::string value(std::string_view script) {
    const auto result = run(script);
    if (!result.ok()) {
      return "<failed: " + (result.diagnostics.empty() ? std::string{} : result.diagnostics.front().text) + ">";
    }
    return result.value.to_string();
  }

  CwdGuard cwd;
  TempDir project{"project"};
  TempDir outside{"outside"};
  DenialRecorder recorder;
  editor::EditorState state;
  script::Interpreter* interp = nullptr;
};

/// The codes of `result`'s diagnostics, in order.
inline std::vector<std::string> codes(const cmd::CommandResult& result) {
  std::vector<std::string> list;
  for (const auto& diagnostic : result.diagnostics) {
    list.push_back(diagnostic.code);
  }
  return list;
}

/// True when `result` carries a diagnostic with `code`.
inline bool has_code(const cmd::CommandResult& result, std::string_view code) {
  for (const auto& diagnostic : result.diagnostics) {
    if (diagnostic.code == code) {
      return true;
    }
  }
  return false;
}

/// The Tcl error message of a failed result.
inline std::string error_text(const cmd::CommandResult& result) {
  return result.ok() || result.diagnostics.empty() ? std::string{} : result.diagnostics.front().text;
}

}  // namespace hefesto::test
